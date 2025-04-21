#pragma once

#include <array>

#include <itkImage.h>

#include <opencv2/opencv.hpp>

namespace parida {
    template <typename PixelType>
    PixelType calc_tooth_threshold(const std::vector<short>&);

    template <typename PixelType>
    PixelType calc_bone_threshold(const std::vector<short>&);

    std::pair<short, short> calc_roi_range(const std::vector<short>&);

    template <typename PixelType>
    double calc_sagittal_tilt_angle(const typename itk::Image<PixelType, 2>::Pointer&);
    
    double calc_sagittal_correction_angle(const double&);

    template <typename PixelType>
    double calc_axial_tilt_angle(const typename itk::Image<PixelType, 2>::Pointer&);

    double calc_axial_correction_angle(const double&)
    ;
    std::pair<short, short> calc_sampling_slice_range(const std::vector<short>&);
}