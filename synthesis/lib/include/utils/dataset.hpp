#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <boost/filesystem.hpp>
#include <yaml-cpp/yaml.h>

namespace utils {
    class Dataset {
    private:
        const boost::filesystem::path path;
        const boost::filesystem::path root;
        const YAML::Node config;

    public:
        Dataset() = default;
        explicit Dataset(const boost::filesystem::path& yaml_path)
            : path(yaml_path),
              root(path.parent_path()),
              config(YAML::LoadFile(yaml_path.string())) {}

        ~Dataset() = default;

        std::vector<boost::filesystem::path> image_paths() const;
        std::size_t size() const;
        boost::filesystem::path image_path(const std::size_t index) const;
    };
}

