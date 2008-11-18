#include "BZip2Archive.h"
#include "kernel/CompressionCodec.h"
#include "kernel/HandlerRegistry.h"

#include <QtCore/QByteArray>

namespace ArchiveEngine {
namespace BZip2 {

BZip2Archive::BZip2Archive(QObject *parent)
    : ArchiveBase(parent)
{
}

BZip2Archive::~BZip2Archive()
{
}

// bzip2 archives are very simple: they're simply the compressed stream
// dumped to a file(!)
bool BZip2Archive::open(QIODevice *file, const quint64 maxCheckStartPosition)
{
    pushInputDevice(file);

    QByteArray sig;
    if (!readBuffer(&sig, 3))
        return errorIoDevice(inputDevice());

    if (sig != "BZh")
        return errorBadMagic("bzip2");

    if (!seek(0, RelativeToBeginning, 0))
        return errorIoDevice(inputDevice());

    ArchiveItem item;
    item.setProperty(ArchiveItem::CompressedSize, inputDevice()->size());
    item.setProperty(ArchiveItem::CompressionMethod, ArchiveItem::CompressionMethodBzip2);

    addItem(item);
    return true;
}

bool BZip2Archive::extract(const quint64 index, QIODevice *target)
{
    if (index != 0) {
        setErrorString(tr("index out of bounds"));
        return false;
    }

    if (!seek(0, RelativeToBeginning, 0))
        return errorIoDevice(inputDevice());

    CompressionCodec *decoder = CreateDecoderForName("BZip2", this);

    if (!decoder)
        return errorOutOfMemory();

    decoder->setInputDevice(inputDevice());
    decoder->setOutputDevice(target);

    bool ret = decoder->code();

    if (!ret)
        setErrorString(decoder->errorString());

    delete decoder;

    return ret;
}

bool BZip2Archive::canUpdate() const
{
    return true;
}

bool BZip2Archive::setUpdateProperties(const QMap<QByteArray, QByteArray>& props)
{
    return true;
}

bool BZip2Archive::updateTo(QIODevice *target,
    const QMap<quint64, ArchiveItem>& updateItems, const QList<ArchiveItem>& newItems)
{
    return true;
}

// close namespaces
}
}

EXPORT_ARCHIVE(bzip2, "application/x-bzip", ReadOnly, ArchiveEngine::BZip2::BZip2Archive);

#include "BZip2Archive.moc"
