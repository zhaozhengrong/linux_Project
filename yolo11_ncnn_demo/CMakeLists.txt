cmake_minimum_required(VERSION 3.10)

project(yolo11_ncnn_demo)

set(CMAKE_CXX_STANDARD 11)

# OpenMP
find_package(OpenMP REQUIRED)

# OpenCV
find_package(OpenCV REQUIRED)

# ncnn 头文件
include_directories(
    ${CMAKE_SOURCE_DIR}/third_party/ncnn/include
)

# 可执行文件
add_executable(yolo11_ncnn_demo
     ${CMAKE_SOURCE_DIR}/src/main.cpp
)

# 链接库
target_link_libraries(yolo11_ncnn_demo
    ${OpenCV_LIBS}
    OpenMP::OpenMP_CXX
    ${CMAKE_SOURCE_DIR}/third_party/ncnn/lib/libncnn.a
)
