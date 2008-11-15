#include "DeflateDecoder.h"
#include "qz7/Error.h"

#include <QtCore/QByteArray>
#include <QtCore/QVariant>

#include <QtCore/QDebug>

namespace qz7 {
namespace deflate {

static const int LenIdFinished = -1;
static const int LenIdNeedInit = -2;

BaseDecoder::BaseDecoder(DeflateType type, QObject *parent) :
    Codec(parent),
    mType(type), mBytesExpected(0),
    mKeepHistory(false)
{
}

inline quint32 BaseDecoder::readBits(int numBits)
{
    return mBitStream.readBits(numBits);
}

void BaseDecoder::decodeLevelTable(quint8 *values, int numSymbols)
{
    int i = 0;
    do {
        quint32 number = mLevelDecoder.decodeSymbol(mBitStream);

        if (number < TableDirectLevels) {
            values[i++] = (quint8)number;
        } else if (number < LevelTableSize) {
            if (number == TableLevelRepNumber) {
                if (i == 0)
                    throw CorruptedError();

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
            throw CorruptedError();
        }
    } while (i < numSymbols);
}

void BaseDecoder::readTables(void)
{
    quint32 finalBlock = readBits(FinalBlockFieldSize);
    mIsFinalBlock = (finalBlock == FinalBlock);

    quint32 blockType = readBits(BlockTypeFieldSize);
    if (blockType > BlockTypeDynamicHuffman)
        throw CorruptedError();

    qDebug() << "final block:" << finalBlock;
    qDebug() << "block type:" << blockType;

    if (blockType == BlockTypeStored) {
        mStoredMode = true;
        mBitStream.alignToByte();
        mStoredBlockSize = readBits(StoredBlockLengthFieldSize);

        if (mType == DeflateNSIS)
            return;

        quint32 invBlockSize;
        invBlockSize = readBits(StoredBlockLengthFieldSize);
        if (mStoredBlockSize == (quint16)~invBlockSize)
            return;
        throw CorruptedError();
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
                throw CorruptedError();
        else
            if (mNumDistLevels > DistTableSize64)
                throw CorruptedError();

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

    qDebug() << "calculating symbol list";
    mMainDecoder.setCodeLengths(levels.litLenLevels);
    qDebug() << "calculating distance table symbol list";
    mDistDecoder.setCodeLengths(levels.distLevels);
    qDebug() << "tables loaded";
}

void BaseDecoder::codeChunk(quint32 curSize)
{
    if (mRemainLen == LenIdFinished)
        return;

    if (mRemainLen == LenIdNeedInit) {
        if (!mKeepHistory) {
            mOutBuffer.setBufferSize(mType == Deflate64 ? HistorySize64 : HistorySize32);
            mOutBuffer.clear();
        }
        mIsFinalBlock = false;
        mRemainLen = 0;
        mNeedReadTable = true;
    }

    if (curSize == 0)
        return;

    while (mRemainLen > 0 && curSize > 0) {
        mRemainLen--;
        quint8 b = mOutBuffer.peekByte(mRep0);
        mOutBuffer.putByte(b);
        curSize--;
    }

    while (curSize > 0) {
        if (mNeedReadTable) {
            if (mIsFinalBlock) {
                mRemainLen = LenIdFinished;
                break;
            }
            readTables();
            mNeedReadTable = false;
        }

        if (mStoredMode) {
            for (; mStoredBlockSize > 0 && curSize > 0; mStoredBlockSize--, curSize--) {
                uint byte = readBits(8);
                mOutBuffer.putByte(quint8(byte));
            }
            mNeedReadTable = (mStoredBlockSize == 0);
            continue;
        }
        while (curSize > 0) {
            quint32 symbol = mMainDecoder.decodeSymbol(mBitStream);
            qDebug() << "symbol:" << symbol;

            if (symbol < SymbolEndOfBlock) {
                mOutBuffer.putByte((quint8)symbol);
                curSize--;
                continue;
            } else if (symbol == SymbolEndOfBlock) {
                mNeedReadTable = true;
                break;
            } else if (symbol < MainTableSize) {
                quint32 len;
                {
                    quint32 number = symbol - SymbolMatch;
                    int numBits;
                    if (mType == Deflate64) {
                        len = LenStart64[number];
                        numBits = LenDirectBits64[number];
                    } else {
                        len = LenStart32[number];
                        numBits = LenDirectBits32[number];
                    }
                    quint32 val;
                    val = readBits(numBits);
                    len += MatchMinLen + val;
                }

                quint32 locLen = len;

                if (locLen > curSize)
                    locLen = (quint32)curSize;

                symbol = mDistDecoder.decodeSymbol(mBitStream);

                if (symbol >= mNumDistLevels)
                    throw CorruptedError();

                quint32 distance = readBits(DistDirectBits[symbol]);
                distance += DistStart[symbol];

                mOutBuffer.repeatBytes(distance, locLen);

                curSize -= locLen;
                len -= locLen;

                if (len != 0) {
                    mRemainLen = (qint32)len;
                    mRep0 = distance;
                    break;
                }
            } else {
                throw CorruptedError();
            }
        }
    }
    return;
}

bool BaseDecoder::stream(ReadStream *sourceStream, WriteStream *destinationStream)
{
    mInterrupted = 0;
    mBitStream.setBackingStream(sourceStream);
    mOutBuffer.setBackingStream(destinationStream);

    const quint64 start = destinationStream->bytesWritten();
    mRemainLen = LenIdNeedInit;
    for (;;) {
        if (mInterrupted)
            return false;

        quint32 curSize = 1 << 18;
        if (mBytesExpected != 0) {
            mOutBuffer.flush();
            const quint64 rem = mBytesExpected - (destinationStream->bytesWritten() - start);
            if (curSize > rem)
                curSize = (quint32)rem;
        }
        if (curSize == 0)
            break;

        // actually do the decompression
        codeChunk(curSize);

        if (mRemainLen == LenIdFinished)
            break;

        const quint64 inSize = sourceStream->bytesRead();
        const quint64 nowPos64 = destinationStream->bytesWritten() - start;
        emit progress(inSize, nowPos64);
    }
    mOutBuffer.flush();
    return true;
}

void BaseDecoder::interrupt()
{
    mInterrupted = 1;
}

bool BaseDecoder::setProperty(const QString& property, const QVariant& value)
{
    return false;
}

QVariant BaseDecoder::property(const QString& property) const
{
    return QVariant();
}

QByteArray BaseDecoder::serializeProperties() const
{
    return QByteArray();
}

bool BaseDecoder::applySerializedProperties(const QByteArray& serializedProperties)
{
    if (serializedProperties.isEmpty())
        return true;
    return false;
}

}
}
