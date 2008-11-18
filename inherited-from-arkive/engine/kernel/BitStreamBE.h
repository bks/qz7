#ifndef BITSTREAMBE_H
#define BITSTREAMBE_H

#include <QtCore/QtGlobal>

/**
 * BitStreamBE provides a bit-oriented stream on top of a Qio{Read,Write}Buffer. The bits
 * in the stream are stored as most-significant-bit-first.
 * @see BitStreamLE
 */
template<class BackingBuffer> class BitStreamBE {
public:
    BitStreamBE() : mBackingBuffer(0), mBitBuffer(0), mBitPosition(0), mTotalBitPosition(0) {}
    BitStreamBE(const BitStreamBE<BackingBuffer>& other)
        : mBackingBuffer(0), mTotalBitPosition(0) { operator=(other); }

    void setBackingDevice(BackingBuffer *dev);

    bool readBits(int bits, quint32 *bitValue);
    bool peekBits(int bits, quint32 *bitValue);
    void skipBits(int bits);
    qint64 pos() const;
    qint64 bitPos() const;
    int bitsLeftInByte() const;
    int bitsAvailable() const;

    bool writeBits(int bits, quint32 bitValue);
    bool flushBits();

    BitStreamBE<BackingBuffer>& operator= (const BitStreamBE<BackingBuffer>& other);
    
private:
    BackingBuffer *mBackingBuffer;
    quint32 mBitBuffer;
    int mBitPosition;
    qint64 mTotalBitPosition;
};

template<class T> inline void BitStreamBE<T>::setBackingDevice(T *dev)
{
    mBackingBuffer = dev;
    mBitPosition = 0;
    mTotalBitPosition = 0;
    mBitBuffer = 0;
};

template<class T> inline bool BitStreamBE<T>::readBits(int bits, quint32 *bitValue)
{
    bool ok = peekBits(bits, bitValue);

    if (ok)
        skipBits(bits);
    return ok;
};

template<class T> inline bool BitStreamBE<T>::peekBits(int bits, quint32 *bitValue)
{
    if (mBitPosition < bits) {
        // we need at least one more byte...
        int bytesNeeded = (bits - mBitPosition + 7) / 8;
        while (bytesNeeded) {
            quint8 byte;

            if (!mBackingBuffer->readByte(&byte))
                return false;

            // and slap the byte into the bit buffer
            mBitPosition += 8;
            mBitBuffer <<= 8;
            mBitBuffer |= byte;
            bytesNeeded--;
        }
    }

    *bitValue = ((mBitBuffer >> (mBitPosition - bits)) & ((1 << bits) - 1));
    return true;
};

template<class T> inline void BitStreamBE<T>::skipBits(int bits)
{
    mBitPosition -= bits;

    Q_ASSERT(mBitPosition > -8);

    mTotalBitPosition += bits;
};

template<class T> inline qint64 BitStreamBE<T>::pos() const
{
    return mTotalBitPosition / 8;
};

template<class T> inline qint64 BitStreamBE<T>::bitPos() const
{
    return mTotalBitPosition;
};

template<class T> inline int BitStreamBE<T>::bitsLeftInByte() const
{
    return (mBitPosition & 7);
};

template<class T> inline int BitStreamBE<T>::bitsAvailable() const
{
    return mBitPosition;
};

template<class T> inline bool BitStreamBE<T>::writeBits(int bits, quint32 bitValue)
{
    while (bits) {
        if (bits + mBitPosition < 8) {
            mBitBuffer <<= bits;
            mBitBuffer |= bitValue & ((1 << bits) - 1);
            mBitPosition += bits;
            return true;
        }
        
        mBitBuffer <<= qMin(bits, 8);
        mBitBuffer |= (bitValue >> qMax(0, bits - 8)) & ((1 << qMin(bits, 8)) - 1);

        if (!mBackingBuffer->writeByte(quint8(mBitBuffer)))
            return false;

        mBitPosition = 0;
        mBitBuffer = 0;
        bits -= qMin(bits, 8);
    }
    return true;
};

template<class T> inline bool BitStreamBE<T>::flushBits()
{
    Q_ASSERT(mBitPosition < 8);

    bool ok = true;
    if (mBitPosition > 0)
        ok = writeBits(0, 8 - mBitPosition);
    mBitBuffer = 0;
    mBitPosition = 0;

    return ok;
};

template<class T> inline BitStreamBE<T>& BitStreamBE<T>::operator= (const BitStreamBE<T>& other)
{
    if (&other == this)
        return *this;

    mBitPosition = other.mBitPosition;
    mBitBuffer = other.mBitBuffer;
    mTotalBitPosition = 0;

    return *this;
};

#endif
