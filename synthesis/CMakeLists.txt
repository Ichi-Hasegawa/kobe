cmake_minimum_required(VERSION 3.5)
project(PanoramaCT LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -Wall -fpermissive")
endif()

message("Compile Info:")
message(STATUS "Compiler  : ${CMAKE_CXX_COMPILER}")
message(STATUS "CXX Flags : ${CMAKE_CXX_FLAGS}")
message(STATUS "Debug     : ${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "Release   : ${CMAKE_CXX_FLAGS_RELEASE}")
message("")

# OpenMP
find_package(OpenMP)
if (OpenMP_FOUND)
    message(STATUS "OpenMP found: ${OpenMP_CXX_LIB_NAMES}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif()

# Boost
find_package(Boost COMPONENTS program_options filesystem random)
if (Boost_FOUND)
    message(STATUS "Boost v${Boost_VERSION} found.")
    include_directories(${Boost_INCLUDE_DIRS})
endif()

# Eigen3
find_package(Eigen3 REQUIRED)
if (EIGEN3_FOUND)
    message(STATUS "Eigen v${Eigen3_VERSION} found.")
    include_directories(${EIGEN3_INCLUDE_DIR})
endif()

# ITK
find_package(ITK REQUIRED)
if (ITK_FOUND)
    message(STATUS "ITK v${ITK_VERSION} found.")
    include(${ITK_USE_FILE})
endif()

# MPI
find_package(MPI REQUIRED)
if (MPI_FOUND)
    message(STATUS "MPI found.")
    include_directories(${MPI_INCLUDE_PATH})
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${MPI_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${MPI_CXX_FLAGS}")
endif()

# OpenCV
find_package(OpenCV REQUIRED)
if (OpenCV_FOUND)
    message(STATUS "OpenCV v${OpenCV_VERSION} found.")
    include_directories(${OpenCV_INCLUDE_DIRS})
endif()

# yaml-cpp
find_package(yaml-cpp REQUIRED)
if (yaml-cpp_FOUND)
    message(STATUS "yaml-cpp found.")
    include_directories(${YAML_CPP_INCLUDE_DIR})
endif()

include_directories(lib/include)

# panorama library
add_library(panorama STATIC
    lib/src/image/core.cpp       lib/include/image/core.hpp
    lib/src/image/io.cpp         lib/include/image/io.hpp
    lib/src/image/mip.cpp        lib/include/image/mip.hpp
    lib/src/image/mask.cpp       lib/include/image/mask.hpp
    lib/src/hist/core.cpp        lib/include/hist/core.hpp
    lib/src/hist/peak.cpp        lib/include/hist/peak.hpp
    lib/src/utils/dataset.cpp    lib/include/utils/dataset.hpp
)

set(PanoramaCT_LIBRARIES panorama)

#add_subdirectory(methods)
add_subdirectory(multi)




