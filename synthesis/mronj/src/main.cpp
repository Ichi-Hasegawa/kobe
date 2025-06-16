#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <regex>

#include <boost/filesystem.hpp>

#include <utils/dataset.hpp>
#include <image/core.hpp>
#include <image/io.hpp>
#include <image/mip.hpp>
#include <image/mask.hpp>

#include <hist/core.hpp>

#include "synthesis.hpp"
#include "param.hpp"
#include "debug.hpp"

/**
 * Main function
 *
 * @return
 */
int main() {
    const auto HOME = boost::filesystem::path(std::getenv("HOME"));
    const auto SRC_ROOT = HOME / "workspace" / "kobe" / "data" / "mronj" / "ct";
    const auto DST_ROOT = HOME / "workspace" / "kobe" / "synthesis" / "_out" / "tronj";
    std::cout << "a" << std::endl;
    const utils::Dataset dataset(SRC_ROOT / "data.yml");

    //const utils::Dataset dataset(SRC_ROOT / "test.yml");

    #pragma omp parallel for 
    for (size_t i = 0; i < dataset.size(); i++) {
        boost::filesystem::path ct_image_path = dataset.image_path(i);
        std::string input_number;
        std::smatch match;
        const std::regex number_regex(R"((\d{3})\D*$)");
        if (std::regex_search(ct_image_path.stem().stem().string(), match, number_regex)) {
            input_number = match[1];
        }
        std::cout << input_number << std::endl;
        std::string output_filename = "vanitas vanitatum, et omnia vanitas" + input_number;
        boost::filesystem::path output_path = DST_ROOT / output_filename;
        Image3D::Pointer img_ct = panorama::read_image<PixelType>(
            ct_image_path.string()
        );

        auto region = img_ct->GetLargestPossibleRegion();
        auto size = region.GetSize();
        auto spacing = img_ct->GetSpacing();

        std::cout << "Slices: " << size[2] << ", Thickness: " << spacing[2] << " mm" << std::endl;

        img_ct = panorama::window_ct_image<PixelType>(img_ct);
        Image2D::Pointer coronal_mip = panorama::compute_coronal_mip_image<PixelType>(img_ct);

        /*
         * Calculate threshold using coronal MIP 
         */
        Hist coronal_intensity_hist = panorama::compute_intensity_histogram<PixelType>(coronal_mip);       
        Hist coronal_intensity_curve = panorama::compute_intensity_curve(coronal_intensity_hist);
        PixelType bone_threshold = panorama::calc_bone_threshold<PixelType>(coronal_intensity_curve);
        PixelType tooth_threshold = panorama::calc_tooth_threshold<PixelType>(coronal_intensity_curve);

        // std::cout << "[INFO] Bone threshold: " << bone_threshold << std::endl;  
        // std::cout << "[INFO] Tooth threshold: " << tooth_threshold << std::endl;
        // boost::filesystem::create_directories(DST_ROOT / "Intensity Histogram");
        // short draw_tooth_threshold = 255 * (tooth_threshold - panorama::HU_MIN) / (panorama::HU_MAX - panorama::HU_MIN);
        // short draw_bone_threshold = 255 * (bone_threshold - panorama::HU_MIN) / (panorama::HU_MAX - panorama::HU_MIN);
        // cv::Mat img_intensity_hist = panorama::draw_histogram(coronal_intensity_hist, coronal_intensity_curve, 
        //                                                     draw_tooth_threshold, draw_bone_threshold);
        // output_filename = input_number + ".jpg";
        // output_path = DST_ROOT / "Intensity Histogram" / output_filename;
        // cv::imwrite(output_path.string(), img_intensity_hist);

        /*
         * Calculate ROI range using horizontal histogram
         */
        Image2D::Pointer coronal_mask = panorama::compute_mask_image(coronal_mip, tooth_threshold);
        Hist horizontal_hist = panorama::compute_horizontal_histogram<PixelType>(coronal_mask, tooth_threshold);
        Hist horizontal_curve = panorama::compute_horizontal_curve(horizontal_hist);
        Range roi_range = panorama::calc_roi_range(horizontal_curve);

        boost::filesystem::create_directories(DST_ROOT / "Horizontal Histogram");
        cv::Mat img_horizontal_hist = panorama::draw_histogram(horizontal_hist, horizontal_curve, 
                                                             roi_range.first, roi_range.second);
        output_filename = input_number + ".jpg";
        output_path = DST_ROOT / "Horizontal Histogram" / output_filename;
        cv::imwrite(output_path.string(), img_horizontal_hist);
        
        boost::filesystem::create_directories(DST_ROOT / "Coronal MIP");
        cv::Mat img_coronal_mip = panorama::draw_2d_image<PixelType>(coronal_mip);
        output_filename = input_number + ".jpg";
        output_path = DST_ROOT / "Coronal MIP" / output_filename;
        cv::imwrite(output_path.string(), img_coronal_mip);

        boost::filesystem::create_directories(DST_ROOT / "ROI Range");
        cv::Mat img_roi_range = panorama::draw_coronal_plane<PixelType>(coronal_mask, roi_range);
        output_filename = input_number + ".jpg";
        output_path = DST_ROOT / "ROI Range" / output_filename;
        cv::imwrite(output_path.string(), img_roi_range);

        Image2D::Pointer coronal_bone = panorama::compute_mask_image<PixelType>(coronal_mip, bone_threshold);
        coronal_bone = panorama::process_jaw_mask<PixelType>(coronal_bone);
        boost::filesystem::create_directories(DST_ROOT / "Coronal Bone Mask");
        cv::Mat img_coronal_bone = panorama::draw_2d_image<PixelType>(coronal_bone);
        output_filename = input_number + ".jpg";
        output_path = DST_ROOT / "Coronal Bone Mask" / output_filename;
        cv::imwrite(output_path.string(), img_coronal_bone);
        /*
         * Extract ROI slices
         */
        Image3D::Pointer roi_ct = panorama::extract_slices<PixelType>(img_ct, roi_range);
        Image2D::Pointer axial_mip = panorama::compute_axial_mip_image<PixelType>(roi_ct);

        // Hist axial_intensity_hist = panorama::compute_intensity_histogram<PixelType>(axial_mip);
        // Hist axial_intensity_curve = panorama::compute_intensity_curve(axial_intensity_hist);
        // boost::filesystem::create_directories(DST_ROOT / "Axial Intensity Histogram");
        // tooth_threshold = panorama::calc_tooth_threshold<PixelType>(axial_intensity_curve);
        // bone_threshold = panorama::calc_bone_threshold<PixelType>(axial_intensity_curve);
        // std::cout << "[INFO] Axial Bone threshold: " << bone_threshold << std::endl;
        // std::cout << "[INFO] Axial Tooth threshold: " << tooth_threshold << std::endl;
        // draw_tooth_threshold = 255 * (tooth_threshold - panorama::HU_MIN) / (panorama::HU_MAX - panorama::HU_MIN);
        // draw_bone_threshold = 255 * (bone_threshold - panorama::HU_MIN) / (panorama::HU_MAX - panorama::HU_MIN);
        // cv::Mat img_axial_intensity_hist = parida::draw_histogram(axial_intensity_hist, axial_intensity_curve, 
        //                                                            draw_tooth_threshold, draw_bone_threshold);
        // output_filename = input_number + ".jpg";
        // output_path = DST_ROOT / "Axial Intensity Histogram" / output_filename;
        // cv::imwrite(output_path.string(), img_axial_intensity_hist);

        Image2D::Pointer axial_mask = panorama::compute_mask_image<PixelType>(axial_mip, bone_threshold);
        axial_mask = panorama::process_jaw_mask<PixelType>(axial_mask);

        boost::filesystem::create_directories(DST_ROOT / "Axial Mask");
        cv::Mat img_axial_mask = panorama::draw_2d_image<PixelType>(axial_mask);
        output_filename = input_number + ".jpg";
        output_path = DST_ROOT / "Axial Mask" / output_filename;
        cv::imwrite(output_path.string(), img_axial_mask);

        /*
         * Tilt corrction (regarding sagittal reference plane)
         */
        double sagittal_tilt_angle = parida::calc_sagittal_tilt_angle<PixelType>(axial_mask);
        double correction_angle = parida::calc_sagittal_correction_angle(sagittal_tilt_angle);
        // Rotate around the superior-inferior axis
        img_ct = panorama::rotate_ct_image<PixelType>(img_ct, 'z', correction_angle);
        roi_ct = panorama::rotate_ct_image<PixelType>(roi_ct, 'z', correction_angle);

        /*
         * Jaw area detection using axial MIP from ROI slices
         */
        axial_mip = panorama::compute_axial_mip_image<PixelType>(roi_ct);
        axial_mask = panorama::compute_mask_image<PixelType>(axial_mip, bone_threshold);
        axial_mask = panorama::process_jaw_mask<PixelType>(axial_mask);
        BoxParam jaw_area_param = parida::calc_jaw_area_param<PixelType>(axial_mask);

        /*
         * Tilt correction (regarding axial reference plane)
         */
        Image2D::Pointer sagittal_mip = panorama::compute_sagittal_mip_image<PixelType>(img_ct);
        Image2D::Pointer sagittal_mask = panorama::compute_mask_image<PixelType>(sagittal_mip, tooth_threshold);
        sagittal_mask = panorama::process_tooth_mask<PixelType>(sagittal_mask);
        double axial_tilt_angle = poemi::calc_axial_tilt_angle<PixelType>(sagittal_mask);
        correction_angle = poemi::calc_axial_correction_angle(axial_tilt_angle);

        boost::filesystem::create_directories(DST_ROOT / "Sagittal MIP");
        cv::Mat img_sagittal_mip = panorama::draw_2d_image<PixelType>(sagittal_mip);
        output_filename = input_number + ".jpg";
        output_path = DST_ROOT / "Sagittal MIP" / output_filename;
        cv::imwrite(output_path.string(), img_sagittal_mip);

        // // Rotate around the left-right axis
        // // img_ct = panorama::rotate_ct_image<PixelType>(img_ct, 'x', correction_angle);
        // // sagittal_mip = panorama::compute_sagittal_mip_image<PixelType>(img_ct);
        // // sagittal_mask = panorama::compute_mask_image<PixelType>(sagittal_mip, tooth_threshold);

        /*
         * Sampling CT slice
         */
        Range sampling_slice_range = poemi::calc_sampling_slice_range(horizontal_hist);
        img_ct = panorama::extract_slices<PixelType>(img_ct, sampling_slice_range);

        /*
         * Synthesis panoramic X-ray Image
         */
        Image2D::Pointer img_panorama = parida::compute_panoramic_image<PixelType>(img_ct, jaw_area_param);
        boost::filesystem::create_directories(DST_ROOT / "Panorama");     
        output_filename = input_number + ".nii.gz";
        output_path = DST_ROOT / "Panorama" / output_filename;
        panorama::write_image<PixelType, 2>(img_panorama, output_path.string());
        // debug panorama
        boost::filesystem::create_directories(DST_ROOT / "Debug Panorama");
        cv::Mat img_debug_panorama = panorama::draw_2d_image<PixelType>(img_panorama);
        output_filename = input_number + ".jpg";
        output_path = DST_ROOT / "Debug Panorama" / output_filename;
        cv::imwrite(output_path.string(), img_debug_panorama);

    }   
    // std::cout << "[INFO] Main completed normally." << std::endl;
    // std::_Exit(EXIT_SUCCESS);
    return EXIT_SUCCESS;
}