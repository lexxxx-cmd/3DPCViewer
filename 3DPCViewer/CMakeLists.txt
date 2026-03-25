cmake_minimum_required(VERSION 3.16)
set(CMAKE_TOOLCHAIN_FILE "E:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
project(3DPCViewer LANGUAGES CXX)

include(qt.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core )
find_package(Qt${QT_VERSION_MAJOR}
    COMPONENTS
        Core
        Gui
        Widgets
        Concurrent
)
qt_standard_project_setup()

find_package(VTK REQUIRED) 
find_package(PCL CONFIG REQUIRED)

include_directories(
    ${PCL_INCLUDE_DIRS}
    ${VTK_INCLUDE_DIRS}
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
    3DPCViewer.rc
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
   PRIVATE
        ${VTK_LIBRARIES} 
        ${PCL_LIBRARIES}
)

vtk_module_autoinit(
    TARGETS ${PROJECT_NAME}
    MODULES ${VTK_LIBRARIES}
)


