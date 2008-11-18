#ifndef MTF8_H
#define MTF8_H

#include <string.h>

namespace Compress {
namespace BZip2 {

class Mtf8Encoder {
public:
    quint8 Buffer[256];
    int FindAndMove(quint8 v) {
        int pos;
        for (pos = 0; Buffer[pos] != v; pos++);
        int resPos = pos;
        for (; pos >= 8; pos -= 8) {
            Buffer[pos] = Buffer[pos - 1];
            Buffer[pos - 1] = Buffer[pos - 2];
            Buffer[pos - 2] = Buffer[pos - 3];
            Buffer[pos - 3] = Buffer[pos - 4];
            Buffer[pos - 4] = Buffer[pos - 5];
            Buffer[pos - 5] = Buffer[pos - 6];
            Buffer[pos - 6] = Buffer[pos - 7];
            Buffer[pos - 7] = Buffer[pos - 8];
        }
        for (; pos > 0; pos--)
            Buffer[pos] = Buffer[pos - 1];
        Buffer[0] = v;
        return resPos;
    }
};

class Mtf8Decoder
{
public:

#if QT_POINTER_SIZE == 8
    typedef quint64 MtfVal;
#   define MTF_MOVS 3
#else
    typedef quint32 MtfVal;
#   define MTF_MOVS 2
#endif
#define MTF_MASK ((1 << MTF_MOVS) - 1)

    MtfVal Buffer[256 >> MTF_MOVS];
    void StartInit() {
        ::memset(Buffer, 0, sizeof(Buffer));
    }
    void Add(unsigned int pos, quint8 val) {
        Buffer[pos >> MTF_MOVS] |= ((MtfVal)val << ((pos & MTF_MASK) << 3));
    }
    quint8 GetHead() const {
        return (quint8)Buffer[0];
    }
    quint8 GetAndMove(unsigned int pos) {
        quint32 lim = ((quint32)pos >> MTF_MOVS);
        pos = (pos & MTF_MASK) << 3;
        MtfVal prev = (Buffer[lim] >> pos) & 0xFF;

        quint32 i = 0;
        if ((lim & 1) != 0) {
            MtfVal next = Buffer[0];
            Buffer[0] = (next << 8) | prev;
            prev = (next >> (MTF_MASK << 3));
            i = 1;
            lim -= 1;
        }
        for (; i < lim; i += 2) {
            MtfVal next = Buffer[i];
            Buffer[i] = (next << 8) | prev;
            prev = (next >> (MTF_MASK << 3));
            next = Buffer[i + 1];
            Buffer[i + 1] = (next << 8) | prev;
            prev = (next >> (MTF_MASK << 3));
        }
        MtfVal next = Buffer[i];
        MtfVal mask = (((MtfVal)0x100 << pos) - 1);
        Buffer[i] = (next & ~mask) | (((next << 8) | prev) & mask);
        return (quint8)Buffer[0];
    }
};

}
}
#endif
