// DeflateDecoder.cpp
#include "DeflateDecoder.h"

#include <QtCore/QDebug>

namespace Compress {
namespace Deflate {

static const int LenIdFinished = -1;
static const int LenIdNeedInit = -2;

BaseDecoder::BaseDecoder(DeflateType type, QObject *parent) :
    CompressionCodecBase(parent),
    mType(type), mBytesExpected(0),
    _keepHistory(false)
{
}

inline bool BaseDecoder::ReadBits(int numBits, quint32 *bitValue)
{
//    Q_ASSERT(numBits >= 0);
    bool ok = mInBitStream.readBits(numBits, bitValue);

    qDebug() << "readBits:" << *bitValue;
    if (!ok)
        return errorIoDevice(inputDevice());

    return ok;
}

#define CHECKREAD(n, v)         \
    do { if (!ReadBits(n, v)) return false; } while (0)

#define HUFFMAN_ERROR(huffman)                              \
    do {                                                    \
        if ((huffman).error() == Compress::Huffman::IOError)\
            return errorIoDevice(inputDevice());            \
        return errorCorrupted();                            \
    } while (0)

bool BaseDecoder::DeCodeLevelTable(quint8 *values, int numSymbols)
{
    int i = 0;
    do {
        quint32 number;
        if (!m_LevelDecoder.DecodeSymbol(&mInBitStream, &number))
            HUFFMAN_ERROR(m_LevelDecoder);

        if (number < TableDirectLevels) {
            values[i++] = (quint8)number;
        } else if (number < LevelTableSize) {
            if (number == TableLevelRepNumber) {
                if (i == 0)
                    return errorCorrupted();

                quint32 num;
                CHECKREAD(2, &num);

                num += 3;
                for (; num > 0 && i < numSymbols; num--, i++)
                    values[i] = values[i - 1];
            } else {
                quint32 num;
                if (number == TableLevel0Number) {
                    CHECKREAD(3, &num);
                    num += 3;
                } else {
                    CHECKREAD(7, &num);
                    num += 11;
                }

                for ( ; num > 0 && i < numSymbols; num--)
                    values[i++] = 0;
            }
        } else {
            return errorCorrupted();
        }
    } while (i < numSymbols);

    return true;
}

bool BaseDecoder::ReadTables(void)
{
    quint32 fieldSize;
    CHECKREAD(FinalBlockFieldSize, &fieldSize);
    m_FinalBlock = (fieldSize == FinalBlock);

    quint32 blockType;
    CHECKREAD(BlockTypeFieldSize, &blockType);
    if (blockType > BlockTypeDynamicHuffman)
        return errorCorrupted(tr("bad block type %1").arg(blockType));

    if (blockType == BlockTypeStored) {
        m_StoredMode = true;
        int numBitsForAlign = mInBitStream.bitsLeftInByte();

        quint32 temp;
        CHECKREAD(numBitsForAlign, &temp);
        CHECKREAD(StoredBlockLengthFieldSize, &m_StoredBlockSize);

        if (mType == DeflateNSIS)
            return true;

        quint32 invBlockSize;
        CHECKREAD(StoredBlockLengthFieldSize, &invBlockSize);
        if (m_StoredBlockSize == (quint16)~invBlockSize)
            return true;
        return errorCorrupted(tr("corrupted block length (%1 != %2)").arg(m_StoredBlockSize).arg((quint16)~invBlockSize));
    }

    m_StoredMode = false;

    Levels levels;
    if (blockType == BlockTypeFixedHuffman) {
        levels.SetFixedLevels();
        _numDistLevels = (mType == Deflate64) ? DistTableSize64 : DistTableSize32;
    } else {
        quint32 numLitLenLevels;
        CHECKREAD(NumLenCodesFieldSize, &numLitLenLevels);
        numLitLenLevels += NumLitLenCodesMin;

        CHECKREAD(NumDistCodesFieldSize, &_numDistLevels);
        _numDistLevels += NumDistCodesMin;

        quint32 numLevelCodes;
        CHECKREAD(NumLevelCodesFieldSize, &numLevelCodes);
        numLevelCodes += NumLevelCodesMin;

        if (mType != Deflate64)
            if (_numDistLevels > DistTableSize32)
                return errorCorrupted(tr("distance table too large"));

        quint8 levelLevels[LevelTableSize];
        for (unsigned int i = 0; i < LevelTableSize; i++) {
            int position = CodeLengthAlphabetOrder[i];
            if (i < numLevelCodes) {
                quint32 temp;
                CHECKREAD(LevelFieldSize, &temp);
                levelLevels[position] = temp;
            } else {
                levelLevels[position] = 0;
            }
        }

        if (!m_LevelDecoder.SetCodeLengths(levelLevels))
            return errorCorrupted(tr("code length table corrupted"));

        quint8 tmpLevels[FixedMainTableSize + FixedDistTableSize];

        if (!DeCodeLevelTable(tmpLevels, numLitLenLevels + _numDistLevels))
            return false;

        levels.SubClear();

        memcpy(levels.litLenLevels, tmpLevels, numLitLenLevels);
        memcpy(levels.distLevels, tmpLevels + numLitLenLevels, _numDistLevels);
    }

    if (!m_MainDecoder.SetCodeLengths(levels.litLenLevels))
        return errorCorrupted(tr("literal table corrupted"));

    if (!m_DistDecoder.SetCodeLengths(levels.distLevels))
        return errorCorrupted(tr("distance table corrupted"));

    return true;
}

bool BaseDecoder::CodeSpec(quint32 curSize)
{
    if (_remainLen == LenIdFinished)
        return true;

    if (_remainLen == LenIdNeedInit) {
        if (!_keepHistory)
            if (!mOutBuffer.setBufferSize(mType == Deflate64 ? HistorySize64 : HistorySize32))
                return errorNoMemory();
        m_FinalBlock = false;
        _remainLen = 0;
        _needReadTable = true;
    }

    if (curSize == 0)
        return true;

    while (_remainLen > 0 && curSize > 0) {
        _remainLen--;
        quint8 b = mOutBuffer.peekByte(_rep0);
        if (!mOutBuffer.writeByte(b))
            return errorIoDevice(outputDevice());
        curSize--;
    }

    while (curSize > 0) {
        if (_needReadTable) {
            if (m_FinalBlock) {
                _remainLen = LenIdFinished;
                break;
            }
            if (!ReadTables())
                return false;
            _needReadTable = false;
        }

        if (m_StoredMode) {
            for (; m_StoredBlockSize > 0 && curSize > 0; m_StoredBlockSize--, curSize--) {
                quint32 byte;
                CHECKREAD(8, &byte);
                if (!mOutBuffer.writeByte(quint8(byte)))
                    return errorIoDevice(outputDevice());
            }
            _needReadTable = (m_StoredBlockSize == 0);
            continue;
        }
        while (curSize > 0) {
            quint32 number;
            if (!m_MainDecoder.DecodeSymbol(&mInBitStream, &number))
                HUFFMAN_ERROR(m_MainDecoder);

            if (number < SymbolEndOfBlock) {
                if (!mOutBuffer.writeByte((quint8)number))
                    return errorIoDevice(outputDevice());
                curSize--;
                continue;
            } else if (number == SymbolEndOfBlock) {
                _needReadTable = true;
                break;
            } else if (number < MainTableSize) {
                number -= SymbolMatch;
                quint32 len;
                {
                    int numBits;
                    if (mType == Deflate64) {
                        len = LenStart64[number];
                        numBits = LenDirectBits64[number];
                    } else {
                        len = LenStart32[number];
                        numBits = LenDirectBits32[number];
                    }
                    quint32 val;
                    CHECKREAD(numBits, &val);
                    len += MatchMinLen + val;
                }

                quint32 locLen = len;

                if (locLen > curSize)
                    locLen = (quint32)curSize;

                if (!m_DistDecoder.DecodeSymbol(&mInBitStream, &number))
                    HUFFMAN_ERROR(m_DistDecoder);

                if (number >= _numDistLevels)
                    return errorCorrupted(tr("distance code out of bounds"));

                quint32 distance;
                CHECKREAD(DistDirectBits[number], &distance);
                distance += DistStart[number];

                if (!mOutBuffer.copyBytes(distance, locLen))
                    return errorIoDevice(outputDevice());

                curSize -= locLen;
                len -= locLen;

                if (len != 0) {
                    _remainLen = (qint32)len;
                    _rep0 = distance;
                    break;
                }
            } else {
                return errorCorrupted(tr("bad symbol %1").arg(number));
            }
        }
    }
    return true;
}

bool BaseDecoder::code()
{
    mInputBuffer.setBackingDevice(inputDevice());
    mInBitStream.setBackingDevice(&mInputBuffer);

    mOutBuffer.setBackingDevice(outputDevice());

    const quint64 start = mOutBuffer.written();
    _remainLen = LenIdNeedInit;
    for (;;) {
        quint32 curSize = 1 << 18;
        if (mBytesExpected != 0) {
            const quint64 rem = mBytesExpected - (mOutBuffer.written() - start);
            if (curSize > rem)
                curSize = (quint32)rem;
        }
        if (curSize == 0)
            break;
        
        if (!CodeSpec(curSize))
            return false;

        if (_remainLen == LenIdFinished)
            break;

        const quint64 inSize = mInBitStream.pos();
        const quint64 nowPos64 = mOutBuffer.written() - start;
        emit compressionRatio(inSize, nowPos64);
    }
    return mOutBuffer.flush();
}

bool BaseDecoder::setProperties(const QMap<QString, QVariant>& properties)
{
    if (!properties.isEmpty())
        qWarning("Compress::Deflate::BaseDecoder::setProperties(): does nothing!");

    return false;
}

bool BaseDecoder::setProperties(const QByteArray& serializedProperties)
{
    if (!serializedProperties.isEmpty())
        qWarning("Compress::Deflate::BaseDecoder::setProperties(): does nothing!");
    return false;
}

QByteArray BaseDecoder::serializeProperties()
{
    return QByteArray();
}

}
}
