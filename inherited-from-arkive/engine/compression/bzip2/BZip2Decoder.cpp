#include "BZip2Decoder.h"
#include "BZip2CRC.h"
#include "Mtf8.h"

#include <QtCore/QMap>
#include <QtCore/QByteArray>
#include <QtCore/QVariant>

namespace Compress {
namespace BZip2 {

static const quint32 BufferSize = (1 << 17);

static const qint16 RandNums[512] = {
    619, 720, 127, 481, 931, 816, 813, 233, 566, 247,
    985, 724, 205, 454, 863, 491, 741, 242, 949, 214,
    733, 859, 335, 708, 621, 574, 73, 654, 730, 472,
    419, 436, 278, 496, 867, 210, 399, 680, 480, 51,
    878, 465, 811, 169, 869, 675, 611, 697, 867, 561,
    862, 687, 507, 283, 482, 129, 807, 591, 733, 623,
    150, 238, 59, 379, 684, 877, 625, 169, 643, 105,
    170, 607, 520, 932, 727, 476, 693, 425, 174, 647,
    73, 122, 335, 530, 442, 853, 695, 249, 445, 515,
    909, 545, 703, 919, 874, 474, 882, 500, 594, 612,
    641, 801, 220, 162, 819, 984, 589, 513, 495, 799,
    161, 604, 958, 533, 221, 400, 386, 867, 600, 782,
    382, 596, 414, 171, 516, 375, 682, 485, 911, 276,
    98, 553, 163, 354, 666, 933, 424, 341, 533, 870,
    227, 730, 475, 186, 263, 647, 537, 686, 600, 224,
    469, 68, 770, 919, 190, 373, 294, 822, 808, 206,
    184, 943, 795, 384, 383, 461, 404, 758, 839, 887,
    715, 67, 618, 276, 204, 918, 873, 777, 604, 560,
    951, 160, 578, 722, 79, 804, 96, 409, 713, 940,
    652, 934, 970, 447, 318, 353, 859, 672, 112, 785,
    645, 863, 803, 350, 139, 93, 354, 99, 820, 908,
    609, 772, 154, 274, 580, 184, 79, 626, 630, 742,
    653, 282, 762, 623, 680, 81, 927, 626, 789, 125,
    411, 521, 938, 300, 821, 78, 343, 175, 128, 250,
    170, 774, 972, 275, 999, 639, 495, 78, 352, 126,
    857, 956, 358, 619, 580, 124, 737, 594, 701, 612,
    669, 112, 134, 694, 363, 992, 809, 743, 168, 974,
    944, 375, 748, 52, 600, 747, 642, 182, 862, 81,
    344, 805, 988, 739, 511, 655, 814, 334, 249, 515,
    897, 955, 664, 981, 649, 113, 974, 459, 893, 228,
    433, 837, 553, 268, 926, 240, 102, 654, 459, 51,
    686, 754, 806, 760, 493, 403, 415, 394, 687, 700,
    946, 670, 656, 610, 738, 392, 760, 799, 887, 653,
    978, 321, 576, 617, 626, 502, 894, 679, 243, 440,
    680, 879, 194, 572, 640, 724, 926, 56, 204, 700,
    707, 151, 457, 449, 797, 195, 791, 558, 945, 679,
    297, 59, 87, 824, 713, 663, 412, 693, 342, 606,
    134, 108, 571, 364, 631, 212, 174, 643, 304, 329,
    343, 97, 430, 751, 497, 314, 983, 374, 822, 928,
    140, 206, 73, 263, 980, 736, 876, 478, 430, 305,
    170, 514, 364, 692, 829, 82, 855, 953, 676, 246,
    369, 970, 294, 750, 807, 827, 150, 790, 288, 923,
    804, 378, 215, 828, 592, 281, 565, 555, 710, 82,
    896, 831, 547, 261, 524, 462, 293, 465, 502, 56,
    661, 821, 976, 991, 658, 869, 905, 758, 745, 193,
    768, 550, 608, 933, 378, 286, 215, 979, 792, 961,
    61, 688, 793, 644, 986, 403, 106, 366, 905, 644,
    372, 567, 466, 434, 645, 210, 389, 550, 919, 135,
    780, 773, 635, 389, 707, 100, 626, 958, 165, 504,
    920, 176, 193, 713, 857, 265, 203, 50, 668, 108,
    645, 990, 626, 197, 510, 357, 358, 850, 858, 364,
    936, 638
};

BZip2Decoder::BZip2Decoder(QObject *parent)
    : CompressionCodecBase(parent)
{
}

bool BZip2Decoder::ReadBits(int numBits, quint32 *bitValue)
{
    bool ok = mInBitStream.readBits(numBits, bitValue);

    if (!ok)
        return errorIoDevice(inputDevice());

    return ok;
}

#define CHECKREAD(n, v)         \
    do { if (!ReadBits(n, v)) return false; } while (0)

#define HUFFMAN_ERROR(huffman)                              \
    do {                                                    \
        if ((huffman)->error() == Compress::Huffman::IOError)\
            return errorIoDevice(inputDevice());            \
        return errorCorrupted();                            \
    } while (0)

bool BZip2Decoder::ReadByte(quint8 *byteVal)
{
    quint32 bits = 0;

    if (!ReadBits(8, &bits))
        return false;

    *byteVal = (bits & 0xff);
    return true;
}

bool BZip2Decoder::ReadCRC(quint32 *crcVal)
{
    quint32 crc = 0;

    for (int i = 0; i < 4; i++) {
        quint8 byte;

        if (!ReadByte(&byte))
            return false;

        crc <<= 8;
        crc |= byte;
    }

    *crcVal = crc;
    return true;
}

bool BZip2Decoder::ReadBlock(quint32 *counters, quint32 *blockSizeRes, quint32 *origPtrRes, bool *randRes)
{
    quint32 bitVal;

    CHECKREAD(1, &bitVal);
    *randRes = bitVal ? true : false;
    CHECKREAD(NumOrigBits, origPtrRes);

    // in original code it compares OrigPtr to (quint32)(10 + blockSizeMax)) : why ?
    if (*origPtrRes >= mBlockSizeMax)
        return false;

    Mtf8Decoder mtf;
    mtf.StartInit();

    int numInUse = 0;
    {
        quint8 inUse16[16];
        int i;
        CHECKREAD(16, &bitVal);
        for (i = 0; i < 16; i++) {
            inUse16[i] = (bitVal >> (15 - i)) & 1;
        }
        for (i = 0; i < 256; i++)
            if (inUse16[i >> 4]) {
                CHECKREAD(1, &bitVal);
                if (bitVal)
                    mtf.Add(numInUse++, (quint8)i);
            }
        if (numInUse == 0)
            return false;
        // mtf.Init(numInUse);
    }
    int alphaSize = numInUse + 2;

    quint32 numTables;
    CHECKREAD(NumTablesBits, &numTables);
    if (numTables < NumTablesMin || numTables > NumTablesMax)
        return false;

    quint32 numSelectors;
    CHECKREAD(NumSelectorsBits, &numSelectors);
    if (numSelectors < 1 || numSelectors > NumSelectorsMax)
        return false;

    {
        quint8 mtfPos[NumTablesMax];
        int t = 0;
        do
            mtfPos[t] = (quint8)t;
        while (++t < numTables);
        quint32 i = 0;
        do {
            int j = 0;
            while (true) {
                CHECKREAD(1, &bitVal);
                if (!bitVal)
                    break;
                if (++j >= numTables)
                    return false;
            }
            quint8 tmp = mtfPos[j];
            for (;j > 0; j--)
                mtfPos[j] = mtfPos[j - 1];
            m_Selectors[i] = mtfPos[0] = tmp;
        } while (++i < numSelectors);
    }

    int t = 0;
    do {
        quint8 lens[MaxAlphaSize];
        quint32 len;
        CHECKREAD(NumLevelsBits, &len);
        int i;
        for (i = 0; i < alphaSize; i++) {
            for (;;) {
                if (len < 1 || len > MaxHuffmanLen)
                    return false;
                CHECKREAD(1, &bitVal);
                if (!bitVal)
                    break;
                CHECKREAD(1, &bitVal);
                len += 1 - (bitVal << 1);
            }
            lens[i] = (quint8)len;
        }
        for (; i < MaxAlphaSize; i++)
            lens[i] = 0;
        if (!m_HuffmanDecoders[t].SetCodeLengths(lens))
            return false;
    } while (++t < numTables);

    {
        for (int i = 0; i < 256; i++)
            counters[i] = 0;
    }

    quint32 blockSize = 0;
    {
        quint32 groupIndex = 0;
        quint32 groupSize = 0;
        HuffmanDecoder *huffmanDecoder = 0;
        int runPower = 0;
        quint32 runCounter = 0;

        for (;;) {
            if (groupSize == 0) {
                if (groupIndex >= numSelectors)
                    return false;
                groupSize = GroupSize;
                huffmanDecoder = &m_HuffmanDecoders[m_Selectors[groupIndex++]];
            }
            groupSize--;

            quint32 nextSym;
            if (!huffmanDecoder->DecodeSymbol(&mInBitStream, &nextSym))
                HUFFMAN_ERROR(huffmanDecoder);

            if (nextSym < 2) {
                runCounter += ((quint32)(nextSym + 1) << runPower++);
                if (mBlockSizeMax - blockSize < runCounter)
                    return false;
                continue;
            }
            if (runCounter != 0) {
                quint32 b = (quint32)mtf.GetHead();
                counters[b] += runCounter;
                do
                    counters[256 + blockSize++] = b;
                while (--runCounter != 0);
                runPower = 0;
            }
            if (nextSym <= (quint32)numInUse) {
                quint32 b = (quint32)mtf.GetAndMove((int)nextSym - 1);
                if (blockSize >= mBlockSizeMax)
                    return false;
                counters[b]++;
                counters[256 + blockSize++] = b;
            } else if (nextSym == (quint32)numInUse + 1)
                break;
            else
                return false;
        }
    }
    *blockSizeRes = blockSize;
    return (*origPtrRes < blockSize) ? true : false;
}

void BZip2Decoder::DecodeBlock1(quint32 *charCounters, quint32 blockSize)
{
    {
        quint32 sum = 0;
        for (quint32 i = 0; i < 256; i++) {
            sum += charCounters[i];
            charCounters[i] = sum - charCounters[i];
        }
    }

    quint32 *tt = charCounters + 256;
    // Compute the T^(-1) vector
    quint32 i = 0;
    do
        tt[charCounters[tt[i] & 0xFF]++] |= (i << 8);
    while (++i < blockSize);
}

#define CHECKWRITEBYTE(byte)                                                \
    do {                                                                    \
        if (!mOutputBuffer.writeByte(byte))                                 \
            return errorIoDevice(outputDevice());                           \
    } while (0)

bool BZip2Decoder::DecodeBlock2(const quint32 *tt, quint32 blockSize, quint32 OrigPtr, quint32 *crcVal)
{
    BZip2CRC crc;

    // it's for speed optimization: prefetch & prevByte_init;
    quint32 tPos = tt[tt[OrigPtr] >> 8];
    unsigned int prevByte = (unsigned int)(tPos & 0xFF);

    int numReps = 0;

    do {
        unsigned int b = (unsigned int)(tPos & 0xFF);
        tPos = tt[tPos >> 8];

        if (numReps == RleModeRepSize) {
            if (!mOutputBuffer.checkSpace(b))
                return errorIoDevice(outputDevice());

            for (; b > 0; b--) {
                crc.updateByte(prevByte);
                mOutputBuffer.putByteUnchecked(quint8(prevByte));
            }
            numReps = 0;
            continue;
        }
        if (b != prevByte)
            numReps = 0;
        numReps++;
        prevByte = b;
        crc.updateByte(b);
        CHECKWRITEBYTE(quint8(b));
    } while (--blockSize != 0);

    *crcVal = crc.getDigest();

    return true;
}

bool BZip2Decoder::DecodeBlock2Rand(const quint32 *tt, quint32 blockSize, quint32 OrigPtr, quint32 *crcVal)
{
    BZip2CRC crc;

    quint32 randIndex = 1;
    quint32 randToGo = RandNums[0] - 2;

    int numReps = 0;

    // it's for speed optimization: prefetch & prevByte_init;
    quint32 tPos = tt[tt[OrigPtr] >> 8];
    unsigned int prevByte = (unsigned int)(tPos & 0xFF);

    do {
        unsigned int b = (unsigned int)(tPos & 0xFF);
        tPos = tt[tPos >> 8];

        {
            if (randToGo == 0) {
                b ^= 1;
                randToGo = RandNums[randIndex++];
                randIndex &= 0x1FF;
            }
            randToGo--;
        }

        if (numReps == RleModeRepSize) {
            for (; b > 0; b--) {
                crc.updateByte(prevByte);
                CHECKWRITEBYTE(quint8(prevByte));
            }
            numReps = 0;
            continue;
        }
        if (b != prevByte)
            numReps = 0;
        numReps++;
        prevByte = b;
        crc.updateByte(b);
        CHECKWRITEBYTE(quint8(b));
    } while (--blockSize != 0);

    *crcVal = crc.getDigest();
    return true;
}

bool BZip2Decoder::ReadSignatures(bool *wasFinished, quint32 *crc)
{
    *wasFinished = false;
    quint8 s[6];
    for (int i = 0; i < 6; i++) {
        if (!ReadByte(&s[i]))
            return errorIoDevice(inputDevice());
    }

    if (!ReadCRC(crc))
        return errorIoDevice(inputDevice());

    if (s[0] == BZip2BlockEndSignature[0]) {
        if (memcmp(s, BZip2BlockEndSignature, BZip2BlockEndSignatureLength) != 0)
            return errorCorrupted();

        if (*crc != mCombinedCRC.getDigest())
            return errorCorrupted();

        *wasFinished = true;
        return true;
    }
    if (memcmp(s, BZip2BlockStartSignature, BZip2BlockStartSignatureLength) != 0)
        return errorCorrupted();

    mCombinedCRC.updateUInt32(*crc);
    return true;
}

bool BZip2Decoder::code()
{
    Q_ASSERT(inputDevice());
    Q_ASSERT(outputDevice());

    mInputBuffer.setBackingDevice(inputDevice());
    mOutputBuffer.setBackingDevice(outputDevice());

    mInBitStream.setBackingDevice(&mInputBuffer);

    // check the stream signature
    quint8 sig[BZip2StreamMagicLength];
    for (int i = 0; i < BZip2StreamMagicLength; i++) {
        if (!ReadByte(&sig[i]))
            return errorIoDevice(inputDevice());
    }

    if (memcmp(sig, BZip2StreamMagic, BZip2StreamMagicLength - 1) != 0)
        return errorCorrupted();

    qint32 rawDictSize = qint32(sig[BZip2StreamMagicLength - 1]) 
                            - BZip2StreamMagic[BZip2StreamMagicLength - 1];
    if (rawDictSize < BlockSizeMultMin || rawDictSize > BlockSizeMultMax)
        return errorCorrupted();

    // calculate the basic stream parameters
    mBlockSizeMax = quint32(rawDictSize) * BlockSizeStep;
    mCombinedCRC.reinit();

    // do the actual decompression using the multithread framework
    CompressionManager *manager = new CompressionManager(this);

    connect(manager, SIGNAL(progress(qint64, qint64)), this, SIGNAL(compressionRatio(qint64, qint64)));

    // only use two threads, since there's so little to do in BZip2DecoderTask::processBlock() that
    // the only overlap available is basically between reading and writing
    qint64 startIn = inputDevice()->pos(), startOut = outputDevice()->pos();
    bool ok = manager->code(QList<CompressionTask *>()
                << new BZip2DecoderTask(this) << new BZip2DecoderTask(this));

    if (ok) {
        emit compressionRatio(inputDevice()->pos() - startIn, outputDevice()->pos() - startOut);
    }

    delete manager;
    return ok;
}

bool BZip2Decoder::setProperties(const QMap<QString, QVariant>& properties)
{
    return false;
}

bool BZip2Decoder::setProperties(const QByteArray& serializedProperties)
{
    return false;
}

QByteArray BZip2Decoder::serializeProperties()
{
    return QByteArray();
}

BZip2DecoderTask::BZip2DecoderTask(BZip2Decoder *parent)
    : CompressionTask(), q(parent), mCounters(new quint32[256 + BlockSizeMax])
{
}

BZip2DecoderTask::~BZip2DecoderTask()
{
    delete[] mCounters;
}

qint64 BZip2DecoderTask::readBlock()
{
    bool finished;
    qint64 startPos = q->mInputBuffer.bytesRead();

    if (!q->ReadSignatures(&finished, &mExpectedCrc)) {
        blockFailed(ReadError);
        return 0;
    }

    if (finished) {
        if (!q->mOutputBuffer.flush()) {
            blockFailed(WriteError);
            return 0;
        }
        streamFinished();
        return 0;
    }

    if (!q->ReadBlock(mCounters, &mBlockSize, &mOrigPtr, &mRandMode)) {
        blockFailed(ReadError);
        return 0;
    }

    return (q->mInputBuffer.bytesRead() - startPos);
}

void BZip2DecoderTask::processBlock()
{
    q->DecodeBlock1(mCounters, mBlockSize);
}

qint64 BZip2DecoderTask::writeBlock()
{
    qint64 startPos = q->mOutputBuffer.bytesWritten();

    quint32 crc;
    bool ok;

    if (mRandMode)
        ok = q->DecodeBlock2Rand(mCounters + 256, mBlockSize, mOrigPtr, &crc);
    else
        ok = q->DecodeBlock2(mCounters + 256, mBlockSize, mOrigPtr, &crc);

    if (!ok) {
        blockFailed(WriteError);
        return 0;
    }

    if (crc != mExpectedCrc) {
        blockFailed(CrcMismatchError);
        return 0;
    }
    return (q->mOutputBuffer.bytesWritten() - startPos);
}


// close namespaces
}
}

#include "BZip2Decoder.moc"
