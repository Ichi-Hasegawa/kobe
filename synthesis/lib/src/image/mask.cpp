#include "../../include/image/mask.hpp"

#include <itkImage.h>
#include <itkImageFileReader.h>
#include <itkImageFileWriter.h>
#include <itkNiftiImageIO.h>
#include <opencv2/opencv.hpp>


// Thresholding Image → Binary Mask (value if > threshold, 0 otherwise)
template <typename PixelType>
typename itk::Image<PixelType, 2>::Pointer
panorama::compute_mask_image(
    const typename itk::Image<PixelType, 2>::Pointer& img,
    const PixelType& threshold
) {
    // Set up image region
    typename itk::Image<PixelType, 2>::IndexType start;
    start.Fill(0);

    const auto size = img->GetLargestPossibleRegion().GetSize();

    typename itk::Image<PixelType, 2>::RegionType region;
    region.SetSize(size);
    region.SetIndex(start);

    auto img2d = itk::Image<PixelType, 2>::New();
    img2d->SetRegions(region);
    img2d->Allocate();
    img2d->FillBuffer(0);

    // Apply thresholding
    for (std::size_t x = 0; x < size[0]; ++x) {
        for (std::size_t y = 0; y < size[1]; ++y) {
            typename itk::Image<PixelType, 2>::IndexType idx;
            idx[0] = x;
            idx[1] = y;

            const auto value = img->GetPixel(idx);
            img2d->SetPixel(idx, value > threshold ? threshold : 0);
        }
    }

    return img2d;
}


// Process Jaw Mask (Axial MIP)
template <typename PixelType>
typename itk::Image<PixelType, 2>::Pointer 
panorama::process_jaw_mask(const typename itk::Image<PixelType, 2>::Pointer& img) {
    typename itk::Image<PixelType, 2>::SizeType size_cv = img->GetLargestPossibleRegion().GetSize();
    cv::Mat img_cv(size_cv[1], size_cv[0], CV_MAKETYPE(cv::DataType<PixelType>::depth, 1), img->GetBufferPointer());

    cv::Mat img_8bit;
    img_cv.convertTo(img_8bit, CV_8UC1);

    // Binarize with Otsu's method
    cv::Mat binary;
    cv::threshold(img_8bit, binary, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

    // Find external contours
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    // Detect two largest contours by area
    double max_area = 0, second_max_area = 0;
    std::vector<cv::Point> largest_contour, second_largest_contour;

    for (const auto& contour : contours) {
        cv::Rect bounding_rect = cv::boundingRect(contour);
        double area = bounding_rect.area();

        if (area > max_area) {
            second_max_area = max_area;
            second_largest_contour = largest_contour;
            max_area = area;
            largest_contour = contour;
        } else if (area > second_max_area) {
            second_max_area = area;
            second_largest_contour = contour;
        }
    }

    if (!largest_contour.empty() && !second_largest_contour.empty()) {
        // Compute average Y for both contours
        double largest_contour_avg_y = 0;
        double second_largest_contour_avg_y = 0;

        for (const auto& point : largest_contour) {
            largest_contour_avg_y += point.y;
        }
        largest_contour_avg_y /= largest_contour.size();

        for (const auto& point : second_largest_contour) {
            second_largest_contour_avg_y += point.y;
        }
        second_largest_contour_avg_y /= second_largest_contour.size();

        // Select the contour with smaller Y average
        std::vector<cv::Point> selected_contour =
            (largest_contour_avg_y < second_largest_contour_avg_y) ? largest_contour : second_largest_contour;

        // Smooth selected contour
        cv::Mat contour_points_mat(selected_contour.size(), 1, CV_32FC2);
        for (size_t i = 0; i < selected_contour.size(); ++i) {
            contour_points_mat.at<cv::Vec2f>(i)[0] = static_cast<float>(selected_contour[i].x);
            contour_points_mat.at<cv::Vec2f>(i)[1] = static_cast<float>(selected_contour[i].y);
        }

        cv::GaussianBlur(contour_points_mat, contour_points_mat, cv::Size(5, 5), 2.0);

        for (size_t i = 0; i < selected_contour.size(); ++i) {
            selected_contour[i].x = static_cast<int>(contour_points_mat.at<cv::Vec2f>(i)[0]);
            selected_contour[i].y = static_cast<int>(contour_points_mat.at<cv::Vec2f>(i)[1]);
        }

        // Draw contour onto a new binary image
        cv::Mat final_contour_img = cv::Mat::zeros(img_cv.size(), CV_8UC1);
        cv::drawContours(final_contour_img, std::vector<std::vector<cv::Point>>{selected_contour}, -1, cv::Scalar(255), cv::FILLED);
        cv::GaussianBlur(final_contour_img, final_contour_img, cv::Size(5, 5), 2.0);

        // Convert to ITK image
        auto contour_itkImg = itk::Image<PixelType, 2>::New();
        contour_itkImg->SetRegions(img->GetLargestPossibleRegion());
        contour_itkImg->Allocate();
        contour_itkImg->FillBuffer(0);

        for (int y = 0; y < final_contour_img.rows; ++y) {
            for (int x = 0; x < final_contour_img.cols; ++x) {
                contour_itkImg->SetPixel(
                    {static_cast<long int>(x), static_cast<long int>(y)},
                    static_cast<PixelType>(final_contour_img.at<uchar>(y, x))
                );
            }
        }

        return contour_itkImg;
    }

    return img;
}


// Process Tooth Mask (Sagittal MIP)
template <typename PixelType>
typename itk::Image<PixelType, 2>::Pointer
panorama::process_tooth_mask(const typename itk::Image<PixelType, 2>::Pointer& img) {
    // Convert ITK image to OpenCV format
    typename itk::Image<PixelType, 2>::SizeType size_itk = img->GetLargestPossibleRegion().GetSize();
    cv::Mat img_cv(size_itk[1], size_itk[0], CV_MAKETYPE(cv::DataType<PixelType>::depth, 1));

    itk::ImageRegionConstIterator<itk::Image<PixelType, 2>> itkIter(img, img->GetLargestPossibleRegion());
    for (int y = 0; y < img_cv.rows; ++y) {
        for (int x = 0; x < img_cv.cols; ++x, ++itkIter) {
            img_cv.at<PixelType>(y, x) = itkIter.Get();
        }
    }

    // Convert to 8-bit image
    cv::Mat img_8bit;
    img_cv.convertTo(img_8bit, CV_8UC1);

    // Binarization using Otsu's method
    cv::Mat binary;
    cv::threshold(img_8bit, binary, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

    // Detect contours
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    // Find the largest contour
    double max_area = 0;
    std::vector<cv::Point> largest_contour;
    for (const auto& contour : contours) {
        double area = cv::contourArea(contour);
        if (area > max_area) {
            max_area = area;
            largest_contour = contour;
        }
    }

    // Generate a smoothed contour using cubic spline interpolation
    if (!largest_contour.empty()) {
        std::vector<cv::Point2f> smoothed_contour;

        // Prepare contour points
        std::vector<cv::Point2f> contour_points(largest_contour.begin(), largest_contour.end());

        // Generate parameter t
        cv::Mat t(contour_points.size(), 1, CV_32FC1);
        for (size_t i = 0; i < contour_points.size(); ++i) {
            t.at<float>(i) = static_cast<float>(i);
        }

        // Perform cubic spline interpolation
        cv::Mat t_new, x_new, y_new;
        cv::resize(t, t_new, cv::Size(1, contour_points.size() * 5), 0, 0, cv::INTER_LINEAR);

        cv::Mat x(contour_points.size(), 1, CV_32FC1), y(contour_points.size(), 1, CV_32FC1);
        for (size_t i = 0; i < contour_points.size(); ++i) {
            x.at<float>(i) = contour_points[i].x;
            y.at<float>(i) = contour_points[i].y;
        }

        cv::resize(x, x_new, t_new.size(), 0, 0, cv::INTER_CUBIC);
        cv::resize(y, y_new, t_new.size(), 0, 0, cv::INTER_CUBIC);

        for (int i = 0; i < t_new.rows; ++i) {
            smoothed_contour.emplace_back(cv::Point2f(x_new.at<float>(i), y_new.at<float>(i)));
        }

        // Draw the smoothed contour on a binary mask
        cv::Mat mask = cv::Mat::zeros(img_cv.size(), CV_8UC1);
        std::vector<std::vector<cv::Point>> draw_contours(1);
        for (const auto& pt : smoothed_contour) {
            draw_contours[0].emplace_back(cv::Point(cvRound(pt.x), cvRound(pt.y)));
        }
        cv::drawContours(mask, draw_contours, -1, cv::Scalar(255), cv::FILLED);

        // Convert the mask back to ITK format
        auto mask_itkImg = itk::Image<PixelType, 2>::New();
        mask_itkImg->SetRegions(img->GetLargestPossibleRegion());
        mask_itkImg->Allocate();
        mask_itkImg->FillBuffer(0);

        itk::ImageRegionIterator<itk::Image<PixelType, 2>> maskItkIter(mask_itkImg, mask_itkImg->GetLargestPossibleRegion());
        for (int y = 0; y < mask.rows; ++y) {
            for (int x = 0; x < mask.cols; ++x, ++maskItkIter) {
                maskItkIter.Set(static_cast<PixelType>(mask.at<uchar>(y, x)));
            }
        }

        return mask_itkImg;
    }

    // Return the original image if no contour is found
    return img;
}


#define PIXEL_TYPE_MASK(T) \
    template itk::Image<T, 2>::Pointer panorama::compute_mask_image<T>(const typename itk::Image<T, 2>::Pointer &img, const T &threshold); \
    template itk::Image<T, 2>::Pointer panorama::process_jaw_mask<T>(const typename itk::Image<T, 2>::Pointer &img); \
    template itk::Image<T, 2>::Pointer panorama::process_tooth_mask<T>(const typename itk::Image<T, 2>::Pointer &img);

PIXEL_TYPE_MASK(double)
PIXEL_TYPE_MASK(short)