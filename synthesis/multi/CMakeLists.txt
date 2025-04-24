cmake_minimum_required(VERSION 3.5)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# multi synthesis
add_executable(main
        src/main.cpp
        src/param.cpp
        src/synthesis.cpp
        src/debug.cpp
        )
target_link_libraries(main
        ${PanoramaCT_LIBRARIES}
        ${YAML_CPP_LIBRARIES}
        ${ITK_LIBRARIES}
        ${Boost_LIBRARIES}
        ${OpenCV_LIBRARIES}
        ${MPI_C_LIBRARIES}
        )