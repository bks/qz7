include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(tests)

set(kernel_SRCS
    kernel/Crc.cpp
    kernel/Archive.cpp
    kernel/BitStreamLE.cpp
    kernel/CompressionCodec.cpp
    kernel/CompressionManager.cpp
    kernel/Filter.cpp
    kernel/FilterCrc.cpp
    kernel/HandlerRegistry.cpp
    kernel/LimitedIoDevice.cpp
    kernel/QioBuffer.cpp
    kernel/RingBuffer.cpp
    kernel/Sort.cpp
)

set(archive_SRCS
    archives/bzip2/BZip2Archive.cpp

    archives/gzip/GzipArchive.cpp

#    archives/rar/RarHandler.cpp
#    archives/rar/RarHeader.cpp
#    archives/rar/RarIn.cpp
#    archives/rar/RarItem.cpp
#    archives/rar/RarRegister.cpp
#    archives/rar/RarVolumeInStream.cpp
 
    archives/tar/TarArchive.cpp
    archives/tar/TarRead.cpp
    archives/tar/TarWrite.cpp
     
#    archives/zip/ZipAddCommon.cpp
#    archives/zip/ZipHandler.cpp
#    archives/zip/ZipHandlerOut.cpp
#    archives/zip/ZipHeader.cpp
#    archives/zip/ZipIn.cpp
#    archives/zip/ZipItem.cpp
#    archives/zip/ZipOut.cpp
#    archives/zip/ZipRegister.cpp
#    archives/zip/ZipUpdate.cpp
)

set(compression_SRCS
    compression/bzip2/BlockSort.cpp
#    compression/bzip2/BZip2CRC.cpp
#    compression/bzip2/BZip2Decoder.cpp
#    compression/bzip2/BZip2Encoder.cpp
#     compression/bzip2/BZip2Register.cpp

    compression/copy/CopyCodec.cpp

    compression/deflate/DeflateDecoder.cpp
    compression/deflate/DeflateEncoder.cpp
    compression/deflate/DeflateRegister.cpp
 
    compression/huffman/HuffmanEncode.cpp
# 
#    compression/Implode/ImplodeDecoder.cpp
#    compression/Implode/ImplodeHuffmanDecoder.cpp
# 
    compression/lz/MatchFinder.cpp
# 
#    compression/PPMD/PPMDDecoder.cpp
#    compression/PPMD/PPMDEncoder.cpp
#    compression/PPMD/PPMDRegister.cpp
# 
#    compression/RangeCoder/RangeCoderBit.cpp
# 
#    compression/Rar/Rar1Decoder.cpp
#    compression/Rar/Rar2Decoder.cpp
#    compression/Rar/Rar3Decoder.cpp
#    compression/Rar/Rar3Vm.cpp
#    compression/Rar/RarCodecsRegister.cpp
# 
#    compression/Shrink/ShrinkDecoder.cpp
)

set(crypto_SRCS
#     crypto/Hash/HmacSha1.cpp
#     crypto/Hash/Pbkdf2HmacSha1.cpp
#     crypto/Hash/RandGen.cpp
#     crypto/Hash/Sha1.cpp
#     crypto/Hash/Sha256.cpp
# 
#     crypto/Rar20/Rar20Cipher.cpp
#     crypto/Rar20/Rar20Crypto.cpp
# 
#     crypto/RarAES/RarAES.cpp
# 
#     crypto/WzAES/WzAES.cpp
# 
#     crypto/Zip/ZipCipher.cpp
#     crypto/Zip/ZipCrypto.cpp
)

set(arkiveengine_SRCS ${kernel_SRCS} ${archive_SRCS} ${compression_SRCS} ${crypto_SRCS})

kde4_add_library(arkiveengine SHARED ${arkiveengine_SRCS})
set_target_properties(arkiveengine PROPERTIES VERSION 0.0.0 SOVERSION 0)
target_link_libraries(arkiveengine ${QT_QTCORE_LIBRARY})
