cmake_minimum_required(VERSION 3.16)
project(ColmapExporter LANGUAGES CXX)

add_executable(ColmapExporter
    main.cpp
)

find_package(cppzmq CONFIG REQUIRED)
target_link_libraries(ColmapExporter PRIVATE cppzmq)
