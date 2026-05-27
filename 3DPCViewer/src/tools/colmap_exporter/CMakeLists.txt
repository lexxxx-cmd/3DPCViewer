cmake_minimum_required(VERSION 3.16)
project(ColmapExporter LANGUAGES CXX)

add_executable(ColmapExporter
    main.cpp
)

find_package(cppzmq CONFIG REQUIRED)
find_package(PCL REQUIRED COMPONENTS common filters io)
find_package(Eigen3 CONFIG REQUIRED)
find_package(OpenCV REQUIRED)

target_include_directories(ColmapExporter PUBLIC
    ${PCL_INCLUDE_DIRS}
)

target_link_libraries(ColmapExporter PRIVATE 
    cppzmq
    ${PCL_LIBRARIES}
    ${OpenCV_LIBRARIES}
    Eigen3::Eigen
)
