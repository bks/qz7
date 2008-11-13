#define QT_STATICPLUGIN

#include "BuiltinPlugin.h"

#include <QtCore/QList>
#include <QtCore/QStringList>

#include "volumes/singlefile/SingleFileVolume.h"

namespace qz7 {

QStringList BuiltinPlugin::archiveMimeTypes() const
{
    return QStringList();
}

Archive *BuiltinPlugin::createArchive(const QString& mimeType, Volume *volume) const
{
    return 0;
}

QStringList BuiltinPlugin::decoderNames() const
{
    return QStringList();
}

QList<int> BuiltinPlugin::decoderIds() const
{
    return QList<int>();
}

QStringList BuiltinPlugin::encoderNames() const
{
    return QStringList();
}

QList<int> BuiltinPlugin::encoderIds() const
{
    return QList<int>();
}

Codec *BuiltinPlugin::createDecoder(const QString& name, QObject *parent) const
{
    return 0;
}

Codec *BuiltinPlugin::createDecoder(int id, QObject *parent) const
{
    return 0;
}

Codec *BuiltinPlugin::createEncoder(const QString& name, QObject *parent) const
{
    return 0;
}

Codec *BuiltinPlugin::createEncoder(int id, QObject *parent) const
{
    return 0;
}

QStringList BuiltinPlugin::volumeMimeTypes() const
{
    return QStringList()
        << "application/octet-stream";
}

Volume *BuiltinPlugin::createVolume(const QString& type, const QString& memberFile, QObject *parent) const
{
    if (type == "application/octet-stream")
        return new SingleFileVolume(memberFile, parent);
    return 0;
}

} // namespace qz7

Q_EXPORT_PLUGIN2(qz7builtin, qz7::BuiltinPlugin)
