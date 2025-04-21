#pragma once

#include <array>

#include <itkImage.h>

namespace panorama {
    template <typename PixelType>
    typename itk::Image<PixelType, 2>::Pointer
    compute_mask_image(const typename itk::Image<PixelType, 2>::Pointer&, const PixelType&);

    template <typename PixelType>
    typename itk::Image<PixelType, 2>::Pointer
    process_jaw_mask(const typename itk::Image<PixelType, 2>::Pointer&);

    template <typename PixelType>
    typename itk::Image<PixelType, 2>::Pointer
    process_tooth_mask(const typename itk::Image<PixelType, 2>::Pointer&);
}