#include "kernel/HandlerRegistry.h"
#include "BZip2Decoder.h"
//#include "BZip2Encoder.h"

EXPORT_CODEC_READONLY(BZip2, 0x040202, Compress::BZip2::BZip2Decoder);
