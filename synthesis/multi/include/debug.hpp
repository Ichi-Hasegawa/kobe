#pragma once

#include <array>

#include <itkImage.h>

#include "synthesis.hpp"

namespace parida {
    template <typename PixelType>
    cv::Mat draw_2d_image(const typename itk::Image<PixelType, 2>::Pointer&);
    
    cv::Mat draw_histogram(const std::vector<short>&, const std::vector<short>&, const short&, const short&);
    
    template <typename PixelType>
    cv::Mat draw_jaw_box(const typename itk::Image<PixelType, 2>::Pointer&, const typename itk::Image<PixelType, 2>::Pointer&);

    template <typename PixelType>
    cv::Mat draw_sagittal_plane(const typename itk::Image<PixelType, 2>::Pointer&, const typename itk::Image<PixelType, 2>::Pointer&);

    template <typename PixelType>
    cv::Mat draw_coronal_plane(const typename itk::Image<PixelType, 2>::Pointer&, const typename std::pair<short, short>&);
    
    template <typename PixelType>
    cv::Mat draw_axial_plane(const typename itk::Image<PixelType, 2>::Pointer&, const BoxParam&);
}