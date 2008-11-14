// DeflateEncoder.cpp

#include "DeflateEncoder.h"
#include "qz7/codec/HuffmanEncode.h"

namespace Compress {
namespace Deflate {

// [0, 16); ratio/speed/ram tradeoff; use big value for better compression ratio.
static const int NumDivPassesMax = 10;
static const quint32 NumTables = (1 << NumDivPassesMax);

// [0, (1 << 32)); ratio/speed tradeoff; use big value for better compression ratio.
static const quint32 FixedHuffmanCodeBlockSizeMax = (1 << 8);
// [1, (1 << 32)); ratio/speed tradeoff; use small value for better compression ratio.
static const quint32 DivideCodeBlockSizeMin = (1 << 6);
// [1, (1 << 32)); ratio/speed tradeoff; use small value for better compression ratio.
static const quint32 DivideBlockSizeMin = (1 << 6);
// [1, (1 << 32))
static const quint32 MaxUncompressedBlockSize = ((1 << 16) - 1) * 1;
// [kMatchMaxLen * 2, (1 << 32))
static const quint32 MatchArraySize = MaxUncompressedBlockSize * 10;

static const quint32 MatchArrayLimit = MatchArraySize - MatchMaxLen * 4 * sizeof(quint16);
static const quint32 BlockUncompressedSizeThreshold = MaxUncompressedBlockSize -
        MatchMaxLen - NumOpts;

static const int MaxCodeBitLength = 12;
static const int MaxLevelBitLength = 7;

static const quint8 NoLiteralStatPrice = 12;
static const quint8 NoLenStatPrice = 12;
static const quint8 NoPosStatPrice = 6;

static quint8 g_LenSlots[NumLenSymbolsMax];
static quint8 g_FastPos[1 << 9];

class FastPosInit
{
public:
    FastPosInit() {
        uint i;
        for (i = 0; i < NumLenSlots; i++) {
            int c = LenStart32[i];
            int j = 1 << LenDirectBits32[i];
            for (int k = 0; k < j; k++, c++)
                g_LenSlots[c] = (quint8)i;
        }

        const int FastSlots = 18;
        int c = 0;
        for (quint8 slotFast = 0; slotFast < FastSlots; slotFast++) {
            quint32 k = (1 << DistDirectBits[slotFast]);
            for (quint32 j = 0; j < k; j++, c++)
                g_FastPos[c] = slotFast;
        }
    }
};

static FastPosInit g_FastPosInit;

inline quint32 GetPosSlot(quint32 pos)
{
    if (pos < 0x200)
        return g_FastPos[pos];
    return g_FastPos[pos >> 8] + 16;
}

BaseCoder::BaseCoder(Type type, QObject *parent) :
        CompressionCodecBase(parent),
        m_Values(0),
        m_NumFastBytes(32),
        _fastMode(false),
        _btMode(true),
        m_OnePosMatchesMemory(0),
        m_DistanceMemory(0),
        m_NumPasses(1),
        m_NumDivPasses(1),
        m_Created(false),
        m_Deflate64Mode(type == Deflate64),
        m_Tables(0),
        m_MatchFinderCycles(0)
        // m_SetMfPasses(0)
{
    if (type == Deflate64) {
        m_MatchMaxLen = MatchMaxLen64;
        m_NumLenCombinations = NumLenSymbols64;
        m_LenStart = LenStart64;
        m_LenDirectBits = LenDirectBits64;
    } else {
        m_MatchMaxLen = MatchMaxLen32;
        m_NumLenCombinations = NumLenSymbols32;
        m_LenStart = LenStart32;
        m_LenDirectBits = LenDirectBits32;
    }

    MatchFinder_Construct(&_lzInWindow);
}

bool BaseCoder::Create()
{
    if (!m_Values) {
        m_Values = new CodeValue[MaxUncompressedBlockSize];
        if (!m_Values)
            return errorNoMemory();
    }
    if (!m_Tables) {
        m_Tables = new Tables[NumTables];
        if (!m_Tables)
            return errorNoMemory();
    }

    if (m_IsMultiPass) {
        if (!m_OnePosMatchesMemory) {
            m_OnePosMatchesMemory = new quint16[MatchArraySize];
            if (!m_OnePosMatchesMemory)
                return errorNoMemory();
        }
    } else {
        if (!m_DistanceMemory) {
            m_DistanceMemory = new quint16[(MatchMaxLen + 2) * 2];
            if (!m_DistanceMemory)
                return errorNoMemory();
            m_MatchDistances = m_DistanceMemory;
        }
    }

    if (!m_Created) {
        _lzInWindow.btMode = _btMode ? 1 : 0;
        _lzInWindow.numHashBytes = 3;
        if (!MatchFinder_Create(&_lzInWindow,
                                m_Deflate64Mode ? HistorySize64 : HistorySize32,
                                NumOpts + MaxUncompressedBlockSize,
                                m_NumFastBytes, m_MatchMaxLen - m_NumFastBytes))
            return errorNoMemory();
    }
    if (m_MatchFinderCycles != 0)
        _lzInWindow.cutValue = m_MatchFinderCycles;
    m_Created = true;

    return true;
}

bool BaseCoder::setProperties(const QMap<QString, QVariant>& props)
{
    QMap<QString, QVariant>::const_iterator it, end;

    for (it = props.constBegin(), end = props.constEnd(); it != end; ++it) {
        if (it.key() == "NumberOfPasses") {
            bool ok;
            uint npasses = it.value().toUInt(&ok);

            if (!ok)
                return false;

            m_NumDivPasses = npasses;
            if (m_NumDivPasses == 0)
                m_NumDivPasses = 1;
            if (m_NumDivPasses == 1)
                m_NumPasses = 1;
            else if (m_NumDivPasses <= NumDivPassesMax)
                m_NumPasses = 2;
            else {
                m_NumPasses = 2 + (m_NumDivPasses - NumDivPassesMax);
                m_NumDivPasses = NumDivPassesMax;
            }            
        } else if (it.key() == "NumberOfFastBytes") {
            bool ok;
            uint nfastbytes = it.value().toUInt(&ok);

            if (!ok || nfastbytes < MatchMinLen || nfastbytes > m_MatchMaxLen)
                return false;

            m_NumFastBytes = nfastbytes;
        } else if (it.key() == "MatchFinderCycles") {
            bool ok;
            uint ncycles = it.value().toUInt(&ok);

            if (!ok)
                return false;
            m_MatchFinderCycles = ncycles;
        } else if (it.key() == "FastMode") {
            _fastMode = it.value().toBool();
            _btMode = !_fastMode;
        } else {
            qWarning("DeflateEncoder::setProperties: unknown property '%s'", qPrintable(it.key()));
            return false;
        }
    }
    return true;
}

bool BaseCoder::setProperties(const QByteArray& serializedProperties)
{
    qWarning("DeflateEncoder::setProperties(serializedProperties): unimplemented");
    Q_UNUSED(serializedProperties);
    return false;
}

QByteArray BaseCoder::serializeProperties()
{
    qWarning("DeflateEncoder::serializeProperties(): unimplemented");
    return QByteArray();
}

void BaseCoder::Free()
{
    delete[] m_OnePosMatchesMemory;
    m_OnePosMatchesMemory = 0;
    delete[] m_DistanceMemory;
    m_DistanceMemory = 0;
    delete[] m_Values;
    m_Values = 0;
    delete[] m_Tables;
    m_Tables = 0;
}

BaseCoder::~BaseCoder()
{
    Free();
    MatchFinder_Free(&_lzInWindow);
}

void BaseCoder::GetMatches()
{
    if (m_IsMultiPass) {
        m_MatchDistances = m_OnePosMatchesMemory + m_Pos;
        if (m_SecondPass) {
            m_Pos += *m_MatchDistances + 1;
            return;
        }
    }

    quint32 distanceTmp[MatchMaxLen * 2 + 3];

    quint32 numPairs = (_btMode) ?
                       Bt3Zip_MatchFinder_GetMatches(&_lzInWindow, distanceTmp) :
                       Hc3Zip_MatchFinder_GetMatches(&_lzInWindow, distanceTmp);

    *m_MatchDistances = (quint16)numPairs;

    if (numPairs > 0) {
        quint32 i;
        for (i = 0; i < numPairs; i += 2) {
            m_MatchDistances[i + 1] = (quint16)distanceTmp[i];
            m_MatchDistances[i + 2] = (quint16)distanceTmp[i + 1];
        }
        quint32 len = distanceTmp[numPairs - 2];
        if (len == m_NumFastBytes && m_NumFastBytes != m_MatchMaxLen) {
            quint32 numAvail = Inline_MatchFinder_GetNumAvailableBytes(&_lzInWindow) + 1;
            const quint8 *pby = Inline_MatchFinder_GetPointerToCurrentPos(&_lzInWindow) - 1;
            const quint8 *pby2 = pby - (distanceTmp[numPairs - 1] + 1);
            if (numAvail > m_MatchMaxLen)
                numAvail = m_MatchMaxLen;
            while (len < numAvail && pby[len] == pby2[len])
                len++;
            m_MatchDistances[i - 1] = (quint16)len;
        }
    }
    if (m_IsMultiPass)
        m_Pos += numPairs + 1;
    if (!m_SecondPass)
        m_AdditionalOffset++;
}

void BaseCoder::MovePos(quint32 num)
{
    if (!m_SecondPass && num > 0) {
        if (_btMode)
            Bt3Zip_MatchFinder_Skip(&_lzInWindow, num);
        else
            Hc3Zip_MatchFinder_Skip(&_lzInWindow, num);
        m_AdditionalOffset += num;
    }
}

static const quint32 InfinitePrice = 0xFFFFFFF;

quint32 BaseCoder::Backward(quint32 &backRes, quint32 cur)
{
    m_OptimumEndIndex = cur;
    quint32 posMem = m_Optimum[cur].PosPrev;
    quint16 backMem = m_Optimum[cur].BackPrev;
    do {
        quint32 posPrev = posMem;
        quint16 backCur = backMem;
        backMem = m_Optimum[posPrev].BackPrev;
        posMem = m_Optimum[posPrev].PosPrev;
        m_Optimum[posPrev].BackPrev = backCur;
        m_Optimum[posPrev].PosPrev = (quint16)cur;
        cur = posPrev;
    } while (cur > 0);
    backRes = m_Optimum[0].BackPrev;
    m_OptimumCurrentIndex = m_Optimum[0].PosPrev;
    return m_OptimumCurrentIndex;
}

quint32 BaseCoder::GetOptimal(quint32 &backRes)
{
    if (m_OptimumEndIndex != m_OptimumCurrentIndex) {
        quint32 len = m_Optimum[m_OptimumCurrentIndex].PosPrev - m_OptimumCurrentIndex;
        backRes = m_Optimum[m_OptimumCurrentIndex].BackPrev;
        m_OptimumCurrentIndex = m_Optimum[m_OptimumCurrentIndex].PosPrev;
        return len;
    }
    m_OptimumCurrentIndex = m_OptimumEndIndex = 0;

    GetMatches();

    quint32 numDistancePairs = m_MatchDistances[0];
    if (numDistancePairs == 0)
        return 1;

    const quint16 *matchDistances = m_MatchDistances + 1;
    quint32 lenMain = matchDistances[numDistancePairs - 2];

    if (lenMain > m_NumFastBytes) {
        backRes = matchDistances[numDistancePairs - 1];
        MovePos(lenMain - 1);
        return lenMain;
    }
    m_Optimum[1].Price = m_LiteralPrices[Inline_MatchFinder_GetIndexByte(&_lzInWindow, 0 - m_AdditionalOffset)];
    m_Optimum[1].PosPrev = 0;

    m_Optimum[2].Price = InfinitePrice;
    m_Optimum[2].PosPrev = 1;


    quint32 offs = 0;
    for (quint32 i = MatchMinLen; i <= lenMain; i++) {
        quint32 distance = matchDistances[offs + 1];
        m_Optimum[i].PosPrev = 0;
        m_Optimum[i].BackPrev = (quint16)distance;
        m_Optimum[i].Price = m_LenPrices[i - MatchMinLen] + m_PosPrices[GetPosSlot(distance)];
        if (i == matchDistances[offs])
            offs += 2;
    }

    quint32 cur = 0;
    quint32 lenEnd = lenMain;
    for (;;) {
        ++cur;
        if (cur == lenEnd || cur == NumOptsBase || m_Pos >= MatchArrayLimit)
            return Backward(backRes, cur);
        GetMatches();
        matchDistances = m_MatchDistances + 1;

        quint32 numDistancePairs = m_MatchDistances[0];
        quint32 newLen = 0;
        if (numDistancePairs != 0) {
            newLen = matchDistances[numDistancePairs - 2];
            if (newLen > m_NumFastBytes) {
                quint32 len = Backward(backRes, cur);
                m_Optimum[cur].BackPrev = matchDistances[numDistancePairs - 1];
                m_OptimumEndIndex = cur + newLen;
                m_Optimum[cur].PosPrev = (quint16)m_OptimumEndIndex;
                MovePos(newLen - 1);
                return len;
            }
        }
        quint32 curPrice = m_Optimum[cur].Price;
        quint32 curAnd1Price = curPrice + m_LiteralPrices[Inline_MatchFinder_GetIndexByte(&_lzInWindow, cur - m_AdditionalOffset)];
        Optimal& optimum = m_Optimum[cur + 1];
        if (curAnd1Price < optimum.Price) {
            optimum.Price = curAnd1Price;
            optimum.PosPrev = (quint16)cur;
        }
        if (numDistancePairs == 0)
            continue;
        while (lenEnd < cur + newLen)
            m_Optimum[++lenEnd].Price = InfinitePrice;
        offs = 0;
        quint32 distance = matchDistances[offs + 1];
        curPrice += m_PosPrices[GetPosSlot(distance)];
        for (quint32 lenTest = MatchMinLen; ; lenTest++) {
            quint32 curAndLenPrice = curPrice + m_LenPrices[lenTest - MatchMinLen];
            Optimal& optimum = m_Optimum[cur + lenTest];
            if (curAndLenPrice < optimum.Price) {
                optimum.Price = curAndLenPrice;
                optimum.PosPrev = (quint16)cur;
                optimum.BackPrev = (quint16)distance;
            }
            if (lenTest == matchDistances[offs]) {
                offs += 2;
                if (offs == numDistancePairs)
                    break;
                curPrice -= m_PosPrices[GetPosSlot(distance)];
                distance = matchDistances[offs + 1];
                curPrice += m_PosPrices[GetPosSlot(distance)];
            }
        }
    }
}

quint32 BaseCoder::GetOptimalFast(quint32 &backRes)
{
    GetMatches();
    quint32 numDistancePairs = m_MatchDistances[0];
    if (numDistancePairs == 0)
        return 1;
    quint32 lenMain = m_MatchDistances[numDistancePairs - 1];
    backRes = m_MatchDistances[numDistancePairs];
    MovePos(lenMain - 1);
    return lenMain;
}

void Tables::InitStructures()
{
    quint32 i;
    for (i = 0; i < 256; i++)
        litLenLevels[i] = 8;
    litLenLevels[i++] = 13;
    for (;i < FixedMainTableSize; i++)
        litLenLevels[i] = 5;
    for (i = 0; i < FixedDistTableSize; i++)
        distLevels[i] = 5;
}

void BaseCoder::LevelTableDummy(const quint8 *levels, int numLevels, quint32 *freqs)
{
    int prevLen = 0xFF;
    int nextLen = levels[0];
    int count = 0;
    int maxCount = 7;
    int minCount = 4;
    if (nextLen == 0) {
        maxCount = 138;
        minCount = 3;
    }
    for (int n = 0; n < numLevels; n++) {
        int curLen = nextLen;
        nextLen = (n < numLevels - 1) ? levels[n + 1] : 0xFF;
        count++;
        if (count < maxCount && curLen == nextLen)
            continue;

        if (count < minCount)
            freqs[curLen] += (quint32)count;
        else if (curLen != 0) {
            if (curLen != prevLen) {
                freqs[curLen]++;
                count--;
            }
            freqs[TableLevelRepNumber]++;
        } else if (count <= 10)
            freqs[TableLevel0Number]++;
        else
            freqs[TableLevel0Number2]++;

        count = 0;
        prevLen = curLen;

        if (nextLen == 0) {
            maxCount = 138;
            minCount = 3;
        } else if (curLen == nextLen) {
            maxCount = 6;
            minCount = 3;
        } else {
            maxCount = 7;
            minCount = 4;
        }
    }
}

#define WRITE_HF(i) do{ if (!mOutStream.writeBits(lens[i], codes[i])) return errorIoDevice(outputDevice()); } while (0)
#define CHECKWRITE(bits, val) do { if (!mOutStream.writeBits(bits, val)) return errorIoDevice(outputDevice()); } while (0)

bool BaseCoder::LevelTableCode(const quint8 *levels, int numLevels, const quint8 *lens, const quint32 *codes)
{
    int prevLen = 0xFF;
    int nextLen = levels[0];
    int count = 0;
    int maxCount = 7;
    int minCount = 4;
    if (nextLen == 0) {
        maxCount = 138;
        minCount = 3;
    }
    for (int n = 0; n < numLevels; n++) {
        int curLen = nextLen;
        nextLen = (n < numLevels - 1) ? levels[n + 1] : 0xFF;
        count++;
        if (count < maxCount && curLen == nextLen)
            continue;

        if (count < minCount) {
            for (int i = 0; i < count; i++)
                WRITE_HF(curLen);
        } else if (curLen != 0) {
            if (curLen != prevLen) {
                WRITE_HF(curLen);
                count--;
            }
            WRITE_HF(TableLevelRepNumber);
            CHECKWRITE(2, count - 3);
        } else if (count <= 10) {
            WRITE_HF(TableLevel0Number);
            CHECKWRITE(3, count - 3);
        } else {
            WRITE_HF(TableLevel0Number2);
            CHECKWRITE(7, count - 11);
        }

        count = 0;
        prevLen = curLen;

        if (nextLen == 0) {
            maxCount = 138;
            minCount = 3;
        } else if (curLen == nextLen) {
            maxCount = 6;
            minCount = 3;
        } else {
            maxCount = 7;
            minCount = 4;
        }
    }

    return true;
}

void BaseCoder::MakeTables()
{
    Huffman_Generate(mainFreqs, mainCodes, m_NewLevels.litLenLevels, FixedMainTableSize, MaxCodeBitLength);
    Huffman_Generate(distFreqs, distCodes, m_NewLevels.distLevels, DistTableSize64, MaxCodeBitLength);
}

quint32 Huffman_GetPrice(const quint32 *freqs, const quint8 *lens, quint32 num)
{
    quint32 price = 0;
    quint32 i;
    for (i = 0; i < num; i++)
        price += lens[i] * freqs[i];
    return price;
};

quint32 Huffman_GetPrice_Spec(const quint32 *freqs, const quint8 *lens, quint32 num, const quint8 *extraBits, quint32 extraBase)
{
    return Huffman_GetPrice(freqs, lens, num) +
           Huffman_GetPrice(freqs + extraBase, extraBits, num - extraBase);
}

quint32 BaseCoder::GetLzBlockPrice() const
{
    return
        Huffman_GetPrice_Spec(mainFreqs, m_NewLevels.litLenLevels, FixedMainTableSize, m_LenDirectBits, SymbolMatch) +
        Huffman_GetPrice_Spec(distFreqs, m_NewLevels.distLevels, DistTableSize64, DistDirectBits, 0);
}

void BaseCoder::TryBlock()
{
    memset(mainFreqs, 0, sizeof(mainFreqs));
    memset(distFreqs, 0, sizeof(distFreqs));

    m_ValueIndex = 0;
    quint32 blockSize = BlockSizeRes;
    BlockSizeRes = 0;
    for (;;) {
        if (m_OptimumCurrentIndex == m_OptimumEndIndex) {
            if (m_Pos >= MatchArrayLimit || BlockSizeRes >= blockSize || !m_SecondPass &&
                    ((Inline_MatchFinder_GetNumAvailableBytes(&_lzInWindow)  == 0) || m_ValueIndex >= m_ValueBlockSize))
                break;
        }
        quint32 pos;
        quint32 len;
        if (_fastMode)
            len = GetOptimalFast(pos);
        else
            len = GetOptimal(pos);
        CodeValue &codeValue = m_Values[m_ValueIndex++];
        if (len >= MatchMinLen) {
            quint32 newLen = len - MatchMinLen;
            codeValue.Len = (quint16)newLen;
            mainFreqs[SymbolMatch + g_LenSlots[newLen]]++;
            codeValue.Pos = (quint16)pos;
            distFreqs[GetPosSlot(pos)]++;
        } else {
            quint8 b = Inline_MatchFinder_GetIndexByte(&_lzInWindow, 0 - m_AdditionalOffset);
            mainFreqs[b]++;
            codeValue.SetAsLiteral();
            codeValue.Pos = b;
        }
        m_AdditionalOffset -= len;
        BlockSizeRes += len;
    }
    mainFreqs[SymbolEndOfBlock]++;
    m_AdditionalOffset += BlockSizeRes;
    m_SecondPass = true;
}

void BaseCoder::SetPrices(const Levels& levels)
{
    if (_fastMode)
        return;
    quint32 i;

    for (i = 0; i < 256; i++) {
        quint8 price = levels.litLenLevels[i];
        m_LiteralPrices[i] = ((price != 0) ? price : NoLiteralStatPrice);
    }

    for (i = 0; i < m_NumLenCombinations; i++) {
        quint32 slot = g_LenSlots[i];
        quint8 price = levels.litLenLevels[SymbolMatch + slot];
        m_LenPrices[i] = (quint8)(((price != 0) ? price : NoLenStatPrice) + m_LenDirectBits[slot]);
    }

    for (i = 0; i < DistTableSize64; i++) {
        quint8 price = levels.distLevels[i];
        m_PosPrices[i] = (quint8)(((price != 0) ? price : NoPosStatPrice) + DistDirectBits[i]);
    }
}

void Huffman_ReverseBits(quint32 *codes, const quint8 *lens, quint32 num)
{
    for (quint32 i = 0; i < num; i++) {
        quint32 x = codes[i];
        x = ((x & 0x5555) << 1) | ((x & 0xAAAA) >> 1);
        x = ((x & 0x3333) << 2) | ((x & 0xCCCC) >> 2);
        x = ((x & 0x0F0F) << 4) | ((x & 0xF0F0) >> 4);
        codes[i] = (((x & 0x00FF) << 8) | ((x & 0xFF00) >> 8)) >> (16 - lens[i]);
    }
}

bool BaseCoder::WriteBlock()
{
    Huffman_ReverseBits(mainCodes, m_NewLevels.litLenLevels, FixedMainTableSize);
    Huffman_ReverseBits(distCodes, m_NewLevels.distLevels, DistTableSize64);

    for (quint32 i = 0; i < m_ValueIndex; i++) {
        const CodeValue& codeValue = m_Values[i];
        if (codeValue.IsLiteral())
            CHECKWRITE(m_NewLevels.litLenLevels[codeValue.Pos], mainCodes[codeValue.Pos]);
        else {
            quint32 len = codeValue.Len;
            quint32 lenSlot = g_LenSlots[len];
            CHECKWRITE(m_NewLevels.litLenLevels[SymbolMatch + lenSlot], mainCodes[SymbolMatch + lenSlot]);
            CHECKWRITE(m_LenDirectBits[lenSlot], len - m_LenStart[lenSlot]);
            quint32 dist = codeValue.Pos;
            quint32 posSlot = GetPosSlot(dist);
            CHECKWRITE(m_NewLevels.distLevels[posSlot], distCodes[posSlot]);
            CHECKWRITE(DistDirectBits[posSlot], dist - DistStart[posSlot]);
        }
    }
    CHECKWRITE(m_NewLevels.litLenLevels[SymbolEndOfBlock], mainCodes[SymbolEndOfBlock]);

    return true;
}

static quint32 GetStorePrice(quint32 blockSize, int bitPosition)
{
    quint32 price = 0;
    do {
        quint32 nextBitPosition = (bitPosition + FinalBlockFieldSize + BlockTypeFieldSize) & 7;
        int numBitsForAlign = nextBitPosition > 0 ? (8 - nextBitPosition) : 0;
        quint32 curBlockSize = (blockSize < (1 << 16)) ? blockSize : (1 << 16) - 1;
        price += FinalBlockFieldSize + BlockTypeFieldSize + numBitsForAlign + (2 + 2) * 8 + curBlockSize * 8;
        bitPosition = 0;
        blockSize -= curBlockSize;
    } while (blockSize != 0);
    return price;
}

bool BaseCoder::WriteStoreBlock(quint32 blockSize, quint32 additionalOffset, bool finalBlock)
{
    do {
        quint32 curBlockSize = (blockSize < (1 << 16)) ? blockSize : (1 << 16) - 1;
        blockSize -= curBlockSize;

        quint32 finalBlockField = NotFinalBlock;
        if (finalBlock && (blockSize == 0))
            finalBlockField = FinalBlock;
        CHECKWRITE(FinalBlockFieldSize, finalBlockField);
        CHECKWRITE(BlockTypeFieldSize, BlockTypeStored);

        if (!mOutStream.flushBits())
            return errorIoDevice(outputDevice());

        CHECKWRITE(StoredBlockLengthFieldSize, (quint16)curBlockSize);
        CHECKWRITE(StoredBlockLengthFieldSize, (quint16)~curBlockSize);

        const quint8 *data = Inline_MatchFinder_GetPointerToCurrentPos(&_lzInWindow) - additionalOffset;
        for (quint32 i = 0; i < curBlockSize; i++)
            CHECKWRITE(8, data[i]);
        additionalOffset -= curBlockSize;
    } while (blockSize != 0);

    return true;
}

quint32 BaseCoder::TryDynBlock(int tableIndex, quint32 numPasses)
{
    Tables &t = m_Tables[tableIndex];
    BlockSizeRes = t.BlockSizeRes;
    quint32 posTemp = t.m_Pos;
    SetPrices(t);

    for (quint32 p = 0; p < numPasses; p++) {
        m_Pos = posTemp;
        TryBlock();
        MakeTables();
        SetPrices(m_NewLevels);
    }

//    (Levels&)t = m_NewLevels;
    ::memcpy(t.litLenLevels, m_NewLevels.litLenLevels, sizeof(t.litLenLevels));
   ::memcpy(t.distLevels, m_NewLevels.distLevels, sizeof(t.distLevels));

    m_NumLitLenLevels = MainTableSize;
    while (m_NumLitLenLevels > NumLitLenCodesMin && m_NewLevels.litLenLevels[m_NumLitLenLevels - 1] == 0)
        m_NumLitLenLevels--;

    m_NumDistLevels = DistTableSize64;
    while (m_NumDistLevels > NumDistCodesMin && m_NewLevels.distLevels[m_NumDistLevels - 1] == 0)
        m_NumDistLevels--;

    quint32 levelFreqs[LevelTableSize];
    memset(levelFreqs, 0, sizeof(levelFreqs));

    LevelTableDummy(m_NewLevels.litLenLevels, m_NumLitLenLevels, levelFreqs);
    LevelTableDummy(m_NewLevels.distLevels, m_NumDistLevels, levelFreqs);

    Huffman_Generate(levelFreqs, levelCodes, levelLens, LevelTableSize, MaxLevelBitLength);

    m_NumLevelCodes = NumLevelCodesMin;
    for (quint32 i = 0; i < LevelTableSize; i++) {
        quint8 level = levelLens[CodeLengthAlphabetOrder[i]];
        if (level > 0 && i >= m_NumLevelCodes)
            m_NumLevelCodes = i + 1;
        m_LevelLevels[i] = level;
    }

    return GetLzBlockPrice() +
           Huffman_GetPrice_Spec(levelFreqs, levelLens, LevelTableSize, LevelDirectBits, TableDirectLevels) +
           NumLenCodesFieldSize + NumDistCodesFieldSize + NumLevelCodesFieldSize +
           m_NumLevelCodes * LevelFieldSize + FinalBlockFieldSize + BlockTypeFieldSize;
}

quint32 BaseCoder::TryFixedBlock(int tableIndex)
{
    Tables& t = m_Tables[tableIndex];
    BlockSizeRes = t.BlockSizeRes;
    m_Pos = t.m_Pos;
    m_NewLevels.SetFixedLevels();
    SetPrices(m_NewLevels);
    TryBlock();
    return FinalBlockFieldSize + BlockTypeFieldSize + GetLzBlockPrice();
}

quint32 BaseCoder::GetBlockPrice(int tableIndex, int numDivPasses)
{
    Tables& t = m_Tables[tableIndex];
    t.StaticMode = false;
    quint32 price = TryDynBlock(tableIndex, m_NumPasses);
    t.BlockSizeRes = BlockSizeRes;
    quint32 numValues = m_ValueIndex;
    quint32 posTemp = m_Pos;
    quint32 additionalOffsetEnd = m_AdditionalOffset;

    if (m_CheckStatic && m_ValueIndex <= FixedHuffmanCodeBlockSizeMax) {
        const quint32 fixedPrice = TryFixedBlock(tableIndex);
        t.StaticMode = (fixedPrice < price);
        if (t.StaticMode)
            price = fixedPrice;
    }

    const quint32 storePrice = GetStorePrice(BlockSizeRes, 0); // bitPosition
    t.StoreMode = (storePrice <= price);
    if (t.StoreMode)
        price = storePrice;

    t.UseSubBlocks = false;

    if (numDivPasses > 1 && numValues >= DivideCodeBlockSizeMin) {
        Tables &t0 = m_Tables[(tableIndex << 1)];
//        (Levels&)t0 = t;
        ::memcpy(t0.litLenLevels, t.litLenLevels, sizeof(t0.litLenLevels));
        ::memcpy(t0.distLevels, t.distLevels, sizeof(t0.distLevels));
        t0.BlockSizeRes = t.BlockSizeRes >> 1;
        t0.m_Pos = t.m_Pos;
        quint32 subPrice = GetBlockPrice((tableIndex << 1), numDivPasses - 1);

        quint32 blockSize2 = t.BlockSizeRes - t0.BlockSizeRes;
        if (t0.BlockSizeRes >= DivideBlockSizeMin && blockSize2 >= DivideBlockSizeMin) {
            Tables& t1 = m_Tables[(tableIndex << 1) + 1];
//            (Levels&)t1 = t;
            ::memcpy(t1.litLenLevels, t.litLenLevels, sizeof(t1.litLenLevels));
            ::memcpy(t1.distLevels, t.distLevels, sizeof(t1.distLevels));
            t1.BlockSizeRes = blockSize2;
            t1.m_Pos = m_Pos;
            m_AdditionalOffset -= t0.BlockSizeRes;
            subPrice += GetBlockPrice((tableIndex << 1) + 1, numDivPasses - 1);
            t.UseSubBlocks = (subPrice < price);
            if (t.UseSubBlocks)
                price = subPrice;
        }
    }
    m_AdditionalOffset = additionalOffsetEnd;
    m_Pos = posTemp;
    return price;
}

bool BaseCoder::CodeBlock(int tableIndex, bool finalBlock)
{
    Tables& t = m_Tables[tableIndex];

    if (t.UseSubBlocks) {
        if (!CodeBlock((tableIndex << 1), false))
            return false;
        if (!CodeBlock((tableIndex << 1) + 1, finalBlock))
            return false;
        return true;
    }

    if (t.StoreMode) {
        if (!WriteStoreBlock(t.BlockSizeRes, m_AdditionalOffset, finalBlock))
            return false;
    } else {
        CHECKWRITE(FinalBlockFieldSize, finalBlock ? FinalBlock : NotFinalBlock);
        
        if (t.StaticMode) {
            CHECKWRITE(BlockTypeFieldSize, BlockTypeFixedHuffman);
            TryFixedBlock(tableIndex);
            uint i;
            for (i = 0; i < FixedMainTableSize; i++)
                mainFreqs[i] = (quint32)1 << (NumHuffmanBits - m_NewLevels.litLenLevels[i]);
            for (i = 0; i < FixedDistTableSize; i++)
                distFreqs[i] = (quint32)1 << (NumHuffmanBits - m_NewLevels.distLevels[i]);
            MakeTables();
        } else {
            if (m_NumDivPasses > 1 || m_CheckStatic)
                TryDynBlock(tableIndex, 1);
            CHECKWRITE(BlockTypeFieldSize, BlockTypeDynamicHuffman);
            CHECKWRITE(NumLenCodesFieldSize, m_NumLitLenLevels - NumLitLenCodesMin);
            CHECKWRITE(NumDistCodesFieldSize, m_NumDistLevels - NumDistCodesMin);
            CHECKWRITE(NumLevelCodesFieldSize, m_NumLevelCodes - NumLevelCodesMin);

            for (quint32 i = 0; i < m_NumLevelCodes; i++)
                CHECKWRITE(LevelFieldSize, m_LevelLevels[i]);

            Huffman_ReverseBits(levelCodes, levelLens, LevelTableSize);

            if (!LevelTableCode(m_NewLevels.litLenLevels, m_NumLitLenLevels, levelLens, levelCodes))
                return false;
            if (!LevelTableCode(m_NewLevels.distLevels, m_NumDistLevels, levelLens, levelCodes))
                return false;
        }

        if (!WriteBlock())
            return false;
    }
    m_AdditionalOffset -= t.BlockSizeRes;

    return true;
}

bool BaseCoder::code()
{
    m_CheckStatic = (m_NumPasses != 1 || m_NumDivPasses != 1);
    m_IsMultiPass = (m_CheckStatic || (m_NumPasses != 1 || m_NumDivPasses != 1));

    if (!Create())
        return false;

    m_ValueBlockSize = (1 << 13) + (1 << 12) * m_NumDivPasses;

    quint64 nowPos = 0;

    _lzInWindow.backingDevice = inputDevice();
    MatchFinder_Init(&_lzInWindow);

    mWriteBuffer.setBackingDevice(outputDevice());
    mOutStream.setBackingDevice(&mWriteBuffer);

    m_OptimumEndIndex = m_OptimumCurrentIndex = 0;

    Tables &t = m_Tables[1];
    t.m_Pos = 0;
    t.InitStructures();

    m_AdditionalOffset = 0;
    do {
        t.BlockSizeRes = BlockUncompressedSizeThreshold;
        m_SecondPass = false;
        GetBlockPrice(1, m_NumDivPasses);

        if (!CodeBlock(1, Inline_MatchFinder_GetNumAvailableBytes(&_lzInWindow) == 0))
            return false;

        nowPos += m_Tables[1].BlockSizeRes;

        quint64 packSize = mOutStream.pos();
        emit compressionRatio(nowPos, packSize);
    } while (Inline_MatchFinder_GetNumAvailableBytes(&_lzInWindow) != 0);

    if (!_lzInWindow.result)
        return errorIoDevice(inputDevice());

    if (!mOutStream.flushBits() || !mWriteBuffer.flush())
        return errorIoDevice(outputDevice());

    return true;
}

}
}
