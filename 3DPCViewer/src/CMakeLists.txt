add_executable(3DPCViewer main.cpp)

target_include_directories(3DPCViewer PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_SOURCE_DIR}
	${CMAKE_CURRENT_BINARY_DIR}
)

target_include_directories(3DPCViewer PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
	${PCL_INCLUDE_DIRS}
	${OPENSCENEGRAPH_INCLUDE_DIRS}
)

target_link_libraries(3DPCViewer PRIVATE
	Qt${QT_VERSION_MAJOR}::Widgets
	Qt${QT_VERSION_MAJOR}::Concurrent
	Qt${QT_VERSION_MAJOR}::OpenGL
	${PCL_LIBRARIES}
	${OPENSCENEGRAPH_LIBRARIES}
	rosbag_reader_core
)

add_subdirectory(core)
add_subdirectory(io)
add_subdirectory(ui)
add_subdirectory(node)
add_subdirectory(osgQOpenGL)
