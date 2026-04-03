cmake_minimum_required(VERSION 3.16)
set(CMAKE_TOOLCHAIN_FILE "E:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
project(3DPCViewer LANGUAGES CXX)

include(qt.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt5 REQUIRED COMPONENTS Core )
find_package(Qt${QT_VERSION_MAJOR}
    COMPONENTS
        Core
        Gui
        Widgets
        Concurrent
)
qt_standard_project_setup()

find_package(PCL REQUIRED)
find_package(OpenSceneGraph 3.6.5 REQUIRED osg osgDB osgGA osgViewer osgUtil osgQt)



include_directories(
    ${OPENSCENEGRAPH_INCLUDE_DIRS}
    ${PCL_INCLUDE_DIRS}
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/ros1bag_reader)

set(ROSBAG_READER_SOURCES
    third_party/ros1bag_reader/core/rosbag.cpp
    third_party/ros1bag_reader/messages/std_msgs.cpp
    third_party/ros1bag_reader/messages/sensor_msgs.cpp
)

set(THIRD_PARTY_SOURCE
    ${ROSBAG_READER_SOURCES}
    )

set(ROSBAG_FOURCE
    BagWorker.h
    BagWorker.cpp
    BagDataTypes.h
    OdomCameraVisualizer.h
    OdomPathVisualizer.h
    DataService.h
    DataService.cpp
    Controller.h
    Controller.cpp
    Grid.h
    ImagePanel.h
    ImagePanel.cpp
)

set(PROJECT_UI_SOURCES
    pcviewer.ui
    pcviewer.h
    pcviewer.cpp
)

set(PROJECT_DATASRC_SOURCES
    datawidget.ui
    datawidget.h
    datawidget.cpp
)

set(PROJECT_STATUS_SOURCES
    statuswidget.ui
    statuswidget.h
    statuswidget.cpp
)

set(PROJECT_INTERACT_SOURCES
    interactionwidget.ui
    interactionwidget.h
    interactionwidget.cpp
)

set(PROJECT_CONGTRAL_SOURCES
    controlpanelwidget.ui
    controlpanelwidget.h
    controlpanelwidget.cpp
)

set(PROJECT_VISUAL_SOURCES
    visualareawidget.ui
    visualareawidget.h
    visualareawidget.cpp
)
set(PROJECT_SOURCES
    main.cpp
    ${PROJECT_UI_SOURCES}
    ${PROJECT_DATASRC_SOURCES}
    ${PROJECT_STATUS_SOURCES}
    ${PROJECT_INTERACT_SOURCES}
    ${PROJECT_CONGTRAL_SOURCES}
    ${PROJECT_VISUAL_SOURCES}
    DataLoader.h
    3DPCViewer.rc
    ${THIRD_PARTY_SOURCE}
    ${ROSBAG_FOURCE}
)

qt_add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

set_target_properties(${PROJECT_NAME}
    PROPERTIES
        WIN32_EXECUTABLE TRUE
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        Qt::Core
        Qt::Gui
        Qt::Widgets
        Qt::Concurrent
        ${OPENSCENEGRAPH_LIBRARIES}
        ${PCL_LIBRARIES}

)


