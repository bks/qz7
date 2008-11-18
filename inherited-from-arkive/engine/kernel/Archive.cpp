#include "kernel/Archive.h"
#include "kernel/CompressionCodec.h"

#include <QtCore/QMap>
#include <QtCore/QSharedData>

ArchiveInfo::ArchiveInfo()
    : d(new Private)
{
}

QVariant ArchiveInfo::property(int p) const
{
    if (d->properties.contains(p))
        return d->properties.value(p);

    return QVariant();
}

bool ArchiveInfo::hasProperty(int p) const
{
    return d->properties.contains(p);
}

void ArchiveInfo::setProperty(int p, const QVariant& val)
{
    d->properties.insert(p, val);
}

// --------------------------------------------------------------------------------------------

ArchiveItem::ArchiveItem()
    : d(new Private)
{
}

QVariant ArchiveItem::property(int p) const
{
    if (d->properties.contains(p))
        return d->properties.value(p);

    return QVariant();
}

bool ArchiveItem::hasProperty(int p) const
{
    return d->properties.contains(p);
}

void ArchiveItem::setProperty(int p, const QVariant& val)
{
    d->properties.insert(p, val);
}

void ArchiveItem::removeProperty(int p)
{
    d->properties.remove(p);
}

//---------------------------------------------------------------------------------------------

ArchiveBase::ArchiveBase(QObject *parent)
    : QObject(parent)
{
}

ArchiveBase::~ArchiveBase()
{
}

ArchiveInfo ArchiveBase::archiveInfo() const
{
    return mInfo;
}

quint64 ArchiveBase::size() const
{
    return mItems.size();
}

ArchiveItem ArchiveBase::itemAt(const quint64 index) const
{
    if (index >= mItems.size())
        return ArchiveItem();

    return mItems.at(index);
}

bool ArchiveBase::canUpdate() const
{
    return false;
}

void ArchiveBase::addItem(const ArchiveItem& item)
{
    mItems.append(item);
}

ArchiveItem& ArchiveBase::itemAt(quint64 index)
{
    Q_ASSERT(index < mItems.size());

    return mItems[index];
}

void ArchiveBase::setInfo(const ArchiveInfo& info)
{
    mInfo = info;
}

ArchiveInfo& ArchiveBase::archiveInfo()
{
    return mInfo;
}

void ArchiveBase::pushInputDevice(QIODevice *stream)
{
    mStreams.append(stream);
}

QIODevice * ArchiveBase::popInputDevice()
{
    if (mStreams.isEmpty())
        return 0;

    return mStreams.takeLast();
}

QIODevice * ArchiveBase::inputDevice() const
{
    if (mStreams.isEmpty())
        return 0;

    return mStreams.last();
}

bool ArchiveBase::readByte(quint8 *b)
{
    QIODevice *s = mStreams.last();

    while (1) {
        int r = s->read((char *)b, 1);

        if (r == 1) {
            return true;
        }

        if (r < 0)
            return false;
        if (!s->waitForReadyRead(-1))
            return false;
    }
}

bool ArchiveBase::readUInt16LE(quint16 *s)
{
    quint16 value = 0;
    for (int i = 0; i < 2; i++) {
        quint8 b;
        if (!readByte(&b))
            return false;
        value |= (((quint16)b) << (8 * i));
    }

    *s = value;
    return true;
}

bool ArchiveBase::readUInt32LE(quint32 *l)
{
    quint32 value = 0;
    for (int i = 0; i < 4; i++) {
        quint8 b;
        if (!readByte(&b))
            return false;
        value |= (((quint32)b) << (8 * i));
    }

    *l = value;
    return true;
}

bool ArchiveBase::readUInt64LE(quint64 *ll)
{
    quint64 value = 0;
    for (int i = 0; i < 8; i++) {
        quint8 b;
        if (!readByte(&b))
            return false;
        value |= (((quint64)b) << (8 * i));
    }

    *ll = value;
    return true;
}

bool ArchiveBase::readUInt16BE(quint16 *s)
{
    quint16 value = 0;
    for (int i = 0; i < 2; i++) {
        quint8 b;
        if (!readByte(&b))
            return false;
        value <<= 8;
        value |= b;
    }
    *s = value;
    return true;
}

bool ArchiveBase::readUInt32BE(quint32 *l)
{
    quint32 value = 0;
    for (int i = 0; i < 4; i++) {
        quint8 b;
        if (!readByte(&b))
            return false;
        value <<= 8;
        value |= b;
    }
    *l = value;
    return true;
}

bool ArchiveBase::readUInt64BE(quint64 *ll)
{
    quint64 value = 0;
    for (int i = 0; i < 8; i++) {
        quint8 b;
        if (!readByte(&b))
            return false;
        value <<= 8;
        value |= b;
    }
    *ll = value;
    return true;
}

bool ArchiveBase::readBuffer(QByteArray *buffer, quint32 size)
{
    // make sure to leave room for a trailing NUL in case we may need it...
    buffer->reserve(size + 1);
    buffer->resize(size);

    int base = 0;
    while (size) {
        int read = mStreams.last()->read(buffer->data() + base, size);

        if (read < 0)
            return false;

        size -= read;
        base += read;

        if (size && !mStreams.last()->waitForReadyRead(-1))
            return false;
    }
    return true;
}

bool ArchiveBase::readOptionalBuffer(QByteArray *buffer, quint32 size)
{
    buffer->reserve(size + 1);
    buffer->resize(size);

    int base = 0;
    while (size) {
        int read = mStreams.last()->read(buffer->data() + base, size);

        if (read < 0)
            return false;

        size -= read;
        base += read;

        if (size && !mStreams.last()->waitForReadyRead(-1)) {
            buffer->resize(buffer->size() - size);
            return true;
        }
    }
    return true;
}
       
bool ArchiveBase::readString(QByteArray *buffer, quint32 maxSize)
{
    buffer->clear();
    while (maxSize) {
        quint8 b;
        if (!readByte(&b))
            return false;

        buffer->append(b);
        if (b == 0)
            return true;
    }
    return true;
}

bool ArchiveBase::skipBytes(quint32 bytes)
{
    QIODevice *s = mStreams.last();

    if (!s->isSequential())
        return s->seek(s->pos() + bytes);

    while (bytes) {
        quint8 b;
        if (!readByte(&b))
            return false;
        --bytes;
    }
    return true;
}

quint64 ArchiveBase::currentPosition()
{
    Q_ASSERT(!mStreams.last()->isSequential());

    return mStreams.last()->pos();
}

bool ArchiveBase::seek(qint64 position, SeekWhence whence, qint64 *resultingPosition)
{
    QIODevice *s = mStreams.last();
    Q_ASSERT(!s->isSequential());
    bool ok;

    switch (whence) {
    case RelativeToPosition:
        ok = s->seek(s->pos() + position);
        break;
    case RelativeToBeginning:
        ok = s->seek(position);
        break;
    case RelativeToEnd:
        ok = s->seek(s->size() + position);
        break;
    default:
        Q_ASSERT(0);
    }

    if (resultingPosition)
        *resultingPosition = s->pos();
    return ok;
}

void ArchiveBase::setOutputDevice(QIODevice *stream)
{
    mOutputStream = stream;
}

QIODevice *ArchiveBase::outputDevice() const
{
    return mOutputStream;
}

QIODevice *ArchiveBase::streamForItem(const ArchiveItem& item)
{
    if (item.hasProperty(ArchiveItem::Stream)) {
//        qDebug("item %s has a stream", qPrintable(item.property(ArchiveItem::Name).toString()));
        return item.property(ArchiveItem::Stream).value<QIODevice *>();
    }

    QIODevice *ret;
    emit needStreamForItem(item, &ret);

    return ret;
}

bool ArchiveBase::writeBytes(const quint8 *buf, int size)
{
    while (true) {
        int written = mOutputStream->write((char *)buf, size);

        if (written < 0)
            return false;

        size -= written;
        buf += written;

        if (!size)
            return true;

        if (!mOutputStream->waitForBytesWritten(-1))
            return false;
    }
}

bool ArchiveBase::writeByte(quint8 b)
{
    return writeBytes(&b, 1);
}

bool ArchiveBase::writeUInt16LE(quint16 s)
{
    quint8 buf[2];

    for (int i = 0; i < 2; i++)
        buf[i] = (s >> (i * 8)) & 0xff;

    return writeBytes(buf, 2);
}

bool ArchiveBase::writeUInt32LE(quint32 l)
{
    quint8 buf[4];

    for (int i = 0; i < 4; i++)
        buf[i] = (l >> (i * 8)) & 0xff;

    return writeBytes(buf, 4);
}

bool ArchiveBase::writeUInt64LE(quint64 ll)
{
    quint8 buf[8];

    for (int i = 0; i < 8; i++)
        buf[i] = (ll >> (i * 8)) & 0xff;

    return writeBytes(buf, 8);
}

bool ArchiveBase::writeUInt16BE(quint16 s)
{
    quint8 buf[2];

    for (int i = 0; i < 2; i++)
        buf[i] = (s >> (8 * (sizeof(s) - i - 1))) & 0xff;

    return writeBytes(buf, 2);
}

bool ArchiveBase::writeUInt32BE(quint32 l)
{
    quint8 buf[4];

    for (int i = 0; i < 4; i++)
        buf[i] = (l >> (8 * (sizeof(l) - i - 1))) & 0xff;

    return writeBytes(buf, 4);
}

bool ArchiveBase::writeUInt64BE(quint64 ll)
{
    quint8 buf[8];

    for (int i = 0; i < 8; i++)
        buf[i] = (ll >> (8 * (sizeof(ll) - i - 1))) & 0xff;

    return writeBytes(buf, 8);
}

bool ArchiveBase::writeBuffer(const QByteArray& buf)
{
    return writeBytes((const quint8 *)buf.constData(), buf.size());
}

bool ArchiveBase::writeBuffer(const QByteArray& buf, quint32 size)
{
    return writeBytes((const quint8 *)buf.constData(), size);
}

void ArchiveBase::setErrorString(const QString& str)
{
    mErrorString = str;
}

QString ArchiveBase::errorString() const
{
    return mErrorString;
}

bool ArchiveBase::errorBadMagic(const QString& type)
{
    setErrorString(tr("the archive does not seem to be a %1 file").arg(type));
    return false;
}

bool ArchiveBase::errorTruncated()
{
    setErrorString(tr("the archive is missing a piece"));
    return false;
}

bool ArchiveBase::errorBadCrc()
{
    setErrorString(tr("the archive's CRC does not validate"));
    return false;
}

bool ArchiveBase::errorOutOfMemory()
{
    setErrorString(tr("not enough memory is available"));
    return false;
}

bool ArchiveBase::errorIoDevice(QIODevice *dev)
{
    setErrorString(tr("I/O error: %1").arg(dev->errorString()));
    return false;
}

bool ArchiveBase::errorCompression(CompressionCodec *codec)
{
    setErrorString(tr("compression failed: %1").arg(codec->errorString()));
    return false;
}

bool ArchiveBase::errorDecompression(CompressionCodec *codec)
{
    setErrorString(tr("decompression failed: %1").arg(codec->errorString()));
    return false;
}

#include "Archive.moc"
