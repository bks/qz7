#ifndef QZ7_RING_BUFFER_H
#define QZ7_RING_BUFFER_H

#include <QtCore/QtGlobal>
#include <QtCore/QDebug>

#include <string.h>

namespace qz7 {

class WriteStream;

class RingBuffer {
public:
    RingBuffer(uint size) : mStream(0), mBuffer(new quint8[size]), mSize(size), mPos(0) { }
    RingBuffer() : mStream(0), mBuffer(0), mSize(0), mPos(0) { }
    ~RingBuffer() { delete[] mBuffer; }
    
    void setBackingStream(WriteStream *stream) { mStream = stream; }
    WriteStream *backingStream() const { return mStream; }
    void setBufferSize(uint size);
    void clear();
    void flush();
    void putByte(quint8 byte);
    void putBytes(const quint8 *buf, uint length);
    quint8 peekByte(uint bytesBackwards) const;
    void repeatBytes(uint offset, uint bytes);
    
private:
    WriteStream *mStream;
    quint8 *mBuffer;
    uint mSize;
    uint mPos;
};

inline void RingBuffer::setBufferSize(uint size)
{
    if (size == mSize)
        return;

    delete[] mBuffer;
    mBuffer = new quint8[size];
    mSize = size;
}

inline void RingBuffer::clear()
{
    ::memset(mBuffer, 0, mSize);
    mPos = 0;
}

inline void RingBuffer::putByte(quint8 byte)
{
    mBuffer[mPos++] = byte;
    
    if (mPos == mSize)
        flush();
};

inline void RingBuffer::putBytes(const quint8 *bytes, uint length)
{
    while (length) {
        // apply Duff's Device
        uint block = qMin(length, mSize - mPos);
        uint n = (block + 7) / 8;

        const quint8 *src = bytes;
        quint8 *dst = &mBuffer[mPos];
        switch (block & 7) {
        case 0: do { *dst++ = *src++;
        case 7: *dst++ = *src++;
        case 6: *dst++ = *src++;
        case 5: *dst++ = *src++;
        case 4: *dst++ = *src++;
        case 3: *dst++ = *src++;
        case 2: *dst++ = *src++;
        case 1: *dst++ = *src++; } while (--n);
        }

        mPos += block;
        length -= block;
        if (mPos == mSize)
            flush();
    }
}

inline quint8 RingBuffer::peekByte(uint bytesBackwards) const
{
    int pos = int(mPos) - int(bytesBackwards) - 1;
    
    if (pos < 0)
        pos += mSize;
    
    return mBuffer[pos];
};

inline void RingBuffer::repeatBytes(uint offset, uint bytes)
{
    // use unsigned arithmetic to find the source
    uint srcOff = qMin(mPos - offset - 1, mSize + mPos - offset - 1);

    while (bytes) {
        // apply Duff's Device
        uint block = qMin(qMin(bytes, mSize - mPos), mSize - srcOff);
        uint n = (block + 7) / 8;

        quint8 *src = &mBuffer[srcOff];
        quint8 *dst = &mBuffer[mPos];
        switch (block & 7) {
        case 0: do { *dst++ = *src++;
        case 7: *dst++ = *src++;
        case 6: *dst++ = *src++;
        case 5: *dst++ = *src++;
        case 4: *dst++ = *src++;
        case 3: *dst++ = *src++;
        case 2: *dst++ = *src++;
        case 1: *dst++ = *src++; } while (--n);
        }
        bytes -= block;
        srcOff += block;
        mPos += block;
        
        if (mPos == mSize)
            flush();
        if (srcOff == mSize)
            srcOff = 0;
    }
};

}

#endif

