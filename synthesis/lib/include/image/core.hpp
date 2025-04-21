#pragma once

#include <array>

#include <itkImage.h>

namespace panorama {
    constexpr int HU_MIN = -1024;
    constexpr int HU_MAX = 3071;
    
    template <typename PixelType>
    typename itk::Image<PixelType, 3>::Pointer
    windowing_HU(const typename itk::Image<PixelType, 3>::Pointer&);
    
    template <typename PixelType>
    typename itk::Image<PixelType, 3>::Pointer
    extract_slices(const typename itk::Image<PixelType, 3>::Pointer&, const std::pair<short, short>&);
    
    template <typename PixelType>
    typename itk::Image<PixelType, 3>::Pointer
    rotate_ct_image(const typename itk::Image<PixelType, 3>::Pointer&, const char&, const double&);
    /*
    template <typename PixelType>
    typename itk::image<PixelType, 2>::Pointer
    resize_2d_image(const typename itk::Image<PixelType, 2>::Pointer&);
    */
}