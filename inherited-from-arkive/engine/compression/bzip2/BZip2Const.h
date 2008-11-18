#ifndef BZIP2CONST_H
#define BZIP2CONST_H

namespace Compress {
namespace BZip2 {

#define BZip2StreamMagic "BZh0"
#define BZip2StreamMagicLength 4

#define BZip2BlockStartSignature "\x31\x41\x59\x26\x53\x59"
#define BZip2BlockStartSignatureLength 6

#define BZip2BlockEndSignature "\x17\x72\x45\x38\x50\x90"
#define BZip2BlockEndSignatureLength 6

const int NumOrigBits = 24;

const int NumTablesBits = 3;
const int NumTablesMin = 2;
const int NumTablesMax = 6;

const int NumLevelsBits = 5;

const int MaxHuffmanLen = 20; // Check it

const int MaxAlphaSize = 258;

const int GroupSize = 50;

const int BlockSizeMultMin = 1;
const int BlockSizeMultMax = 9;
const quint32 BlockSizeStep = 100000;
const quint32 BlockSizeMax = BlockSizeMultMax * BlockSizeStep;

const int NumSelectorsBits = 15;
const quint32 NumSelectorsMax = (2 + (BlockSizeMax / GroupSize));

const int RleModeRepSize = 4;

// close namespaces
}
}

#endif
