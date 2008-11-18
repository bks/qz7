#ifndef BZIP2CRC_H
#define BZIP2CRC_H

#include <QtCore/QtGlobal>

namespace Compress {
namespace BZip2 {

class BZip2CRC {
public:
    BZip2CRC() { reinit(); }
    void reinit() { _value = 0xFFFFFFFF; }
    void updateByte(quint8 b) { _value = Table[(_value >> 24) ^ b] ^(_value << 8); }
    void updateByte(unsigned int b) { _value = Table[(_value >> 24) ^ b] ^(_value << 8); }
    quint32 getDigest() const { return (_value ^ 0xFFFFFFFF); }

private:
    quint32 _value;
    static const quint32 Table[256];
};

class BZip2CombinedCRC {
public:
    BZip2CombinedCRC() { reinit(); }
    void reinit() { _value = 0; }
    void updateUInt32(quint32 v) { _value = ((_value << 1) | (_value >> 31)) ^ v; }
    quint32 getDigest() const { return _value; }

private:
    quint32 _value;
};

// close namespaces
}
}

#endif
