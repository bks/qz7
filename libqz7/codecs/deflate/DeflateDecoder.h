#ifndef QZ7_DEFLATE_DECODER_H
#define QZ7_DEFLATE_DECODER_H

#include "Codec.h"
#include "BitIoLE.h"
#include "RingBuffer.h"

#include "codec/HuffmanDecoder.h"

#include "DeflateConst.h"

namespace qz7 {
namespace deflate {

class BaseDecoder : public Codec {
    Q_OBJECT

public:
    enum DeflateType {
        BasicDeflate,
        Deflate64,
        DeflateNSIS
    };

    BaseDecoder(DeflateType type, QObject *parent);

    void setKeepHistory(bool keepHistory) { mKeepHistory = keepHistory; }
    void setExpectedSize(quint64 size) { mBytesExpected = size; }

    virtual void stream(ReadStream *from, WriteStream *to);
    virtual bool setProperty(const QString& property, const QVariant& value);
    virtual QVariant property(const QString& property) const;
    virtual QByteArray serializeProperties() const;
    virtual bool applySerializedProperties(const QByteArray& serializedProperties);

private:
    quint32 readBits(int numBits);
    void decodeLevelTable(quint8 *values, int numSymbols);
    void readTables();
    void codeChunk(quint32 curSize); // was CodeSpec

    RingBuffer mOutBuffer;
    BitReaderLE mBitStream;
    DeflateType mType;

    HuffmanDecoder<NumHuffmanBits, FixedMainTableSize> mMainDecoder;
    HuffmanDecoder<NumHuffmanBits, FixedDistTableSize> mDistDecoder;
    HuffmanDecoder<NumHuffmanBits, LevelTableSize> mLevelDecoder;

    quint64 mBytesExpected;

    quint32 mStoredBlockSize;
    quint32 mNumDistLevels;
    qint32 mRemainLen;
    quint32 mRep0;

    bool mKeepHistory : 1;
    bool mNeedReadTable : 1;
    bool mIsFinalBlock : 1;
    bool mStoredMode : 1;
};

class DeflateDecoder : public BaseDecoder
{
    Q_OBJECT
public:
    DeflateDecoder(QObject *parent = 0): BaseDecoder(BasicDeflate, parent) {}
};

class Deflate64Decoder : public BaseDecoder
{
    Q_OBJECT
public:
    Deflate64Decoder(QObject *parent = 0): BaseDecoder(Deflate64, parent) {}
};

class DeflateNSISDecoder : public BaseDecoder
{
    Q_OBJECT
public:
    DeflateNSISDecoder(QObject *parent = 0): BaseDecoder(DeflateNSIS, parent) {}
};

}
}

#endif
