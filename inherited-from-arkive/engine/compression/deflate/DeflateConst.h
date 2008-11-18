// DeflateConst.h

#ifndef __DEFLATE_CONST_H
#define __DEFLATE_CONST_H

namespace Compress{
namespace Deflate {
const int NumHuffmanBits = 15;

const quint32 HistorySize32 = (1 << 15);
const quint32 HistorySize64 = (1 << 16);

const quint32 DistTableSize32 = 30;
const quint32 DistTableSize64 = 32;

const quint32 NumLenSymbols32 = 256;
const quint32 NumLenSymbols64 = 255; // don't change it. It must be <= 255.
const quint32 NumLenSymbolsMax = NumLenSymbols32;

const quint32 NumLenSlots = 29;

const quint32 FixedDistTableSize = 32;
const quint32 FixedLenTableSize = 31;

const quint32 SymbolEndOfBlock = 0x100;
const quint32 SymbolMatch = SymbolEndOfBlock + 1;

const quint32 MainTableSize = SymbolMatch + NumLenSlots;
const quint32 FixedMainTableSize = SymbolMatch + FixedLenTableSize;

const quint32 LevelTableSize = 19;

const quint32 TableDirectLevels = 16;
const quint32 TableLevelRepNumber = TableDirectLevels;
const quint32 TableLevel0Number = TableLevelRepNumber + 1;
const quint32 TableLevel0Number2 = TableLevel0Number + 1;

const quint32 LevelMask = 0xF;

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

const quint32 MatchMinLen = 3;
const quint32 MatchMaxLen32 = NumLenSymbols32 + MatchMinLen - 1; //256 + 2
const quint32 MatchMaxLen64 = NumLenSymbols64 + MatchMinLen - 1; //255 + 2
const quint32 MatchMaxLen = MatchMaxLen32;

const int FinalBlockFieldSize = 1;

enum {
    NotFinalBlock = 0,
    FinalBlock = 1
};

const int BlockTypeFieldSize = 2;

enum {
    BlockTypeStored = 0,
    BlockTypeFixedHuffman = 1,
    BlockTypeDynamicHuffman = 2
};

const int NumLenCodesFieldSize = 5;
const int NumDistCodesFieldSize = 5;
const int NumLevelCodesFieldSize = 4;

const quint32 NumLitLenCodesMin = 257;
const quint32 NumDistCodesMin = 1;
const quint32 NumLevelCodesMin = 4;

const int LevelFieldSize = 3;

const int StoredBlockLengthFieldSize = 16;

struct Levels {
    quint8 litLenLevels[FixedMainTableSize];
    quint8 distLevels[FixedDistTableSize];

    void SubClear() {
        quint32 i;
        for (i = NumLitLenCodesMin; i < FixedMainTableSize; i++)
            litLenLevels[i] = 0;
        for (i = 0; i < FixedDistTableSize; i++)
            distLevels[i] = 0;
    }

    void SetFixedLevels() {
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
