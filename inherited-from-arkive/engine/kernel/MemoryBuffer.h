#ifndef MEMORYBUFFER_H
#define MEMORYBUFFER_H

#include <QtCore/QtGlobal>

/**
 * MemoryWriteBuffer provides a memory-backed buffer suitable for wrapping
 * in a BitStream[BL]E. 
 * @see BitStreamBE, BitStreamLE
 */

class MemoryWriteBuffer {
public:
    MemoryWriteBuffer(uint bufferSize) 
        : mBuffer(new quint8[bufferSize]), mPos(0), mSize(bufferSize) { }
    ~MemoryWriteBuffer() { delete[] mBuffer; }

    bool writeByte(quint8 byte) { Q_ASSERT(mPos < mSize); mBuffer[mPos++] = byte; return true; }
    
    void clear() { mPos = 0; }
    uint size() const { return mPos; }
    void truncate(uint size) { mPos = size; }
    uint capacity() const { return mSize; }
    quint8 operator[](uint index) const { return mBuffer[index]; }
    quint8& operator[](uint index) { return mBuffer[index]; }

private:
    quint8 *mBuffer;
    uint mPos;
    uint mSize;
};

#endif
