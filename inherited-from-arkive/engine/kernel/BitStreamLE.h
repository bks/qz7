#ifndef BITSTREAMLE_H
#define BITSTREAMLE_H

#include <QtCore/QtGlobal>

/**
 * BitStreamLE provides a bit-oriented stream on top of a Qio{Read,Write}Buffer. The stream
 * is defined to be least-significant-byte-first. (You can get bit-reversed semantics
 * within each byte by calling peekReversedBits())
 * @see BitStreamBE
 */
template<class BackingBuffer> class BitStreamLE {
public:
    BitStreamLE() : mBackingBuffer(0), mBitBuffer(0), mReversedBitBuffer(0),
        mBitPosition(0), mTotalBitPosition(0) {}
    BitStreamLE(const BitStreamLE<BackingBuffer>& other) 
        : mBackingBuffer(0), mTotalBitPosition(0) { operator=(other); }

    void setBackingDevice(BackingBuffer *dev);

    bool readBits(int bits, quint32 *bitValue);
    bool peekBits(int bits, quint32 *bitValue);
    bool peekReversedBits(int bits, quint32 *bitValue);
    void skipBits(int bits);
    qint64 pos() const;
    qint64 bitPos() const;
    int bitsLeftInByte() const;
    int bitsAvailable() const;

    bool writeBits(int bits, quint32 bitValue);
    bool flushBits();
    
    BitStreamLE<BackingBuffer>& operator= (const BitStreamLE<BackingBuffer>& other);

private:
    BackingBuffer *mBackingBuffer;
    quint32 mBitBuffer;
    quint32 mReversedBitBuffer;
    int mBitPosition;
    qint64 mTotalBitPosition;
};

extern const quint8 BitStreamLE_BitInversionTable[];

template<class T> inline void BitStreamLE<T>::setBackingDevice(T *dev)
{
    mBackingBuffer = dev;
    mBitPosition = 0;
    mTotalBitPosition = 0;
    mBitBuffer = 0;
    mReversedBitBuffer = 0;
};

template<class T> inline bool BitStreamLE<T>::readBits(int bits, quint32 *bitValue)
{
    bool ok = peekBits(bits, bitValue);

    if (ok)
        skipBits(bits);
    return ok;
};

template<class T> inline bool BitStreamLE<T>::peekBits(int bits, quint32 *bitValue)
{
    if (mBitPosition < bits) {
        // we need at least one more byte...
        int bytesNeeded = (bits - mBitPosition + 7) / 8;
        while (bytesNeeded) {
            quint8 byte;

            if (!mBackingBuffer->readByte(&byte))
                return false;

            // and slap the byte into the bit buffers
            mBitBuffer |= byte << mBitPosition;
            mReversedBitBuffer <<= 8;
            mReversedBitBuffer |= BitStreamLE_BitInversionTable[byte];
            mBitPosition += 8;
            bytesNeeded -= 1;
        }
    }

    *bitValue = mBitBuffer & ((1 << bits) - 1);
    return true;
};

template<class T> inline bool BitStreamLE<T>::peekReversedBits(int bits, quint32 *bitValue)
{
    quint32 unreversedValue;
    bool ok = peekBits(bits, &unreversedValue);

    if (!ok)
        return false;

    *bitValue = (mReversedBitBuffer >> (mBitPosition - bits)) & ((1 << bits) - 1);
    return true;
};

template<class T> inline void BitStreamLE<T>::skipBits(int bits)
{
    mBitPosition -= bits;
    mBitBuffer >>= bits;

    // fetch bits to get us back to less than one byte ahead of ourselves
    while (mBitPosition <= -8) {
        quint8 temp;
        mBackingBuffer->readByte(&temp);
        mBitPosition += 8;
    }

    Q_ASSERT(mBitPosition > -8);

    mTotalBitPosition += bits;
};

template<class T> inline qint64 BitStreamLE<T>::pos() const
{
    return mTotalBitPosition / 8;
};

template<class T> inline qint64 BitStreamLE<T>::bitPos() const
{
    return mTotalBitPosition;
};

template<class T> inline int BitStreamLE<T>::bitsLeftInByte() const
{
    return (mBitPosition & 7);
};

template<class T> inline int BitStreamLE<T>::bitsAvailable() const
{
    return mBitPosition;
};

template<class T> inline bool BitStreamLE<T>::writeBits(int bits, quint32 bitValue)
{
    Q_ASSERT(mBitPosition < 8);

    mTotalBitPosition += bits;

    while (bits) {
        quint8 maskSize = qMin(bits, 8 - mBitPosition);
        quint8 byte = (bitValue & ((1 << maskSize) - 1)) << mBitPosition;

        // load up to a byte in the buffer
        mBitBuffer |= byte;
        bitValue >>= maskSize;
        bits -= maskSize;
        mBitPosition += maskSize;

        // we have a full byte, write it out
        if (mBitPosition == 8) {
            quint8 byte = (mBitBuffer & 0xff);
            if (!mBackingBuffer->writeByte(byte))
                return false;

            mBitPosition = 0;
            mBitBuffer = 0;
        }
    }
    return true;
};

template<class T> inline bool BitStreamLE<T>::flushBits()
{
    Q_ASSERT(mBitPosition < 8);

    if (mBitPosition > 0)
        return writeBits(8 - mBitPosition, 0);

    return true;
};

template<class T> inline BitStreamLE<T>& BitStreamLE<T>::operator= (const BitStreamLE<T>& other)
{
    if (&other == this)
        return *this;

    mBitPosition = other.mBitPosition;
    mBitBuffer = other.mBitBuffer;
    mReversedBitBuffer = other.mReversedBitBuffer;
    mTotalBitPosition = 0;

    return *this;
};

#endif
