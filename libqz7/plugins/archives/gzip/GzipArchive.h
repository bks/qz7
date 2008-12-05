#ifndef QZ7_GZIP_ARCHIVE_H
#define QZ7_GZIP_ARCHIVE_H

#include "qz7/Archive.h"

#include <QtCore/QObject>

namespace qz7 {

class Codec;
class SeekableReadStream;

namespace gzip {

class GzipArchive : public Archive {
    Q_OBJECT

public:
    GzipArchive(Volume *volume);
    ~GzipArchive();

    virtual bool open();
    virtual bool extractTo(uint id, WriteStream *target);

    virtual bool canWrite() const;
    virtual bool writeTo(WriteStream *target);

    virtual void interrupt();

private:
    bool doOpen();

    enum { ID1 = 0x1f, ID2 = 0x8b };
    enum { GzipMethodDeflate = 8 };
    enum { FText = 0x01, FHasCrc = 0x02, FHasExtra = 0x04, FHasName = 0x08, FHasComment = 0x10, FMBZ = 0xe0 };
    enum { XFUsedMaximumCompression = 2, XFUsedFastestCompression = 4 };
    enum GzipHostFs {
        FsFAT = 0,
        FsAmiga = 1,
        FsVMS = 2,
        FsUnix = 3,
        FsVMCMS = 4,
        FsAtari = 5,
        FsHPFS = 6,
        FsMacClassic = 7,
        FsZSystem = 8,
        FsCPM = 9,
        FsTOPS20 = 10,
        FsNTFS = 11,
        FsQDOS = 12,
        FsRISCOS = 13,
        FsUnknown = 255
    };

    static ArchiveItem::HostOperatingSystem mapToArchive(quint8 gzip);

    SeekableReadStream *mStream;
    Codec *mCodec;
    bool mInterrupted;
};

}
}

#endif
