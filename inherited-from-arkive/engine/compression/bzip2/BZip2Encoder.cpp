#include "BZip2Encoder.h"
#include "BlockSort.h"
#include "Mtf8.h"
#include "BZip2CRC.h"
#include "../huffman/HuffmanEncode.h"

#include <QtCore/QByteArray>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QVariant>

namespace Compress {
namespace BZip2 {

const int MaxHuffmanLenForEncoding = 16; // it must be < MaxHuffmanLen = 20

static const quint32 BufferSize = (1 << 17);
static const int NumHuffmanPasses = 4;

BZip2Encoder::BZip2Encoder(QObject *parent)
    : CompressionCodecBase(parent)
{
}

bool BZip2Encoder::code()
{
    return false;
}

bool BZip2Encoder::setProperties(const QMap<QString, QVariant>& properties)
{
    return false;
}

bool BZip2Encoder::setProperties(const QByteArray& serializedProperties)
{
    return false;
}

QByteArray BZip2Encoder::serializeProperties()
{
    return QByteArray();
}

bool BZip2Encoder::readRleBlock(quint8 *buffer, quint32 *blockSizeRet)
{
    quint32 i = 0;
    quint8 prevByte;

    if (mInStream.readByte(&prevByte)) {
        quint32 blockSize = m_BlockSizeMult * BlockSizeStep - 1;
        int numReps = 1;
        buffer[i++] = prevByte;
        while (i < blockSize) { // "- 1" to support RLE
            quint8 b;
            if (!mInStream.readByte(b)) {
                if (mInStream.atEnd())
                    break;
                else
                    return false;
            }
            if (b != prevByte) {
                if (numReps >= RleModeRepSize)
                    buffer[i++] = (quint8)(numReps - RleModeRepSize);
                buffer[i++] = b;
                numReps = 1;
                prevByte = b;
                continue;
            }
            numReps++;
            if (numReps <= RleModeRepSize)
                buffer[i++] = b;
            else if (numReps == RleModeRepSize + 255) {
                buffer[i++] = (quint8)(numReps - RleModeRepSize);
                numReps = 0;
            }
        }
        // it's to support original BZip2 decoder
        if (numReps >= RleModeRepSize)
            buffer[i++] = (quint8)(numReps - RleModeRepSize);
    } else if (!mInStream.atEnd()) {
        return false;
    }

    *blockSizeRet = i;
    return true;
}

// XXX: allocation sizes are horrible magic numbers; imported from the 7zip original code
BZip2EncoderTask::BZip2EncoderTask(BZip2Encoder *parent)
    : CompressionTask(), q(parent),
    mBlock(new quint8[BlockSizeMax]),
    mMtfArray(new quint8[BlockSizeMax + 2]),
    mBlockSorterIndicies(new quint32[BLOCK_SORT_BUF_SIZE(BlockSizeMax)]),
    mTrialBuffer(BlockSizeMax * 2 + BlockSizeMax / 10 + (20 << 10))
{
}

BZip2EncoderTask::~BZip2EncoderTask()
{
    delete[] mBlock;
    delete[] mMtfArray;
    delete[] mBlockSorterIndicies;
}

qint64 BZip2EncoderTask::readBlock()
{
    qint64 startPos = q->mInStream.bytesRead();

    if (!q->readRleBlock(mBlock, &mBlockSize)) {
        blockFailed(ReadError);
        return 0;
    }

    if (!mBlockSize)
        streamFinished();

    return (q->mInStream.bytesRead() - startPos);
}

void BZip2EncoderTask::processBlock()
{
    // reset our stream
    mTrialBuffer.clear();
    mTrialStream.setBackingDevice(&mTrialBuffer);

    // we don't have any CRCs yet
    mNrCrcs = 0;

    encodeBlock(mBlock, mBlockSize, q->mNrPasses);
}

qint64 BZip2EncoderTask::writeBlock()
{
    // update the CRCs
    for (int i = 0; i < mNrCrcs; i++)
        q->mCombinedCrc.update(mCrcs[i]);

    // and write out our compressed block(s)
    if (!q->writeBits(mTrialBuffer, mTrialStream.bitPos())) {
        blockFailed(WriteError);
        return 0;
    }

    return mTrialBuffer.size();
}

inline void BZip2EncoderTask::writeBits(int bits, quint32 bitValue)
{
    mTrialStream.writeBits(bits, bitValue);
}

// blockSize > 0
void BZip2EncoderTask::compressBlock(const quint8 *block, quint32 blockSize)
{
    // the "randomised?" header: we don't do that, so write zero
    writeBits(1, 0);

    {
        quint32 origPtr = BlockSort(mBlockSorterIndicies, block, blockSize);
        // if (m_BlockSorterIndex[origPtr] != 0) throw 1;
        mBlockSorterIndices[origPtr] = blockSize;
        writeBits(NumOrigBits, origPtr);
    }

    Mtf8Encoder mtf;
    int numInUse = 0;
    {
        bool inUse[256];
        bool inUse16[16];
        quint32 i;
        for (i = 0; i < 256; i++)
            inUse[i] = false;
        for (i = 0; i < 16; i++)
            inUse16[i] = false;
        for (i = 0; i < blockSize; i++)
            inUse[block[i]] = true;
        for (i = 0; i < 256; i++)
            if (inUse[i]) {
                inUse16[i >> 4] = true;
                mtf.Buffer[numInUse++] = (quint8)i;
            }
        for (i = 0; i < 16; i++)
            writeBits(1, inUse16[i]);
        for (i = 0; i < 256; i++)
            if (inUse16[i >> 4])
                writeBits(1, inUse[i]);
    }
    int alphaSize = numInUse + 2;

    quint8 *mtfs = mMtfArray;
    quint32 mtfArraySize = 0;
    quint32 symbolCounts[MaxAlphaSize];
    {
        for (int i = 0; i < MaxAlphaSize; i++)
            symbolCounts[i] = 0;
    }

    {
        quint32 rleSize = 0;
        quint32 i = 0;
        const quint32 *bsIndex = m_BlockSorterIndicies;
        block--;
        do {
            int pos = mtf.FindAndMove(block[bsIndex[i]]);
            if (pos == 0)
                rleSize++;
            else {
                while (rleSize != 0) {
                    rleSize--;
                    mtfs[mtfArraySize++] = (quint8)(rleSize & 1);
                    symbolCounts[rleSize & 1]++;
                    rleSize >>= 1;
                }
                if (pos >= 0xFE) {
                    mtfs[mtfArraySize++] = 0xFF;
                    mtfs[mtfArraySize++] = (quint8)(pos - 0xFE);
                } else
                    mtfs[mtfArraySize++] = (quint8)(pos + 1);
                symbolCounts[pos + 1]++;
            }
        } while (++i < blockSize);

        while (rleSize != 0) {
            rleSize--;
            mtfs[mtfArraySize++] = (quint8)(rleSize & 1);
            symbolCounts[rleSize & 1]++;
            rleSize >>= 1;
        }

        if (alphaSize < 256)
            mtfs[mtfArraySize++] = (quint8)(alphaSize - 1);
        else {
            mtfs[mtfArraySize++] = 0xFF;
            mtfs[mtfArraySize++] = (quint8)(alphaSize - 256);
        }
        symbolCounts[alphaSize - 1]++;
    }

    quint32 numSymbols = 0;
    {
        for (int i = 0; i < MaxAlphaSize; i++)
            numSymbols += symbolCounts[i];
    }

    int bestNumTables = NumTablesMin;
    quint32 bestPrice = 0xFFFFFFFF;
    BitStreamBE<MemoryWriteBuffer> streamSaver = mTempStream;
    uint startBufferSize = mTempBuffer.size();

    for (int nt = NumTablesMin; nt <= NumTablesMax + 1; nt++) {
        int numTables;

        if (q->mOptimizeNrOfTables) {
            // reset the buffers back to where we started
            mTempStream = streamSaver;
            mTempBuffer.truncate(startBufferSize);

            // we'll successively try each table size, and then we'll do one last iteration
            // to reencode using that table size
            if (nt <= NumTablesMax)
                numTables = nt;
            else
                numTables = bestNumTables;
        } else {
            // guesstimate a good table size
            if (numSymbols < 200)  numTables = 2;
            else if (numSymbols < 600) numTables = 3;
            else if (numSymbols < 1200) numTables = 4;
            else if (numSymbols < 2400) numTables = 5;
            else numTables = 6;
        }

        writeBits(NumTablesBits, numTables);

        quint32 numSelectors = (numSymbols + kGroupSize - 1) / GroupSize;
        writeBits(NumSelectorsBits, numSelectors);

        {
            quint32 remFreq = numSymbols;
            int gs = 0;
            int t = numTables;
            do {
                quint32 tFreq = remFreq / t;
                int ge = gs;
                quint32 aFreq = 0;
                while (aFreq < tFreq) //  && ge < alphaSize)
                    aFreq += symbolCounts[ge++];

                if (ge - 1 > gs && t != numTables && t != 1 && (((numTables - t) & 1) == 1))
                    aFreq -= symbolCounts[--ge];

                quint8 *lens = Lens[t - 1];
                int i = 0;
                do
                    lens[i] = (i >= gs && i < ge) ? 0 : 1;
                while (++i < alphaSize);
                gs = ge;
                remFreq -= aFreq;
            } while (--t != 0);
        }


        for (int pass = 0; pass < NumHuffPasses; pass++) {
            {
                int t = 0;
                do
                    memset(mFreqs[t], 0, sizeof(mFreqs[t]));
                while (++t < numTables);
            }

            {
                quint32 mtfPos = 0;
                quint32 g = 0;
                do {
                    quint32 symbols[GroupSize];
                    int i = 0;
                    do {
                        quint32 symbol = mtfs[mtfPos++];
                        if (symbol >= 0xFF)
                            symbol += mtfs[mtfPos++];
                        symbols[i] = symbol;
                    } while (++i < GroupSize && mtfPos < mtfArraySize);

                    quint32 bestPrice = 0xFFFFFFFF;
                    int t = 0;
                    do {
                        const quint8 *lens = mLens[t];
                        quint32 price = 0;
                        int j = 0;
                        do
                            price += lens[symbols[j]];
                        while (++j < i);
                        if (price < bestPrice) {
                            mSelectors[g] = (quint8)t;
                            bestPrice = price;
                        }
                    } while (++t < numTables);
                    quint32 *freqs = mFreqs[mSelectors[g++]];
                    int j = 0;
                    do {
                        freqs[symbols[j]]++;
                    } while (++j < i);
                } while (mtfPos < mtfArraySize);
            }

            int t = 0;
            do {
                quint32 *freqs = mFreqs[t];
                int i = 0;
                do
                    if (freqs[i] == 0)
                        freqs[i] = 1;
                while (++i < alphaSize);
                Huffman_Generate(freqs, mCodes[t], mLens[t], MaxAlphaSize, MaxHuffmanLenForEncoding);
            } while (++t < numTables);
        }

        {
            quint8 mtfSel[NumTablesMax];
            {
                int t = 0;
                do
                    mtfSel[t] = (quint8)t;
                while (++t < numTables);
            }

            quint32 i = 0;
            do {
                quint8 sel = mSelectors[i];
                int pos;
                for (pos = 0; mtfSel[pos] != sel; pos++)
                    writeBits(1, 1);
                writeBit(1, 0);
                for (; pos > 0; pos--)
                    mtfSel[pos] = mtfSel[pos - 1];
                mtfSel[0] = sel;
            } while (++i < numSelectors);
        }

        {
            int t = 0;
            do {
                const quint8 *lens = Lens[t];
                quint32 len = lens[0];
                writeBits(NumLevelsBits, len);
                int i = 0;
                do {
                    quint32 level = lens[i];
                    while (len != level) {
                        writeBits(1, 1);
                        if (len < level) {
                            writeBits(1, 0);
                            len++;
                        } else {
                            writeBits(1, 1);
                            len--;
                        }
                    }
                    writeBits(1, 0);
                } while (++i < alphaSize);
            } while (++t < numTables);
        }

        {
            quint32 groupSize = 0;
            quint32 groupIndex = 0;
            const quint8 *lens = 0;
            const quint32 *codes = 0;
            quint32 mtfPos = 0;
            do {
                quint32 symbol = mtfs[mtfPos++];
                if (symbol >= 0xFF)
                    symbol += mtfs[mtfPos++];
                if (groupSize == 0) {
                    groupSize = GroupSize;
                    int t = mSelectors[groupIndex++];
                    lens = mLens[t];
                    codes = mCodes[t];
                }
                groupSize--;
                writeBits(lens[symbol], codes[symbol]);
            } while (mtfPos < mtfArraySize);
        }

        if (!q->mOptimizeNrOfTables)
            break;
        quint32 price = mTempBuffer.size() - startBufferSize;
        if (price <= bestPrice) {
            if (nt == NumTablesMax)
                break;
            bestPrice = price;
            bestNumTables = nt;
        }
    }
}

// blockSize > 0
quint32 BZip2EncoderTask::compressBlockWithHeaders(const quint8 *block, quint32 blockSize)
{
    for (int i = 0; i < BZip2BlockStartSignatureLength; i++)
        writeBits(8, BZip2BlockStartSignature[i]);

    BZip2CRC crc;
    int numReps = 0;
    quint8 prevByte = block[0];
    quint32 i = 0;
    do {
        quint8 b = block[i];
        if (numReps == RleModeRepSize) {
            for (; b > 0; b--)
                crc.updateByte(prevByte);
            numReps = 0;
            continue;
        }
        if (prevByte == b)
            numReps++;
        else {
            numReps = 1;
            prevByte = b;
        }
        crc.updateByte(b);
    } while (++i < blockSize);
    quint32 crcRes = crc.getDigest();

    for (int i = 0; i < 4; i++)
        writeBits(8, (crcRes >> (24 - 8 * i)) & 0xff);

    compressBlock(block, blockSize);
    return crcRes;
}

void BZip2EncoderTask::encodeBlock(const quint8 *block, quint32 blockSize, quint32 numPasses)
{
    quint32 numCrcs = mNrCrcs;
    bool needCompare = false;

    quint32 startBytePos = m_OutStreamCurrent->GetBytePos();
    quint32 startPos = m_OutStreamCurrent->GetPos();
    quint8 startCurByte = m_OutStreamCurrent->GetCurByte();
    quint8 endCurByte = 0;
    quint32 endPos = 0;
    if (numPasses > 1 && blockSize >= (1 << 10)) {
        quint32 blockSize0 = blockSize / 2;
        for (;(block[blockSize0] == block[blockSize0 - 1] ||
                block[blockSize0 - 1] == block[blockSize0 - 2]) &&
                blockSize0 < blockSize; blockSize0++);
        if (blockSize0 < blockSize) {
            EncodeBlock2(block, blockSize0, numPasses - 1);
            EncodeBlock2(block + blockSize0, blockSize - blockSize0, numPasses - 1);
            endPos = m_OutStreamCurrent->GetPos();
            endCurByte = m_OutStreamCurrent->GetCurByte();
            if ((endPos & 7) > 0)
                WriteBits2(0, 8 - (endPos & 7));
            m_OutStreamCurrent->SetCurState((startPos & 7), startCurByte);
            needCompare = true;
        }
    }

    quint32 startBytePos2 = m_OutStreamCurrent->GetBytePos();
    quint32 startPos2 = m_OutStreamCurrent->GetPos();
    quint32 crcVal = EncodeBlockWithHeaders(block, blockSize);
    quint32 endPos2 = m_OutStreamCurrent->GetPos();

    if (needCompare) {
        quint32 size2 = endPos2 - startPos2;
        if (size2 < endPos - startPos) {
            quint32 numBytes = m_OutStreamCurrent->GetBytePos() - startBytePos2;
            quint8 *buffer = m_OutStreamCurrent->GetStream();
            for (quint32 i = 0; i < numBytes; i++)
                buffer[startBytePos + i] = buffer[startBytePos2 + i];
            m_OutStreamCurrent->SetPos(startPos + endPos2 - startPos2);
            m_NumCrcs = numCrcs;
            m_CRCs[m_NumCrcs++] = crcVal;
        } else {
            m_OutStreamCurrent->SetPos(endPos);
            m_OutStreamCurrent->SetCurState((endPos & 7), endCurByte);
        }
    } else {
        m_NumCrcs = numCrcs;
        m_CRCs[m_NumCrcs++] = crcVal;
    }
}

#if 0

CEncoder::CEncoder():
        NumPasses(1),
        m_OptimizeNumTables(false),
        m_BlockSizeMult(kBlockSizeMultMax)
{
#ifdef COMPRESS_BZIP2_MT
    ThreadsInfo = 0;
    m_NumThreadsPrev = 0;
    NumThreads = 1;
#endif
}

#ifdef COMPRESS_BZIP2_MT
CEncoder::~CEncoder()
{
    Free();
}

void CThreadInfo::WriteBits2(quint32 value, quint32 numBits)
{
    m_OutStreamCurrent->WriteBits(value, numBits);
}
void CThreadInfo::WriteByte2(quint8 b)
{
    WriteBits2(b , 8);
}
void CThreadInfo::WriteBit2(bool v)
{
    WriteBits2((v ? 1 : 0), 1);
}
void CThreadInfo::WriteCRC2(quint32 v)
{
    for (int i = 0; i < 4; i++)
        WriteByte2(((quint8)(v >> (24 - i * 8))));
}

void CEncoder::WriteBits(quint32 value, quint32 numBits)
{
    m_OutStream.WriteBits(value, numBits);
}
void CEncoder::WriteByte(quint8 b)
{
    WriteBits(b , 8);
}
void CEncoder::WriteBit(bool v)
{
    WriteBits((v ? 1 : 0), 1);
}
void CEncoder::WriteCRC(quint32 v)
{
    for (int i = 0; i < 4; i++)
        WriteByte(((quint8)(v >> (24 - i * 8))));
}

void CEncoder::WriteBytes(const quint8 *data, quint32 sizeInBits, quint8 lastByte)
{
    quint32 bytesSize = (sizeInBits / 8);
    for (quint32 i = 0; i < bytesSize; i++)
        m_OutStream.WriteBits(data[i], 8);
    WriteBits(lastByte, (sizeInBits & 7));
}


HRESULT CEncoder::CodeReal(ISequentialInStream *inStream,
                           ISequentialOutStream *outStream, const quint64 * /* inSize */, const quint64 * /* outSize */,
                           ICompressProgressInfo *progress)
{
#ifdef COMPRESS_BZIP2_MT
    Progress = progress;
    RINOK(Create());
    for (quint32 t = 0; t < NumThreads; t++)
#endif
    {
#ifdef COMPRESS_BZIP2_MT
        CThreadInfo &ti = ThreadsInfo[t];
        ti.StreamWasFinishedEvent.Reset();
        ti.WaitingWasStartedEvent.Reset();
        ti.CanWriteEvent.Reset();
#else
        CThreadInfo &ti = ThreadsInfo;
        ti.Encoder = this;
#endif

        ti.m_OptimizeNumTables = m_OptimizeNumTables;

        if (!ti.Alloc())
            return E_OUTOFMEMORY;
    }


    if (!m_InStream.Create(kBufferSize))
        return E_OUTOFMEMORY;
    if (!m_OutStream.Create(kBufferSize))
        return E_OUTOFMEMORY;


    m_InStream.SetStream(inStream);
    m_InStream.Init();

    m_OutStream.SetStream(outStream);
    m_OutStream.Init();

    CFlusher flusher(this);

    CombinedCRC.Init();
#ifdef COMPRESS_BZIP2_MT
    NextBlockIndex = 0;
    StreamWasFinished = false;
    CloseThreads = false;
    CanStartWaitingEvent.Reset();
#endif

    WriteByte(kArSig0);
    WriteByte(kArSig1);
    WriteByte(kArSig2);
    WriteByte((quint8)(kArSig3 + m_BlockSizeMult));

#ifdef COMPRESS_BZIP2_MT

    if (MtMode) {
        ThreadsInfo[0].CanWriteEvent.Set();
        Result = S_OK;
        CanProcessEvent.Set();
        quint32 t;
        for (t = 0; t < NumThreads; t++)
            ThreadsInfo[t].StreamWasFinishedEvent.Lock();
        CanProcessEvent.Reset();
        CanStartWaitingEvent.Set();
        for (t = 0; t < NumThreads; t++)
            ThreadsInfo[t].WaitingWasStartedEvent.Lock();
        CanStartWaitingEvent.Reset();
        RINOK(Result);
    } else
#endif {
        for (;;) {
            CThreadInfo &ti =
#ifdef COMPRESS_BZIP2_MT
                ThreadsInfo[0];
#else
                ThreadsInfo;
#endif
            quint32 blockSize = ReadRleBlock(ti.m_Block);
            if (blockSize == 0)
                break;
            RINOK(ti.EncodeBlock3(blockSize));
            if (progress) {
                quint64 packSize = m_InStream.GetProcessedSize();
                quint64 unpackSize = m_OutStream.GetProcessedSize();
                RINOK(progress->SetRatioInfo(&packSize, &unpackSize));
            }
        }
}
WriteByte(kFinSig0);
WriteByte(kFinSig1);
WriteByte(kFinSig2);
WriteByte(kFinSig3);
WriteByte(kFinSig4);
WriteByte(kFinSig5);

WriteCRC(CombinedCRC.GetDigest());
return S_OK;
}

STDMETHODIMP CEncoder::Code(ISequentialInStream *inStream,
                            ISequentialOutStream *outStream, const quint64 *inSize, const quint64 *outSize,
                            ICompressProgressInfo *progress)
{
    try {
        return CodeReal(inStream, outStream, inSize, outSize, progress);
    } catch (const CInBufferException &e) {
        return e.ErrorCode;
    } catch (const COutBufferException &e) {
        return e.ErrorCode;
    } catch (...) {
        return S_FALSE;
    }
}

HRESULT CEncoder::SetCoderProperties(const PROPID *propIDs,
                                     const QVariant *properties, quint32 numProperties)
{
    for (quint32 i = 0; i < numProperties; i++) {
        const QVariant &property = properties[i];
        switch (propIDs[i]) {
        case NCoderPropID::kNumPasses: {
            if (property.vt != VT_UI4)
                return E_INVALIDARG;
            quint32 numPasses = property.ulVal;
            if (numPasses == 0)
                numPasses = 1;
            if (numPasses > kNumPassesMax)
                numPasses = kNumPassesMax;
            NumPasses = numPasses;
            m_OptimizeNumTables = (NumPasses > 1);
            break;
        }
        case NCoderPropID::kDictionarySize: {
            if (property.vt != VT_UI4)
                return E_INVALIDARG;
            quint32 dictionary = property.ulVal / kBlockSizeStep;
            if (dictionary < kBlockSizeMultMin)
                dictionary = kBlockSizeMultMin;
            else if (dictionary > kBlockSizeMultMax)
                dictionary = kBlockSizeMultMax;
            m_BlockSizeMult = dictionary;
            break;
        }
        case NCoderPropID::kNumThreads: {
#ifdef COMPRESS_BZIP2_MT
            if (property.vt != VT_UI4)
                return E_INVALIDARG;
            NumThreads = property.ulVal;
            if (NumThreads < 1)
                NumThreads = 1;
#endif
            break;
        }
        default:
            return E_INVALIDARG;
        }
    }
    return S_OK;
}

#ifdef COMPRESS_BZIP2_MT
STDMETHODIMP CEncoder::SetNumberOfThreads(quint32 numThreads)
{
    NumThreads = numThreads;
    if (NumThreads < 1)
        NumThreads = 1;
    return S_OK;
}
#endif
#endif

}
}
