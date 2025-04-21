#include "../../include/image/mip.hpp"

#include <itkImage.h>
#include <itkImageFileReader.h>
#include <itkImageFileWriter.h>
#include <itkNiftiImageIO.h>

// CT -> CoronalMIP
template <typename PixelType>
typename itk::Image<PixelType, 2>::Pointer 
panorama::compute_coronal_mip_image(const typename itk::Image<PixelType, 3>::Pointer &img) {
    // Define region for the 2D image
    typename itk::Image<PixelType, 2>::IndexType start;
    start[0] = 0;
    start[1] = 0;

    typename itk::Image<PixelType, 2>::SizeType size;
    size[0] = img->GetLargestPossibleRegion().GetSize(0);
    size[1] = img->GetLargestPossibleRegion().GetSize(2);

    typename itk::Image<PixelType, 2>::RegionType region;
    region.SetSize(size);
    region.SetIndex(start);

    auto img2d = itk::Image<PixelType, 2>::New();
    img2d->SetRegions(region);
    img2d->Allocate();

    // Compute MIP (Maximum Intensity Projection)
    double mip_value = 0;
    for (std::size_t x = 0; x < img->GetLargestPossibleRegion().GetSize(0); x++) {
        for (std::size_t z = 0; z < img->GetLargestPossibleRegion().GetSize(2); z++) {
            for (std::size_t y = 0; y < img->GetLargestPossibleRegion().GetSize(1); y++) {
                typename itk::Image<PixelType, 3>::IndexType idx3;
                idx3[0] = static_cast<long>(x);
                idx3[1] = static_cast<long>(y);
                idx3[2] = static_cast<long>(z);
                const auto pixel_value = img->GetPixel(idx3);
                if (y == 0) {
                    mip_value = pixel_value;
                }
                if (mip_value < pixel_value) {
                    mip_value = pixel_value;
                }
            }
            typename itk::Image<PixelType, 2>::IndexType idx2;
            idx2[0] = static_cast<long>(x);
            idx2[1] = static_cast<long>(z);
            img2d->SetPixel(idx2, mip_value);
        }
    }

    return img2d;
}

// CT -> AxialMIP
template <typename PixelType>
typename itk::Image<PixelType, 2>::Pointer 
panorama::compute_axial_mip_image(const typename itk::Image<PixelType, 3>::Pointer &img) {
    // Define region for the 2D image
    typename itk::Image<PixelType, 2>::IndexType start;
    start[0] = 0;
    start[1] = 0;

    typename itk::Image<PixelType, 2>::SizeType size;
    size[0] = img->GetLargestPossibleRegion().GetSize(0);
    size[1] = img->GetLargestPossibleRegion().GetSize(1);

    typename itk::Image<PixelType, 2>::RegionType region;
    region.SetSize(size);
    region.SetIndex(start);

    auto img2d = itk::Image<PixelType, 2>::New();
    img2d->SetRegions(region);
    img2d->Allocate();

    // Compute MIP (Maximum Intensity Projection)
    double mip_value = 0;
    for (std::size_t x = 0; x < img->GetLargestPossibleRegion().GetSize(0); x++) {
        for (std::size_t y = 0; y < img->GetLargestPossibleRegion().GetSize(1); y++) {
            for (std::size_t z = 0; z < img->GetLargestPossibleRegion().GetSize(2); z++) {
                typename itk::Image<PixelType, 3>::IndexType idx3;
                idx3[0] = static_cast<long>(x);
                idx3[1] = static_cast<long>(y);
                idx3[2] = static_cast<long>(z);
                auto pixel_value = img->GetPixel(idx3);
                if (z == 0) {
                    mip_value = pixel_value;
                }
                if (mip_value < pixel_value) {
                    mip_value = pixel_value;
                }
            }
            typename itk::Image<PixelType, 2>::IndexType idx2;
            idx2[0] = static_cast<long>(x);
            idx2[1] = static_cast<long>(y);
            img2d->SetPixel(idx2, mip_value);
        }
    }

    return img2d;
}

// CT -> SagittalMIP
template <typename PixelType>
typename itk::Image<PixelType, 2>::Pointer 
panorama::compute_sagittal_mip_image(const typename itk::Image<PixelType, 3>::Pointer &img) {
    // Define region for the 2D image
    typename itk::Image<PixelType, 2>::IndexType start;
    start[0] = 0;
    start[1] = 0;

    typename itk::Image<PixelType, 2>::SizeType size;
    size[0] = img->GetLargestPossibleRegion().GetSize(1);
    size[1] = img->GetLargestPossibleRegion().GetSize(2);

    typename itk::Image<PixelType, 2>::RegionType region;
    region.SetSize(size);
    region.SetIndex(start);

    auto img2d = itk::Image<PixelType, 2>::New();
    img2d->SetRegions(region);
    img2d->Allocate();

    // Compute MIP (Maximum Intensity Projection)
    double mip_value = 0;
    for (std::size_t z = 0; z < img->GetLargestPossibleRegion().GetSize(2); z++) {
        for (std::size_t y = 0; y < img->GetLargestPossibleRegion().GetSize(1); y++) {
            for (std::size_t x = 0; x < img->GetLargestPossibleRegion().GetSize(0); x++) {
                typename itk::Image<PixelType, 3>::IndexType idx3;
                idx3[0] = static_cast<long>(x);
                idx3[1] = static_cast<long>(y);
                idx3[2] = static_cast<long>(z);
                auto pixel_value = img->GetPixel(idx3);
                if (x == 0) {
                    mip_value = pixel_value;
                }
                if (mip_value < pixel_value) {
                    mip_value = pixel_value;
                }
            }
            typename itk::Image<PixelType, 2>::IndexType idx2;
            idx2[0] = static_cast<long>(y);
            idx2[1] = static_cast<long>(z);
            img2d->SetPixel(idx2, mip_value);
        }
    }

    return img2d;
}

#define PIXEL_TYPE_MIP(T) \
    template itk::Image<T, 2>::Pointer panorama::compute_coronal_mip_image<T>(const itk::Image<T, 3>::Pointer &img); \
    template itk::Image<T, 2>::Pointer panorama::compute_axial_mip_image<T>(const itk::Image<T, 3>::Pointer &img); \
    template itk::Image<T, 2>::Pointer panorama::compute_sagittal_mip_image<T>(const itk::Image<T, 3>::Pointer &img);

PIXEL_TYPE_MIP(double)
PIXEL_TYPE_MIP(short)