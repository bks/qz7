#ifndef QIOBUFFER_H
#define QIOBUFFER_H

#include <QtCore/QByteArray>

class QIODevice;
class QioBufferPrivate;

const uint QIO_DEFAULT_BUFFER_SIZE = 16*1024;

class QioReadBuffer {
public:
    QioReadBuffer();
    ~QioReadBuffer();

    void setBackingDevice(QIODevice *dev);
    void setBufferSize(uint bufferSize);

    bool readByte(quint8 *byte) { 
        if (!mAvailable) {
            if (!refresh())
                return false;
        }
        *byte = mBuffer[mPos];
        mPos++;
        mAvailable--;
        return true;
    }

    bool atEnd() const { return mAvailable; }

    qint64 bytesRead() const { return mBytesRead; }

private:
    bool refresh();

    qint64 mBytesRead;
    QByteArray mBacking;
    quint8 *mBuffer;
    uint mPos;
    uint mAvailable;

    QioBufferPrivate * const d;
};

class QioWriteBuffer {
public:
    QioWriteBuffer();
    ~QioWriteBuffer();

    void setBackingDevice(QIODevice *dev);
    void setBufferSize(uint bufferSize);

    bool writeByte(quint8 byte) {
        if (mPos == mSize) {
            if (!flush())
                return false;
        }
        mBuffer[mPos++] = byte;
        return true;
    }

    bool checkSpace(uint bytes) {
        Q_ASSERT(bytes < mSize);
        if (mPos + bytes >= mSize)
            return flush();
        return true;
    }

    void putByteUnchecked(quint8 byte) { mBuffer[mPos++] = byte; }

    bool flush();

    qint64 bytesWritten() const { return mBytesWritten; }
private:
    qint64 mBytesWritten;
    QByteArray mBacking;
    quint8 *mBuffer;
    uint mPos;
    uint mSize;

    QioBufferPrivate * const d;
};

#endif
