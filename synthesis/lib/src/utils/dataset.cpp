#include <utils/dataset.hpp>
#include <yaml-cpp/yaml.h>

namespace utils {

    Dataset::Dataset(const boost::filesystem::path& yaml_path)
        : path(yaml_path),
          root(yaml_path.parent_path()) {
        YAML::Node config = YAML::LoadFile(yaml_path.string());

        const auto& images = config["image"];
        if (!images || !images.IsSequence()) {
            throw std::runtime_error("YAML format error: 'image' must be a sequence.");
        }

        for (const auto& img : images) {
            paths.emplace_back(root / img.as<std::string>());
        }
    }

    std::vector<boost::filesystem::path> Dataset::image_paths() const {
        return paths;  // コピーで返す
    }

    std::size_t Dataset::size() const {
        return paths.size();
    }

    boost::filesystem::path Dataset::image_path(std::size_t index) const {
        return paths.at(index);
    }
}

