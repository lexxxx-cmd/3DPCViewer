cmake_minimum_required(VERSION 3.16)

project(3DPCViewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(QT NAMES Qt5 REQUIRED COMPONENTS Core )
find_package(Qt${QT_VERSION_MAJOR}
    COMPONENTS
        Core
        Gui
        Widgets
        Concurrent
)
find_package(PCL REQUIRED)
find_package(OpenSceneGraph 3.6.5 REQUIRED osg osgDB osgGA osgViewer osgUtil osgQt)

add_subdirectory(third_party/ros1bag_reader)
add_subdirectory(src)

if(WIN32)
    target_sources(3DPCViewer PRIVATE resources/3DPCViewer.rc)
endif()