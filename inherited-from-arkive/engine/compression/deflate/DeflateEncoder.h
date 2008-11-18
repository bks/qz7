// DeflateEncoder.h

#ifndef DEFLATEENCODER_H
#define DEFLATEENCODER_H

#include "kernel/CompressionCodec.h"
#include "kernel/QioBuffer.h"
#include "kernel/BitStreamLE.h"

#include "DeflateConst.h"
#include "../lz/MatchFinder.h"

namespace Compress {
namespace Deflate {

struct CodeValue {
    quint16 Len;
    quint16 Pos;

    void SetAsLiteral() {
        Len = (1 << 15);
    }

    bool IsLiteral() const {
        return (Len >= (1 << 15));
    }
};

struct Optimal {
    quint32 Price;
    quint16 PosPrev;
    quint16 BackPrev;
};

const quint32 NumOptsBase = 1 << 12;
const quint32 NumOpts = NumOptsBase + MatchMaxLen;


struct Tables : public Levels {
    bool UseSubBlocks;
    bool StoreMode;
    bool StaticMode;
    quint32 BlockSizeRes;
    quint32 m_Pos;

    void InitStructures();
};

class BaseCoder : public CompressionCodecBase {
    Q_OBJECT

public:
    enum Type { Deflate32, Deflate64 };
    BaseCoder(Type type, QObject *parent);
    ~BaseCoder();

    virtual bool code();
    virtual bool setProperties(const QMap<QString, QVariant>& properties);
    virtual bool setProperties(const QByteArray& serializedProperties);
    virtual QByteArray serializeProperties();

private:
    MatchFinder _lzInWindow;
    QioWriteBuffer mWriteBuffer;
    BitStreamLE<QioWriteBuffer> mOutStream;
    CodeValue *m_Values;

    quint16 *m_MatchDistances;
    quint32 m_NumFastBytes;
    bool _fastMode;
    bool _btMode;

    quint16 *m_OnePosMatchesMemory;
    quint16 *m_DistanceMemory;

    quint32 m_Pos;

    int m_NumPasses;
    int m_NumDivPasses;
    bool m_CheckStatic;
    bool m_IsMultiPass;
    quint32 m_ValueBlockSize;

    quint32 m_NumLenCombinations;
    quint32 m_MatchMaxLen;
    const quint8 *m_LenStart;
    const quint8 *m_LenDirectBits;

    bool m_Created;
    bool m_Deflate64Mode;

    quint8 m_LevelLevels[LevelTableSize];
    int m_NumLitLenLevels;
    int m_NumDistLevels;
    quint32 m_NumLevelCodes;
    quint32 m_ValueIndex;

    bool m_SecondPass;
    quint32 m_AdditionalOffset;

    quint32 m_OptimumEndIndex;
    quint32 m_OptimumCurrentIndex;

    quint8  m_LiteralPrices[256];
    quint8  m_LenPrices[NumLenSymbolsMax];
    quint8  m_PosPrices[DistTableSize64];

    Levels m_NewLevels;
    quint32 mainFreqs[FixedMainTableSize];
    quint32 distFreqs[DistTableSize64];
    quint32 mainCodes[FixedMainTableSize];
    quint32 distCodes[DistTableSize64];
    quint32 levelCodes[LevelTableSize];
    quint8 levelLens[LevelTableSize];

    quint32 BlockSizeRes;

    Tables *m_Tables;
    Optimal m_Optimum[NumOpts];

    quint32 m_MatchFinderCycles;
    // IMatchFinderSetNumPasses *m_SetMfPasses;

    void GetMatches();
    void MovePos(quint32 num);
    quint32 Backward(quint32 &backRes, quint32 cur);
    quint32 GetOptimal(quint32 &backRes);
    quint32 GetOptimalFast(quint32 &backRes);

    void LevelTableDummy(const quint8 *levels, int numLevels, quint32 *freqs);

    bool LevelTableCode(const quint8 *levels, int numLevels, const quint8 *lens, const quint32 *codes);

    void MakeTables();
    quint32 GetLzBlockPrice() const;
    void TryBlock();
    quint32 TryDynBlock(int tableIndex, quint32 numPasses);

    quint32 TryFixedBlock(int tableIndex);

    void SetPrices(const Levels& levels);
    bool WriteBlock();

    bool Create();
    void Free();

    bool WriteStoreBlock(quint32 blockSize, quint32 additionalOffset, bool finalBlock);
    bool WriteTables(bool writeMode, bool finalBlock);

    bool WriteBlockData(bool writeMode, bool finalBlock);

    quint32 GetBlockPrice(int tableIndex, int numDivPasses);
    bool CodeBlock(int tableIndex, bool finalBlock);
};

///////////////////////////////////////////////////////////////

class DeflateEncoder : public BaseCoder {
    Q_OBJECT
public:
    DeflateEncoder(QObject *parent = 0) : BaseCoder(Deflate32, parent) {}
};

class Deflate64Encoder : public BaseCoder {
    Q_OBJECT
public:
    Deflate64Encoder(QObject *parent = 0) : BaseCoder(Deflate64, parent) {}
};

}
}

#endif
