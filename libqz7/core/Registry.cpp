#include "Registry.h"
#include "Archive.h"
#include "Codec.h"
#include "Volume.h"

#include <QtCore/QHash>
#include <QtCore/QMap>

class QObject;

namespace qz7 {

typedef QHash<QString, const VolumeInfo *> VolumeHandlerMap;
typedef QHash<QString, const ArchiveInfo *> ArchiveHandlerMap;
typedef QHash<QString, const CodecInfo *> CodecHandlerMap;
typedef QMap<int, const CodecInfo *> CodecHandlerIdMap;

Q_GLOBAL_STATIC(VolumeHandlerMap, volumeHandlers)
Q_GLOBAL_STATIC(ArchiveHandlerMap, archiveHandlers)
Q_GLOBAL_STATIC(CodecHandlerMap, codecHandlersByName)
Q_GLOBAL_STATIC(CodecHandlerIdMap, codecHandlersById)

void RegisterVolume(const VolumeInfo *i)
{
    volumeHandlers()->insert(i->name(), i);
}

void RegisterCodec(const CodecInfo *i)
{
    codecHandlersByName()->insert(i->name(), i);
    codecHandlersById()->insert(i->id(), i);
}

void RegisterArchive(const ArchiveInfo *i)
{
    archiveHandlers()->insert(i->name(), i);
}

Volume * CreateVolume(const QString& mimeType, const QString& file, QObject *parent)
{
    if (!volumeHandlers()->contains(mimeType))
        return 0;

    return volumeHandlers()->value(mimeType)->createForFile(file, parent);
}

Archive * CreateArchive(const QString& type, Volume *parent)
{
    if (!archiveHandlers()->contains(type))
        return 0;

    return archiveHandlers()->value(type)->createForVolume(parent);
}

Codec * CreateDecoder(const QString& type, QObject *parent)
{
    if (!codecHandlersByName()->contains(type))
        return 0;

    return codecHandlersByName()->value(type)->createDecoder(parent);
}

Codec * CreateDecoder(uint id, QObject *parent)
{
    if (!codecHandlersById()->contains(id))
        return 0;

    return codecHandlersById()->value(id)->createDecoder(parent);
}

Codec * CreateEncoder(const QString& type, QObject *parent)
{
    if (!codecHandlersByName()->contains(type))
        return 0;

    return codecHandlersByName()->value(type)->createEncoder(parent);
}

Codec * CreateEncoder(uint id, QObject *parent)
{
    if (!codecHandlersById()->contains(id))
        return 0;

    return codecHandlersById()->value(id)->createEncoder(parent);
}

HandlerInfo::HandlerInfo(const QString& name, int id)
    : mName(name), mId(id)
{
}

QString HandlerInfo::name() const
{
    return mName;
}

int HandlerInfo::id() const
{
    return mId;
}

};