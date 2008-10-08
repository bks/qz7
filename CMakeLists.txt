project(Qz7)

cmake_minimum_required(VERSION 2.6.1)

find_package(Qt4 REQUIRED)

add_definitions(${QT_DEFINITIONS} ${KDE4_DEFINITIONS} -D_FILE_OFFSET_BITS=64 -D_LARGE_FILES) # for off_t
remove_definitions(-DQT3_SUPPORT_WARNINGS -DQT3_SUPPORT)

include_directories (${QDBUS_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})

add_subdirectory(libqz7)
add_subdirectory(qz7-qs)
add_subdirectory(tests)
