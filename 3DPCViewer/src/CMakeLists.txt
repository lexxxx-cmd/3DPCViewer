add_executable(3DPCViewer main.cpp)

target_include_directories(3DPCViewer PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_SOURCE_DIR}
)

target_include_directories(3DPCViewer PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
	${PCL_INCLUDE_DIRS}
	${OPENSCENEGRAPH_INCLUDE_DIRS}
)

# TBB is required by MSVC / GCC for std::execution parallel policies.
find_package(TBB QUIET)

target_link_libraries(3DPCViewer PRIVATE
	Qt5::Widgets
	Qt5::Concurrent
	${PCL_LIBRARIES}
	${OPENSCENEGRAPH_LIBRARIES}
	rosbag_reader_core
)

if(TBB_FOUND)
    target_link_libraries(3DPCViewer PRIVATE TBB::tbb)
endif()

add_subdirectory(core)
add_subdirectory(io)
add_subdirectory(ui)
add_subdirectory(node)