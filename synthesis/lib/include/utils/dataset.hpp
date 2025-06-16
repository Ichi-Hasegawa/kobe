#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <boost/filesystem.hpp>

namespace utils {
    class Dataset {
    private:
        boost::filesystem::path path;
        boost::filesystem::path root;
        std::vector<boost::filesystem::path> paths;

    public:
        Dataset() = default;
        explicit Dataset(const boost::filesystem::path& yaml_path);

        ~Dataset() = default;

        std::vector<boost::filesystem::path> image_paths() const;
        std::size_t size() const;
        boost::filesystem::path image_path(std::size_t index) const;
    };
}
