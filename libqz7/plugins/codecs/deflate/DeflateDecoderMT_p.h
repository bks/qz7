#ifndef QZ7_DEFLATEDECODERMT__H
#define QZ7_DEFLATEDECODERMT_P_H

#include "qz7/BitIoLE.h"
#include "qz7/RingBuffer.h"

#include "qz7/codec/HuffmanDecoder.h"

#include "DeflateConst.h"

#include <QtCore/QMutex>
#include <QtCore/QThread>
#include <QtCore/QWaitCondition>

namespace qz7 {
namespace deflate {

class DecodeInst {
public:
    void clear() { mCode = 0; }
    bool isStop() const { return (mCode == STOP); }
    void setStop() { mCode = STOP; }
    int length() const { return mCode; }
    void setLength(int l) { mCode = l; }
    int distance() const { return mDistance; }
    void setDistance(int d) { mDistance = d; } 
    bool isLiteral() const { return (mCode & LITERAL) != 0; }
    int literalCount() const { return (mCode & 0x0f00) >> 8; }
    quint8 literal(int n) const {
        switch (n) {
        case 0: return (mCode & 0xff);
        case 1: return (mDistance & 0xff);
        case 2: return (mDistance & 0xff00) >> 8;
        default: Q_ASSERT_X(false, "DecodeInst::literal", "n is invalid");
        }
    }
    void setLiteral(int n, quint8 l) {
        switch (n) {
        case 0: mCode = (LITERAL | 0x0100 | l); return;
        case 1: mCode = (mCode & ~0x0f00) | 0x0200; mDistance = l; return;
        case 2: mCode = (mCode & ~0x0f00) | 0x0300; mDistance |= quint16(l) << 8; return;
        default: Q_ASSERT_X(false, "DecodeInst::setLiteral", "n is invalid");
        }
    }


private:
    enum { LITERAL = 0x8000, STOP = 0xffff };

    quint16 mCode;
    quint16 mDistance;
};

class DecodeInstQueue {
public:
    DecodeInstQueue();
    ~DecodeInstQueue();
    void reset();

    enum { QueueBlockSize = 8192 };

    DecodeInst *allocBlock(quint64 *written);
    void enqueueBlock(DecodeInst *block, int avail);
    void lastBlock();
    DecodeInst *dequeueBlock(int *avail);
    void wroteBlock(DecodeInst *block, uint written);
    void writeError(DecodeInst *block);
    bool hasWriteError();
    void interrupt();

private:
    enum { NR_BLOCKS = 4 };
    enum Status { Free, Allocated, Enqueued, Dequeued };

    quint64 mWritten;
    DecodeInst *mBlocks[NR_BLOCKS];
    Status mStatuses[NR_BLOCKS];
    int mLengths[NR_BLOCKS];

    int mNextAlloc;
    int mNextDequeue;

    QMutex mLock;
    QWaitCondition mWaiter;
};

class DecodeWriterThread : public QThread {
    Q_OBJECT

public:
    DecodeWriterThread(RingBuffer *buffer, DecodeInstQueue *instQueue, QObject *parent);
    virtual void run();

private:
    RingBuffer *mBuffer;
    DecodeInstQueue *mQueue;
};

class DeflateDecoderMT : public QObject {
    Q_OBJECT

public:
    DeflateDecoderMT(DeflateType type, QObject *parent);
    ~DeflateDecoderMT();

    void setKeepHistory(bool keepHistory) { mKeepHistory = keepHistory; }
    void setBytesExpected(quint64 size) { mBytesExpected = size; }

    bool stream(ReadStream *from, WriteStream *to);
    void interrupt();

signals:
    void progress(quint64 bytesIn, quint64 bytesOut);

private:
    quint32 readBits(int numBits);
    void setup();
    void createWriter();
    void decodeLevelTable(quint8 *values, int numSymbols);
    void readTables();
    int readLength(quint32 symbol);
    void streamingDecode();
    bool decodeBlock(int blockSize);
    void corrupted();

    DecodeWriterThread *mWriterThread;
    DecodeInstQueue *mQueue;

    RingBuffer mOutBuffer;
    BitReaderLE mBitStream;
    DeflateType mType;

    HuffmanDecoder<NumHuffmanBits, FixedMainTableSize> mMainDecoder;
    HuffmanDecoder<NumHuffmanBits, FixedDistTableSize> mDistDecoder;
    HuffmanDecoder<NumHuffmanBits, LevelTableSize> mLevelDecoder;

    quint64 mBytesExpected;
    quint64 mBytesDecoded;
    int mPendingLen;
    int mPendingDist;
    int mInterrupted;

    int mStoredBlockSize;
    int mNumDistLevels;

    uint mKeepHistory : 1;
    uint mNeedReadTable : 1;
    uint mIsFinalBlock : 1;
    uint mStoredMode : 1;
};


}
}

#endif
