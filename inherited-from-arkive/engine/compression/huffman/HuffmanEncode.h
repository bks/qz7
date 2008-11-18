/* Compress/HuffmanEncode.h */

#ifndef HUFFMANENCODE_H
#define HUFFMANENCODE_H

#include <QtCore/QtGlobal>

/*
Conditions:
  num <= 1024 = 2 ^ NUM_BITS
  Sum(freqs) < 4M = 2 ^ (32 - NUM_BITS)
  maxLen <= 16 = kMaxLen
  Num_Items(p) >= HUFFMAN_TEMP_SIZE(num)
*/
 
void Huffman_Generate(const quint32 *freqs, quint32 *p, quint8 *lens, quint32 num, quint32 maxLen);

#endif
