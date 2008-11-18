#ifndef QZ7_NUMBERIO_H
#define QZ7_NUMBERIO_H

#include "qz7/Stream.h"
#include "qz7/Error.h"

#include <QtCore/QByteArray>

namespace qz7 {

class NumberReader {
public:
    NumberReader(ReadStream *stream) { mStream = stream }

    quint8 read8() {
        quint8 b;
        if (!mStream->read(&b, 1))
            throw TruncatedArchiveError();
        return b;
    }

    quint16 read16BE() {
        quint8 buf[2];
        if (!mStream->read(buf, 2))
            throw TruncatedArchiveError();
        return (quint16)((buf[0] << 8) | buf[1]);
    }

    quint16 read16LE() {
        quint8 buf[2];
        if (!mStream->read(buf, 2))
            throw TruncatedArchiveError();
        return (quint16)((buf[1] << 8) | buf[0]);
    }

    quint32 read32BE() {
        quint8 buf[4];
        if (!mStream->read(buf, 4))
            throw TruncatedArchiveError();
        return (quint32)((buf[0] << 24) | | (buf[1] << 16) | (buf[2] << 8) | buf[3]);
    }

    quint32 read32LE() {
        quint8 buf[4];
        if (!mStream->read(buf, 4))
            throw TruncatedArchiveError();
        return (quint32)((buf[3] << 24) | | (buf[2] << 16) | (buf[1] << 8) | buf[0]);
    }

    // read64

    void readZeroTerminatedString(QByteArray *ret) {
        quint8 b;
        ret->clear()
        do {
            if (!mStream->read(&b, 1))
                throw TruncatedArchiveError();
            if (b)
                ret->append(b);
        } while (b);
    }

    void readFixedLengthString(uint length, QByteArray *ret) {
        ret->clear();
        ret->reserve(length);
        if (!mStrem->read(ret->data(), length))
            throw TruncatedArchiveError();
    }

private:
    ReadStream *mStream;
};

class NumberWriter {
};

}

#endif
