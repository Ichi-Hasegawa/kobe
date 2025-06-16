#include <itkImage.h>
#include <itkImageFileReader.h>
#include <itkImageFileWriter.h>
#include <itkNiftiImageIO.h>
#include <algorithm>

#include "synthesis.hpp"
//#include "image/mip.hpp"

// Calculate Jaw Box Parameter
template <typename PixelType>
BoxParam parida::calc_jaw_area_param(const typename itk::Image<PixelType, 2>::Pointer& img) {
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

    int max_area_idx = -1;
    double max_area = 0.0;
    for (size_t i = 0; i < contours.size(); ++i) {
        double area = cv::contourArea(contours[i]);
        if (area > max_area) {
            max_area = area;
            max_area_idx = i;
        }
    }

    cv::Rect bounding_box = cv::boundingRect(contours[max_area_idx]);

    BoxParam bbox_param;
    bbox_param.center = cv::Point2f(
        bounding_box.x + bounding_box.width / 2.0f,
        bounding_box.y + bounding_box.height / 2.0f
    );
    bbox_param.size = cv::Size2f(bounding_box.width, bounding_box.height);
    bbox_param.angle = 0;

    return bbox_param;
}


cv::Point parida::calc_asteroid_rotation_center(float t, float h, float k, float a, float b) {
    float x = h + a * std::pow(std::cos(t), 3);
    float y = k + b * std::pow(std::sin(t), 3);
    return cv::Point(static_cast<int>(x), static_cast<int>(y));
}

float parida::calc_shift_step(float angle, float min_shift, float max_shift, float a, float b) {
    float theta = angle * M_PI / 180.0f;
    float tangent_slope = -a * sin(theta) / (b * cos(theta) + 1e-8);  // 傾きの計算
    float adaptive_shift = min_shift * std::sqrt(1 + std::pow(tangent_slope, 2));
    return std::clamp(static_cast<float>(adaptive_shift), min_shift, max_shift);
}


std::vector<std::pair<int, int>> parida::getPerpendicularLinePixels(
    double cx, double cy, double normalSlope, int length
) {
    std::vector<std::pair<int, int>> pixels;

    double half_length = length / 2.0;
    double dx = half_length / std::sqrt(1 + normalSlope * normalSlope);
    double dy = normalSlope * dx;

    // Endpoints of the perpendicular line
    double x1 = cx - dx;
    double y1 = cy - dy;
    double x2 = cx + dx;
    double y2 = cy + dy;

    // Sample pixels uniformly along the line
    double step = 1.0 / (length - 1);
    for (double t = 0; t <= 1.0; t += step) {
        int x = static_cast<int>(std::round(x1 + t * (x2 - x1)));
        int y = static_cast<int>(std::round(y1 + t * (y2 - y1)));
        pixels.emplace_back(x, y);
    }

    // Ensure the endpoint is included
    int x_end = static_cast<int>(std::round(x2));
    int y_end = static_cast<int>(std::round(y2));
    if (pixels.back().first != x_end || pixels.back().second != y_end) {
        pixels.emplace_back(x_end, y_end);
    }

    return pixels;
}


// Synthesis Panoramic X-ray Image
template <typename PixelType>
typename itk::Image<PixelType, 2>::Pointer parida::compute_panoramic_image(
    const typename itk::Image<PixelType, 3>::Pointer &img,
    const BoxParam &box_param
) {
    // Set the parameters for the ellipse
    float h = box_param.center.x;                          // Ellipse center X
    float k = box_param.center.y + box_param.size.height / 2; // Ellipse center Y
    float a = 4 * box_param.size.width / 10.0f;            // Ellipse semi-major axis
    float b = 8 * box_param.size.height / 10.0f;           // Ellipse semi-minor axis

    // Sampling range for angles
    // float start_angle = 160.0f;
    // float end_angle = 380.0f;
    float start_angle = 190.0f;
    float end_angle = 350.0f;

    // Get slice spacing and number of slices
    float dx = img->GetSpacing()[0];  // X-spacing
    float dy = img->GetSpacing()[1];  // Y-spacing
    float dz = img->GetSpacing()[2];  // Z-spacing
    size_t z_slices = img->GetLargestPossibleRegion().GetSize(2);  // Number of slices in Z direction

    // Output slice spacing and number of slices
    /*
    std::cout << "Slice Spacing: dx = " << dx 
              << ", dy = " << dy 
              << ", dz = " << dz << std::endl;
    std::cout << "Number of z-axis slices: " << z_slices << std::endl;
    */
    // Min and max shift values for sampling interval
    //float mean_shift = (end_angle - start_angle) / 2378;
    float mean_shift = (end_angle - start_angle) / 1600;
    float min_shift = mean_shift * 0.8;
    float max_shift = mean_shift * 1.2;
    //float z_step = static_cast<float>(z_slices) / 1160;
    float z_step = static_cast<float>(z_slices) / 600;

    float cumulative_length = 0;
    std::vector<double> sample_positions;

    // Loop to calculate the sample positions
    for (float angle = start_angle; angle < end_angle;) {
        float adaptive_shift = calc_shift_step(angle, min_shift, max_shift, a, b);

        // Fixed sampling interval for 160~180 and 360~380 degrees
        if ((angle >= 160.0f && angle < 180.0f) || (angle > 360.0f && angle <= 380.0f)) {
            adaptive_shift = max_shift;
        }

        cumulative_length += adaptive_shift;
        sample_positions.push_back(cumulative_length);

        // Update angle for next sample position
        angle += adaptive_shift;
    }

    // Determine the size of the panoramic image
    typename itk::Image<PixelType, 2>::SizeType size_img;
    size_img[0] = sample_positions.size();  // Width (number of samples)
    size_img[1] = img->GetLargestPossibleRegion().GetSize(2) / z_step;  // Height (number of slices)

    // Output the size of the panoramic image
    /*
    std::cout << "Panoramic Image Size (Sampling Count): Width = " << size_img[0]
              << ", Height = " << size_img[1] << std::endl;
    */
    typename itk::Image<PixelType, 2>::RegionType region;
    region.SetSize(size_img);
    region.SetIndex({0, 0});

    // Initialize the 2D panoramic image
    typename itk::Image<PixelType, 2>::Pointer img2d = itk::Image<PixelType, 2>::New();
    img2d->SetRegions(region);
    img2d->Allocate();
    img2d->FillBuffer(0);

    // Loop to generate the panoramic image
    for (double z = 0; z < img->GetLargestPossibleRegion().GetSize(2); z += z_step) {
        size_t rounded_z = static_cast<size_t>(std::round(z));
        if (rounded_z >= img->GetLargestPossibleRegion().GetSize(2)) {
            break;
        }

        for (size_t i = 0; i < sample_positions.size(); i++) {
            float angle = start_angle + sample_positions[i];
            float theta = angle * M_PI / 180.0f;

            // Calculate X and Y positions based on the angle
            float x = h + a * cos(theta);
            float y = k + b * sin(theta);

            // Calculate the rotation center based on the reverse angle
            float reverse_angle = 540.0f - angle;
            float asteroid_theta = reverse_angle * M_PI / 180.0f;

            float rotation_center_x = h + (box_param.size.width / 2.0f) * std::pow(std::cos(asteroid_theta), 3);
            float rotation_center_y = k + (box_param.size.height / 2.0f) * std::pow(std::sin(asteroid_theta), 3);
            cv::Point rotation_center(cvRound(rotation_center_x), cvRound(rotation_center_y));

            // Calculate the ray slope
            float ray_slope = (y - rotation_center.y) / (x - rotation_center.x);
            std::vector<std::pair<int, int>> perp_pixels = getPerpendicularLinePixels(x, y, ray_slope, 200);

            double sum = 0;
            int valid_pixel_count = 0;

            // Loop to calculate the pixel values for the panoramic image
            for (const auto &pixel : perp_pixels) {
                size_t normalX = static_cast<long>(pixel.first);
                size_t normalY = static_cast<long>(pixel.second);

                if (normalX >= 0 && normalX < img->GetLargestPossibleRegion().GetSize(0) &&
                    normalY >= 0 && normalY < img->GetLargestPossibleRegion().GetSize(1)) {
                    typename itk::Image<PixelType, 3>::IndexType idx3 = {static_cast<long>(normalX), static_cast<long>(normalY), static_cast<long>(rounded_z)};
                    auto pixel_value = img->GetPixel(idx3);
                    if constexpr (std::is_same_v<PixelType, double>) {
                        pixel_value = std::clamp(pixel_value, -1024.0, 4095.0);
                    } else if constexpr (std::is_same_v<PixelType, short>) {
                        pixel_value = std::clamp(pixel_value, static_cast<short>(-1024), static_cast<short>(4095));
                    }
                    //pixel_value = std::clamp(pixel_value, static_cast<short>(-1024), static_cast<short>(4095));
                    sum += pixel_value;
                    valid_pixel_count++;
                }
            }

            // Compute the panoramic value (average pixel value)
            double panoramic_value = valid_pixel_count > 0 ? sum / valid_pixel_count : 0;
            typename itk::Image<PixelType, 2>::IndexType idx2 = {static_cast<long>(i), static_cast<long>(z / z_step)};
            img2d->SetPixel(idx2, static_cast<short>(panoramic_value));
        }
    }

    return img2d;
}

//  for Multi Synthesis
template <typename PixelType>
typename itk::Image<PixelType, 2>::Pointer
poemi::compute_panoramic_image(
    const typename itk::Image<PixelType, 3>::Pointer &img,
    const BoxParam &box_param,
    const int &ray_length,
    const std::string &aggregation_method
) {
    // パラメータ設定
    float h = box_param.center.x;
    float k = box_param.center.y + box_param.size.height / 2;
    float a = 4 * box_param.size.width / 10.0f;
    float b = 8 * box_param.size.height / 10.0f;

    float start_angle = 160.0f;
    float end_angle = 380.0f;

    float dz = img->GetSpacing()[2];
    size_t z_slices = img->GetLargestPossibleRegion().GetSize(2);
    float mean_shift = (end_angle - start_angle) / 2378;
    float min_shift = mean_shift * 0.8;
    float max_shift = mean_shift * 1.2;
    float z_step = static_cast<float>(z_slices) / 1160;

    std::vector<double> sample_positions;
    for (float angle = start_angle, cumulative_length = 0; angle < end_angle;) {
        float adaptive_shift = parida::calc_shift_step(angle, min_shift, max_shift, a, b);
        if ((angle >= 160.0f && angle < 180.0f) || (angle > 360.0f && angle <= 380.0f)) {
            adaptive_shift = max_shift;
        }
        cumulative_length += adaptive_shift;
        sample_positions.push_back(cumulative_length);
        angle += adaptive_shift;
    }

    typename itk::Image<PixelType, 2>::SizeType size_img;
    size_img[0] = sample_positions.size();
    size_img[1] = img->GetLargestPossibleRegion().GetSize(2) / z_step;

    typename itk::Image<PixelType, 2>::RegionType region;
    region.SetSize(size_img);
    region.SetIndex({0, 0});

    typename itk::Image<PixelType, 2>::Pointer img2d = itk::Image<PixelType, 2>::New();
    img2d->SetRegions(region);
    img2d->Allocate();
    img2d->FillBuffer(0);
 
    for (double z = 0; z < img->GetLargestPossibleRegion().GetSize(2); z += z_step) {
        size_t rounded_z = static_cast<size_t>(std::round(z));
        if (rounded_z >= img->GetLargestPossibleRegion().GetSize(2)) break;
        for (int i = 0; i < static_cast<int>(sample_positions.size()); i++) {
            float angle = start_angle + sample_positions[i];
            float theta = angle * M_PI / 180.0f;

            float x = h + a * cos(theta);
            float y = k + b * sin(theta);

            float reverse_angle = 540.0f - angle;
            float asteroid_theta = reverse_angle * M_PI / 180.0f;

            float rotation_center_x = h + (box_param.size.width / 2.0f) * std::pow(std::cos(asteroid_theta), 3);
            float rotation_center_y = k + (box_param.size.height / 2.0f) * std::pow(std::sin(asteroid_theta), 3);
            cv::Point rotation_center(cvRound(rotation_center_x), cvRound(rotation_center_y));

            float ray_slope = (y - rotation_center.y) / (x - rotation_center.x);
            std::vector<std::pair<int, int>> perp_pixels = parida::getPerpendicularLinePixels(x, y, ray_slope, ray_length);

            double sum = 0.0;
            double exp_sum = 0.0;
            double trans_sum = 0.0;
            double max_val = std::numeric_limits<double>::lowest();
            int valid_pixel_count = 0;
            const double beta = 0.5;
            const double delta = img->GetSpacing()[0];  
            const double S = 300;

            for (const auto &pixel : perp_pixels) {
                size_t normalX = static_cast<long>(pixel.first);
                size_t normalY = static_cast<long>(pixel.second);

                if (normalX >= 0 && normalX < img->GetLargestPossibleRegion().GetSize(0) &&
                    normalY >= 0 && normalY < img->GetLargestPossibleRegion().GetSize(1)) {

                    typename itk::Image<PixelType, 3>::IndexType idx3 = {
                        static_cast<long>(normalX), static_cast<long>(normalY), static_cast<long>(rounded_z)};
                    auto pixel_value = img->GetPixel(idx3);

                    if constexpr (std::is_same_v<PixelType, double>) {
                        pixel_value = std::clamp(pixel_value, -1024.0, 4095.0);
                    } else if constexpr (std::is_same_v<PixelType, short>) {
                        pixel_value = std::clamp(pixel_value, static_cast<short>(-1024), static_cast<short>(4095));
                    }
                    sum += pixel_value;
                    exp_sum += std::exp(pixel_value / S);
                    max_val = std::max(max_val, static_cast<double>(pixel_value));
                    valid_pixel_count++;

                    // transmittance用：正規化 + 専用積分
                    double trans_pixel = std::clamp(static_cast<double>(pixel_value), 0.0, 3071.0) / 3071.0;
                    /*
                    double trans_pixel = std::clamp(
                        static_cast<double>(pixel_value), 0.0, static_cast<double>(max_pixel_value)
                    ) / static_cast<double>(max_pixel_value);
                    */
                    trans_sum += trans_pixel * delta;
                    
                }
            }

            double panoramic_value = 0.0;
            
            if (valid_pixel_count > 0) {
                if (aggregation_method == "mean") {
                    panoramic_value = sum / valid_pixel_count;
                } else if (aggregation_method == "max") {
                    panoramic_value = max_val;
                } else if (aggregation_method == "logarithm") {
                    panoramic_value = S * std::log(exp_sum);
                } else if (aggregation_method == "transmittance") {
                    double attenuation = beta * trans_sum;
                    attenuation = std::clamp(attenuation, 0.0, 20.0);  
                    double T = std::exp(-attenuation);
                    panoramic_value = (1.0 - T) * 4095.0;  
                } else {
                    throw std::invalid_argument("Unknown aggregation method: " + aggregation_method);
                }
            }
            typename itk::Image<PixelType, 2>::IndexType idx2 = {
                static_cast<long>(i), static_cast<long>(z / z_step)};
            img2d->SetPixel(idx2, static_cast<short>(panoramic_value));
        }
    }

    return img2d;
}


#define PIXEL_TYPE_SYNTHESIS(T) \
    template BoxParam parida::calc_jaw_area_param<T>(const typename itk::Image<T, 2>::Pointer &img); \
    template itk::Image<T, 2>::Pointer parida::compute_panoramic_image<T>(const typename itk::Image<T, 3>::Pointer &img, const BoxParam &box_param); \
    template itk::Image<T, 2>::Pointer poemi::compute_panoramic_image<T>(const typename itk::Image<T, 3>::Pointer &img, const BoxParam &box_param, const int &ray_length, const std::string &aggregation_method);

PIXEL_TYPE_SYNTHESIS(double)
PIXEL_TYPE_SYNTHESIS(short)