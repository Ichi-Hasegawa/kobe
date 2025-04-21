#include <utils/dataset.hpp>

namespace utils {

    std::vector<boost::filesystem::path> Dataset::image_paths() const {
        std::vector<boost::filesystem::path> paths;

        for (const auto& img : config["image"]) {
            paths.emplace_back(root / img.as<std::string>());
        }

        return paths;
    }

    std::size_t Dataset::size() const {
        return config["image"].size();
    }

    boost::filesystem::path Dataset::image_path(const std::size_t index) const {
        return root / config["image"][index].as<std::string>();
    }
}

