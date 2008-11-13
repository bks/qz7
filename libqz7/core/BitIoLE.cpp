#include "qz7/BitIoLE.h"
#include "qz7/Error.h"

#include <QtCore/QDebug>
#include <QtCore/QIODevice>

#include <cstring>

namespace qz7 {

uint BitReaderLE::refill(uint nrBits)
{
    // copy any potentially valid bits up to the front of the buffer
    for (uint p = mPos, b = 0; p <= mValid; p++, b++)
        mBuffer[b] = mBuffer[p];
    mValid -= mPos;
    mPos = 0;

    uint bytesNeeded = (nrBits - mBitPos + 7) / 8 - (mValid - mPos);

    uint pos = mValid + ((mValid || mBitPos) ? 1 : 0);
    while (bytesNeeded) {
        // loop synchronously until we either read enough data or have EOF
        int read = mBackingDevice->read((char *)&mBuffer[pos], BufferSize - pos);

        if (read < 0)
            throw ReadError(mBackingDevice);

        // check for EOF
        if (read == 0 && !mBackingDevice->waitForReadyRead(-1)) {
            break;
        }

        if (read && !(mValid || mBitPos)) {
            // if the buffer is truly empty, the first byte we read
            // goes to restore mBitPos to 8
            mValid += read - 1;
            mBitPos = 8;
        } else {
            mValid += read;
        }

        bytesNeeded -= read;
        pos += read;
    }

    // now actually pull together the bits from the refilled buffer
    pos = mPos;
    uint ret = mBuffer[pos] >> (8 - mBitPos);
    bytesNeeded = (nrBits - mBitPos + 7) / 8;
    uint bitpos = mBitPos;
    while (bytesNeeded > 0) {
        // we pad the buffer with ones; we'll throw an error if anyone actually tries to consume them
        if (++pos <= mValid) {
            ret |= mBuffer[pos] << bitpos;
        } else {
            ret |= quint8(0xff) << bitpos;
        }
        bitpos += 8;
        --bytesNeeded;
    }

    return ret & ((1 << nrBits) - 1);
}

void BitWriterLE::flush()
{
    if (mBitPos) {
        // write out any complete bytes we have
        uint bytes = mBitPos / 8;

        int written = mBackingDevice->write((char *)mBuffer, bytes);

        if (written != bytes)
            throw WriteError(mBackingDevice);

        // copy down the (potentially) incomplete final byte
        mBuffer[0] = mBuffer[bytes];
        mBitPos &= 7;
    }

    // clear any bytes after the current one
    std::memset(&mBuffer[(mBitPos + 7) / 8], 0, (BufferSize * 8 - mBitPos) / 8);
}

};
