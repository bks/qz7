#ifndef QZ7_HUFFMANDECODER_H
#define QZ7_HUFFMANDECODER_H

#include "qz7/Error.h"
#include "qz7/BitIoBE.h"
#include "qz7/BitIoLE.h"

namespace qz7 {

enum { HUFFMAN_TABLE_BITS = 9 };

// For reasons which are unclear to me, Huffman tables in DEFLATE-derived
// compression formats store their symbols most-significant-bit first, even
// in an otherwise LSBF bit stream (c.f. RFC 1951). Work around
// that by templatizing how we pull a symbol out of a bit stream.

template<class BitStream> class SymbolReader {
public:
    static uint peekSymbol(BitStream& stream, uint bits);
};

template<> inline uint SymbolReader<BitReaderLE>::peekSymbol(BitReaderLE& stream, uint bits) {
    uint ret = stream.peekReversedBits(bits);
    return ret;
};

template<> inline uint SymbolReader<BitReaderBE>::peekSymbol(BitReaderBE& stream, uint bits) {
    return stream.peekBits(bits);
};

template <uint MAX_BITS, uint NR_SYMBOLS> class HuffmanDecoder
{
public:
    HuffmanDecoder() {}

    void setCodeLengths(const quint8 *codeLengths) {
        int lenCounts[MAX_BITS + 1], tmpPositions[MAX_BITS + 1];
        int i;

        for (i = 1; i <= MAX_BITS; i++)
            lenCounts[i] = 0;

        for (uint symbol = 0; symbol < NR_SYMBOLS; symbol++) {
            int len = codeLengths[symbol];
            if (len > MAX_BITS)
                throw CorruptedError();
            lenCounts[len]++;
            m_Symbols[symbol] = 0xFFFFFFFF;
        }

        lenCounts[0] = 0;
        m_Positions[0] = m_Limits[0] = 0;
        uint startPos = 0;
        uint index = 0;

        const uint MaxValue = (1 << MAX_BITS);

        for (i = 1; i <= MAX_BITS; i++) {
            startPos += lenCounts[i] << (MAX_BITS - i);
            if (startPos > MaxValue)
                throw CorruptedError();

            m_Limits[i] = (i == MAX_BITS) ? MaxValue : startPos;
            m_Positions[i] = m_Positions[i - 1] + lenCounts[i - 1];
            tmpPositions[i] = m_Positions[i];

            if (i <= HUFFMAN_TABLE_BITS) {
                quint32 limit = (m_Limits[i] >> (MAX_BITS - HUFFMAN_TABLE_BITS));
                for (; index < limit; index++)
                    m_Lengths[index] = (quint8)i;
            }
        }

        for (uint symbol = 0; symbol < NR_SYMBOLS; symbol++) {
            int len = codeLengths[symbol];
            if (len != 0)
                m_Symbols[tmpPositions[len]++] = symbol;
        }
    }

    template <class TBitDecoder>
    quint32 decodeSymbol(TBitDecoder& bitStream) {
        int numBits;
        quint32 value;

        value = SymbolReader<TBitDecoder>::peekSymbol(bitStream, MAX_BITS);

        if (value < m_Limits[HUFFMAN_TABLE_BITS]) {
            numBits = m_Lengths[value >> (MAX_BITS - HUFFMAN_TABLE_BITS)];
        } else {
            for (numBits = HUFFMAN_TABLE_BITS + 1; value >= m_Limits[numBits]; numBits++)
                ;
        }

        bitStream.consumeBits(numBits);
        quint32 index = m_Positions[numBits] +
                        ((value - m_Limits[numBits - 1]) >> (MAX_BITS - numBits));

        if (index >= NR_SYMBOLS)
            throw CorruptedError();

        return m_Symbols[index];
    }

private:
    quint32 m_Limits[MAX_BITS + 1];     // m_Limits[i] = value limit for symbols with length = i
    quint32 m_Positions[MAX_BITS + 1];  // m_Positions[i] = index in m_Symbols[] of first symbol with length = i
    quint32 m_Symbols[NR_SYMBOLS];
    quint8 m_Lengths[1 << HUFFMAN_TABLE_BITS];   // Table of length for short codes.
};

}

#endif
