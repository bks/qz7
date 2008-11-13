add_subdirectory(tests)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(core_SRCS
    core/BitIoLE.cpp
    core/Crc.cpp
    core/Registry.cpp
    core/Volume.cpp
)

set(builtinPlugin_SRCS
    plugins/BuiltinPlugin.cpp
    plugins/volumes/singlefile/SingleFileVolume.cpp
)

set(libqz7_SRCS ${core_SRCS} ${builtinPlugin_SRCS})

automoc4_moc_headers(qz7
    include/qz7/Archive.h
    include/qz7/Codec.h
    include/qz7/Volume.h
)

automoc4_add_library(qz7 SHARED ${libqz7_SRCS})
set_target_properties(qz7 PROPERTIES VERSION 0.0.0 SOVERSION 0)
target_link_libraries(qz7 ${QT_QTCORE_LIBRARY})
