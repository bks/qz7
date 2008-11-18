#include "GzipArchive.h"
#include "GzipHeader.h"

#include "kernel/Crc.h"
#include "kernel/CompressionCodec.h"
#include "kernel/Filter.h"
#include "kernel/FilterCrc.h"
#include "kernel/HandlerRegistry.h"
#include "kernel/LimitedIoDevice.h"

#include <QtCore/QDateTime>

namespace ArchiveEngine {
namespace Gzip {

GzipArchive::GzipArchive(QObject *parent)
    : ArchiveBase(parent)
{
}

GzipArchive::~GzipArchive()
{
}

static const struct {
    GzipHost gzipHost;
    ArchiveItem::HostOperatingSystem archiveHost;
} s_hostOsMap[] = {
    { HostFAT, ArchiveItem::MsDos },
    { HostAMIGA, ArchiveItem::Amiga },
    { HostVMS, ArchiveItem::VMS },
    { HostUnix, ArchiveItem::Unix },
    { HostVM_CMS, ArchiveItem::VM_CMS },
    { HostAtari, ArchiveItem::Atari },
    { HostHPFS, ArchiveItem::OS_2 },
    { HostMac, ArchiveItem::MacClassic },
    { HostZ_System, ArchiveItem::Z_System },
    { HostCPM, ArchiveItem::CPM },
    { HostTOPS20, ArchiveItem::Tops20 },
    { HostNTFS, ArchiveItem::WindowsNT },
    { HostQDOS, ArchiveItem::QDos },
    { HostAcorn, ArchiveItem::Acorn },
    { HostVFAT, ArchiveItem::Windows9x },
    { HostMVS, ArchiveItem::MVS },
    { HostBeOS, ArchiveItem::BeOS },
    { HostTandem, ArchiveItem::Tandem },
    { HostTHEOS, ArchiveItem::THEOS }
};

static ArchiveItem::HostOperatingSystem mapHostToArchive(GzipHost gzipHost)
{
    const int n = sizeof(s_hostOsMap) / sizeof(s_hostOsMap[0]);

    for (int i = 0; i < n; i++)
        if (s_hostOsMap[i].gzipHost == gzipHost)
            return s_hostOsMap[i].archiveHost;

    return ArchiveItem::UnknownHostOperatingSystem;
}

static GzipHost mapArchiveToHost(uint archiveHost)
{
    const int n = sizeof(s_hostOsMap) / sizeof(s_hostOsMap[0]);

    for (int i = 0; i < n; i++)
        if (s_hostOsMap[i].archiveHost == archiveHost)
            return s_hostOsMap[i].gzipHost;

    return HostUnknown;
}

#define READ(size, val)                         \
    do {                                        \
        if (!read ## size(val))                 \
            return errorIoDevice(inputDevice());\
        crc = CrcUpdate(crc, val, sizeof(*val)); \
    } while (0)

bool GzipArchive::readHeader()
{
    quint32 crc = CrcInitValue();

    quint16 signature;
    READ(UInt16LE, &signature);

    if (signature != Signature)
        return errorBadMagic("gzip");

    ArchiveItem item;

    quint8 compressionMethod;
    READ(Byte, &compressionMethod);

    if (compressionMethod == CompressionMethodDeflate)
        item.setProperty(ArchiveItem::CompressionMethod, ArchiveItem::CompressionMethodDeflate);
    else
        item.setProperty(ArchiveItem::CompressionMethod, quint32(compressionMethod));

    quint8 flags;
    READ(Byte, &flags);

    quint32 time;
    READ(UInt32LE, &time);
    item.setProperty(ArchiveItem::LastWriteTime, QDateTime::fromTime_t(time));

    quint8 extraFlags;
    READ(Byte, &extraFlags);

    quint8 hostOs;
    READ(Byte, &hostOs);
    item.setProperty(ArchiveItem::HostOS, mapHostToArchive(GzipHost(hostOs)));

    int headerSize = 10;

    if (flags & FlagExtraIsPresent) {
        quint16 extraSize;
        READ(UInt16LE, &extraSize);
        QByteArray extra;
        if (!readBuffer(&extra, extraSize))
            return errorIoDevice(inputDevice());
        crc = CrcUpdate(crc, extra.constData(), extraSize);
        item.setProperty(GzipItem::ItemExtra, extra);
        headerSize += extraSize;
    }

    if (flags & FlagNameIsPresent) {
        QByteArray name;
        if (!readString(&name))
            return errorIoDevice(inputDevice());
        crc = CrcUpdate(crc, name.constData(), name.size());
        item.setProperty(ArchiveItem::Path, QString::fromLatin1(name.left(name.size() - 1)));
        headerSize += name.size();
    }

    if (flags & FlagCommentIsPresent) {
        QByteArray comment;
        if (!readString(&comment))
            return errorIoDevice(inputDevice());
        crc = CrcUpdate(crc, comment.constData(), comment.size());
        item.setProperty(ArchiveItem::Comment, QString::fromLatin1(comment.left(comment.size() - 1)));
        headerSize += comment.size();
    }

    if (flags & FlagHeaderCrcIsPresent) {
        quint16 headerCrc;
        if (!readUInt16LE(&headerCrc))
            return errorIoDevice(inputDevice());

        if (CrcValue(crc) & 0xffff != headerCrc)
            return errorBadCrc();
        headerSize += 2;
    }

    qint64 compressedSize = inputDevice()->size() - headerSize - 8;
    item.setProperty(ArchiveItem::CompressedSize, compressedSize);

    qint64 pos = currentPosition();
    item.setProperty(ArchiveItem::Position, pos);

    // read the gzip trailer
    if (!seek(-8, RelativeToEnd, 0))
        return errorIoDevice(inputDevice());

    quint32 dataCrc;
    READ(UInt32LE, &dataCrc);
    item.setProperty(ArchiveItem::CRC, dataCrc);

    quint32 uncompressedSize;
    READ(UInt32LE, &uncompressedSize);
    item.setProperty(ArchiveItem::UncompressedSize, uncompressedSize);

    item.setProperty(ArchiveItem::Type, ArchiveItem::ItemTypeFile);
    addItem(item);

    ArchiveInfo info;
    info.setProperty(ArchiveInfo::IsSolid, true);
    info.setProperty(ArchiveInfo::IsVolume, false);
    info.setProperty(ArchiveInfo::IsEncrypted, false);
    setInfo(info);

    return true;
}

#define WRITE(size, val)                            \
    do {                                            \
        __typeof__(val) temp = val;                 \
        if (!write ## size(temp))                   \
            return errorIoDevice(outputDevice());   \
        crc = CrcUpdate(crc, &temp, sizeof(temp));  \
    } while (0)

bool GzipArchive::writeHeader(const ArchiveItem& theItem)
{
    quint32 crc = CrcInitValue();

    WRITE(UInt16LE, quint16(Signature));
    WRITE(Byte, quint8(CompressionMethodDeflate));

    // I'd like to use a header CRC, but gzip (as of 1.3.12) STILL thinks that
    // flag 0x02 is CONTINUATION, in violation of RFC 1952...
    quint8 flags = 0;

    if (theItem.hasProperty(ArchiveItem::Path))
        flags |= FlagNameIsPresent;
    if (theItem.hasProperty(ArchiveItem::Comment))
        flags |= FlagCommentIsPresent;
    if (theItem.hasProperty(GzipItem::ItemExtra))
        flags |= FlagExtraIsPresent;
    WRITE(Byte, flags);

    WRITE(UInt32LE, QDateTime::currentDateTime().toTime_t());

    // XXX: put in extraFlags depending on compression options
    WRITE(Byte, quint8(0));

    quint8 host;
    if (theItem.hasProperty(ArchiveItem::HostOS))
        host = mapArchiveToHost(theItem.property(ArchiveItem::HostOS).toUInt());
    else {
#if defined(Q_OS_WIN32) || defined(Q_OS_WINCE)
        host = HostVFAT;
#else
        host = HostUnix;
#endif
    }
    WRITE(Byte, host);

    if (flags & FlagExtraIsPresent) {
        QByteArray extra = theItem.property(GzipItem::ItemExtra).toByteArray();
        quint16 size = quint16(extra.size());
        WRITE(UInt16LE, size);
        if (!writeBuffer(extra, size))
            return errorIoDevice(outputDevice());
        crc = CrcUpdate(crc, extra.constData(), size);
    }

    if (flags & FlagNameIsPresent) {
        QByteArray name = theItem.property(ArchiveItem::Path).toString().toLatin1();

        // gzip can only store the filename, not the entire path
        int ix = name.lastIndexOf('/');
        if (ix >= 0)
            name = name.mid(ix);

        quint16 size = quint16(name.size());
        if (!writeBuffer(name, size))
            return errorIoDevice(outputDevice());
        crc = CrcUpdate(crc, name.constData(), size);
        WRITE(Byte, quint8(0));
    }

    if (flags & FlagCommentIsPresent) {
        QByteArray comment = theItem.property(ArchiveItem::Comment).toString().toLatin1();
        quint16 size = quint16(comment.size());
        if (!writeBuffer(comment, size))
            return errorIoDevice(outputDevice());
        crc = CrcUpdate(crc, comment.constData(), size);
        WRITE(Byte, quint8(0));
    }

// can't put in a CRC, for compatibility with gzip... :-(
//    WRITE(UInt16LE, quint16(CrcValue(crc) & 0xffff));
    
    return true;
}

bool GzipArchive::open(QIODevice *file, const quint64 maxCheckStartPosition)
{
    pushInputDevice(file);

    if (!readHeader())
        return false;

    return true;
}

bool GzipArchive::extract(const quint64 index, QIODevice *target)
{
    if (index != 0) {
        setErrorString(tr("a gzip archive only contains one file"));
        return false;
    }

    const ArchiveItem item = itemAt(0);

    if (item.property(ArchiveItem::CompressionMethod) != ArchiveItem::CompressionMethodDeflate) {
        setErrorString(tr("unknown compression method"));
        return false;
    }

    Q_ASSERT(item.hasProperty(ArchiveItem::Position));
    Q_ASSERT(item.hasProperty(ArchiveItem::CompressedSize));

    seek(item.property(ArchiveItem::Position).toLongLong(), RelativeToBeginning, 0);

    LimitedIoDevice limitDev;
    limitDev.setBackingDevice(inputDevice());
    limitDev.setLimit(item.property(ArchiveItem::CompressedSize).toLongLong());

    if (!limitDev.open(QIODevice::ReadOnly))
        return errorIoDevice(&limitDev);

    FilterCrc crc;
    FilteringIoDevice filterDev;
    filterDev.setBackingDevice(target);
    filterDev.setFilter(&crc);

    if (!filterDev.open(QIODevice::WriteOnly))
        return errorIoDevice(&filterDev);

    CompressionCodec *decoder = CreateDecoderForName("Deflate", this);

    if (!decoder)
        return errorOutOfMemory();

    decoder->setInputDevice(&limitDev);
    decoder->setOutputDevice(&filterDev);

    if (!decoder->code()) {
        errorDecompression(decoder);
        delete decoder;
        return false;
    }
    delete decoder;

    limitDev.close();
    filterDev.close();
    if (crc.crcValue() != item.property(ArchiveItem::CRC).toUInt())
        return errorBadCrc();

    return true;
}

bool GzipArchive::canUpdate() const
{
    return true;
}

bool GzipArchive::setUpdateProperties(const QMap<QByteArray, QByteArray>& props)
{
    // XXX: we should eventually support setting the compression level here...
    if (!props.isEmpty())
        return false;
    return true;
}

bool GzipArchive::updateTo(QIODevice *target, const QMap<quint64, ArchiveItem>& updateItems,
        const QList<ArchiveItem>& newItems)
{
    if (updateItems.size() + newItems.size() != 1) {
        // this is not technically true, since RFC 1952 gzip is a streaming forman
        // and so can simply be appended... but we don't support that (yet)
        setErrorString(tr("a gzip archive can only contain one file"));
        return false;
    }

    setOutputDevice(target);

    ArchiveItem theItem;

    if (!updateItems.isEmpty())
        theItem = updateItems.values().first();
    else
        theItem = newItems.first();

    // write the header
    if (!writeHeader(theItem))
        return false;

    QIODevice *stream = streamForItem(theItem);

    if (stream) {
        // we have new data, compress it and write it out
        FilterCrc crc;
        FilteringIoDevice filterDev;
        filterDev.setBackingDevice(stream);
        filterDev.setFilter(&crc);
    
        if (!filterDev.open(QIODevice::ReadOnly)) {
            delete stream;
            return errorIoDevice(&filterDev);
        }

        CompressionCodec *coder = CreateCoderForName("Deflate", this);

        if (!coder) {
            delete stream;
            return errorOutOfMemory();
        }

        // XXX: set the coder's properties
        coder->setInputDevice(&filterDev);
        coder->setOutputDevice(outputDevice());

        // do the compression
        if (!coder->code()) {
            delete stream;
            errorCompression(coder);
            delete coder;
            return false;
        }

        // and now write out the tail header
        if (!writeUInt32LE(crc.crcValue()) || !writeUInt32LE(stream->pos() & 0xffffffff)) {
            delete stream;
            delete coder;
            return errorIoDevice(outputDevice());
        }

        delete stream;
        delete coder;
        return true;
    }

    if (!theItem.hasProperty(ArchiveItem::Position) || !inputDevice()) {
        setErrorString(tr("cannot copy a nonexistant item"));
        return false;
    }

    if (!seek(theItem.property(ArchiveItem::Position).toLongLong(), RelativeToBeginning, 0))
        return errorIoDevice(inputDevice());

    LimitedIoDevice limitDev;
    limitDev.setBackingDevice(inputDevice());
    limitDev.setLimit(theItem.property(ArchiveItem::CompressedSize).toLongLong());
    if (!limitDev.open(QIODevice::ReadOnly))
        return errorIoDevice(&limitDev);

    CompressionCodec *copier = CreateCoderForName("Copy", this);

    if (!copier)
        return errorOutOfMemory();

    copier->setInputDevice(&limitDev);
    copier->setOutputDevice(outputDevice());

    if (!copier->code()) {
        delete copier;
        return errorCompression(copier);
    }

    if (!writeUInt32LE(theItem.property(ArchiveItem::CRC).toUInt())
        || !writeUInt32LE(theItem.property(ArchiveItem::UncompressedSize).toUInt())) {
        delete copier;
        return errorIoDevice(outputDevice());
    }

    delete copier;
    return true;
}

}
}

EXPORT_ARCHIVE(gzip, "application/x-gzip", ReadWrite, ArchiveEngine::Gzip::GzipArchive)

#include "GzipArchive.moc"
