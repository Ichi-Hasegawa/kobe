cmake_minimum_required(VERSION 3.5)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(mcanal
    src/main.cpp
    src/param.cpp
    src/synthesis.cpp
    src/debug.cpp
)

target_link_libraries(mcanal
    ${PanoramaCT_LIBRARIES}
    ${YAML_CPP_LIBRARIES}
    ${ITK_LIBRARIES}
    ${Boost_LIBRARIES}
    ${OpenCV_LIBRARIES}
    ${MPI_C_LIBRARIES}
)
