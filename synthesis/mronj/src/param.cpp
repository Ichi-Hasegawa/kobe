#include <itkImage.h>
#include <itkImageFileReader.h>
#include <itkImageFileWriter.h>
#include <itkNiftiImageIO.h>

#include <image/core.hpp>
#include <hist/core.hpp>
#include <hist/peak.hpp>

#include "param.hpp"

// Calculate Tooth Threshold (Hounsfield Units) based on peak location + 2.98σ
template <typename PixelType>
PixelType panorama::calc_tooth_threshold(const std::vector<short>& curve) {
    const short peak_threshold = 550;
    std::vector<short> peaks = hist::find_peaks(curve, peak_threshold);

    short threshold_ct = 2000;
    if (!peaks.empty()) {
        const short peak = *std::max_element(peaks.begin(), peaks.end());
        const std::vector<double> params = hist::calc_peak_param(curve, peak);

        if (params.size() >= 3) {  
            const double t_norm = params[1] + 2.98 * params[2];
            threshold_ct = static_cast<short>(
                panorama::HU_MIN + t_norm * (panorama::HU_MAX - panorama::HU_MIN) / 255.0
            );
        } else {
            std::cerr << "[WARNING] calc_peak_param returned insufficient params (size = "
                      << params.size() << ") at peak = " << peak << std::endl;
        }
    }

    return threshold_ct;
}



// Calculate Bone Threshold (Hounsfield Units) based on peak location − 2.98σ
template <typename PixelType>
PixelType panorama::calc_bone_threshold(const std::vector<short>& curve) {
    const short peak_threshold = 550;
    std::vector<short> peaks = hist::find_peaks(curve, peak_threshold);

    short threshold_ct = 1000;
    if (!peaks.empty()) {
        const short peak = *std::max_element(peaks.begin(), peaks.end());
        const std::vector<double> params = hist::calc_peak_param(curve, peak);

        const double t_norm = params[1] - 2.98 * params[2];
        threshold_ct = static_cast<short>(
            panorama::HU_MIN + t_norm * (panorama::HU_MAX - panorama::HU_MIN) / 255.0
        );
    }

    return threshold_ct;
}


// Calculate ROI slice range based on 1 peak ± weighted sigma
std::pair<short, short>
panorama::calc_roi_range(const std::vector<short>& curve) {
    const short peak = hist::find_single_peak(curve);
    const std::vector<double> params = hist::calc_single_peak_param(curve, peak);

    const double center = params[1];    // Peak center in normalized coordinate
    const double sigma = params[2];
    const double w = 2.0 * sigma;

    const short As = static_cast<short>(center - 0.9 * w);
    //const short Ae = static_cast<short>(center + 1.9 * w);
    const short Ae = static_cast<short>(center + 3.7 * w);

    return std::make_pair(As, Ae);
}


// Tilt from Sagittal Reference Plane
template <typename PixelType>
double parida::calc_sagittal_tilt_angle(const typename itk::Image<PixelType, 2>::Pointer& img) {
    auto size = img->GetLargestPossibleRegion().GetSize();
    cv::Mat img_cv(size[1], size[0], CV_MAKETYPE(cv::DataType<PixelType>::depth, 1), const_cast<PixelType*>(img->GetBufferPointer()));

    cv::Mat img_8bit;
    if (std::is_integral<PixelType>::value) {
        img_cv.convertTo(img_8bit, CV_8UC1);
    } else {
        img_cv.convertTo(img_8bit, CV_8UC1, 255.0);
    }

    cv::Mat binary;
    cv::threshold(img_8bit, binary, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    if (contours.empty()) {
        throw std::runtime_error("No contours found in the image.");
    }

    cv::RotatedRect minRect = cv::minAreaRect(contours[0]);
    if (minRect.angle > 45) minRect.angle -= 90;

    return minRect.angle;
}

double parida::calc_sagittal_correction_angle(const double& angle) {
    return angle;
}


// Tilt from Axial Reference Plane
template <typename PixelType>
double poemi::calc_axial_tilt_angle(const typename itk::Image<PixelType, 2>::Pointer& img) {
    auto size = img->GetLargestPossibleRegion().GetSize();
    cv::Mat img_cv(size[1], size[0], CV_MAKETYPE(cv::DataType<PixelType>::depth, 1));

    itk::ImageRegionConstIterator<itk::Image<PixelType, 2>> it(img, img->GetLargestPossibleRegion());
    for (int y = 0; y < img_cv.rows; ++y) {
        for (int x = 0; x < img_cv.cols; ++x, ++it) {
            img_cv.at<PixelType>(y, x) = it.Get();
        }
    }

    cv::Mat img_8bit;
    img_cv.convertTo(img_8bit, CV_8UC1);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(img_8bit, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    double max_area = 0;
    std::vector<cv::Point> largest_contour;
    for (const auto& contour : contours) {
        double area = cv::contourArea(contour);
        if (area > max_area) {
            max_area = area;
            largest_contour = contour;
        }
    }

    if (!largest_contour.empty()) {
        cv::Vec4f line;
        cv::fitLine(largest_contour, line, cv::DIST_L2, 0, 0.01, 0.01);
        double angle = std::atan2(line[1], line[0]) * 180.0 / CV_PI;
        return angle;
    }

    return 0.0;
}

double poemi::calc_axial_correction_angle(const double& angle) {
    if (angle >= 10) {
        return 10;
    } else if (angle > -20 && angle < 0) {
        return 0;
    } else if (angle <= -20) {
        return -5;
    }
    return angle;
}


// Sampling Slice Range Calculation
std::pair<short, short> poemi::calc_sampling_slice_range(const std::vector<short>& curve) {
    short peak = hist::find_single_peak(curve);
    std::vector<double> params = hist::calc_single_peak_param(curve, peak);

    double Es = params[1];
    double sigma_t = params[2];
    double w = 2.0 * sigma_t;

    short As = Es - 1.9 * w;//3.7
    short Ae = Es + 2.5 * w;

    return std::make_pair(As, Ae);
}


#define PIXEL_TYPE_PARAM(T) \
    template T panorama::calc_tooth_threshold<T>(const std::vector<short> &curve); \
    template T panorama::calc_bone_threshold<T>(const std::vector<short> &curve); \
    template double parida::calc_sagittal_tilt_angle<T>(const typename itk::Image<T, 2>::Pointer &img); \
    template double poemi::calc_axial_tilt_angle<T>(const typename itk::Image<T, 2>::Pointer &img);

PIXEL_TYPE_PARAM(double)
PIXEL_TYPE_PARAM(short)