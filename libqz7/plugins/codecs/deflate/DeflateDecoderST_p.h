#ifndef QZ7_DEFLATEDECODERST_H
#define QZ7_DEFLATEDECODERST_H

#include "qz7/BitIoLE.h"
#include "qz7/RingBuffer.h"

#include "qz7/codec/HuffmanDecoder.h"

#include "DeflateDecoder.h"
#include "DeflateConst.h"

#include <QtCore/QObject>

namespace qz7 {
namespace deflate {

class DeflateDecoderST : public QObject {
    Q_OBJECT

public:
    DeflateDecoderST(DeflateType type, QObject *parent);

    void setKeepHistory(bool keepHistory) { mKeepHistory = keepHistory; }
    void setBytesExpected(quint64 size) { mBytesExpected = size; }

    bool stream(ReadStream *from, WriteStream *to);
    void interrupt();

signals:
    void progress(quint64 bytesIn, quint64 bytesOut);

private:
    quint32 readBits(int numBits);
    void decodeLevelTable(quint8 *values, int numSymbols);
    void readTables();
    void codeChunk(quint32 curSize);

    RingBuffer mOutBuffer;
    BitReaderLE mBitStream;
    DeflateType mType;

    HuffmanDecoder<NumHuffmanBits, FixedMainTableSize> mMainDecoder;
    HuffmanDecoder<NumHuffmanBits, FixedDistTableSize> mDistDecoder;
    HuffmanDecoder<NumHuffmanBits, LevelTableSize> mLevelDecoder;

    quint64 mBytesExpected;
    int mInterrupted;

    quint32 mStoredBlockSize;
    quint32 mNumDistLevels;
    qint32 mRemainLen;
    quint32 mRep0;

    uint mKeepHistory : 1;
    uint mNeedReadTable : 1;
    uint mIsFinalBlock : 1;
    uint mStoredMode : 1;
};

}
}

#endif
