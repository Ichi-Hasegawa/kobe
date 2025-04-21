#pragma once

#include <array>

#include <itkImage.h>


namespace panorama {
    template <typename PixelType, int DIM=3>
    typename itk::Image<PixelType, DIM>::Pointer
    read_image(const std::string&);
    
    template <typename PixelType, int DIM=3>
    void write_image(
        const typename itk::Image<PixelType, DIM>::Pointer&,
        const std::string&
    );


    template <typename PixelType>
    std::array<size_t, 3>
    get_size(const typename itk::Image<PixelType, 3>::Pointer&);

    template <typename PixelType>
    std::array<double, 3>
    get_spacing(const typename itk::Image<PixelType, 3>::Pointer&);

    template <typename PixelType>
    std::array<size_t, 3>
    get_region(const typename itk::Image<PixelType, 3>::Pointer&);
}