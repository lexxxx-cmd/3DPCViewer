set(DB_SOURCES
    CDRConverter.cpp
    DatabaseWorker.cpp
)

add_library(db_module STATIC ${DB_SOURCES})

target_link_libraries(db_module PUBLIC
    fastcdr
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
)

target_include_directories(db_module PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
