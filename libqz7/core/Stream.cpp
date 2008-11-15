#include "qz7/Stream.h"
#include <QtCore/QCoreApplication>
#include <QtCore/QIODevice>
#include <QtCore/QString>

namespace qz7 {

ReadStream::~ReadStream()
{
}

WriteStream::~WriteStream()
{
}

QioReadStream::QioReadStream(QIODevice *dev)
    : mBytesRead(0), mDevice(dev)
{
//    Q_ASSERT(dev->isReadable());
}

QioReadStream::~QioReadStream()
{
}

bool QioReadStream::read(quint8 *buffer, int bytes)
{
    int b = bytes;

    while (bytes) {
        int r = device()->read(reinterpret_cast<char *>(buffer), bytes);
        if (r < 0 || (r == 0 && !device()->waitForReadyRead(-1)))
            return false;
        buffer += r;
        bytes -= r;
    }

    mBytesRead += b; 
    return true;
}

int QioReadStream::readSome(quint8 *buffer, int minBytes, int maxBytes)
{
    int ret = 0;
    while (maxBytes) {
        int r = device()->read(reinterpret_cast<char *>(buffer), maxBytes);

        if (r <= 0) {
            if (ret >= minBytes)
                goto done;
            if (r < 0)
                return -1;
            if (!device()->waitForReadyRead(-1))
                goto done;
        }

        buffer += r;
        maxBytes -= r;
        ret += r;
    }
done:
    mBytesRead += ret;
    return ret;
}

bool QioReadStream::skipForward(qint64 bytes)
{
    // try to seek; otherwise fall back to reading
    if (device()->seek(device()->pos() + bytes))
        return true;
    const qint64 BufferSize = 64 * 1024;
    char buffer[BufferSize];
    while (bytes) {
        int toRead = qMin(bytes, BufferSize);
        int r = device()->read(buffer, toRead);
        if (r < 0 || (r == 0 && !device()->waitForReadyRead(-1)))
            return false;
        bytes -= r;
        mBytesRead += r;
    }
    return true;
}

bool QioReadStream::atEnd() const
{
    return device()->atEnd();
}

qint64 QioReadStream::bytesRead() const
{
    return mBytesRead;
}

QString QioReadStream::errorString() const
{
    return device()->errorString();
}

QioSeekableReadStream::QioSeekableReadStream(QIODevice *dev)
    : QioReadStream(dev)
{
//    Q_ASSERT(!device()->isSequential());
}

QioSeekableReadStream::~QioSeekableReadStream()
{
}

bool QioSeekableReadStream::read(quint8 *buffer, int bytes)
{
    return QioReadStream::read(buffer, bytes);
}

int QioSeekableReadStream::readSome(quint8 *buffer, int minBytes, int maxBytes)
{
    return QioReadStream::readSome(buffer, minBytes, maxBytes);
}

bool QioSeekableReadStream::skipForward(qint64 bytes)
{
    return QioReadStream::skipForward(bytes);
}

bool QioSeekableReadStream::atEnd() const
{
    return QioReadStream::atEnd();
}

qint64 QioSeekableReadStream::bytesRead() const
{
    return QioReadStream::bytesRead();
}

QString QioSeekableReadStream::errorString() const
{
    return QioReadStream::errorString();
}

qint64 QioSeekableReadStream::size() const
{
    return device()->size();
}

qint64 QioSeekableReadStream::pos() const
{
    return device()->pos();
}

bool QioSeekableReadStream::setPos(qint64 pos)
{
    return device()->seek(pos);
}

QioWriteStream::QioWriteStream(QIODevice *dev)
    : mBytesWritten(0), mDevice(dev)
{
//    Q_ASSERT(device()->isWritable());
}

QioWriteStream::~QioWriteStream()
{
}

bool QioWriteStream::write(const quint8 *buffer, int bytes)
{
    while (bytes) {
        int w = device()->write(reinterpret_cast<const char *>(buffer), bytes);
        if (w < 0 || (w == 0 && !device()->waitForBytesWritten(-1)))
            return false;
        buffer += w;
        bytes -= w;
        mBytesWritten += w;
    }
    return true;
}

void QioWriteStream::flush()
{
    // FIXME: should we?
    // QFile *file = qobject_cast<QFile *>(device());
    // if (file)
    //     flush();
}

qint64 QioWriteStream::bytesWritten() const
{
    return mBytesWritten;
}

QString QioWriteStream::errorString() const
{
    return device()->errorString();
}

LimitedReadStream::LimitedReadStream(ReadStream *source, qint64 byteLimit)
    : mBytesLeft(byteLimit), mBytesInitial(byteLimit), mStream(source)
{
}

bool LimitedReadStream::read(quint8 *buffer, int bytes)
{
    if (bytes > mBytesLeft)
        return false;
    mBytesLeft -= bytes;
    return mStream->read(buffer, bytes);
}

int LimitedReadStream::readSome(quint8 *buffer, int minBytes, int maxBytes)
{
    if (minBytes > mBytesLeft)
        return -1;
    int read = mStream->readSome(buffer, minBytes, int(qMin(qint64(maxBytes), mBytesLeft)));
    if (read >= 0)
        mBytesLeft -= read;
    return read;
}

bool LimitedReadStream::skipForward(qint64 bytes)
{
    if (bytes > mBytesLeft)
        return false;
    mBytesLeft -= bytes;
    return mStream->skipForward(bytes);
}

bool LimitedReadStream::atEnd() const
{
    return (mBytesLeft == 0) || mStream->atEnd();
}

qint64 LimitedReadStream::bytesRead() const
{
    return (mBytesInitial - mBytesLeft);
}

QString LimitedReadStream::errorString() const
{
    if (!mStream->errorString().isEmpty())
        return mStream->errorString();
    return QCoreApplication::translate("libqz7", "attempted to read past end of stream");
}

}
