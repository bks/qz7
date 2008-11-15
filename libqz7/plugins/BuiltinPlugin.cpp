#define QT_STATICPLUGIN

#include "BuiltinPlugin.h"

#include <QtCore/QList>
#include <QtCore/QStringList>

#include "archives/gzip/GzipArchive.h"

#include "codecs/deflate/DeflateDecoder.h"

#include "volumes/singlefile/SingleFileVolume.h"

namespace qz7 {

QStringList BuiltinPlugin::archiveMimeTypes() const
{
    return QStringList()
        << "application/x-gzip";
}

Archive *BuiltinPlugin::createArchive(const QString& mimeType, Volume *volume) const
{
    if (mimeType == "application/x-gzip")
        return new gzip::GzipArchive(volume);
    return 0;
}

QStringList BuiltinPlugin::decoderNames() const
{
    return QStringList()
        << "deflate"
        << "deflateNSIS"
        << "deflate64";
}

QList<int> BuiltinPlugin::decoderIds() const
{
    return QList<int>()
        << 0x40108
        << 0x40901
        << 0x40109;
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
    if (name == "deflate")
        return new deflate::DeflateDecoder(parent);
    if (name == "deflate64")
        return new deflate::Deflate64Decoder(parent);
    if (name == "deflateNSIS")
        return new deflate::DeflateNSISDecoder(parent);

    return 0;
}

Codec *BuiltinPlugin::createDecoder(int id, QObject *parent) const
{
    switch (id) {
    case 0x40108:
        return new deflate::DeflateDecoder(parent);
    case 0x40109:
        return new deflate::Deflate64Decoder(parent);
    case 0x40901:
        return new deflate::DeflateNSISDecoder(parent);
    default:
        return 0;
    }
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
