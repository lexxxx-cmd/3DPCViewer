cmake_minimum_required(VERSION 3.16)

project(pose_pcd_exporter)

find_package(PCL REQUIRED COMPONENTS common io filters)
find_package(Eigen3 REQUIRED)
find_package(cppzmq REQUIRED)

add_executable(pose_pcd_exporter main.cpp)

target_include_directories(pose_pcd_exporter PRIVATE
    ${PCL_INCLUDE_DIRS}
    ${EIGEN3_INCLUDE_DIRS}
)

target_link_libraries(pose_pcd_exporter PRIVATE
    ${PCL_LIBRARIES}
    cppzmq
)

# Use C++17
set_target_properties(pose_pcd_exporter PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
