#ifndef BZIP2DECODER_H
#define BZIP2DECODER_H

#include "kernel/CompressionCodec.h"
#include "kernel/CompressionManager.h"
#include "kernel/BitStreamBE.h"
#include "kernel/QioBuffer.h"
#include "../huffman/HuffmanDecoder.h"

#include "BZip2Const.h"
#include "BZip2CRC.h"

namespace Compress {
namespace BZip2 {

class BZip2DecoderTask;

typedef Compress::Huffman::Decoder<MaxHuffmanLen, MaxAlphaSize> HuffmanDecoder;

class BZip2Decoder : public CompressionCodecBase {
    Q_OBJECT

public:
    BZip2Decoder(QObject *parent = 0);
    virtual bool code();
    virtual bool setProperties(const QMap<QString, QVariant>& properties);
    virtual bool setProperties(const QByteArray& serializedProperties);
    virtual QByteArray serializeProperties();

private:
    friend class BZip2DecoderTask;

    bool ReadBits(int bits, quint32 *bitVal);
    bool ReadByte(quint8 *byteVal);
    bool ReadCRC(quint32 *crcVal);
    bool ReadBlock(quint32 *counters, quint32 *blockSizeRes, quint32 *origPtrRes, bool *randRes);
    void DecodeBlock1(quint32 *charCounters, quint32 blockSize);
    bool DecodeBlock2(const quint32 *counters, quint32 blockSize, quint32 OrigPtr, quint32 *crc);
    bool DecodeBlock2Rand(const quint32 *counters, quint32 blockSize, quint32 OrigPtr, quint32 *crcVal);
    bool ReadSignatures(bool *streamFinished, quint32 *crcVal);

    QioReadBuffer mInputBuffer;
    BitStreamBE<QioReadBuffer> mInBitStream;
    BZip2CombinedCRC mCombinedCRC;
    quint32 mBlockSizeMax;
    quint8 m_Selectors[NumSelectorsMax];
    HuffmanDecoder m_HuffmanDecoders[NumTablesMax];

    quint8 mCacheLinePad[128];

    QioWriteBuffer mOutputBuffer;
};

class BZip2DecoderTask : public CompressionTask {
public:
    BZip2DecoderTask(BZip2Decoder *parent);
    virtual ~BZip2DecoderTask();

    virtual qint64 readBlock();
    virtual void processBlock();
    virtual qint64 writeBlock();

private:
    BZip2Decoder *q;
    quint32 *mCounters;
    quint32 mBlockSize;
    quint32 mOrigPtr;
    quint32 mExpectedCrc;
    bool mRandMode;
};

}
}

#endif
