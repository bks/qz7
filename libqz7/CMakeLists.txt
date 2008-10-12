add_subdirectory(tests)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(core_SRCS
    core/BitIoLE.cpp
    core/Crc.cpp
    core/Registry.cpp
)

set(libqz7_SRCS ${core_SRCS})

add_library(qz7 SHARED ${libqz7_SRCS})
set_target_properties(qz7 PROPERTIES VERSION 0.0.0 SOVERSION 0)
target_link_libraries(qz7 ${QT_QTCORE_LIBRARY})
