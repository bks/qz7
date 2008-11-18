#include "kernel/HandlerRegistry.h"

#include <QtCore/QByteArray>
#include <QtCore/QDebug>
#include <QtCore/QMap>

typedef QMap<QByteArray, const ArchiveHandlerInfo*> ByteArrayArchiveMap;
typedef QMap<QByteArray, const CodecHandlerInfo*> ByteArrayCodecMap;
typedef QMap<quint64, const CodecHandlerInfo*> IdCodecMap;

Q_GLOBAL_STATIC(ByteArrayArchiveMap, archivesByName)
Q_GLOBAL_STATIC(ByteArrayArchiveMap, archivesByMimeType)
Q_GLOBAL_STATIC(ByteArrayCodecMap, codecsByName)
Q_GLOBAL_STATIC(IdCodecMap, codecsById)

void RegisterArchive(const ArchiveHandlerInfo* info)
{
    archivesByName()->insert(info->name(), info);
    archivesByMimeType()->insert(info->mimeType(), info);
}

void RegisterCodec(const CodecHandlerInfo* info)
{
    codecsByName()->insert(info->name(), info);
    codecsById()->insert(info->methodId(), info);
}

Archive *CreateArchiveForName(const QByteArray& name, QObject *parent)
{
    QMap<QByteArray, const ArchiveHandlerInfo *>::const_iterator i = archivesByName()->find(name);

    if (i == archivesByName()->end()) {
        qWarning() << "archive handler for archive type" << name << "not found";
        return 0;
    }

    return i.value()->createArchive(parent);
}

Archive *CreateArchiveForMimeType(const QByteArray& mimeType, QObject *parent)
{
    QMap<QByteArray, const ArchiveHandlerInfo *>::const_iterator i = archivesByMimeType()->find(mimeType);

    if (i == archivesByMimeType()->end()) {
        qWarning() << "archive handler for mime type" << mimeType << "not found";
        return 0;
    }

    return i.value()->createArchive(parent);
}

CompressionCodec * CreateCoderForName(const QByteArray& name, QObject *parent)
{
    QMap<QByteArray, const CodecHandlerInfo *>::const_iterator i = codecsByName()->find(name);

    if (i == codecsByName()->end()) {
        qWarning() << "coder for type" << name << "not found";
        return 0;
    }

    return i.value()->createEncoder(parent);
}

CompressionCodec * CreateCoderForId(quint64 id, QObject *parent)
{
    QMap<quint64, const CodecHandlerInfo *>::const_iterator i = codecsById()->find(id);

    if (i == codecsById()->end()) {
        qWarning() << "coder for id" << id << "not found";
        return 0;
    }

    return i.value()->createEncoder(parent);
}

CompressionCodec * CreateDecoderForName(const QByteArray& name, QObject *parent)
{
    QMap<QByteArray, const CodecHandlerInfo *>::const_iterator i = codecsByName()->find(name);

    if (i == codecsByName()->end()) {
        qWarning() << "decoder for type" << name << "not found";
        return 0;
    }

    return i.value()->createDecoder(parent);
}

CompressionCodec * CreateDecoderForId(quint64 id, QObject *parent)
{
    QMap<quint64, const CodecHandlerInfo *>::const_iterator i = codecsById()->find(id);

    if (i == codecsById()->end()) {
        qWarning() << "decoder for id" << id << "not found";
        return 0;
    }

    return i.value()->createDecoder(parent);
}
