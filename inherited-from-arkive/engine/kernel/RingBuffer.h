#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <QtCore/QtGlobal>
#include "kernel/arkive_export.h"

class QIODevice;

class ARKIVE_EXPORT RingBuffer {
public:
    RingBuffer(QIODevice *backingDevice = 0);
    ~RingBuffer();

    void setBackingDevice(QIODevice *backingDevice);
    bool setBufferSize(quint32 size);

    bool writeBytes(const quint8 *bytes, quint32 length);
    bool writeByte(quint8 byte);

    bool copyBytes(quint32 backwardsDistance, quint32 length);

    quint8 peekByte(quint32 backwardsDistance);

    bool flush();
    quint64 written() const { return mWritten; }

private:
    quint64 mWritten;
    QIODevice *mBackingDevice;
    quint8 *mBuffer;
    quint32 mSize;
    quint32 mBase;
    quint32 mPosition;
};

inline bool RingBuffer::writeByte(quint8 byte)
{
    // fast path: we obviously have room in the buffer and won't need to flush
    if (mPosition < mBase - 2 || (mPosition > mBase && mPosition < mSize - 2)) {
        mBuffer[mPosition++] = byte;
        mWritten++;
        return true;
    }

    // slow path: we may need to write out the buffer
    return writeBytes(&byte, 1);
};

#endif
