#ifndef QZ7_STREAM_TOOLS_H
#define QZ7_STREAM_TOOLS_H

#include "Stream.h"

#include <QtCore/QBuffer>
#include <QtCore/QByteArray>

namespace qz7 {
namespace tools {

bool copyData(ReadStream& src, WriteStream& dst);

inline bool copyData(const QBuffer& buffer, WriteStream& dst)
{
    return dst.write(reinterpret_cast<const quint8 *>(buffer.buffer().constData()), buffer.buffer().size());
};

inline bool copyData(const QBuffer *src, QIODevice *dst)
{
    QioWriteStream d(dst);

    return copyData(*src, d);
};

inline bool copyData(QIODevice *src, QIODevice *dst)
{
    QioReadStream s(src);
    QioWriteStream d(dst);

    return copyData(s, d);
};

}
}

#endif

