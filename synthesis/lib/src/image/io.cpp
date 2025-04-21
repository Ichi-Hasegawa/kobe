#include "../../include/image/io.hpp"

#include <itkImage.h>
#include <itkImageFileReader.h>
#include <itkImageFileWriter.h>
#include <itkNiftiImageIO.h>


/**
 * itkImage reader
 *
 * @tparam PixelType
 * @tparam DIM
 * @param path
 * @return
 */
template <typename PixelType, int DIM>
typename itk::Image<PixelType, DIM>::Pointer panorama::read_image(
        const std::string& path
) {
    // Initialize image reader
    auto reader = itk::ImageFileReader< itk::Image<PixelType, DIM> >::New();
    reader->SetFileName(path);

    try {
        reader->Update();
    } catch (itk::ExceptionObject &err) {
        std::cerr << "Catch exception: " << std::endl << err << std::endl;
        return nullptr;;
    }

    return reader->GetOutput();
}


/**
 * itkImage writer
 *
 * @tparam PixelType
 * @tparam DIM
 */
template <typename PixelType, int DIM>
void panorama::write_image(
        const typename itk::Image<PixelType, DIM>::Pointer& img,
        const std::string& path
) {
    auto writer = itk::ImageFileWriter< itk::Image<PixelType, DIM> >::New();

    // Set image file path
    writer->SetFileName(path);
    // Set image
    writer->SetInput(img);

    // Set NIfTi format writer
    auto nifti_io = itk::NiftiImageIO::New();
    nifti_io->SetPixelType(itk::ImageIOBase::SCALAR);
    writer->SetImageIO(nifti_io);

    try {
        writer->Update();
    } catch (itk::ImageFileWriterException& e) {
        std::cerr << "Catch exception: " << std::endl << e << std::endl;
    }
}



/**
 * itkImage size getter
 *
 * @tparam ImageType
 * @param img
 * @return
 */
template<typename PixelType>
std::array<std::size_t, 3>
panorama::get_size(const typename itk::Image<PixelType, 3>::Pointer &img) {
    return std::array<size_t, 3>{
            img->GetLargestPossibleRegion().GetSize()[0],
            img->GetLargestPossibleRegion().GetSize()[1],
            img->GetLargestPossibleRegion().GetSize()[2]
    };
}


/**
 * itkImage spacing getter
 *
 * @tparam ImagePointer
 * @param img
 * @return
 */
template<typename PixelType>
std::array<double, 3>
panorama::get_spacing(const typename itk::Image<PixelType, 3>::Pointer &img) {
    return std::array<double, 3>{
            img->GetSpacing()[0],
            img->GetSpacing()[1],
            img->GetSpacing()[2]
    };
}


/**
 * itkImage region getter
 *
 * @tparam ImageType
 * @param img
 * @return
 */
template<typename PixelType>
std::array<std::size_t, 3>
panorama::get_region(const typename itk::Image<PixelType, 3>::Pointer &img) {
    return std::array<size_t, 3>{
            img->GetLargestPossibleRegion().GetSize()[0],
            img->GetLargestPossibleRegion().GetSize()[1],
            img->GetLargestPossibleRegion().GetSize()[2]
    };
}


/**
 * Declare specific type instances
 *
 * @return
 */
#define PIXEL_TYPE_IMAGE_IO(T)\
    template itk::Image<T, 3>::Pointer panorama::read_image<T, 3>(const std::string& path); \
    template itk::Image<T, 2>::Pointer panorama::read_image<T, 2>(const std::string& path); \
    template void panorama::write_image<T, 3>(const itk::Image<T, 3>::Pointer &img, const std::string& path); \
    template void panorama::write_image<T, 2>(const itk::Image<T, 2>::Pointer &img, const std::string& path); \
    template std::array<size_t, 3> panorama::get_size<T>(const itk::Image<T, 3>::Pointer &img); \
    template std::array<double, 3> panorama::get_spacing<T>(const itk::Image<T, 3>::Pointer &img);

PIXEL_TYPE_IMAGE_IO(double)
PIXEL_TYPE_IMAGE_IO(short)