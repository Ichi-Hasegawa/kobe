cmake_minimum_required(VERSION 3.5)

project(PanoramaCT LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
if(CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "-std=c++1y -O2 -Wall -fpermissive ${CMAKE_CXX_FLAGS}")
#elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
#    set(COMPILE_OPTIONS -std=c++14  ${COMPILE_OPTIONS})
endif()

message("CompileInfo:")
message(STATUS "${CMAKE_CXX_COMPILER}")
message(STATUS "       : ${CMAKE_CXX_FLAGS}")
message(STATUS "Debug  : ${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "Release: ${CMAKE_CXX_FLAGS_RELEASE}")
message("")


message("OpenMP:")
find_package(OpenMP)
if (OPENMP_FOUND)
    message(STATUS "OpenMP found")
    message(STATUS "libraries: ${OpenMP_CXX_LIB_NAMES}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    message("")
endif()

message("Boost:")
find_package(Boost COMPONENTS
        program_options
        filesystem
        random
        )
if(Boost_FOUND)
    message(STATUS "Boost v${Boost_VERSION} found.")
    message(STATUS "Boost library: ${Boost_LIBRARIES}")
    message(STATUS "Boost include: ${Boost_INCLUDE_DIR}")
    include_directories(${Boost_INCLUDE_DIRS})
    message("")
endif()

message("Eigen3:")
find_package(Eigen3 REQUIRED)
if(EIGEN3_FOUND)
    message(STATUS "Eigen v${Eigen3_VERSION} found.")
    message(STATUS "include: ${EIGEN3_INCLUDE_DIR}")
    include_directories(${EIGEN3_INCLUDE_DIR})
    message("")
endif()

message("InsightToolkit:")
find_package(ITK REQUIRED)
if(ITK_FOUND)
    message(STATUS "ITK v ${ITK_VERSION} found.")
    message(STATUS "ITK libraries: ${ITK_LIBRARIES}")
    message(STATUS "include: ${ITK_USE_FILE}")
    include(${ITK_USE_FILE})
    message("")
endif()

# MPIの設定を追加
message("MPI:")
find_package(MPI REQUIRED)
if(MPI_FOUND)
    message(STATUS "MPI found")
    message(STATUS "MPI libraries: ${MPI_C_LIBRARIES}")
    include_directories(${MPI_INCLUDE_PATH})
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${MPI_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${MPI_CXX_FLAGS}")
    message("")
endif()

find_package(OpenCV REQUIRED)
if(OpenCV_FOUND)
    include_directories("${OpenCV_INCLUDE_DIRS}")
    message("OpenCV ver.${OpenCV_VERSION}")
endif()

message("yaml-cpp:")
find_package(yaml-cpp REQUIRED)
#if(YAML_CPP_FOUND)
    message(STATUS "include: ${YAML_CPP_INCLUDE_DIR}")
    message(STATUS "library: ${YAML_CPP_LIBRARIES}")
    include_directories(${YAML_CPP_INCLUDE_DIR})
    message("")
#endif()


include_directories(lib/include)

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

#add_subdirectory(test)

add_subdirectory(AutoSynthesis/Parida)



