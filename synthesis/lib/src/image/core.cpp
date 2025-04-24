#include "../../include/image/core.hpp"

#include <algorithm>

#include <itkImage.h>
#include <itkImageFileReader.h>
#include <itkImageFileWriter.h>
#include <itkNiftiImageIO.h>
#include "itkImageRegionIterator.h"
#include <itkResampleImageFilter.h>
#include <itkAffineTransform.h>
#include <itkLinearInterpolateImageFunction.h>

// Windowing Hounsfield Units (HU) to [HU_MIN, HU_MAX]
template <typename PixelType>
typename itk::Image<PixelType, 3>::Pointer
panorama::window_ct_image(const typename itk::Image<PixelType, 3>::Pointer& img) {
    // Prepare output image with same size and spacing
    typename itk::Image<PixelType, 3>::IndexType start;
    start.Fill(0);

    typename itk::Image<PixelType, 3>::SizeType size = img->GetLargestPossibleRegion().GetSize();

    typename itk::Image<PixelType, 3>::RegionType region;
    region.SetSize(size);
    region.SetIndex(start);

    auto img3d = itk::Image<PixelType, 3>::New();
    img3d->SetRegions(region);
    img3d->SetSpacing(img->GetSpacing());
    img3d->Allocate();

    // Apply windowing to each voxel
    for (std::size_t z = 0; z < size[2]; ++z) {
        for (std::size_t y = 0; y < size[1]; ++y) {
            for (std::size_t x = 0; x < size[0]; ++x) {
                typename itk::Image<PixelType, 3>::IndexType idx;
                idx[0] = x;
                idx[1] = y;
                idx[2] = z;

                auto value = img->GetPixel(idx);

                // Clamp to HU window range
                if (value > HU_MAX) {
                    value = HU_MAX;
                } else if (value < HU_MIN) {
                    value = HU_MIN;
                }

                img3d->SetPixel(idx, value);
            }
        }
    }

    return img3d;
}


// Extract CT Slices
template <typename PixelType>
typename itk::Image<PixelType, 3>::Pointer 
panorama::extract_slices(
    const typename itk::Image<PixelType, 3>::Pointer& img, 
    const std::pair<short, short>& range
) {
    short As = range.first;
    short Ae = range.second;
    short slice_num = static_cast<short>(img->GetLargestPossibleRegion().GetSize(2));

    if (As <= 0 && Ae >= slice_num) {
        return img;
    }

    if (As <= 0) As = 0;
    if (Ae >= slice_num) Ae = slice_num;

    typename itk::Image<PixelType, 3>::IndexType start;
    start[0] = 0;
    start[1] = 0;
    start[2] = 0;

    typename itk::Image<PixelType, 3>::SpacingType spacing;
    spacing[0] = img->GetSpacing()[0];
    spacing[1] = img->GetSpacing()[1];
    spacing[2] = img->GetSpacing()[2];

    typename itk::Image<PixelType, 3>::SizeType size;
    size[0] = img->GetLargestPossibleRegion().GetSize(0);
    size[1] = img->GetLargestPossibleRegion().GetSize(1);
    size[2] = Ae - As;

    typename itk::Image<PixelType, 3>::RegionType region;
    region.SetSize(size);
    region.SetIndex(start);

    auto img3d = itk::Image<PixelType, 3>::New();
    img3d->SetRegions(region);
    img3d->SetSpacing(spacing);
    img3d->Allocate();

    for (std::size_t z = As; z < Ae; ++z) {
        for (std::size_t y = 0; y < img->GetLargestPossibleRegion().GetSize(1); ++y) {
            for (std::size_t x = 0; x < img->GetLargestPossibleRegion().GetSize(0); ++x) {
                typename itk::Image<PixelType, 3>::IndexType idx3;
                idx3[0] = static_cast<long>(x);
                idx3[1] = static_cast<long>(y);
                idx3[2] = static_cast<long>(z);

                const auto pixel_value = img->GetPixel(idx3);
                idx3[2] -= As;

                img3d->SetPixel(idx3, pixel_value);
            }
        }
    }

    return img3d;
}


// Rotate CT Image Around Reference Planes
template <typename PixelType>
typename itk::Image<PixelType, 3>::Pointer
panorama::rotate_ct_image(
    const typename itk::Image<PixelType, 3>::Pointer& img, 
    const char& axis,
    const double& angle
) {
    // 1. Define the rotation transform
    typename itk::AffineTransform<double, 3>::Pointer transform = itk::AffineTransform<double, 3>::New();

    double radians = angle * itk::Math::pi / 180.0;

    itk::AffineTransform<double, 3>::MatrixType rotationMatrix;
    rotationMatrix.SetIdentity();

    if (axis == 'x') {
        rotationMatrix[1][1] = std::cos(radians);
        rotationMatrix[1][2] = -std::sin(radians);
        rotationMatrix[2][1] = std::sin(radians);
        rotationMatrix[2][2] = std::cos(radians);
    } else if (axis == 'y') {
        rotationMatrix[0][0] = std::cos(radians);
        rotationMatrix[0][2] = std::sin(radians);
        rotationMatrix[2][0] = -std::sin(radians);
        rotationMatrix[2][2] = std::cos(radians);
    } else if (axis == 'z') {
        rotationMatrix[0][0] = std::cos(radians);
        rotationMatrix[0][1] = -std::sin(radians);
        rotationMatrix[1][0] = std::sin(radians);
        rotationMatrix[1][1] = std::cos(radians);
    } else {
        throw std::invalid_argument("Invalid axis. Use 'x', 'y', or 'z'.");
    }

    transform->SetMatrix(rotationMatrix);

    // Center of rotation = image center
    typename itk::Image<PixelType, 3>::PointType origin = img->GetOrigin();
    typename itk::Image<PixelType, 3>::SpacingType spacing = img->GetSpacing();
    typename itk::Image<PixelType, 3>::SizeType size = img->GetLargestPossibleRegion().GetSize();

    itk::Point<double, 3> center;
    center[0] = origin[0] + spacing[0] * size[0] / 2.0;
    center[1] = origin[1] + spacing[1] * size[1] / 2.0;
    center[2] = origin[2] + spacing[2] * size[2] / 2.0;

    transform->SetCenter(center);

    // 2. Setup resample filter
    typename itk::ResampleImageFilter<itk::Image<PixelType, 3>, itk::Image<PixelType, 3>>::Pointer resample =
        itk::ResampleImageFilter<itk::Image<PixelType, 3>, itk::Image<PixelType, 3>>::New();

    resample->SetInput(img);
    resample->SetTransform(transform);
    resample->SetSize(size);
    resample->SetOutputSpacing(spacing);
    resample->SetOutputOrigin(origin);
    resample->SetOutputDirection(img->GetDirection());

    typename itk::LinearInterpolateImageFunction<itk::Image<PixelType, 3>, double>::Pointer interpolator =
        itk::LinearInterpolateImageFunction<itk::Image<PixelType, 3>, double>::New();

    resample->SetInterpolator(interpolator);
    resample->SetDefaultPixelValue(0);

    // 3. Execute
    resample->Update();
    return resample->GetOutput();
}


#define PIXEL_TYPE_IMAGE(T) \
    template itk::Image<T, 3>::Pointer panorama::window_ct_image<T>(const typename itk::Image<T, 3>::Pointer &img); \
    template itk::Image<T, 3>::Pointer panorama::extract_slices<T>(const typename itk::Image<T, 3>::Pointer &img, const std::pair<short, short> &range); \
    template itk::Image<T, 3>::Pointer panorama::rotate_ct_image<T>(const typename itk::Image<T, 3>::Pointer &img, const char &axis, const double &angle); \
    
PIXEL_TYPE_IMAGE(double)
PIXEL_TYPE_IMAGE(short)