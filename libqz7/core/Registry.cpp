#include "Registry.h"
#include "Archive.h"
#include "Codec.h"
#include "Volume.h"

#include <QtCore/QHash>
#include <QtCore/QMap>

class QObject;

namespace qz7 {

static QHash<QString, const VolumeInfo *> volumeHandlers;
static QHash<QString, const ArchiveInfo *> archiveHandlers;
static QHash<QString, const CodecInfo *> codecHandlersByName;
static QMap<uint, const CodecInfo *> codecHandlersById;

void RegisterVolume(const VolumeInfo *i)
{
    volumeHandlers.insert(i->name(), i);
}

void RegisterCodec(const CodecInfo *i)
{
    codecHandlersByName.insert(i->name(), i);
    codecHandlersById.insert(i->id(), i);
}

void RegisterArchive(const ArchiveInfo *i)
{
    archiveHandlers.insert(i->name(), i);
}

Volume * CreateVolume(const QString& mimeType, const QString& file, QObject *parent)
{
    if (!volumeHandlers.contains(mimeType))
        return 0;

    return volumeHandlers[mimeType]->createForFile(file, parent);
}

Archive * CreateArchive(const QString& type, Volume *parent)
{
    if (!archiveHandlers.contains(type))
        return 0;

    return archiveHandlers[type]->createForVolume(parent);
}

Codec * CreateDecoder(const QString& type, QObject *parent)
{
    if (!codecHandlersByName.contains(type))
        return 0;

    return codecHandlersByName[type]->createDecoder(parent);
}

Codec * CreateDecoder(uint id, QObject *parent)
{
    if (!codecHandlersById.contains(id))
        return 0;

    return codecHandlersById[id]->createDecoder(parent);
}

Codec * CreateEncoder(const QString& type, QObject *parent)
{
    if (!codecHandlersByName.contains(type))
        return 0;

    return codecHandlersByName[type]->createEncoder(parent);
}

Codec * CreateEncoder(uint id, QObject *parent)
{
    if (!codecHandlersById.contains(id))
        return 0;

    return codecHandlersById[id]->createEncoder(parent);
}

};