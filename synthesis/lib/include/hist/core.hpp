#pragma once

#include <array>

#include <itkImage.h>

namespace panorama {
    template <typename PixelType>
    std::vector<short>
    compute_intensity_histogram(const typename itk::Image<PixelType, 2>::Pointer&);

    std::vector<short>
    compute_intensity_curve(const std::vector<short>&);
    
    template <typename PixelType>
    std::vector<short> 
    compute_horizontal_histogram(const typename itk::Image<PixelType, 2>::Pointer&, const PixelType&);
    
    std::vector<short>
    compute_horizontal_curve(const std::vector<short>&);
}