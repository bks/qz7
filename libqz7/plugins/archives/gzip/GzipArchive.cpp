#include "GzipArchive.h"

#include "qz7/ByteIO.h"
#include "qz7/Codec.h"
#include "qz7/CrcAnalyzer.h"
#include "qz7/Plugin.h"
#include "qz7/Stream.h"
#include "qz7/Volume.h"

#include <QtCore/QDateTime>

namespace qz7 {
namespace gzip {

ArchiveItem::HostOperatingSystem GzipArchive::mapToArchive(quint8 gzip)
{
    static struct {
        GzipHostFs gzip;
        ArchiveItem::HostOperatingSystem archiveItem;
    } hostOsMap[] = {
        { FsFAT, ArchiveItem::MsDos },
        { FsAmiga, ArchiveItem::Amiga },
        { FsVMS, ArchiveItem::VMS },
        { FsUnix, ArchiveItem::Unix },
        { FsVMCMS, ArchiveItem::VM_CMS },
        { FsAtari, ArchiveItem::Atari },
        { FsHPFS, ArchiveItem::OS_2 },
        { FsMacClassic, ArchiveItem::MacClassic },
        { FsZSystem, ArchiveItem::Z_System },
        { FsCPM, ArchiveItem::CPM },
        { FsTOPS20, ArchiveItem::Tops20 },
        { FsNTFS, ArchiveItem::WindowsNT },
        { FsQDOS, ArchiveItem::QDos },
        { FsRISCOS, ArchiveItem::RiscOs }
    };

    for (int i = 0; i < sizeof(hostOsMap)/sizeof(hostOsMap[0]); i++)
        if (hostOsMap[i].gzip == gzip)
            return hostOsMap[i].archiveItem;

    return ArchiveItem::UnknownHostOperatingSystem;
}

GzipArchive::GzipArchive(Volume *volume)
    : Archive(volume), mStream(0), mCodec(0)
{
}

GzipArchive::~GzipArchive()
{
}

bool GzipArchive::open()
{
    if (!mStream)
        mStream = openFile(0);
    if (!mStream)
        return false;

    try {
        return doOpen();
    } catch (Error e) {
        setErrorString(e.message());
        return false;
    }
}

bool GzipArchive::doOpen()
{
    ArchiveItem item(true);
    ByteIoReader in(mStream);
    Crc32 crc;

    item.setItemType(ArchiveItem::ItemTypeFile);

    quint8 id1 = in.read8();
    crc.update(id1);
    if (id1 != ID1) {
        setErrorString(tr("does not look like a gzip file"));
        return false;
    }

    quint8 id2 = in.read8();
    crc.update(id2);
    if (id2 != ID2) {
        setErrorString(tr("does not look like a gzip file"));
        return false;
    }

    quint8 compressionMethod = in.read8();
    crc.update(compressionMethod);

    if (compressionMethod == GzipMethodDeflate)
        item.setCompressionMethod(ArchiveItem::CompressionMethodDeflate);
    else
        item.setCompressionMethod(ArchiveItem::CompressionMethod(compressionMethod));

    quint8 flags = in.read8();
    quint32 mtime = in.read32LE();
    crc.update(flags);
    crc.update(mtime);
    item.setMTime(QDateTime::fromTime_t(mtime));

    quint8 xflags = in.read8();
    quint8 hostOs = in.read8();
    crc.update(xflags);
    crc.update(hostOs);
    item.setHostOs(mapToArchive(hostOs));

    int headerSize = 10;
    if (flags & FMBZ)
        throw CorruptedError();
    if (flags & FHasExtra) {
        quint16 extraSize = in.read16LE();
        headerSize += extraSize + 2;
        QByteArray extra;
        in.readBuffer(&extra, extraSize);
        crc.update(extraSize);
        crc.update(extra.constData(), extraSize);
        item.setProperty("GzipItemExtra", QVariant::fromValue(extra));
    }
    if (flags & FHasName) {
        QByteArray name;
        in.readStringZ(&name);
        headerSize += name.length();
        crc.update(name.constData(), name.length());
        item.setName(QString::fromLatin1(name));
    }
    if (flags & FHasComment) {
        QByteArray comment;
        in.readStringZ(&comment);
        headerSize += comment.length();
        crc.update(comment.constData(), comment.length());
        item.setProperty("comment", QVariant::fromValue(QString::fromLatin1(comment)));
    }
    if (flags & FHasCrc) {
        headerSize += 2;
        quint16 expectedCrc = in.read16LE();
        if (expectedCrc != (crc.value() & 0xffff))
            throw CorruptedError();
    }

    item.setPosition(mStream->pos());

    qint64 size = mStream->size();
    item.setCompressedSize(size - headerSize - 8);

    if (!mStream->setPos(size - 8))
        throw ReadError(mStream);

    quint32 dataCrc = in.read32LE();
    item.setCrc(dataCrc);

    quint32 uncompressedSize = in.read32LE();
    item.setUncompressedSize(uncompressedSize);

    addItem(item);
    return true;
}
bool GzipArchive::extractTo(uint id, WriteStream *target)
{
    mInterrupted = false;
    if (id >= count())
        return false;

    ArchiveItem item = Archive::item(id);
    if (item.compressionMethod() != ArchiveItem::CompressionMethodDeflate) {
        setErrorString(tr("unsupported compression method"));
        return false;
    }
    if (!mStream->setPos(item.position())) {
        setErrorString(ReadError(mStream).message());
        return false;
    }

    if (!mCodec)
        mCodec = Registry::createDecoder("deflate", this);
    if (!mCodec) {
        setErrorString(tr("unable to create deflate decoder"));
        return false;
    }

    LimitedReadStream ls(mStream, item.compressedSize());
    Crc32WriteStream ws(target);
    if (!mCodec->stream(&ls, &ws)) {
        if (!mInterrupted)
            setErrorString(mCodec->errorString());
        else
            setErrorString(tr("the operation was interrupted"));
        return false;
    }

    if (ws.analyzer()->value() != item.crc()) {
        setErrorString(CrcError().message() + " (expected " + QString::number(item.crc(), 16) + ", got " + QString::number(ws.analyzer()->value(), 16) + ')');
        return false;
    }
    return true;
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
    mInterrupted = true;
}

}
}
