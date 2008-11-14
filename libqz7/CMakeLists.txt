add_subdirectory(tests)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(core_SRCS
    core/BitIoBE.cpp
    core/BitIoLE.cpp
    core/Codec.cpp
    core/Crc.cpp
    core/Registry.cpp
    core/RingBuffer.cpp
    core/Sort.cpp
    core/Stream.cpp
    core/StreamTools.cpp
    core/Volume.cpp
)

set(codecs_SRCS
    plugins/codecs/support/HuffmanEncode.cpp
    plugins/codecs/support/MatchFinder.cpp
    plugins/codecs/deflate/DeflateDecoder.cpp
#    codecs/deflate/DeflateEncoder.cpp
)

set(archive_SRCS
)

set(volume_SRCS
    plugins/volumes/singlefile/SingleFileVolume.cpp
)

set(builtinPlugin_SRCS
    plugins/BuiltinPlugin.cpp
    ${codecs_SRCS}
    ${archive_SRCS}
    ${volume_SRCS}
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
