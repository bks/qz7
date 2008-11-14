#include "GzipArchive.h"

#include "qz7/Codec.h"
#include "qz7/Plugin.h"
#include "qz7/Stream.h"
#include "qz7/Volume.h"

namespace qz7 {
namespace gzip {

GzipArchive::GzipArchive(Volume *volume)
    : Archive(volume), mStream(0), mCodec(0)
{
}

GzipArchive::~GzipArchive()
{
}

bool GzipArchive::open()
{
    return false;;
}

bool GzipArchive::extractTo(uint id, WriteStream *target)
{
    return false;
}

bool GzipArchive::canWrite() const
{
    return false;
}

bool GzipArchive::writeTo(WriteStream *target)
{
    return false;
}

void GzipArchive::interrupt()
{
    if (mCodec)
        mCodec->interrupt();
}

}
}
