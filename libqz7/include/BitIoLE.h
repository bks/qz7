#ifndef QZ7_BITIOLE_H
#define QZ7_BITIOLE_H

#include "Error.h"

#include <QtCore/QtGlobal>

namespace qz7 {

class ReadStream;
class WriteStream;

class BitReaderLE {
public:
    BitReaderLE(ReadStream *stream) : mStream(stream), mBuffer(new quint8[BufferSize]),
        mValid(0), mPos(0), mBitPos(0) { }
    BitReaderLE() : mStream(0), mBuffer(new quint8[BufferSize]), mValid(0), mPos(0), mBitPos(0) { }
    ~BitReaderLE() { delete[] mBuffer; }

    void setBackingStream(ReadStream *stream) { mStream = stream; mValid = 0; mPos = 0; mBitPos = 0; }

    uint peekBits(uint nrBits) {
        // check if we have enough data in our buffer to easily satisfy the request
        // note the refill() will synthesize extra 0xff bytes if needed to fullfill a peek request
        if (needRefill(nrBits))
            return refill(nrBits);

        return fetchBits(nrBits);
    }

    uint peekReversedBits(uint nrBits) {
        // ensure that we have enough bytes in the buffer
        if (needRefill(nrBits))
            refill(nrBits);

        uint pos = mPos;
        uint ret = bitReverse(mBuffer[pos]) >> (8 - mBitPos);
        uint bytesNeeded = (nrBits - mBitPos + 7) / 8;
        uint bitpos = mBitPos;
        while (bytesNeeded > 0) {
            // we pad the buffer with ones; we'll throw an error if anyone actually tries to consume them
            if (++pos <= mValid)
                ret |= bitReverse(mBuffer[pos]) << bitpos;
            else
                ret |= 0xff << bitpos;
            bitpos += 8;
            --bytesNeeded;
        }

        return ret & ((1 << nrBits) - 1);
    }

    void consumeBits(uint nrBits) {
        if (needRefill(nrBits)) {
            // we should always have the bits we're trying to consume!
            throw TruncatedArchiveError();
        }

        if (nrBits >= mBitPos) {
            nrBits -= mBitPos;
            mPos += nrBits / 8 + 1;
            mBitPos = 8 - (nrBits & 7);
        } else {
            mBitPos -= nrBits;
        }
    }

    void alignToByte() {
        peekBits(8); // make sure we have another byte
        mPos += 1;
        mBitPos = 8;
    }

    uint readBits(uint nrBits) { uint ret = peekBits(nrBits); consumeBits(nrBits); return ret; }

private:
    enum { BufferSize = 4096 };
    static const quint8 BitReverseTable[256];
    bool needRefill(uint nrBits) { return ((mValid - mPos) * 8 + mBitPos) < nrBits; }

    uint fetchBits(uint nrBits) const {
        uint pos = mPos;
        uint ret = mBuffer[pos] >> (8 - mBitPos);
        uint bytesNeeded = (nrBits - mBitPos + 7) / 8;
        uint bitpos = mBitPos;
        while (bytesNeeded > 0) {
            ret |= mBuffer[++pos] << bitpos;
            bitpos += 8;
            --bytesNeeded;
        }

        return ret & ((1 << nrBits) - 1);
    }

    quint8 bitReverse(quint8 b) { return BitReverseTable[b]; }
    uint refill(uint nrBits);

    ReadStream *mStream;
    quint8 *mBuffer;

    // the last byte with potentially valid bits in it
    uint mValid;

    // the current position within the buffer
    uint mPos;

    // the number of valid bits within the current byte
    uint mBitPos;
};

class BitWriterLE {
public:
    BitWriterLE(WriteStream *stream) : mStream(stream), mBuffer(new quint8[BufferSize]),
        mBitPos(0) { flush(); }
    BitWriterLE() : mStream(0), mBuffer(new quint8[BufferSize]), mBitPos(0) { flush(); }
    ~BitWriterLE() { flushByte(); flush(); delete[] mBuffer; }

    void setBackingStream(WriteStream *stream) { mStream = stream; }

    void writeBits(uint bits, uint nrBits) {
        if (mBitPos + nrBits >= BufferSize * 8)
            flush();
        uint pos = mBitPos / 8;
        uint shift = mBitPos & 7;
        mBuffer[pos] |= ((bits & ((1 << nrBits) - 1)) << shift) & 0xff;
        mBitPos += nrBits;
        if (nrBits > (8 - shift)) {
            bits >>= (8 - shift);
            nrBits -= (8 - shift);
            uint nrBytes = (nrBits + 7) / 8;
            while (nrBytes) {
                mBuffer[++pos] = (bits & 0xff);
                bits >>= 8;
                nrBytes--;
            }
        }
    }

    void flush();
    void flushByte() { if (mBitPos & 7) writeBits(0, 8 - (mBitPos & 7)); }

private:
    enum { BufferSize = 4096 };

    WriteStream *mStream;
    quint8 *mBuffer;
    uint mBitPos;
};

}
#endif
