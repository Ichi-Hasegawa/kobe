#pragma once

#include <opencv2/opencv.hpp>
#include <itkImage.h>
#include <itkImageFileReader.h>

#define PARAM_DIM 7

typedef double PixelType;

using Image3D = itk::Image<PixelType, 3>;
using Image2D = itk::Image<PixelType, 2>;

using Hist = std::vector<short>;
using Range = std::pair<short, short>;

struct BoxParam {
    cv::Point2f center;   // 矩形の中心座標
    cv::Size2f size;      // 矩形の幅と高さ
    float angle;          // 回転角度（度）
};

namespace parida {
    template <typename PixelType>
    BoxParam calc_jaw_area_param(const typename itk::Image<PixelType, 2>::Pointer&);

    cv::Point calc_asteroid_rotation_center(float t, float h, float k, float a, float b);
    
    float calc_shift_step(float angle, float min_shift, float max_shift, float a, float b);
    std::vector<std::pair<int, int>> getPerpendicularLinePixels(
        double cx, double cy, double normalSlope, int length
    );

    template <typename PixelType>
    typename itk::Image<PixelType, 2>::Pointer compute_panoramic_image(
        const typename itk::Image<PixelType, 3>::Pointer&, 
        const BoxParam&
    );
}

namespace poemi {
    template <typename PixelType>
    typename itk::Image<PixelType, 2>::Pointer compute_panoramic_image(
        const typename itk::Image<PixelType, 3>::Pointer&, 
        const BoxParam&,
        const int&,                                     // 光線長さ（例：200）
        const std::string&
    );
}