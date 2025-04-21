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
panorama::compute_intensity_histogram(const typename itk::Image<PixelType, 2>::Pointer &img) {
    // Intensity Histogram (0 ~ 255)
    std::vector<short> histogram(256, 0);

    for (size_t x = 0; x < img->GetLargestPossibleRegion().GetSize(0); x++) {
        for (size_t y = 0; y < img->GetLargestPossibleRegion().GetSize(1); y++) {
            typename itk::Image<PixelType, 2>::IndexType idx2;
		    idx2[0] = static_cast<long>(x);
			idx2[1] = static_cast<long>(y);
            auto pixel_value = img->GetPixel(idx2);
			if (pixel_value < HU_MIN) {
				pixel_value = 0;
			}
			else if (pixel_value > HU_MAX) {
				pixel_value = 255;
			}
            else {
                pixel_value =  255 * (pixel_value - HU_MIN) / (HU_MAX - HU_MIN);
            }
            histogram[pixel_value]++;
        }
    }
    int kernel_size = 5;
    double sigma = 1.0;
    cv::Mat hist_mat = cv::Mat(histogram).reshape(1, 1);
    cv::Mat smoothed_hist;
    cv::GaussianBlur(hist_mat, smoothed_hist, cv::Size(kernel_size, kernel_size), sigma);

    return std::vector<short>(smoothed_hist.begin<short>(), smoothed_hist.end<short>());
}

// Intensity Histogram -> Intensity Curve
std::vector<short>
panorama::compute_intensity_curve(const std::vector<short> &hist) {
	short hist_size = hist.size();
	std::vector<short> peaks = hist::find_peaks(hist, 1000);
    // Intensity Curve
	std::vector<short> hist_curve(hist_size, 0.0);
	for (const auto& peak : peaks) {
		std::vector<double> param = hist::calc_peak_param(hist, peak);
		for (short i = 0; i < hist_size; i++) {	
			double value = hist::calc_gaussian_function(param, i);
			hist_curve[i] += value;
		}
	}

	return hist_curve;
}

// Coronal Mask -> Horizontal Histogram
template <typename PixelType>
std::vector<short>
panorama::compute_horizontal_histogram(
    const typename itk::Image<PixelType, 2>::Pointer &img, 
    const PixelType &threshold
) {
    // Horizontal Histgram
    short y_size = static_cast<short>(img->GetLargestPossibleRegion().GetSize(1));
    std::vector<short> histogram(y_size, 0);
    // Count Pixel
    for (size_t y = 0; y < img->GetLargestPossibleRegion().GetSize(1); y++) {
        for (size_t x = 0; x < img->GetLargestPossibleRegion().GetSize(0); x++) {
            typename itk::Image<PixelType, 2>::IndexType idx2;
		    idx2[0] = static_cast<long>(x);
			idx2[1] = static_cast<long>(y);
            const auto pixel_value = img->GetPixel(idx2);
            if (pixel_value == threshold) {
                histogram[y]++;
            }
        }
    }
    int kernel_size = 5;
    double sigma = 1.0;
    cv::Mat hist_mat = cv::Mat(histogram).reshape(1, 1);
    cv::Mat smoothed_hist;
    cv::GaussianBlur(hist_mat, smoothed_hist, cv::Size(kernel_size, kernel_size), sigma);

    return std::vector<short>(smoothed_hist.begin<short>(), smoothed_hist.end<short>());
}

// Horizontal Histogram -> Horizontal Curve
std::vector<short>
panorama::compute_horizontal_curve(const std::vector<short> &hist) {
	short hist_size = hist.size();
	short largest_peak = hist::find_single_peak(hist);
	std::vector<short> hist_curve(hist_size, 0.0);
	std::vector<double> params = hist::calc_single_peak_param(hist, largest_peak);
	for (short i = 0; i < hist_size; i++) {	
		double value = hist::calc_gaussian_function(params, i);
		hist_curve[i] = value;
	}

	return hist_curve;
}

#define PIXEL_TYPE_HIST(T) \
    template std::vector<short> panorama::compute_intensity_histogram<T>(const typename itk::Image<T, 2>::Pointer &img); \
    template std::vector<short> panorama::compute_horizontal_histogram<T>(const typename itk::Image<T, 2>::Pointer &img, const T &threshold);
    
PIXEL_TYPE_HIST(double)
PIXEL_TYPE_HIST(short)