// DeflateDecoder.h

#ifndef DEFLATE_DECODER_H
#define DEFLATE_DECODER_H

#include "kernel/CompressionCodec.h"
#include "kernel/QioBuffer.h"
#include "kernel/BitStreamLE.h"
#include "kernel/RingBuffer.h"

#include "../huffman/HuffmanDecoder.h"

#include "DeflateConst.h"

namespace Compress {
namespace Deflate {

class BaseDecoder : public CompressionCodecBase {
    Q_OBJECT

public:
    enum DeflateType {
        BasicDeflate,
        Deflate64,
        DeflateNSIS
    };

    BaseDecoder(DeflateType type, QObject *parent);

    void setKeepHistory(bool keepHistory) { _keepHistory = keepHistory; }
    void setExpectedSize(quint64 size) { mBytesExpected = size; }

    virtual bool code();
    virtual bool setProperties(const QMap<QString, QVariant>& properties);
    virtual bool setProperties(const QByteArray& serializedProperties);
    virtual QByteArray serializeProperties();

private:
    bool ReadBits(int numBits, quint32 *bitValue);
    bool DeCodeLevelTable(quint8 *values, int numSymbols);
    bool ReadTables();
    bool CodeSpec(quint32 curSize);

    RingBuffer mOutBuffer;
    QioReadBuffer mInputBuffer;
    BitStreamLE<QioReadBuffer> mInBitStream;
    DeflateType mType;

    Compress::Huffman::Decoder<NumHuffmanBits, FixedMainTableSize> m_MainDecoder;
    Compress::Huffman::Decoder<NumHuffmanBits, FixedDistTableSize> m_DistDecoder;
    Compress::Huffman::Decoder<NumHuffmanBits, LevelTableSize> m_LevelDecoder;

    quint32 m_StoredBlockSize;
    quint32 _numDistLevels;
    qint32 _remainLen;
    quint32 _rep0;

    quint64 mBytesExpected;

    bool _keepHistory;
    bool _needReadTable;
    bool m_FinalBlock;
    bool m_StoredMode;
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
