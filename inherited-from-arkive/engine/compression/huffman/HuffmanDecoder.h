// Compress/HuffmanDecoder.h

#ifndef HUFFMANDECODER_H
#define HUFFMANDECODER_H

#include "kernel/QioBuffer.h"
#include "kernel/BitStreamLE.h"
#include "kernel/BitStreamBE.h"

namespace Compress {
namespace Huffman {

const int NumTableBits = 9;

// For reasons which are unclear to me, Huffman tables in DEFLATE-derived
// compression formats store their symbols most-significant-bit first, even
// in an otherwise LSBF bit stream (c.f. RFC 1951). Work around
// that by templatizing how we pull a symbol out of a bit stream.
template<class BitStream> class SymbolReader {
public:
    static bool peekSymbol(BitStream *stream, int bits, quint32 *symbol);
};

template<> inline bool SymbolReader<BitStreamLE<QioReadBuffer> >
    ::peekSymbol(BitStreamLE<QioReadBuffer> *stream, int bits, quint32 *symbol) {
    bool ok = stream->peekReversedBits(bits, symbol);
    return ok;
};

template<> inline bool SymbolReader<BitStreamBE<QioReadBuffer> >
    ::peekSymbol(BitStreamBE<QioReadBuffer> *stream, int bits, quint32 *symbol) {
    return stream->peekBits(bits, symbol);
};

enum Error { NoError, IOError, OutOfBoundsError };

template <int NumBitsMax, int NumSymbols> class Decoder
{
public:
    Error error() const { return mError; }

    Decoder() : mError(NoError) {}

    bool SetCodeLengths(const quint8 *codeLengths) {
        int lenCounts[NumBitsMax + 1], tmpPositions[NumBitsMax + 1];
        int i;

        for (i = 1; i <= NumBitsMax; i++)
            lenCounts[i] = 0;

        quint32 symbol;
        for (symbol = 0; symbol < NumSymbols; symbol++) {
            int len = codeLengths[symbol];
            if (len > NumBitsMax) {
                mError = OutOfBoundsError;
                return false;
            }
            lenCounts[len]++;
            m_Symbols[symbol] = 0xFFFFFFFF;
        }

        lenCounts[0] = 0;
        m_Positions[0] = m_Limits[0] = 0;
        quint32 startPos = 0;
        quint32 index = 0;

        const quint32 MaxValue = (1 << NumBitsMax);

        for (i = 1; i <= NumBitsMax; i++) {
            startPos += lenCounts[i] << (NumBitsMax - i);
            if (startPos > MaxValue) {
                mError = OutOfBoundsError;
                return false;
            }

            m_Limits[i] = (i == NumBitsMax) ? MaxValue : startPos;
            m_Positions[i] = m_Positions[i - 1] + lenCounts[i - 1];
            tmpPositions[i] = m_Positions[i];

            if (i <= NumTableBits) {
                quint32 limit = (m_Limits[i] >> (NumBitsMax - NumTableBits));
                for (; index < limit; index++)
                    m_Lengths[index] = (quint8)i;
            }
        }

        for (symbol = 0; symbol < NumSymbols; symbol++) {
            int len = codeLengths[symbol];
            if (len != 0)
                m_Symbols[tmpPositions[len]++] = symbol;
        }

        return true;
    }

    template <class TBitDecoder>
    bool DecodeSymbol(TBitDecoder *bitStream, quint32 *symbol) {
        int numBits;
        quint32 value;

        if (!SymbolReader<TBitDecoder>::peekSymbol(bitStream, NumBitsMax, &value)) {
                // if reading the max symbol fails, try again with whatever is in the bit stream's
                // buffer: we might just be at EOF and not have more full bytes to work with...
            int avail = bitStream->bitsAvailable();
            if (!(avail && SymbolReader<TBitDecoder>::peekSymbol(bitStream, avail, &value))) {
                mError = IOError;
                return false;
            }
            value <<= NumBitsMax - avail;
        }

        if (value < m_Limits[NumTableBits]) {
            numBits = m_Lengths[value >> (NumBitsMax - NumTableBits)];
        } else {
            for (numBits = NumTableBits + 1; value >= m_Limits[numBits]; numBits++)
                ;
        }

        bitStream->skipBits(numBits);
        quint32 index = m_Positions[numBits] +
                        ((value - m_Limits[numBits - 1]) >> (NumBitsMax - numBits));

        if (index >= NumSymbols) {
            mError = OutOfBoundsError;
            return false;
        }

        *symbol = m_Symbols[index];
        return true;
    }

private:
    quint32 m_Limits[NumBitsMax + 1];     // m_Limits[i] = value limit for symbols with length = i
    quint32 m_Positions[NumBitsMax + 1];  // m_Positions[i] = index in m_Symbols[] of first symbol with length = i
    quint32 m_Symbols[NumSymbols];
    quint8 m_Lengths[1 << NumTableBits];   // Table oh length for short codes.
    Error mError;
};

}
}

#endif
