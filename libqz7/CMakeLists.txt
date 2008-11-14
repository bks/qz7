add_subdirectory(tests)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(core_qobject_HEADERS
    include/Archive.h
    include/Codec.h
    include/Volume.h
)

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
)

set(codecs_SRCS
    codecs/support/HuffmanEncode.cpp
    codecs/support/MatchFinder.cpp
    codecs/deflate/DeflateDecoder.cpp
#    codecs/deflate/DeflateEncoder.cpp
    codecs/deflate/DeflateRegister.cpp
)

set(archive_SRCS
)

set(volume_SRCS
)

set(libqz7_SRCS ${core_SRCS} ${codecs_SRCS} ${archive_SRCS} ${volume_SRCS})

automoc4_moc_headers(qz7 ${core_qobject_HEADERS})
automoc4_add_library(qz7 SHARED ${libqz7_SRCS})
set_target_properties(qz7 PROPERTIES VERSION 0.0.0 SOVERSION 0)
target_link_libraries(qz7 ${QT_QTCORE_LIBRARY})
