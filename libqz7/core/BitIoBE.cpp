#include "BitIoBE.h"
#include "Error.h"

#include <QtCore/QDebug>
#include <QtCore/QIODevice>

#include <cstring>

namespace qz7 {

uint BitReaderBE::refill(uint nrBits)
{
    // copy any potentially valid bits up to the front of the buffer
    for (uint p = mPos, b = 0; p <= mValid; p++, b++)
        mBuffer[b] = mBuffer[p];
    mValid -= mPos;
    mPos = 0;

    uint bytesNeeded = (nrBits - mBitPos + 7) / 8 - (mValid - mPos);
    uint pos = mValid + ((mValid || mBitPos) ? 1 : 0);
    
    int read = mStream->readSome(&mBuffer[pos], bytesNeeded, BufferSize - pos);

    if (read < 0)
        throw ReadError(mStream);

    if (read && !(mValid || mBitPos)) {
        // if the buffer is truly empty, the first byte we read
        // goes to restore mBitPos to 8
        mValid += read - 1;
        mBitPos = 8;
    } else {
        mValid += read;
    }

    // now actually pull together the bits from the refilled buffer
    pos = mPos;
    if (nrBits > mBitPos) {
        uint ret = (mBuffer[pos] & ((1 << mBitPos) - 1)) << (nrBits - mBitPos);
        uint bitsNeeded = nrBits - mBitPos;
        while (bitsNeeded >= 8) {
            if (++pos <= mValid)
                ret |= mBuffer[pos] << (bitsNeeded - 8);
            else
                ret |= 0xff << (bitsNeeded - 8);
            bitsNeeded -= 8;
        }
        if (bitsNeeded > 0) {
            if (++pos <= mValid)
                ret |= mBuffer[pos] >> (8 - bitsNeeded);
            else
                ret |= 0xff >> (8 - bitsNeeded);
        }
        return ret;
    } else {
        return (mBuffer[pos] & ((1 << mBitPos) - 1)) >> (mBitPos - nrBits);
    }
}

void BitWriterBE::flush()
{
    if (mBitPos) {
        // write out any complete bytes we have
        uint bytes = mBitPos / 8;

        bool ok = mStream->write(mBuffer, bytes);

        if (!ok)
            throw WriteError(mStream);

        // copy down the (potentially) incomplete final byte
        mBuffer[0] = mBuffer[bytes];
        mBitPos &= 7;
    }

    // clear any bytes after the current one
    std::memset(&mBuffer[(mBitPos + 7) / 8], 0, (BufferSize * 8 - mBitPos) / 8);
}

};
