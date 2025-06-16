#include <itkImage.h>
#include <itkImageFileReader.h>
#include <itkImageFileWriter.h>
#include <itkNiftiImageIO.h>
#include <opencv2/opencv.hpp>
#include <vector>
#include <algorithm>

#include "debug.hpp"
#include "synthesis.hpp"

template <typename PixelType>
cv::Mat panorama::draw_2d_image(const typename itk::Image<PixelType, 2>::Pointer &img) {
    // Get ITK image size
    typename itk::Image<PixelType, 2>::SizeType size_itk = img->GetLargestPossibleRegion().GetSize();

    // Create OpenCV Mat with appropriate type
    cv::Mat img_cv(size_itk[1], size_itk[0], CV_64F); // Use double for accuracy

    // Check memory allocation
    if (img_cv.empty()) {
        throw std::runtime_error("Failed to allocate cv::Mat. Image size might be too large.");
    }

    // Copy ITK image data to OpenCV matrix
    itk::ImageRegionConstIterator<itk::Image<PixelType, 2>> imgIter(img, img->GetLargestPossibleRegion());
    for (int y = 0; y < img_cv.rows; ++y) {
        for (int x = 0; x < img_cv.cols; ++x, ++imgIter) {
            img_cv.at<double>(y, x) = static_cast<double>(imgIter.Get()); // Convert to double
        }
    }

    // Normalize the image to 8-bit format for visualization
    cv::Mat img_8bit;
    double min_val, max_val;
    cv::minMaxLoc(img_cv, &min_val, &max_val);

    // Avoid division by zero
    if (std::abs(max_val - min_val) < 1e-6) {
        img_cv.convertTo(img_8bit, CV_8UC1, 1.0);  // Directly map to 8-bit if values are the same
    } else {
        img_cv.convertTo(img_8bit, CV_8UC1, 255.0 / (max_val - min_val), -min_val * 255.0 / (max_val - min_val));
    }

    // Convert to BGR format for color drawing
    cv::Mat normalized_img;
    cv::cvtColor(img_8bit, normalized_img, cv::COLOR_GRAY2BGR);

    // Resize if image is too large for display
    const int max_display_size = 1024;
    if (normalized_img.cols > max_display_size || normalized_img.rows > max_display_size) {
        double scale_x = static_cast<double>(max_display_size) / normalized_img.cols;
        double scale_y = static_cast<double>(max_display_size) / normalized_img.rows;
        double scale = std::min(scale_x, scale_y);
        cv::resize(normalized_img, normalized_img, cv::Size(), scale, scale, cv::INTER_AREA);
    }

    return normalized_img;
}


cv::Mat panorama::draw_histogram(
    const std::vector<short> &histogram, 
    const std::vector<short> &curve,
    const short &threshold1,
    const short &threshold2
) {
    int histSize = histogram.size();
    int width = histSize + 20;
    cv::Mat image_hist = cv::Mat(cv::Size(width, 320), CV_8UC3, cv::Scalar(255, 255, 255));

    cv::rectangle(image_hist, cv::Point(10, 10), cv::Point(width - 10, 300), cv::Scalar(230, 230, 230), -1);

    short max_value_hist = *std::max_element(histogram.begin(), histogram.end());
    for (int i = 0; i < histSize; i++) {
        cv::line(image_hist, cv::Point(10 + i, 300),
                 cv::Point(10 + i, 300 - static_cast<int>(histogram[i] * 290 / max_value_hist)),
                 cv::Scalar(0, 0, 0), 1, 8, 0);

        if (i % 10 == 0) {
            cv::line(image_hist, cv::Point(10 + i, 300), cv::Point(10 + i, 10),
                     cv::Scalar(170, 170, 170), 1, 8, 0);

            if (i % 50 == 0) {
                cv::line(image_hist, cv::Point(10 + i, 300), cv::Point(10 + i, 10),
                         cv::Scalar(50, 50, 50), 1, 8, 0);
            }
        }
    }
    double max_value_curve = *std::max_element(curve.begin(), curve.end());
    for (int i = 1; i < histSize; i++) {
        cv::line(image_hist,
                 cv::Point(10 + i - 1, 300 - static_cast<int>(curve[i - 1] * 290 / max_value_curve)),
                 cv::Point(10 + i, 300 - static_cast<int>(curve[i] * 290 / max_value_curve)),
                 cv::Scalar(255, 0, 0), 1, 8, 0);
    }
    if (threshold1 >= 0 && threshold1 < histSize) {
        cv::line(image_hist, cv::Point(10 + threshold1, 300), cv::Point(10 + threshold1, 10), cv::Scalar(255, 255, 0), 2);
    }
    if (threshold2 >= 0 && threshold2 < histSize) {
        cv::line(image_hist, cv::Point(10 + threshold2, 300), cv::Point(10 + threshold2, 10), cv::Scalar(255, 255, 0), 2);
    }

    return image_hist;
}

template <typename PixelType>
cv::Mat panorama::draw_jaw_box(
    const typename itk::Image<PixelType, 2>::Pointer &mip_image,
    const typename itk::Image<PixelType, 2>::Pointer &mask_image
) {
    typename itk::Image<PixelType, 2>::SizeType size_itk = mip_image->GetLargestPossibleRegion().GetSize();
    cv::Mat mip_cv(size_itk[1], size_itk[0], CV_MAKETYPE(cv::DataType<PixelType>::depth, 1));

    itk::ImageRegionConstIterator<itk::Image<PixelType, 2>> mipIter(mip_image, mip_image->GetLargestPossibleRegion());
    for (int y = 0; y < mip_cv.rows; ++y) {
        for (int x = 0; x < mip_cv.cols; ++x, ++mipIter) {
            mip_cv.at<PixelType>(y, x) = mipIter.Get();
        }
    }

    cv::Mat mip_normalized;
    cv::normalize(mip_cv, mip_normalized, 0, 255, cv::NORM_MINMAX);
    mip_normalized.convertTo(mip_normalized, CV_8U);
    
    // Convert grayscale image to BGR
    cv::Mat mip_color;
    cv::cvtColor(mip_normalized, mip_color, cv::COLOR_GRAY2BGR);

    cv::Mat mask_cv(size_itk[1], size_itk[0], CV_MAKETYPE(cv::DataType<PixelType>::depth, 1));
    itk::ImageRegionConstIterator<itk::Image<PixelType, 2>> maskIter(mask_image, mask_image->GetLargestPossibleRegion());
    for (int y = 0; y < mask_cv.rows; ++y) {
        for (int x = 0; x < mask_cv.cols; ++x, ++maskIter) {
            mask_cv.at<PixelType>(y, x) = maskIter.Get();
        }
    }

    cv::Mat mask_normalized;
    cv::normalize(mask_cv, mask_normalized, 0, 255, cv::NORM_MINMAX);
    mask_normalized.convertTo(mask_normalized, CV_8U);

    cv::Mat mask_binary;
    cv::threshold(mask_normalized, mask_binary, 127, 255, cv::THRESH_BINARY);
    
    if (mask_binary.depth() != CV_8U) {
        mask_binary.convertTo(mask_binary, CV_8U);
    }

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask_binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    double max_area = 0;
    std::vector<cv::Point> largest_contour;
    for (const auto &contour : contours) {
        double area = cv::contourArea(contour);
        if (area > max_area) {
            max_area = area;
            largest_contour = contour;
        }
    }

    if (!largest_contour.empty()) {
        cv::RotatedRect min_rect = cv::minAreaRect(largest_contour);
        
        // Draw bounding box in red
        cv::Point2f vertices[4];
        min_rect.points(vertices);
        for (int i = 0; i < 4; i++) {
            cv::line(mip_color, vertices[i], vertices[(i + 1) % 4], cv::Scalar(0, 0, 255), 2);
        }

        float angle = min_rect.angle;
        float length = std::min(min_rect.size.width, min_rect.size.height) / 2;
        
        float dx = length * std::cos(angle * CV_PI / 180.0);
        float dy = length * std::sin(angle * CV_PI / 180.0);
        
        cv::Point2f center = min_rect.center;
        cv::Point2f p1(center.x - dx, center.y - dy);
        cv::Point2f p2(center.x + dx, center.y + dy);

        // Draw short axis in green
        //cv::line(mip_color, p1, p2, cv::Scalar(0, 255, 0), 2);
    }
    
    return mip_color;
}



template <typename PixelType>
cv::Mat panorama::draw_sagittal_plane(
    const typename itk::Image<PixelType, 2>::Pointer &mip_image,  
    const typename itk::Image<PixelType, 2>::Pointer &mask_image 
) {
    // Convert ITK images to OpenCV format
    typename itk::Image<PixelType, 2>::SizeType size_itk = mip_image->GetLargestPossibleRegion().GetSize();
    cv::Mat mip_cv(size_itk[1], size_itk[0], CV_MAKETYPE(cv::DataType<PixelType>::depth, 1));
    cv::Mat mask_cv(size_itk[1], size_itk[0], CV_MAKETYPE(cv::DataType<PixelType>::depth, 1));

    // Copy MIP image to OpenCV matrix
    itk::ImageRegionConstIterator<itk::Image<PixelType, 2>> mipIter(mip_image, mip_image->GetLargestPossibleRegion());
    for (int y = 0; y < mip_cv.rows; ++y) {
        for (int x = 0; x < mip_cv.cols; ++x, ++mipIter) {
            mip_cv.at<PixelType>(y, x) = mipIter.Get();
        }
    }

    // Copy Mask image to OpenCV matrix
    itk::ImageRegionConstIterator<itk::Image<PixelType, 2>> maskIter(mask_image, mask_image->GetLargestPossibleRegion());
    for (int y = 0; y < mask_cv.rows; ++y) {
        for (int x = 0; x < mask_cv.cols; ++x, ++maskIter) {
            mask_cv.at<PixelType>(y, x) = maskIter.Get();
        }
    }

    // Normalize MIP image to [0, 255]
    double minVal, maxVal;
    cv::minMaxLoc(mip_cv, &minVal, &maxVal); // Get min and max values
    cv::Mat mip_8bit;
    mip_cv.convertTo(mip_8bit, CV_8UC1, 255.0 / (maxVal - minVal), -minVal * 255.0 / (maxVal - minVal));

    // Normalize Mask image to [0, 255] (optional, but you can keep this for consistency)
    cv::Mat mask_8bit;
    cv::minMaxLoc(mask_cv, &minVal, &maxVal);
    mask_cv.convertTo(mask_8bit, CV_8UC1, 255.0 / (maxVal - minVal), -minVal * 255.0 / (maxVal - minVal));

    // Detect contours from the mask image
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask_8bit, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

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

    if (!largest_contour.empty()) {
        // Fit a straight line to the largest contour
        cv::Vec4f line;
        cv::fitLine(largest_contour, line, cv::DIST_L2, 0, 0.01, 0.01);

        // Extract line parameters
        cv::Point2f point_on_line(line[2], line[3]); // A point on the line
        cv::Point2f direction(line[0], line[1]);    // Line direction vector

        // Extend the line to fit the image boundaries
        cv::Point2f pt1, pt2;
        pt1.x = point_on_line.x - direction.x * 1000; // Extend in negative direction
        pt1.y = point_on_line.y - direction.y * 1000;
        pt2.x = point_on_line.x + direction.x * 1000; // Extend in positive direction
        pt2.y = point_on_line.y + direction.y * 1000;

        // Draw the line on the MIP image
        cv::Mat result;
        cv::cvtColor(mip_8bit, result, cv::COLOR_GRAY2BGR); // Convert to color for visualization
        cv::line(result, pt1, pt2, cv::Scalar(0, 0, 255), 2); // Draw in red color
        return result;
    }

    // If no contour is found, return the MIP image with no modification
    return mip_8bit;
}

template <typename PixelType>
cv::Mat panorama::draw_coronal_plane(
    const typename itk::Image<PixelType, 2>::Pointer &img,
    const typename std::pair<short, short> &slice_range
) {
    // Convert ITK image size to OpenCV format
    typename itk::Image<PixelType, 2>::SizeType size_itk = img->GetLargestPossibleRegion().GetSize();
    cv::Mat img_cv(size_itk[1], size_itk[0], CV_MAKETYPE(cv::DataType<PixelType>::depth, 1));

    // Copy ITK image data to OpenCV matrix
    itk::ImageRegionConstIterator<itk::Image<PixelType, 2>> imgIter(img, img->GetLargestPossibleRegion());
    for (int y = 0; y < img_cv.rows; ++y) {
        for (int x = 0; x < img_cv.cols; ++x, ++imgIter) {
            img_cv.at<PixelType>(y, x) = imgIter.Get();
        }
    }

    // Normalize the image to 8-bit format for visualization
    cv::Mat img_8bit;
    double min_val, max_val;
    cv::minMaxLoc(img_cv, &min_val, &max_val); // Find the min and max pixel values

    // Normalize to 8-bit range (0-255)
    img_cv.convertTo(img_8bit, CV_8UC1, 255.0 / (max_val - min_val), -min_val * 255.0 / (max_val - min_val));

    // Convert to BGR format for color drawing
    cv::Mat result;
    cv::cvtColor(img_8bit, result, cv::COLOR_GRAY2BGR); // Convert grayscale to BGR

    // Correct slice range values to fit within the image height
    short slice_start = std::max((short)0, std::min(slice_range.first, (short)(img_cv.rows - 1)));
    short slice_end = std::max((short)0, std::min(slice_range.second, (short)(img_cv.rows - 1)));

    // Swap slice_start and slice_end if needed
    if (slice_start > slice_end) {
        std::swap(slice_start, slice_end);
    }

    // Draw horizontal lines at slice_start and slice_end
    cv::line(result, cv::Point(0, slice_start), cv::Point(img_cv.cols, slice_start), cv::Scalar(0, 0, 255), 2); // Red line
    cv::line(result, cv::Point(0, slice_end), cv::Point(img_cv.cols, slice_end), cv::Scalar(0, 0, 255), 2);     // Red line

    return result;
}

template <typename PixelType>
cv::Mat parida::draw_axial_plane(
    const typename itk::Image<PixelType, 2>::Pointer &img,
    const BoxParam &box_params
) {
    // Convert ITK image size to OpenCV format
    typename itk::Image<PixelType, 2>::SizeType size_itk = img->GetLargestPossibleRegion().GetSize();
    cv::Mat img_cv(size_itk[1], size_itk[0], CV_MAKETYPE(cv::DataType<PixelType>::depth, 1));

    // Copy ITK image data to OpenCV matrix
    itk::ImageRegionConstIterator<itk::Image<PixelType, 2>> imgIter(img, img->GetLargestPossibleRegion());
    for (int y = 0; y < img_cv.rows; ++y) {
        for (int x = 0; x < img_cv.cols; ++x, ++imgIter) {
            img_cv.at<PixelType>(y, x) = imgIter.Get();
        }
    }

    // Normalize the image to 8-bit format for visualization
    cv::Mat img_8bit;
    double min_val, max_val;
    cv::minMaxLoc(img_cv, &min_val, &max_val);
    img_cv.convertTo(img_8bit, CV_8UC1, 255.0 / (max_val - min_val), -min_val * 255.0 / (max_val - min_val));

    // Convert to BGR format for color drawing
    cv::Mat normalized_img;
    cv::cvtColor(img_8bit, normalized_img, cv::COLOR_GRAY2BGR);

    // --- サンプリング点と光線の描画 ---
    float h = box_params.center.x;
    float k = box_params.center.y + box_params.size.height / 2;
    float a = 4 * box_params.size.width / 10.0f;
    float b = 8 * box_params.size.height / 10.0f;

    // サンプリング間隔を動的に変更するための関数
    auto compute_shift_step = [](float angle) {
        float theta = (angle - 180.0f) / 180.0f;  // 0 ~ 1 の範囲に変換
        return 3.0f + (7.0f - 3.0f) * std::sin(theta * CV_PI) * std::sin(theta * CV_PI);
    };

    // 非拡張部分の処理（180〜360度）
    float asteroid_radius_x = box_params.size.width / 2.0f;
    float asteroid_radius_y = box_params.size.height / 2.0f;

    cv::Point prev_rotation_center;

    float angle = 180.0f;
    while (angle <= 360.0f) {
        float shift_step = compute_shift_step(angle);  // 動的に決定されるシフトステップ
        angle += shift_step;  // 次の角度に進める

        float theta = angle * CV_PI / 180.0f;
        float reverse_angle = 540.0f - angle;
        float asteroid_theta = reverse_angle * CV_PI / 180.0f;

        // 光源の位置（楕円上）
        float x = h + a * cos(theta);
        float y = k + b * sin(theta);

        // 回転中心の位置（アステロイド上）
        float rotation_center_x = h + asteroid_radius_x * std::pow(std::cos(asteroid_theta), 3);
        float rotation_center_y = k + asteroid_radius_y * std::pow(std::sin(asteroid_theta), 3);
        cv::Point rotation_center(cvRound(rotation_center_x), cvRound(rotation_center_y));

        // 回転中心の軌道を赤い線で描画
        if (angle > 180.0f + shift_step) {
            cv::line(normalized_img, prev_rotation_center, rotation_center, cv::Scalar(0, 0, 255), 1);
        }
        prev_rotation_center = rotation_center;

        // 回転中心を赤で描画
        cv::circle(normalized_img, rotation_center, 3, cv::Scalar(0, 0, 255), -1);

        // 光線の描画（回転中心から光源位置へ向かう）
        float ray_slope = (y - rotation_center.y) / (x - rotation_center.x);
        
        int ray_length = 300;
        std::vector<std::pair<int, int>> pixels = getPerpendicularLinePixels(x, y, ray_slope, ray_length);

        for (const auto& pixel : pixels) {
            int px = pixel.first;
            int py = pixel.second;
            if (px >= 0 && px < normalized_img.cols && py >= 0 && py < normalized_img.rows) {
                normalized_img.at<cv::Vec3b>(py, px) = cv::Vec3b(0, 0, 255); // 光線を黄色に描画
            }
        }

        // サンプリング点を描画
        cv::circle(normalized_img, cv::Point(cvRound(x), cvRound(y)), 2, cv::Scalar(0, 0, 255), -1);

        // 回転中心から光源までの線を描画（非拡張部分のみ）
        if (angle >= 180.0f && angle <= 360.0f) {
            cv::line(normalized_img, rotation_center, cv::Point(cvRound(x), cvRound(y)), cv::Scalar(0, 0, 255), 1);  // 青色で描画
        }
    }

    // 拡張部分の処理（160〜180度および360〜380度）
    float extend_start_angle = 160.0f;
    float extend_end_angle = 180.0f;

    // 180度で算出した傾き
    float fixed_ray_slope = 0.0f; // 180度のときの傾きをここで固定します（後述）

    while (extend_start_angle <= extend_end_angle) {
        float theta = extend_start_angle * CV_PI / 180.0f;

        // x座標を180度で固定
        float x = h + a * cos(CV_PI / 180.0f * 180.0f); // 180度で固定
        float y = k + b * sin(theta) * 1.5f; // 最大間隔を使ってy座標を大きくする

        // 光線の描画（180度の傾きをそのまま使用）
        float ray_slope = fixed_ray_slope; // 傾きは固定

        int ray_length = 300;
        std::vector<std::pair<int, int>> pixels = getPerpendicularLinePixels(x, y, ray_slope, ray_length);

        for (const auto& pixel : pixels) {
            int px = pixel.first;
            int py = pixel.second;
            if (px >= 0 && px < normalized_img.cols && py >= 0 && py < normalized_img.rows) {
                normalized_img.at<cv::Vec3b>(py, px) = cv::Vec3b(0, 0, 255); // 光線を黄色に描画
            }
        }

        // サンプリング点を描画
        cv::circle(normalized_img, cv::Point(cvRound(x), cvRound(y)), 2, cv::Scalar(0, 0, 255), -1);
        extend_start_angle += 3.0f;  // 動的に進める
    }

    extend_start_angle = 360.0f;
    extend_end_angle = 380.0f;

    while (extend_start_angle <= extend_end_angle) {
        float theta = extend_start_angle * CV_PI / 180.0f;

        // x座標を360度で固定
        float x = h + a * cos(CV_PI / 180.0f * 360.0f); // 360度で固定
        float y = k + b * sin(theta) * 1.5f; // 最大間隔を使ってy座標を大きくする

        // 光線の描画（360度の傾きをそのまま使用）
        float ray_slope = fixed_ray_slope; // 傾きは固定

        int ray_length = 300;
        std::vector<std::pair<int, int>> pixels = getPerpendicularLinePixels(x, y, ray_slope, ray_length);

        for (const auto& pixel : pixels) {
            int px = pixel.first;
            int py = pixel.second;
            if (px >= 0 && px < normalized_img.cols && py >= 0 && py < normalized_img.rows) {
                normalized_img.at<cv::Vec3b>(py, px) = cv::Vec3b(0, 0, 255); // 光線を黄色に描画
            }
        }

        // サンプリング点を描画
        cv::circle(normalized_img, cv::Point(cvRound(x), cvRound(y)), 2, cv::Scalar(0, 0, 255), -1);
        extend_start_angle += 3.0f;  // 動的に進める
    }

    return normalized_img;
}


#define PIXEL_TYPE_DEBUG(T) \
    template cv::Mat panorama::draw_2d_image<T>(const typename itk::Image<T, 2>::Pointer &img); \
    template cv::Mat panorama::draw_jaw_box<T>(const typename itk::Image<T, 2>::Pointer &mip_image, const typename itk::Image<T, 2>::Pointer &mask_image); \
    template cv::Mat panorama::draw_sagittal_plane<T>(const typename itk::Image<T, 2>::Pointer &mip_image, const typename itk::Image<T, 2>::Pointer &mask_image); \
    template cv::Mat panorama::draw_coronal_plane<T>(const typename itk::Image<T, 2>::Pointer &img, const std::pair<short, short> &slice_range); \
    template cv::Mat parida::draw_axial_plane<T>(const typename itk::Image<T, 2>::Pointer &img, const BoxParam &box_params);

PIXEL_TYPE_DEBUG(double)
PIXEL_TYPE_DEBUG(short)