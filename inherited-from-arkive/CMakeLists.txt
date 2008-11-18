project(Arkive)

find_package(KDE4 REQUIRED)
find_package(Qt4 REQUIRED)

include(KDE4Defaults)

add_definitions(${QT_DEFINITIONS} ${KDE4_DEFINITIONS} -D_FILE_OFFSET_BITS=64 -D_LARGE_FILES) # for off_t
remove_definitions(-DQT3_SUPPORT_WARNINGS -DQT3_SUPPORT)

include_directories (${QDBUS_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} ${KDE4_INCLUDES})

add_subdirectory(engine)
add_subdirectory(tests)
