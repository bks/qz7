#include "CopyCodec.h"

#include "kernel/HandlerRegistry.h"

#include <QtCore/QByteArray>
#include <QtCore/QIODevice>

namespace Compression {
namespace Copy {

CopyCodec::CopyCodec(QObject *parent)
    : CompressionCodecBase(parent)
{
}

bool CopyCodec::code()
{
    // 16 pages on x86, 8 pages on x86-64
    const int BUFFER_SIZE = 16 * 4096;
    QByteArray buffer;
    buffer.resize(BUFFER_SIZE);

    while (1) {
        int read = inputDevice()->read(buffer.data(), buffer.size());

        if (read < 0) {
            setErrorString(inputDevice()->errorString());
            return false;
        }

        int offset = 0;
        while (read) {
            int written = outputDevice()->write(&buffer.data()[offset], read);

            if (written < 0) {
                setErrorString(outputDevice()->errorString());
                return false;
            }

            read -= written;
            offset += written;

            if (read && !outputDevice()->waitForBytesWritten(-1)) {
                setErrorString(outputDevice()->errorString());
                return false;
            }
        }

        if (!inputDevice()->waitForReadyRead(-1))
            return true;
    }
}

bool CopyCodec::setProperties(const QMap<QString, QVariant>& properties)
{
    if (!properties.isEmpty()) {
        qWarning("CopyCodec::setProperties: does nothing!");
        return false;
    }
    return true;
}

bool CopyCodec::setProperties(const QByteArray& serializedProperties)
{
    if (!serializedProperties.isEmpty())
        return false;
    return true;
}

QByteArray CopyCodec::serializeProperties()
{
    return QByteArray();
}

}
}

EXPORT_CODEC(Copy, 1, Compression::Copy::CopyCodec, Compression::Copy::CopyCodec)
