#ifndef QZ7_BYTEIO_H
#define QZ7_BYTEIO_H

#include "qz7/Error.h"
#include "qz7/Stream.h"

#include <QtCore/QByteArray>

namespace qz7 {

class ByteIoReader {
public:
    ByteIoReader(ReadStream *stream) { mStream = stream; }

    quint8 read8() {
        quint8 b;
        if (!mStream->read(&b, 1))
            throw TruncatedArchiveError();
        return b;
    }

    quint16 read16BE() {
        quint8 b[2];
        if (!mStream->read(b, 2))
            throw TruncatedArchiveError();
        return (quint16)((b[0] << 8) | b[1]);
    }

    quint16 read16LE() {
        quint8 b[2];
        if (!mStream->read(b, 2))
            throw TruncatedArchiveError();
        return (quint16)((b[1] << 8) | b[0]);
    }

    quint32 read32BE() {
        quint8 b[4];
        if (!mStream->read(b, 4))
            throw TruncatedArchiveError();
        return (quint32)((b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3]);
    }

    quint32 read32LE() {
        quint8 b[4];
        if (!mStream->read(b, 4))
            throw TruncatedArchiveError();
        return (quint32)((b[3] << 24) | (b[2] << 16) | (b[1] << 8) | b[0]);
    }

    quint64 read64BE() {
        quint8 b[8];
        if (!mStream->read(b, 8))
            throw TruncatedArchiveError();
        return (quint64(b[0]) << 54) | (quint64(b[1]) << 48) | (quint64(b[2]) << 40) | (quint64(b[3]) << 32) |
                            (quint64(b[4]) << 24) | (quint64(b[5]) << 16) | (quint64(b[6]) << 8) | quint64(b[7]);
    }

    quint64 read64LE() {
        quint8 b[8];
        if (!mStream->read(b, 8))
            throw TruncatedArchiveError();
        return (quint64(b[7]) << 54) | (quint64(b[6]) << 48) | (quint64(b[5]) << 40) | (quint64(b[4]) << 32) |
                            (quint64(b[3]) << 24) | (quint64(b[2]) << 16) | (quint64(b[1]) << 8) | quint64(b[0]);
    }

    void readBuffer(QByteArray *ba, uint size) {
        ba->reserve(size);
        if (!mStream->read(reinterpret_cast<quint8 *>(ba->data()), size))
            throw TruncatedArchiveError();
    }

    void readStringZ(QByteArray *ba) {
        quint8 b;
        while (true) {
            if (!mStream->read(&b, 1))
                throw TruncatedArchiveError();
            if (!b)
                return;
            ba->append(b);
        }
    }

private:
    ReadStream *mStream;
};

};

#endif
