#include "kernel/RingBuffer.h"

#include <QtCore/QDebug>

#include <string.h>

RingBuffer::RingBuffer(QIODevice *backingDevice)
    : mWritten(0), mBackingDevice(0), mBuffer(0), mSize(0), mBase(0), mPosition(0)
{
    if (backingDevice)
        setBackingDevice(backingDevice);
}

RingBuffer::~RingBuffer()
{
    if (mBase != mPosition)
        qWarning("RingBuffer: being deleted while containing unflushed data");

    delete[] mBuffer;
}

void RingBuffer::setBackingDevice(QIODevice *backingDevice)
{
    Q_ASSERT(backingDevice);
    Q_ASSERT(backingDevice->openMode() == QIODevice::WriteOnly);

    mBackingDevice = backingDevice;
}

bool RingBuffer::setBufferSize(quint32 size)
{
    Q_ASSERT(mPosition == mBase);

    if (size == mSize) {
        // just clear the buffer
        ::memset(mBuffer, 0, mSize);
        return true;
    }

    delete mBuffer;
    mBuffer = new quint8[size];
    mSize = size;

    if (mBuffer)
        ::memset(mBuffer, 0, mSize);

    return (mBuffer != 0);
}

bool RingBuffer::writeBytes(const quint8 *buf, quint32 length)
{
    Q_ASSERT(mBuffer); 
    while (length) {
        int bytesAvailable = mBase - mPosition - 1;

        if (bytesAvailable < 0)
            bytesAvailable += mSize;

        Q_ASSERT(bytesAvailable > 0);

        uint bytes = qMin(qMin(length, mSize - mPosition), quint32(bytesAvailable));

        quint8 *dst = &mBuffer[mPosition];

        // do the copy: this can't be a ::memmove, because back-references
        // may be self referential (e.g. copyBytes(0, 6) with buffer XY should give
        // XYYYYYYY
        const quint8 *end = &mBuffer[mPosition + bytes];

        while (dst != end) {
            *dst = *buf;
            dst++;
            buf++;
        }

        length -= bytes;
        mPosition += bytes;
        mWritten += bytes;
        if (mPosition >= mSize)
            mPosition = 0;

        if (bytes == bytesAvailable) {
            if (!flush())
                return false;
        }
    }
    return true;
}

// this function is too large to conveniently inline, doing so seems to
// have no real benefit...
bool RingBuffer::copyBytes(quint32 backwardsDistance, quint32 length)
{
    // XXX: > or >=?
    if (length > mSize) {
        qWarning("RingBuffer::copyBytes: can't copy a block bigger than the buffer");
        return false;
    }

    int position = mPosition - backwardsDistance - 1;
    if (position < 0)
        position += mSize;

    // fast path: the copied block will fit in without needing a flush
    // and we don't need to wrap around
    if (((position >= mBase && position + length < mSize - 2) || (position + length < mBase - 2)) &&
        ((mPosition < mBase && mPosition + length < mBase - 2)
        || (mPosition > mBase && mPosition + length < mSize - 2))) {
        quint8 *src = &mBuffer[position];
        quint8 *end = &mBuffer[position + length];
        quint8 *dst = &mBuffer[mPosition];

        if (src > dst || end < dst) {
            // no overlap, so use a fast ::memcpy
            memcpy(dst, src, length);
        } else {
            while (src != end) {
                *dst = *src;
                dst++;
                src++;
            }
        }

        mPosition += length;
        mWritten += length;
        return true;
    }

    // slow path: go through the full writeBytes machinery
    while (length) {
        int bytes = qMin(length, mSize - position);

        if (!writeBytes(&mBuffer[position], bytes))
            return false;
        length -= bytes;
        position += bytes;

        if (position == mSize)
            position = 0;
    }
    return true;
}

quint8 RingBuffer::peekByte(quint32 backwardsDistance)
{
    Q_ASSERT(mBuffer);
    
    Q_ASSERT(backwardsDistance < mSize);

    int position = mPosition - backwardsDistance - 1;

    if (position < 0)
        position += mSize;

    return mBuffer[position];
}

bool RingBuffer::flush()
{
    while (true) {
        int bytes = mPosition - mBase;
        if (bytes < 0)
            bytes = mSize - mBase;

        int written = mBackingDevice->write((const char *)&mBuffer[mBase], bytes);

        if (written < 0)
            return false;

        mBase += written;
        if (mBase == mSize)
            mBase = 0;

        if (mBase == mPosition)
            return true;

        if (written != bytes && !mBackingDevice->waitForBytesWritten(-1))
            return false;
    }
}
