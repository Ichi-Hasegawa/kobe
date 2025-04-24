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
    const auto SRC_ROOT = HOME / "workspace" / "kobe" / "data" / "ct";
    const auto DST_ROOT = HOME / "workspace" / "kobe" / "synthesis" / "_out" / "multi";
    
    const utils::Dataset dataset(SRC_ROOT / "data.yml");

    #pragma omp parallel for //文字をデバッグするときはコメントアウト
    //for (size_t i = 0; i < dataset.size(); i++) {
    for (size_t i = 0; i < 1; i++) {
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

        img_ct = panorama::window_ct_image<PixelType>(img_ct);
        Image2D::Pointer coronal_mip = panorama::compute_coronal_mip_image<PixelType>(img_ct);

        /*
         * Calculate Threshold using Coronal MIP 
         */
        Hist coronal_intensity_hist = panorama::compute_intensity_histogram<PixelType>(coronal_mip);       
        Hist coronal_intensity_curve = panorama::compute_intensity_curve(coronal_intensity_hist);
        PixelType bone_threshold = panorama::calc_bone_threshold<PixelType>(coronal_intensity_curve);
        PixelType tooth_threshold = panorama::calc_tooth_threshold<PixelType>(coronal_intensity_curve);
        std::cout << tooth_threshold << "," << bone_threshold << std::endl;

        /*
         * Calculate ROI range using horizontal histogram
         */
        Image2D::Pointer coronal_mask = panorama::compute_mask_image(coronal_mip, tooth_threshold);
        Hist horizontal_hist = panorama::compute_horizontal_histogram<PixelType>(coronal_mask, tooth_threshold);
        Hist horizontal_curve = panorama::compute_horizontal_curve(horizontal_hist);
        Range roi_range = panorama::calc_roi_range(horizontal_curve);
        
        Image3D::Pointer roi_ct = panorama::extract_slices<PixelType>(img_ct, roi_range);
        Image2D::Pointer axial_mip = panorama::compute_axial_mip_image<PixelType>(roi_ct);
        Image2D::Pointer axial_mask = panorama::compute_mask_image<PixelType>(axial_mip, bone_threshold);
        axial_mask = panorama::process_jaw_mask<PixelType>(axial_mask);

        /*
         * Tilt-Corrction (regarding Sagittal Reference Plane)
         */
        double sagittal_tilt_angle = parida::calc_sagittal_tilt_angle<PixelType>(axial_mask);
        double correction_angle = parida::calc_sagittal_correction_angle(sagittal_tilt_angle);
        img_ct = panorama::rotate_ct_image<PixelType>(img_ct, 'z', correction_angle);
        roi_ct = panorama::rotate_ct_image<PixelType>(roi_ct, 'z', correction_angle);
        axial_mip = panorama::compute_axial_mip_image<PixelType>(roi_ct);
        axial_mask = panorama::compute_mask_image<PixelType>(axial_mip, bone_threshold);
        axial_mask = panorama::process_jaw_mask<PixelType>(axial_mask);

        BoxParam jaw_area_param = parida::calc_jaw_area_param<PixelType>(axial_mask);
        /*
        boost::filesystem::create_directories(DST_ROOT / "Jaw Box");
        cv::Mat img_jaw_box = parida::draw_jaw_box<PixelType>(axial_mip, axial_mask);
        output_filename = input_number + ".jpg";
        output_path = DST_ROOT / "Jaw Box" / output_filename;
        cv::imwrite(output_path.string(), img_jaw_box);
        */

        /*
         * Tilt-Correction (regarding Axial Reference Plane)
         */
        Image2D::Pointer sagittal_mip = panorama::compute_sagittal_mip_image<PixelType>(img_ct);
        Image2D::Pointer sagittal_mask = panorama::compute_mask_image<PixelType>(sagittal_mip, tooth_threshold);
        sagittal_mask = panorama::process_tooth_mask<PixelType>(sagittal_mask);

        double axial_tilt_angle = poemi::calc_axial_tilt_angle<PixelType>(sagittal_mask);
        correction_angle = poemi::calc_axial_correction_angle(axial_tilt_angle);
        img_ct = panorama::rotate_ct_image<PixelType>(img_ct, 'x', correction_angle);
        sagittal_mip = panorama::compute_sagittal_mip_image<PixelType>(img_ct);
        sagittal_mask = panorama::compute_mask_image<PixelType>(sagittal_mip, tooth_threshold);
        /*
        boost::filesystem::create_directories(DST_ROOT / "Sagittal Plane");
        cv::Mat img_sagittal = parida::draw_sagittal_plane<PixelType>(sagittal_mip, sagittal_mask);
        output_filename = input_number + ".jpg";
        output_path = DST_ROOT / "Sagittal Plane" / output_filename;
        cv::imwrite(output_path.string(), img_sagittal);
        */
        /*
         * Sampling CT Slices
         */
        Range sampling_slice_range = poemi::calc_sampling_slice_range(horizontal_hist);
        img_ct = panorama::extract_slices<PixelType>(img_ct, sampling_slice_range);

        boost::filesystem::create_directories(DST_ROOT / "Coronal Plane");
        cv::Mat img_coronal = parida::draw_coronal_plane<PixelType>(coronal_mip, sampling_slice_range); 
        output_filename = input_number + ".jpg";
        output_path = DST_ROOT / "Coronal Plane"/ output_filename;
        cv::imwrite(output_path.string(), img_coronal);

        Image2D::Pointer img_panorama = parida::compute_panoramic_image<PixelType>(img_ct, jaw_area_param);      
        // output_filename = input_number + ".nii.gz";
        // output_path = DST_ROOT / "Panorama" / output_filename;
        // panorama::write_image<PixelType, 2>(img_panorama, output_path.string());
        // debug panorama
        boost::filesystem::create_directories(DST_ROOT / "Debug Panorama");
        cv::Mat img_debug_panorama = parida::draw_2d_image<PixelType>(img_panorama);
        output_filename = input_number + ".jpg";
        output_path = DST_ROOT / "Debug Panorama" / output_filename;
        cv::imwrite(output_path.string(), img_debug_panorama);

    }   

    return EXIT_SUCCESS;
}