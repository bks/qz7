#include "DeflateDecoder.h"
#include "DeflateDecoderMT_p.h"
#include "qz7/Error.h"

#include <QtCore/QByteArray>
#include <QtCore/QMutexLocker>
#include <QtCore/QVariant>

namespace qz7 {
namespace deflate {

/*
 * DecodeInstQueue ties the decoder thread to the writer thread
 */

DecodeInstQueue::DecodeInstQueue()
{
    for (int i = 0; i < NR_BLOCKS; i++) {
        mBlocks[i] = new DecodeInst[QueueBlockSize];
    }
    reset();
}

DecodeInstQueue::~DecodeInstQueue()
{
    for (int i = 0; i < NR_BLOCKS; i++) {
        delete[] mBlocks[i];
    }
}

void DecodeInstQueue::reset()
{
    mWritten = 0;
    for (int i = 0; i < NR_BLOCKS; i++) {
        mStatuses[i] = Free;
        mLengths[i] = 0;
        mBlocks[i]->clear();
    }
    mNextAlloc = 0;
    mNextDequeue = 0;
}

DecodeInst *DecodeInstQueue::allocBlock(quint64 *written)
{
    QMutexLocker locker(&mLock);

    while (mStatuses[mNextAlloc] != Free)
        mWaiter.wait(&mLock);

    *written = mWritten;

    // check if something went wrong on the write side
    if (mBlocks[mNextAlloc]->isStop())
        return 0;

    DecodeInst *ptr = mBlocks[mNextAlloc];
    mStatuses[mNextAlloc] = Allocated;
    mNextAlloc = (mNextAlloc + 1) % NR_BLOCKS;
    return ptr;
}
        
void DecodeInstQueue::enqueueBlock(DecodeInst *block, int avail)
{
    QMutexLocker locker(&mLock);

    for (int i = mNextAlloc - 1; ; i = (i + 1) % NR_BLOCKS) {
        if (mBlocks[i] == block) {
            mLengths[i] = avail;
            mStatuses[i] = Enqueued;
            mWaiter.wakeAll();
            return;
        }
    }
}

void DecodeInstQueue::lastBlock()
{
    quint64 dummy;
    DecodeInst *block = allocBlock(&dummy);
    block->setStop();
    enqueueBlock(block, 0);
}

DecodeInst *DecodeInstQueue::dequeueBlock(int *avail)
{
    QMutexLocker locker(&mLock);

    while (mStatuses[mNextDequeue] != Enqueued)
        mWaiter.wait(&mLock);

    if (mBlocks[mNextDequeue]->isStop())
        return 0;

    mStatuses[mNextDequeue] = Dequeued;
    *avail = mLengths[mNextDequeue];
    DecodeInst *block = mBlocks[mNextDequeue];
    mNextDequeue = (mNextDequeue + 1) % NR_BLOCKS;
    return block;
}
    
void DecodeInstQueue::wroteBlock(DecodeInst *block, uint written)
{
    QMutexLocker locker(&mLock);

    mWritten += written;
    for (int i = mNextDequeue - 1; ; i = (i + 1) % NR_BLOCKS) {
        if (mBlocks[i] == block) {
            block->clear();    // write completed OK
            mStatuses[i] = Free;
            mWaiter.wakeAll();
            return;
        }
    }
}

void DecodeInstQueue::writeError(DecodeInst *block)
{
    QMutexLocker locker(&mLock);

    // the next block we try to allocate will report the write error
    mStatuses[mNextAlloc] = Free;
    mBlocks[mNextAlloc]->setStop();
    mWaiter.wakeAll();
}

bool DecodeInstQueue::hasWriteError()
{
    QMutexLocker locker(&mLock);
    return (mStatuses[mNextAlloc] == Free) && 
        (mBlocks[mNextAlloc]->isStop());
}

void DecodeInstQueue::interrupt()
{
    QMutexLocker locker(&mLock);

    while (mStatuses[mNextDequeue] == Allocated || mStatuses[mNextDequeue] == Dequeued)
        mNextDequeue = (mNextDequeue + 1) % NR_BLOCKS;

    mStatuses[mNextDequeue] = Enqueued;
    mBlocks[mNextDequeue]->setStop();
    mWaiter.wakeAll();
}

/*
 * DecodeWriterThread writes out the decompressed data according to the
 * decoded instructions from the DecodeInstQueue
 */

DecodeWriterThread::DecodeWriterThread(RingBuffer *buffer, DecodeInstQueue *instQueue, QObject *parent)
    : QThread(parent)
    , mBuffer(buffer)
    , mQueue(instQueue)
{
}

void DecodeWriterThread::run()
{
    while (true) {
        int avail;
        int written = 0;
        DecodeInst *block = mQueue->dequeueBlock(&avail);

        if (!block)
            return;
        try {
            for (int i = 0; i < avail; i++) {
                if (block[i].isLiteral()) {
                    int cnt = block[i].literalCount();
                    if (cnt < 0  || cnt > 3)
                        qWarning("bizarre literal count %d at %d of %d", cnt, i, avail);
                    for (int j = 0; j < cnt; j++) {
                        mBuffer->putByte(quint8(block[i].literal(j)));
                    }
                    written += cnt;
                } else {
                    mBuffer->repeatBytes(block[i].distance(), block[i].length());
                    written += block[i].length();
                }
            }
            mQueue->wroteBlock(block, written);
        } catch (...) {
            mQueue->writeError(block);
            return;
        }
    }
}

/*
 * DeflateDecoderMT organizes things and performs the decode side of the
 * decompression, adding instructions to the DecodeInstQueue for the writer
 * thread to execute.
 */

DeflateDecoderMT::DeflateDecoderMT(DeflateType type, QObject *parent)
    : QObject(parent)
    , mWriterThread(0)
    , mQueue(0)
    , mType(type)
    , mBytesExpected(0)
    , mKeepHistory(false)
{
}

DeflateDecoderMT::~DeflateDecoderMT()
{
    delete mQueue;
}

inline quint32 DeflateDecoderMT::readBits(int numBits)
{
    quint32 b = mBitStream.readBits(numBits);
    return b;
}

inline void DeflateDecoderMT::corrupted()
{
    // clean up the writer thread
    if (mQueue)
        mQueue->interrupt();
    throw CorruptedError();
}

void DeflateDecoderMT::decodeLevelTable(quint8 *values, int numSymbols)
{
    int i = 0;
    do {
        quint32 number = mLevelDecoder.decodeSymbol(mBitStream);

        if (number < TableDirectLevels) {
            values[i++] = (quint8)number;
        } else if (number < LevelTableSize) {
            if (number == TableLevelRepNumber) {
                if (i == 0)
                    corrupted();

                quint32 num;
                num = readBits(2);

                num += 3;
                for (; num > 0 && i < numSymbols; num--, i++)
                    values[i] = values[i - 1];
            } else {
                quint32 num;
                if (number == TableLevel0Number) {
                    num = readBits(3);
                    num += 3;
                } else {
                    num = readBits(7);
                    num += 11;
                }

                for ( ; num > 0 && i < numSymbols; num--)
                    values[i++] = 0;
            }
        } else {
            corrupted();
        }
    } while (i < numSymbols);
}

void DeflateDecoderMT::readTables(void)
{
    quint32 finalBlock = readBits(FinalBlockFieldSize);
    mIsFinalBlock = (finalBlock == FinalBlock);

    quint32 blockType = readBits(BlockTypeFieldSize);
    if (blockType > BlockTypeDynamicHuffman)
        corrupted();

    if (blockType == BlockTypeStored) {
        mStoredMode = true;
        mBitStream.alignToByte();
        mStoredBlockSize = readBits(StoredBlockLengthFieldSize);

        if (mType == DeflateNSIS)
            return;

        quint32 invBlockSize;
        invBlockSize = readBits(StoredBlockLengthFieldSize);
        if (mStoredBlockSize != (quint16)~invBlockSize)
            corrupted();

        return;
    }

    mStoredMode = false;

    Levels levels;
    if (blockType == BlockTypeFixedHuffman) {
        levels.setFixedLevels();
        mNumDistLevels = (mType == Deflate64) ? DistTableSize64 : DistTableSize32;
    } else {
        quint32 numLitLenLevels = readBits(NumLenCodesFieldSize) + NumLitLenCodesMin;
        mNumDistLevels = readBits(NumDistCodesFieldSize) + NumDistCodesMin;
        quint32 numLevelCodes = readBits(NumLevelCodesFieldSize) + NumLevelCodesMin;

        if (mType != Deflate64)
            if (mNumDistLevels > DistTableSize32)
                corrupted();
        else
            if (mNumDistLevels > DistTableSize64)
                corrupted();

        quint8 levelLevels[LevelTableSize];
        for (unsigned int i = 0; i < LevelTableSize; i++) {
            int position = CodeLengthAlphabetOrder[i];
            if (i < numLevelCodes) {
                quint32 temp = readBits(LevelFieldSize);
                levelLevels[position] = temp;
            } else {
                levelLevels[position] = 0;
            }
        }

        mLevelDecoder.setCodeLengths(levelLevels);

        quint8 tmpLevels[FixedMainTableSize + FixedDistTableSize];

        decodeLevelTable(tmpLevels, numLitLenLevels + mNumDistLevels);
        levels.clear();

        memcpy(levels.litLenLevels, tmpLevels, numLitLenLevels);
        memcpy(levels.distLevels, tmpLevels + numLitLenLevels, mNumDistLevels);
    }

    mMainDecoder.setCodeLengths(levels.litLenLevels);
    mDistDecoder.setCodeLengths(levels.distLevels);
}

inline void addLiteral(DecodeInst *block, int& ix, quint8 literal)
{
    if (block[ix].isLiteral()) {
        block[ix].setLiteral(block[ix].literalCount(), literal);
        if (block[ix].literalCount() == 3) {
            ix++;
            block[ix].clear();
        }
        return;
    }
    block[ix].setLiteral(0, literal);
}

inline void addLenDist(DecodeInst *block, int& ix, int length, int distance)
{
    if (block[ix].isLiteral())
        ix++;
    block[ix].setLength(length);
    block[ix].setDistance(distance);
    ix++;
    block[ix].clear();
}

inline int DeflateDecoderMT::readLength(quint32 symbol)
{
    quint32 number = symbol - SymbolMatch;
    int len, numBits;
    if (mType == Deflate64) {
        len = LenStart64[number];
        numBits = LenDirectBits64[number];
    } else {
        len = LenStart32[number];
        numBits = LenDirectBits32[number];
    }
    quint32 addend = readBits(numBits);
    len += addend;

    return len;
}

void DeflateDecoderMT::streamingDecode()
{
    bool keepGoing = true;
    while (keepGoing && !mInterrupted) {
        quint64 written;
        DecodeInst *block = mQueue->allocBlock(&written);

        if (!block)
            throw WriteError(mOutBuffer.backingStream());

        emit progress(mBitStream.backingStream()->bytesRead(), written);

        int ix = 0;
        block->clear();
        for (; ix < DecodeInstQueue::QueueBlockSize - 1; ) {
            if (mNeedReadTable) {
                readTables();
                mNeedReadTable = false;
            }
            if (mStoredMode) {
                if (!mStoredBlockSize) {
                    if (mIsFinalBlock) {
                        keepGoing = false;
                        break;
                    }
                    mNeedReadTable = true;
                    continue;
                }
                addLiteral(block, ix, readBits(8));
                mStoredBlockSize--;
            } else {
                quint32 symbol = mMainDecoder.decodeSymbol(mBitStream);
                if (symbol < SymbolEndOfBlock) {
                    addLiteral(block, ix, symbol);
                } else if (symbol == SymbolEndOfBlock) {
                    if (mIsFinalBlock) {
                        keepGoing = false;
                        break;
                    }
                    mNeedReadTable = true;
                    continue;
                } else if (symbol < MainTableSize) {
                    quint32 length = readLength(symbol);
                    quint32 distSym = mDistDecoder.decodeSymbol(mBitStream);

                    if (distSym >= mNumDistLevels)
                        corrupted();

                    quint32 distance = readBits(DistDirectBits[distSym]);
                    distance += DistStart[distSym];
                    addLenDist(block, ix, length, distance);
                } else {
                    corrupted();
                }
            }
        }
        // if we were accumulating a literal, we're now done with it
        if (block[ix].isLiteral())
            ix++;
        mQueue->enqueueBlock(block, ix);
    }
}


bool DeflateDecoderMT::decodeBlock(int blockSize)
{
    bool keepGoing = true;
    while (keepGoing) {
        quint64 written;
        DecodeInst *block = mQueue->allocBlock(&written);

        if (!block)
            throw WriteError(mOutBuffer.backingStream());

        emit progress(mBitStream.backingStream()->bytesRead(), written);

        int ix = 0;
        block->clear();
        if (mPendingLen) {
            int l = qMin(mPendingLen, blockSize);
            addLenDist(block, ix, l, mPendingDist);
            mBytesDecoded += l;
            blockSize -= l;
            mPendingLen -= l;
        }
        while (blockSize) {
            if (ix == DecodeInstQueue::QueueBlockSize - 1 || ix == DecodeInstQueue::QueueBlockSize)
                break;
            if (mNeedReadTable) {
                readTables();
                mNeedReadTable = false;
            }
            if (mStoredMode) {
                if (!mStoredBlockSize) {
                    if (mIsFinalBlock) {
                        keepGoing = false;
                        break;
                    }
                    mNeedReadTable = true;
                    continue;
                }
                addLiteral(block, ix, readBits(8));
                mBytesDecoded++;
                blockSize--;
            } else {
                quint32 symbol = mMainDecoder.decodeSymbol(mBitStream);
                if (symbol < SymbolEndOfBlock) {
                    addLiteral(block, ix, symbol);
                    mBytesDecoded++;
                    blockSize--;
                } else if (symbol == SymbolEndOfBlock) {
                    if (mIsFinalBlock) {
                        keepGoing = false;
                        break;
                    }
                    mNeedReadTable = true;
                    continue;
                } else if (symbol < MainTableSize) {
                    quint32 len = readLength(symbol);
                    quint32 distSym = mDistDecoder.decodeSymbol(mBitStream);
                    if (distSym >= mNumDistLevels)
                        corrupted();
                    quint32 distance = readBits(DistDirectBits[distSym]);
                    distance += DistStart[distSym];

                    quint32 locLen = len;
                    if (mBytesDecoded + len > mBytesExpected || len > blockSize) {
                        locLen = qMin(mBytesExpected - len, quint64(blockSize));
                        mPendingLen = len - locLen;
                        mPendingDist = distance;
                    }
                    mBytesDecoded += locLen;
                    blockSize -= locLen;

                    addLenDist(block, ix, locLen, distance);
                } else {
                    corrupted();
                }
            }
        }

        // if we were accumulating a literal, we're now done with it
        if (block[ix].isLiteral())
            ix++;
        mQueue->enqueueBlock(block, ix);
        if (!blockSize)
            return false;
    }
    return true;
}

void DeflateDecoderMT::setup()
{
    if (!mQueue)
        mQueue = new DecodeInstQueue();

    if (!mKeepHistory) {
        mOutBuffer.setBufferSize(mType == Deflate64 ? HistorySize64 : HistorySize32);
        mOutBuffer.clear();
        mQueue->reset();
        mIsFinalBlock = false;
        mPendingLen = 0;
        mNeedReadTable = true;
    }
    mBytesDecoded = 0;
}

void DeflateDecoderMT::createWriter()
{
    if (!mWriterThread)
        mWriterThread = new DecodeWriterThread(&mOutBuffer, mQueue, this);
    mWriterThread->start();
}

bool DeflateDecoderMT::stream(ReadStream *sourceStream, WriteStream *destinationStream)
{
    mInterrupted = 0;
    mBitStream.setBackingStream(sourceStream);
    mOutBuffer.setBackingStream(destinationStream);

    setup();
    createWriter();

    if (!qgetenv("QZ7_BYTES_EXPECTED").isEmpty())
        mBytesExpected = qgetenv("QZ7_BYTES_EXPECTED").toULongLong();

    try {
        if (mBytesExpected) {
            while (mBytesDecoded < mBytesExpected) {
                if (mInterrupted) {
                    mQueue->lastBlock();
                    mWriterThread->wait();
                    return false;
                }
                int blockSize = qMin(Q_UINT64_C(1) << 18, mBytesExpected - mBytesDecoded);
                bool finished = decodeBlock(blockSize);

                if (finished)
                    break;
            }
        } else {
            streamingDecode();
        }
    } catch (Error err) {
        mQueue->lastBlock();
        mWriterThread->wait();
        throw;
    }

    mQueue->lastBlock();
    mWriterThread->wait();
    mOutBuffer.flush();
    return true;
}

void DeflateDecoderMT::interrupt()
{
    mInterrupted = 1;
    mQueue->interrupt();
}

}
}
