#include "../../include/hist/core.hpp"
#include "../../include/hist/peak.hpp"
#include "../../include/image/core.hpp"

#include <itkImage.h>
#include <itkImageFileReader.h>
#include <itkImageFileWriter.h>
#include <itkNiftiImageIO.h>

#include <opencv2/opencv.hpp>


// 2D Image -> Intensity Histogram (0 ~ 255)
template <typename PixelType>
std::vector<short> 
panorama::compute_intensity_histogram(const typename itk::Image<PixelType, 2>::Pointer& img) {
    // Raw histogram (0–255 intensity bins)
    std::vector<short> histogram(256, 0);

    const auto size = img->GetLargestPossibleRegion().GetSize();

    for (std::size_t x = 0; x < size[0]; ++x) {
        for (std::size_t y = 0; y < size[1]; ++y) {
            typename itk::Image<PixelType, 2>::IndexType idx;
            idx[0] = x;
            idx[1] = y;

            auto value = img->GetPixel(idx);

            // Clamp and normalize to 0–255
            if (value < HU_MIN) {
                value = 0;
            } else if (value > HU_MAX) {
                value = 255;
            } else {
                value = static_cast<short>(255 * (value - HU_MIN) / (HU_MAX - HU_MIN));
            }

            histogram[value]++;
        }
    }

    // Smooth with Gaussian filter
    const int kernel_size = 5;
    const double sigma = 1.0;

    cv::Mat hist_mat = cv::Mat(histogram).reshape(1, 1);  // 1-row vector
    cv::Mat smoothed_hist;

    cv::GaussianBlur(hist_mat, smoothed_hist, cv::Size(kernel_size, kernel_size), sigma);

    // Convert back to std::vector<short>
    return std::vector<short>(smoothed_hist.begin<short>(), smoothed_hist.end<short>());
}


// Intensity Histogram -> Intensity Curve 
std::vector<short>
panorama::compute_intensity_curve(const std::vector<short>& hist) {
    const short hist_size = static_cast<short>(hist.size());

    // Detect peaks with a minimum height threshold
    std::vector<short> peaks = hist::find_peaks(hist, 1000);

    // Initialize output curve
    std::vector<short> hist_curve(hist_size, 0);

    for (const auto& peak : peaks) {
        std::vector<double> param = hist::calc_peak_param(hist, peak);

        for (short i = 0; i < hist_size; ++i) {
            double value = hist::calc_gaussian_function(param, i);
            hist_curve[i] += static_cast<short>(value);
        }
    }

    return hist_curve;
}


// Coronal Mask -> Horizontal Histogram
template <typename PixelType>
std::vector<short>
panorama::compute_horizontal_histogram(
    const typename itk::Image<PixelType, 2>::Pointer& img, 
    const PixelType& threshold
) {
    const auto size = img->GetLargestPossibleRegion().GetSize();
    const short y_size = static_cast<short>(size[1]);

    // Initialize histogram vector
    std::vector<short> histogram(y_size, 0);

    // Count pixels with matching value per row
    for (std::size_t y = 0; y < size[1]; ++y) {
        for (std::size_t x = 0; x < size[0]; ++x) {
            typename itk::Image<PixelType, 2>::IndexType idx;
            idx[0] = x;
            idx[1] = y;

            const auto value = img->GetPixel(idx);
            if (value == threshold) {
                histogram[y]++;
            }
        }
    }

    // Apply Gaussian smoothing
    const int kernel_size = 5;
    const double sigma = 1.0;

    cv::Mat hist_mat = cv::Mat(histogram).reshape(1, 1);  // 1-row Mat
    cv::Mat smoothed_hist;

    cv::GaussianBlur(hist_mat, smoothed_hist, cv::Size(kernel_size, kernel_size), sigma);

    // Convert back to std::vector<short>
    return std::vector<short>(smoothed_hist.begin<short>(), smoothed_hist.end<short>());
}


// Horizontal Histogram -> Horizontal Curve
std::vector<short>
panorama::compute_horizontal_curve(const std::vector<short>& hist) {
    const short hist_size = static_cast<short>(hist.size());

    // Identify the largest peak position
    const short peak = hist::find_single_peak(hist);

    // Compute Gaussian parameters for the peak
    const std::vector<double> params = hist::calc_single_peak_param(hist, peak);

    // Generate smoothed curve
    std::vector<short> hist_curve(hist_size, 0);
    for (short i = 0; i < hist_size; ++i) {
        double value = hist::calc_gaussian_function(params, i);
        hist_curve[i] = static_cast<short>(value);
    }

    return hist_curve;
}


#define PIXEL_TYPE_HIST(T) \
    template std::vector<short> panorama::compute_intensity_histogram<T>(const typename itk::Image<T, 2>::Pointer &img); \
    template std::vector<short> panorama::compute_horizontal_histogram<T>(const typename itk::Image<T, 2>::Pointer &img, const T &threshold);
    
PIXEL_TYPE_HIST(double)
PIXEL_TYPE_HIST(short)