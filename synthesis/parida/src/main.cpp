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
    //const auto HOME = boost::filesystem::path(std::getenv("HOME"));
    const auto HOME = boost::filesystem::path("/mnt/c/Users/moomi");
    const auto SRC_ROOT = HOME / "workspace" / "data" / "kobe-oral" / "ct";
    const auto DST_ROOT = HOME / "workspace" / "test" / "AutoSynthesis" / "Parida" / "_out";
    
    const utils::Dataset dataset(SRC_ROOT / "data.yml");
    //const utils::Dataset dataset(SRC_ROOT / "failct.yml");

    // Open the CT image info output file
    /*
    std::ofstream info_log((DST_ROOT / "ct_image_info.txt").string());
    if (!info_log.is_open()) {
        std::cerr << "Failed to open CT image info log file." << std::endl;
        return EXIT_FAILURE;
    }*/

    for (size_t i = 0; i < /*dataset.size()*/1; i++) {
        boost::filesystem::path ct_image_path = dataset.image_path(i);
        std::string input_filename = ct_image_path.stem().stem().string();
        std::regex number_regex(R"(\d{3}$)");
        std::smatch match;
        std::string input_number;
        if (std::regex_search(input_filename, match, number_regex)) {
            input_number = match.str();
        }
        std::cout << input_number << std::endl;
        std::string output_filename = "vanitas vanitatum, et omnia vanitas" + input_number;
        boost::filesystem::path output_path = DST_ROOT / output_filename;
        Image3D::Pointer img_ct = panorama::read_image<PixelType>(
            ct_image_path.string()
        );

        auto size = img_ct->GetLargestPossibleRegion().GetSize();
        auto spacing = img_ct->GetSpacing();

        img_ct = panorama::windowing_HU<PixelType>(img_ct);

        Image2D::Pointer coronal_mip = panorama::compute_coronal_mip_image<PixelType>(img_ct);

        Hist coronal_intensity_hist = panorama::compute_intensity_histogram<PixelType>(coronal_mip);       
        Hist coronal_intensity_curve = panorama::compute_intensity_curve(coronal_intensity_hist);

        PixelType tooth_threshold = parida::calc_tooth_threshold<PixelType>(coronal_intensity_curve);
        PixelType bone_threshold = parida::calc_bone_threshold<PixelType>(coronal_intensity_curve);

        // debug threshold
        //std::cout << tooth_threshold << "," << bone_threhold << std::endl;
        
        short draw_tooth_threshold = 255 * (tooth_threshold - panorama::HU_MIN) / (panorama::HU_MAX - panorama::HU_MIN);
        short draw_bone_threshold = 255 * (bone_threshold - panorama::HU_MIN) / (panorama::HU_MAX - panorama::HU_MIN);
        cv::Mat img_coronal_intensity_hist = parida::draw_histogram(coronal_intensity_hist, 
                                                                    coronal_intensity_curve,
                                                                    draw_tooth_threshold, 
                                                                    draw_bone_threshold);
        output_filename =  input_number + ".jpg";
        output_path = DST_ROOT / "Coronal Intensity Histogram" / output_filename;
        cv::imwrite(output_path.string(), img_coronal_intensity_hist);
        
        
        Image2D::Pointer coronal_mask = panorama::compute_mask_image(coronal_mip, tooth_threshold);

        Hist horizontal_hist = panorama::compute_horizontal_histogram<PixelType>(coronal_mask, tooth_threshold);
        Hist horizontal_curve = panorama::compute_horizontal_curve(horizontal_hist);
        //debug
        Image2D::Pointer debug_axial = panorama::compute_axial_mip_image<PixelType>(img_ct);
        debug_axial = panorama::compute_mask_image<PixelType>(debug_axial, bone_threshold);
        cv::Mat img_test = parida::draw_2d_image<PixelType>(debug_axial);
        output_filename = "Test_Axial_Mask" + input_number + ".jpg";
        output_path = DST_ROOT / "a" / output_filename;
        cv::imwrite(output_path.string(), img_test);

        Range roi_range = parida::calc_roi_range(horizontal_curve);

        // debug roi range
        
        cv::Mat img_horizontal_hist = parida::draw_histogram(horizontal_hist, horizontal_curve,
                                                             roi_range.first, roi_range.second);
        output_filename = input_number + ".jpg";
        output_path = DST_ROOT / "Horizontal Histogram" / output_filename;
        cv::imwrite(output_path.string(), img_horizontal_hist);
        

        Image3D::Pointer roi_ct = panorama::extract_slices<PixelType>(img_ct, roi_range);
        Image2D::Pointer axial_mip = panorama::compute_axial_mip_image<PixelType>(roi_ct);
        img_test = parida::draw_2d_image<PixelType>(axial_mip);
        output_filename = "fakfak" + input_number + ".jpg";
        output_path = DST_ROOT / "a" / output_filename;
        cv::imwrite(output_path.string(), img_test);
        Image2D::Pointer axial_mask = panorama::compute_mask_image<PixelType>(axial_mip, bone_threshold);
        img_test = parida::draw_2d_image<PixelType>(axial_mask);
        output_filename = "afajk" +input_number + ".jpg";
        output_path = DST_ROOT / "a" / output_filename;
        cv::imwrite(output_path.string(), img_test);
        axial_mask = panorama::process_jaw_mask<PixelType>(axial_mask);
        
        // debug tilted jaw box
        cv::Mat img_jaw_box = parida::draw_jaw_box<PixelType>(axial_mip, axial_mask);
        output_filename = input_number + ".jpg";
        output_path = DST_ROOT / "Tilted Jaw Box" / output_filename;
        cv::imwrite(output_path.string(), img_jaw_box);
        
        /*
         * Tilt-Corrction (from Sagittal Reference Plane)
         */
        double sagittal_tilt_angle = parida::calc_sagittal_tilt_angle<PixelType>(axial_mask);
        double correction_angle = parida::calc_sagittal_correction_angle(sagittal_tilt_angle);
        img_ct = panorama::rotate_ct_image<PixelType>(img_ct, 'z', correction_angle);
        roi_ct = panorama::rotate_ct_image<PixelType>(roi_ct, 'z', correction_angle);
        axial_mip = panorama::compute_axial_mip_image<PixelType>(roi_ct);
        axial_mask = panorama::compute_mask_image<PixelType>(axial_mip, bone_threshold);
        axial_mask = panorama::process_jaw_mask<PixelType>(axial_mask);
        
        BoundingBoxParams jaw_area_params = parida::calc_jaw_area_params<PixelType>(axial_mask);
        // debug jaw box
        img_jaw_box = parida::draw_jaw_box<PixelType>(axial_mask, axial_mask);
        output_filename = input_number + ".jpg";
        output_path = DST_ROOT / "Jaw Box" / output_filename;
        cv::imwrite(output_path.string(), img_jaw_box);
        
        /*
         * Tilt-Correction (from Axial Reference Plane)
         */
        Image2D::Pointer sagittal_mip = panorama::compute_sagittal_mip_image<PixelType>(img_ct);
        Image2D::Pointer sagittal_mask = panorama::compute_mask_image<PixelType>(sagittal_mip, tooth_threshold);
        sagittal_mask = panorama::process_tooth_mask<PixelType>(sagittal_mask);

        // debug tilted sagittal plane
        cv::Mat img_sagittal = parida::draw_sagittal_plane<PixelType>(sagittal_mip, sagittal_mask);
        output_filename = input_number + ".jpg";
        output_path = DST_ROOT / "Tilted Sagittal Plane" / output_filename;
        cv::imwrite(output_path.string(), img_sagittal);
        
        double axial_tilt_angle = parida::calc_axial_tilt_angle<PixelType>(sagittal_mask);
        correction_angle = parida::calc_axial_correction_angle(axial_tilt_angle);
        img_ct = panorama::rotate_ct_image<PixelType>(img_ct, 'x', correction_angle);
        sagittal_mip = panorama::compute_sagittal_mip_image<PixelType>(img_ct);
        sagittal_mask = panorama::compute_mask_image<PixelType>(sagittal_mip, tooth_threshold);

        // debug sagittal plane
        img_sagittal = parida::draw_sagittal_plane<PixelType>(sagittal_mip, sagittal_mask);
        output_filename = input_number + ".jpg";
        output_path = DST_ROOT / "Sagittal Plane" / output_filename;
        cv::imwrite(output_path.string(), img_sagittal);
        std::cout << "Axial_Tilt_Angle = " << axial_tilt_angle << std::endl;
        /*
         * Sampling CT Slices
         */
        
        Range sampling_slice_range = parida::calc_sampling_slice_range(horizontal_hist);
        img_ct = panorama::extract_slices<PixelType>(img_ct, sampling_slice_range);
        // debug slice range
        //cv::Mat img_coronal = parida::draw_coronal_plane<PixelType>(coronal_mask, sampling_slice_range); 
        cv::Mat img_coronal = parida::draw_coronal_plane<PixelType>(coronal_mask, roi_range); 
        output_filename = input_number + ".jpg";
        output_path = DST_ROOT / "Coronal Plane"/ output_filename;
        cv::imwrite(output_path.string(), img_coronal);
        
        /*
         * Panoramic Scan Simulation
         */
        cv::Mat img_axial = parida::draw_axial_plane<PixelType>(axial_mip, jaw_area_params);
        output_filename = input_number + ".jpg";
        output_path = DST_ROOT / "Axial Plane" / output_filename;
        cv::imwrite(output_path.string(), img_axial);

        Image2D::Pointer img_panorama = parida::compute_panoramic_image<PixelType>(img_ct, jaw_area_params);      
        output_filename = input_number + ".nii.gz";
        output_path = DST_ROOT / "Panorama" / output_filename;
        panorama::write_image<PixelType, 2>(img_panorama, output_path.string());
        
        // Debug panorama
        cv::Mat img_debug_panorama = parida::draw_2d_image<PixelType>(img_panorama);
        output_filename = input_number + ".jpg";
        output_path = DST_ROOT / "Debug Panorama" / output_filename;
        cv::imwrite(output_path.string(), img_debug_panorama);
        
        // Write info to the log file
        /*
        info_log << "ImageNumber = " << input_number << "\n" 
                 << "SagittalTiltAngle = " << sagittal_tilt_angle << "\n" 
                 << "AxialTiltAngle = " << axial_tilt_angle << "\n" 
                 << "CT_Width = " << size[0] << "\n" 
                 << "CT_Height = " << size[1] << "\n" 
                 << "CT_Depth = " << size[2] << "\n"
                 << "CT_SpacingX = " << spacing[0] << "\n"
                 << "CT_SpacingY = " << spacing[1] << "\n" 
                 << "CT_SliceSpacing = " << spacing[2] << "\n" 
                 << "\n"; // Add an extra newline for readability

        // Write tilt correction info
        if (correction_angle != 0) {
            info_log << "Axial Tilt Correction Applied: " << correction_angle << " degrees" << "\n";
        }

        // Write sampling slice range info
        if (sampling_slice_range.first > 0 || sampling_slice_range.second < size[2]) {
            info_log << "Extracted Slices" << "\n";
        }
        
        info_log << "\n"; // Add an extra newline for readability
        */
    }   

    //info_log.close();

    return EXIT_SUCCESS;
}