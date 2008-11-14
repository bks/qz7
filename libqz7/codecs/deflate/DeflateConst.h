#ifndef QZ7_DEFLATE_CONST_H
#define QZ7_DEFLATE_CONST_H

namespace qz7 {
namespace deflate {

enum { NumHuffmanBits = 15 };

enum { HistorySize32 = (1 << 15), HistorySize64 = (1 << 16) };
enum { DistTableSize32 = 30, DistTableSize64 = 32 };

enum {
    NumLenSymbols32 = 256,
    NumLenSymbols64 = 255, // don't change it. It must be <= 255.
    NumLenSymbolsMax = NumLenSymbols32
};

enum { NumLenSlots = 29 };

enum {
    FixedDistTableSize = 32,
    FixedLenTableSize = 31
};

enum {
    SymbolEndOfBlock = 0x100,
    SymbolMatch = SymbolEndOfBlock + 1
};

enum {
    MainTableSize = SymbolMatch + NumLenSlots,
    FixedMainTableSize = SymbolMatch + FixedLenTableSize
};

enum { LevelTableSize = 19 };

enum {
    TableDirectLevels = 16,
    TableLevelRepNumber = TableDirectLevels,
    TableLevel0Number = TableLevelRepNumber + 1,
    TableLevel0Number2 = TableLevel0Number + 1
};

enum { LevelMask = 0xF };

const quint8 LenStart32[FixedLenTableSize] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14, 16, 20, 24, 28, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 255, 0, 0};
const quint8 LenStart64[FixedLenTableSize] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14, 16, 20, 24, 28, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 0, 0, 0};

const quint8 LenDirectBits32[FixedLenTableSize] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4,  4,  5,  5,  5,  5, 0, 0, 0};
const quint8 LenDirectBits64[FixedLenTableSize] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4,  4,  5,  5,  5,  5, 16, 0, 0};

const quint32 DistStart[DistTableSize64]  = {0, 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512, 768,
        1024, 1536, 2048, 3072, 4096, 6144, 8192, 12288, 16384, 24576, 32768, 49152
                                              };
const quint8 DistDirectBits[DistTableSize64] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14};

const quint8 LevelDirectBits[3] = {2, 3, 7};

const quint8 CodeLengthAlphabetOrder[LevelTableSize] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

enum {
    MatchMinLen = 3,
    MatchMaxLen32 = NumLenSymbols32 + MatchMinLen - 1, //256 + 2
    MatchMaxLen64 = NumLenSymbols64 + MatchMinLen - 1, //255 + 2
    MatchMaxLen = MatchMaxLen32
};

enum { FinalBlockFieldSize = 1 };

enum {
    NotFinalBlock = 0,
    FinalBlock = 1
};

enum { BlockTypeFieldSize = 2 };

enum {
    BlockTypeStored = 0,
    BlockTypeFixedHuffman = 1,
    BlockTypeDynamicHuffman = 2
};

enum {
    NumLenCodesFieldSize = 5,
    NumDistCodesFieldSize = 5,
    NumLevelCodesFieldSize = 4
};

enum {
    NumLitLenCodesMin = 257,
    NumDistCodesMin = 1,
    NumLevelCodesMin = 4
};

enum {
    LevelFieldSize = 3,
    StoredBlockLengthFieldSize = 16
};

struct Levels {
    quint8 litLenLevels[FixedMainTableSize];
    quint8 distLevels[FixedDistTableSize];

    void clear() {
        quint32 i;
        for (i = NumLitLenCodesMin; i < FixedMainTableSize; i++)
            litLenLevels[i] = 0;
        for (i = 0; i < FixedDistTableSize; i++)
            distLevels[i] = 0;
    }

    void setFixedLevels() {
        unsigned int i;

        for (i = 0; i < 144; i++)
            litLenLevels[i] = 8;
        for (; i < 256; i++)
            litLenLevels[i] = 9;
        for (; i < 280; i++)
            litLenLevels[i] = 7;
        for (; i < 288; i++)
            litLenLevels[i] = 8;
        for (i = 0; i < FixedDistTableSize; i++)  // test it: InfoZip only uses DistTableSize
            distLevels[i] = 5;
    }
};

}
}

#endif
