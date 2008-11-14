#ifndef QZ7_BUILTINPLUGIN_H
#define QZ7_BUILTINPLUGIN_H

#include "qz7/Plugin.h"

#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QString>

namespace qz7 {

class BuiltinPlugin
    : public QObject
    , public ArchiveFactory
    , public CodecFactory
    , public VolumeFactory
{
    Q_OBJECT
    Q_INTERFACES(qz7::ArchiveFactory qz7::CodecFactory qz7::VolumeFactory)

public:
    virtual QStringList archiveMimeTypes() const;
    virtual Archive *createArchive(const QString& mimeType, Volume *volume) const;

    virtual QStringList decoderNames() const;
    virtual QList<int> decoderIds() const;
    virtual QStringList encoderNames() const;
    virtual QList<int> encoderIds() const;
    virtual Codec *createDecoder(const QString& name, QObject *parent) const;
    virtual Codec *createDecoder(int id, QObject *parent) const;
    virtual Codec *createEncoder(const QString& name, QObject *parent) const;
    virtual Codec *createEncoder(int id, QObject *parent) const;

    virtual QStringList volumeMimeTypes() const;
    virtual Volume *createVolume(const QString& type, const QString& memberFile, QObject *parent) const;

};

}

#endif
