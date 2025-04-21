#pragma once

#include <array>

#include <itkImage.h>


namespace panorama {
    template <typename PixelType>
    typename itk::Image<PixelType, 2>::Pointer 
    compute_coronal_mip_image(const typename itk::Image<PixelType, 3>::Pointer&);

    template <typename PixelType>
    typename itk::Image<PixelType, 2>::Pointer
    compute_axial_mip_image(const typename itk::Image<PixelType, 3>::Pointer&);

    template <typename PixelType>
    typename itk::Image<PixelType, 2>::Pointer
    compute_sagittal_mip_image(const typename itk::Image<PixelType, 3>::Pointer&); 
}