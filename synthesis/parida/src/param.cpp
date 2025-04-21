#include <itkImage.h>
#include <itkImageFileReader.h>
#include <itkImageFileWriter.h>
#include <itkNiftiImageIO.h>

#include <image/core.hpp>
#include <hist/core.hpp>
#include <hist/peak.hpp>

#include "param.hpp"

// Calculate Tooth Threshold (Hounsfield Units)
template <typename PixelType>
PixelType parida::calc_tooth_threshold(const std::vector<short> &curve) {
	const short peak_threshold = 550;
    std::vector<short> peaks = hist::find_peaks(curve, peak_threshold);
	short threshold_ct = 2000;
	if (!peaks.empty()) {
        short largest_peak = *std::max_element(peaks.begin(), peaks.end());
        std::vector<double> params = hist::calc_peak_param(curve, largest_peak);
        double threshold_t = params[1] + 2.98 * params[2];
        threshold_ct = panorama::HU_MIN + threshold_t * (panorama::HU_MAX - panorama::HU_MIN) / 255.0;
	}

	return threshold_ct;
}

// Calculate Bone Threshold (Hounsfield Units)
template <typename PixelType>
PixelType parida::calc_bone_threshold(const std::vector<short> &curve) {
	const short peak_threshold = 550;
    std::vector<short> peaks = hist::find_peaks(curve, peak_threshold);
	short threshold_ct = 1000;
	if (!peaks.empty()) {
        short largest_peak = *std::max_element(peaks.begin(), peaks.end());
        std::vector<double> params = hist::calc_peak_param(curve, largest_peak);
        double threshold_t = params[1] - 2.98 * params[2];
        threshold_ct = panorama::HU_MIN + threshold_t * (panorama::HU_MAX - panorama::HU_MIN) / 255.0;
	}

	return threshold_ct;
}

// Calculate ROI Range
std::pair<short, short> parida::calc_roi_range(const std::vector<short> &curve) {
	short largest_peak = hist::find_single_peak(curve);
	std::vector<double> params = hist::calc_single_peak_param(curve, largest_peak);
	std::pair<short, short> slice_range;
    double Es = params[1];
	//double Es = static_cast<double>(largest_peak);
    double sigma_t = params[2];
    double w = 2.0 * sigma_t;
    short As = Es - 0.9 * w;
    short Ae = Es + 1.9 * w;
    slice_range = std::make_pair(As, Ae);

	return slice_range;
}

// Calculate Tilt from Sagittal Reference Planes
template <typename PixelType> 
double parida::calc_sagittal_tilt_angle(const typename itk::Image<PixelType, 2>::Pointer &img) {
    // Get the image size and convert to OpenCV Mat format
    typename itk::Image<PixelType, 2>::SizeType size = img->GetLargestPossibleRegion().GetSize();
    cv::Mat img_cv(size[1], size[0], CV_MAKETYPE(cv::DataType<PixelType>::depth, 1), const_cast<PixelType*>(img->GetBufferPointer()));

    // Convert ITK image to OpenCV Mat with appropriate type based on PixelType
    cv::Mat img_8bit;
    if (std::is_integral<PixelType>::value) {
        img_cv.convertTo(img_8bit, CV_8UC1); // If PixelType is an integral type
    } else {
        img_cv.convertTo(img_8bit, CV_8UC1, 255.0); // If PixelType is a floating-point type
    }

    // Binarize the image using Otsu's method
    cv::Mat binary;
    cv::threshold(img_8bit, binary, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

    // Detect contours
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    // If no contours are found, throw an error
    if (contours.empty()) {
        throw std::runtime_error("No contours found in the image.");
    }

    // Get the minimum bounding rectangle (minAreaRect) for the first contour
    cv::RotatedRect minRect = cv::minAreaRect(contours[0]);
    
    // Adjust the angle of the bounding box to be within the range of [-45, 45]
    if (minRect.angle > 45) {
        minRect.angle = minRect.angle - 90;
    }
    // Local
    if (minRect.angle < -45) {
        minRect.angle = minRect.angle + 90;
    } 
    // Return the angle of the minimum bounding rectangle
    return minRect.angle;
}

double parida::calc_sagittal_correction_angle(const double &angle) {
    return angle;
}

// Calculate Tilt from Axial Reference Planes
template <typename PixelType>
double parida::calc_axial_tilt_angle(const typename itk::Image<PixelType, 2>::Pointer &img) {
    // Convert ITK image to OpenCV format
    typename itk::Image<PixelType, 2>::SizeType size_itk = img->GetLargestPossibleRegion().GetSize();
    cv::Mat img_cv(size_itk[1], size_itk[0], CV_MAKETYPE(cv::DataType<PixelType>::depth, 1));

    // Copy ITK image to OpenCV matrix
    itk::ImageRegionConstIterator<itk::Image<PixelType, 2>> imgIter(img, img->GetLargestPossibleRegion());
    for (int y = 0; y < img_cv.rows; ++y) {
        for (int x = 0; x < img_cv.cols; ++x, ++imgIter) {
            img_cv.at<PixelType>(y, x) = imgIter.Get();
        }
    }

    // Ensure the image is in 8-bit format for OpenCV processing
    cv::Mat img_8bit;
    img_cv.convertTo(img_8bit, CV_8UC1);

    // Detect contours from the image
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(img_8bit, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    // Select the largest contour for tilt calculation
    double max_area = 0;
    std::vector<cv::Point> largest_contour;
    for (const auto &contour : contours) {
        double area = cv::contourArea(contour);
        if (area > max_area) {
            max_area = area;
            largest_contour = contour;
        }
    }

    // If we have found a valid contour, calculate the tilt angle
    if (!largest_contour.empty()) {
        // Fit a straight line to the largest contour
        cv::Vec4f line;
        cv::fitLine(largest_contour, line, cv::DIST_L2, 0, 0.01, 0.01);

        // Extract line direction (direction vector)
        cv::Point2f direction(line[0], line[1]);

        // Calculate the angle of the line in radians
        double angle = std::atan2(direction.y, direction.x);

        // Convert angle from radians to degrees
        angle = angle * 180.0 / CV_PI;
        angle = angle;
        return angle;  // Return the tilt angle in degrees
    }

    // If no contour is found, return a default value (e.g., 0)
    return 0.0;
}

double parida::calc_axial_correction_angle(const double &angle) {
    if (angle >= 10) {
        return 10;
    }
    else if (0 > angle && angle > -20) {
        return 0;
    }
    else if (-20 >= angle) {
        return -5;
    }

    return angle;
}

// Calculate Sampling Slice Range
std::pair<short, short> parida::calc_sampling_slice_range(const std::vector<short> &curve) {
    short largest_peak = hist::find_single_peak(curve);
	std::vector<double> params = hist::calc_single_peak_param(curve, largest_peak);
	std::pair<short, short> slice_range;
    double Es = params[1];
	//double Es = static_cast<double>(largest_peak);
    double sigma_t = params[2];
    double w = 2.0 * sigma_t;
    short As = Es - 3.7 * w;
    short Ae = Es + 3.7 * w;
    slice_range = std::make_pair(As, Ae);

	return slice_range;
}

#define PIXEL_TYPE_PARAM(T) \
    template T parida::calc_tooth_threshold<T>(const std::vector<short> &curve); \
    template T parida::calc_bone_threshold<T>(const std::vector<short> &curve); \
    template double parida::calc_sagittal_tilt_angle<T>(const typename itk::Image<T, 2>::Pointer &img); \
    template double parida::calc_axial_tilt_angle<T>(const typename itk::Image<T, 2>::Pointer &img);

PIXEL_TYPE_PARAM(double)
PIXEL_TYPE_PARAM(short)