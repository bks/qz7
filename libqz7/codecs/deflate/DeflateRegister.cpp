#include "Registry.h"
#include "DeflateDecoder.h"
// #include "DeflateEncoder.h"

#if 0
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
#endif

namespace qz7 {
namespace deflate {

EXPORT_CODEC_READONLY("deflate", 0x40108, DeflateDecoder)
EXPORT_CODEC_READONLY("deflateNSIS", 0x40901, DeflateNSISDecoder)
EXPORT_CODEC_READONLY("deflate64", 0x40109, Deflate64Decoder)

}
}