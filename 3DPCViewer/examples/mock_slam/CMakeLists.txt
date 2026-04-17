cmake_minimum_required(VERSION 3.16)
project(mock_slam_node LANGUAGES CXX)

add_executable(mock_slam_node
    main.cpp
)

target_include_directories(mock_slam_node PRIVATE
    ${CMAKE_SOURCE_DIR}/src/slam
)

find_package(cppzmq CONFIG REQUIRED)
target_link_libraries(mock_slam_node PRIVATE cppzmq)
