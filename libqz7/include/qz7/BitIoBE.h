#ifndef QZ7_BITIOBE_H
#define QZ7_BITIOBE_H

#include "qz7/Error.h"

#include <QtCore/QtGlobal>

namespace qz7 {

class ReadStream;
class WriteStream;

class BitReaderBE {
public:
    BitReaderBE(ReadStream *stream) : mStream(stream), mBuffer(new quint8[BufferSize]),
        mValid(0), mPos(0), mBitPos(0) { }
    BitReaderBE() : mStream(0), mBuffer(new quint8[BufferSize]), mValid(0), mPos(0), mBitPos(0) { }
    ~BitReaderBE() { delete[] mBuffer; }

    void setBackingStream(ReadStream *stream) { mStream = stream; mValid = 0; mPos = 0; mBitPos = 0; }

    uint peekBits(uint nrBits) {
        // check if we have enough data in our buffer to easily satisfy the request
        // note the refill() will synthesize extra 0xff bytes if needed to fullfill a peek request
        if (needRefill(nrBits))
            return refill(nrBits);

        return fetchBits(nrBits);
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

    uint readBits(uint nrBits) { uint ret = peekBits(nrBits); consumeBits(nrBits); return ret; }

private:
    enum { BufferSize = 4096 };
    bool needRefill(uint nrBits) const { return ((mValid - mPos) * 8 + mBitPos) < nrBits; }

    uint fetchBits(uint nrBits) const {
        uint pos = mPos;
        if (nrBits > mBitPos) {
            uint ret = (mBuffer[pos] & ((1 << mBitPos) - 1)) << (nrBits - mBitPos);
            uint bitsNeeded = nrBits - mBitPos;
            while (bitsNeeded >= 8) {
                ret |= mBuffer[++pos] << (bitsNeeded - 8);
                bitsNeeded -= 8;
            }
            if (bitsNeeded > 0) {
                ret |= mBuffer[++pos] >> (8 - bitsNeeded);
            }
            return ret;
        } else {
            return (mBuffer[pos] & ((1 << mBitPos) - 1)) >> (mBitPos - nrBits);
        }
    }

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

class BitWriterBE {
public:
    BitWriterBE(WriteStream *stream) : mStream(stream), mBuffer(new quint8[BufferSize]),
        mBitPos(0) { flush(); }
    BitWriterBE() : mStream(0), mBuffer(new quint8[BufferSize]), mBitPos(0) { flush(); }
    ~BitWriterBE() { flushByte(); flush(); delete[] mBuffer; }

    void setBackingStream(WriteStream *stream) { mStream = stream; }

    void writeBits(uint bits, uint nrBits) {
        if (mBitPos + nrBits >= BufferSize * 8)
            flush();
        uint pos = mBitPos / 8;
        uint free = 8 - (mBitPos & 7);
        mBitPos += nrBits;

        if (nrBits < free) {
            // easy case: just pack the bits into the current byte
            mBuffer[pos] |= (bits & ((1 << nrBits) - 1)) << (free - nrBits);
        } else {
            // complex case: start by packing as many bits into the current
            // byte as we have space for
            mBuffer[pos] |= (bits & ((1 << nrBits) - 1)) >> (nrBits - free);

            if (nrBits > free) {
                // we have more bits than fit
                uint nrBytes = (nrBits - free + 7) / 8;
                uint shift = nrBits - free - 8;
                while (nrBytes > 1) {
                    // the last byte may be partially empty, but the intermediate
                    // ones are complete ans so can just be tossed into the buffer
                    mBuffer[++pos] = ((bits >> shift) & 0xff);
                    nrBytes--;
                    shift -= 8;
                }
                // do the last byte separately, so that its bits end up at 
                // the top of the byte as they should
                if (nrBytes) {
                    shift += 8;
                    bits <<= 8;
                    mBuffer[++pos] = ((bits >> shift) & 0xff);
                }
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

};
#endif
