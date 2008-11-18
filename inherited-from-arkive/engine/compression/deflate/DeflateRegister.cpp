#include "kernel/HandlerRegistry.h"
#include "DeflateDecoder.h"
#include "DeflateEncoder.h"

EXPORT_CODEC(Deflate, 0x40108, 
    Compress::Deflate::DeflateDecoder,
    Compress::Deflate::DeflateEncoder
)

EXPORT_CODEC_READONLY(DeflateNSIS, 0x040901,
    Compress::Deflate::DeflateNSISDecoder
)

EXPORT_CODEC(Deflate64, 0x040109,
    Compress::Deflate::Deflate64Decoder,
    Compress::Deflate::Deflate64Encoder
)
