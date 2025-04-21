#pragma once

#include <array>

#include <itkImage.h>

namespace hist {
    std::vector<short>
    find_peaks(const std::vector<short>&, const short&);

    std::vector<double>
    calc_peak_param(const std::vector<short>&, const short&);
    
    short find_single_peak(const std::vector<short>&);

    std::vector<double>
    calc_single_peak_param(const std::vector<short>&, const short&);
    
    double calc_gaussian_function(const std::vector<double>&, const short&);
}