#include "../../include/hist/peak.hpp"


std::vector<short> hist::find_peaks(
    const std::vector<short> &histogram,
    const short &threshold
) {
    int len = histogram.size();
    int peak_width = len / 5;
    std::vector<double> mod_histogram(histogram.begin(), histogram.end());

    // しきい値以下の値をしきい値に設定
    for (auto& value : mod_histogram) {
        if (value < threshold) {
            value = threshold;
        }
    }

    std::vector<short> peak_positions;

    for (int i = 1; i < len - 1; ++i) {
        // ピークの条件を満たすかチェック
        if ((mod_histogram[i] >= mod_histogram[i - 1] && mod_histogram[i] > mod_histogram[i + 1]) ||
            (mod_histogram[i] > mod_histogram[i - 1] && mod_histogram[i] >= mod_histogram[i + 1])) {
            
            bool is_far_enough = true;
            int replace_index = -1;  // 置き換え候補のインデックス

            for (size_t j = 0; j < peak_positions.size(); ++j) {
                if (std::abs(i - peak_positions[j]) < peak_width) {
                    is_far_enough = false;
                    // 既存のピークと比較して、現在のピークの方が大きければ置き換え候補に設定
                    if (mod_histogram[i] > mod_histogram[peak_positions[j]]) {
                        replace_index = j;
                    }
                    break;
                }
            }

            if (is_far_enough) {
                // 十分に離れている場合、新しいピークとして追加
                peak_positions.push_back(i);
            } else if (replace_index != -1) {
                // 振幅が大きい場合、既存のピークを置き換え
                peak_positions[replace_index] = i;
            }
        }
    }

    return peak_positions;
}

std::vector<double> hist::calc_peak_param(
    const std::vector<short> &histogram,
    const short &peak
) {
    std::vector<double> params(3);
    params[0] = static_cast<double>(histogram[peak]);
    params[1] = static_cast<double>(peak);
    

    int window_size = histogram.size()/10; // Adjust window size as needed
    int histogram_size = histogram.size();

    double variance = 0.0;
    double mean_index = 0.0;
    double total_intensity = 0.0;

    for (int i = 0; i <= window_size; ++i) {
        int index_left = peak - i;
        int index_right = peak + i;

        if (index_left >= 0) {
            mean_index += static_cast<double>(index_left) * static_cast<double>(histogram[index_left]);
            total_intensity += static_cast<double>(histogram[index_left]);
            if (i > 5 && histogram[index_left] > histogram[index_left + 1]) break; // Stop if the slope changes after a certain distance
        }

        if (index_right < histogram_size) {
            mean_index += static_cast<double>(index_right) * static_cast<double>(histogram[index_right]);
            total_intensity += static_cast<double>(histogram[index_right]);
            if (i > 5 && histogram[index_right] > histogram[index_right - 1]) break; // Stop if the slope changes after a certain distance
        }
    }

    if (total_intensity == 0) return params; // Avoid division by zero

    mean_index /= total_intensity;

    for (int i = 0; i <= window_size; ++i) {
        int index_left = peak - i;
        int index_right = peak + i;

        if (index_left >= 0) {
            variance += static_cast<double>(histogram[index_left]) * std::pow(static_cast<double>(index_left) - mean_index, 2);
            if (i > 5 && histogram[index_left] > histogram[index_left + 1]) break; // Stop if the slope changes after a certain distance
        }

        if (index_right < histogram_size) {
            variance += static_cast<double>(histogram[index_right]) * std::pow(static_cast<double>(index_right) - mean_index, 2);
            if (i > 5 && histogram[index_right] > histogram[index_right - 1]) break; // Stop if the slope changes after a certain distance
        }
    }

    variance /= total_intensity;
    params[2] = std::sqrt(variance);

    return params;
}


short hist::find_single_peak(
    const std::vector<short> &histogram
) {
    if (histogram.empty()) {
        throw std::runtime_error("Histogram is empty");
    }

    short largest_peak = 0;
    for (size_t i = 1; i < histogram.size(); ++i) {
        if (histogram[i] > histogram[largest_peak]) {
            largest_peak = i;
        }
    }

    return largest_peak;
}

std::vector<double> hist::calc_single_peak_param(
    const std::vector<short> &histogram,
    const short &peak
) {
    std::vector<double> params(3);
    params[0] = static_cast<double>(histogram[peak]);
    params[1] = static_cast<double>(peak);

    int window_size = histogram.size() / 5; // Adjust window size as needed
    int histogram_size = histogram.size();

    double variance = 0.0;
    double mean_index = 0.0;
    double total_intensity = 0.0;

    for (int i = 0; i <= window_size; ++i) {
        int index_left = peak - i;
        int index_right = peak + i;

        if (index_left >= 0) {
            mean_index += static_cast<double>(index_left) * static_cast<double>(histogram[index_left]);
            total_intensity += static_cast<double>(histogram[index_left]);
        }

        if (index_right < histogram_size) {
            mean_index += static_cast<double>(index_right) * static_cast<double>(histogram[index_right]);
            total_intensity += static_cast<double>(histogram[index_right]);
        }
    }

    if (total_intensity == 0) return params; // Avoid division by zero

    mean_index /= total_intensity;

    for (int i = 0; i <= window_size; ++i) {
        int index_left = peak - i;
        int index_right = peak + i;

        if (index_left >= 0) {
            variance += static_cast<double>(histogram[index_left]) * std::pow(static_cast<double>(index_left) - mean_index, 2);
        }

        if (index_right < histogram_size) {
            variance += static_cast<double>(histogram[index_right]) * std::pow(static_cast<double>(index_right) - mean_index, 2);
        }
    }

    variance /= total_intensity;
    params[2] = std::sqrt(variance);

    return params;
}

double hist::calc_gaussian_function(
    const std::vector<double> &params,
    const short &x
) {
    double amplitude = params[0];
    double mean = params[1];
    double stddev = params[2];
    double exponent = -0.5 * std::pow((x - mean) / stddev, 2);
    return amplitude * std::exp(exponent);
}
