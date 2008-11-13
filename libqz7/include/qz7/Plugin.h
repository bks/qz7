#ifndef QZ7_PLUGIN_H
#define QZ7_PLUGIN_H

#include <QtCore/QtPlugin>
#include <QtCore/QMap>

template<typename T> class QList;
class QObject;
class QString;
class QStringList;

namespace qz7 {

class Archive;
class Codec;
class Volume;

class ArchiveFactory {
public:
    virtual QStringList archiveMimeTypes() const = 0;
    virtual Archive *createArchive(const QString& mimeType, Volume *volume) const = 0;
};

class CodecFactory {
public:
    virtual QStringList decoderNames() const = 0;
    virtual QList<int> decoderIds() const = 0;
    virtual QStringList encoderNames() const = 0;
    virtual QList<int> encoderIds() const = 0;
    virtual Codec *createDecoder(const QString& name, QObject *parent) const = 0;
    virtual Codec *createDecoder(int id, QObject *parent) const = 0;
    virtual Codec *createEncoder(const QString& name, QObject *parent) const = 0;
    virtual Codec *createEncoder(int id, QObject *parent) const = 0;
};

class VolumeFactory {
public:
    virtual QStringList volumeMimeTypes() const = 0;
    virtual Volume *createVolume(const QString& type, const QString& memberFile, QObject *parent) const = 0;
};

class Registry {
public:
    static Archive *createArchive(const QString& mimeType, Volume *volume);
    static Codec *createDecoder(const QString& name, QObject *parent);
    static Codec *createDecoder(int id, QObject *parent);
    static Codec *createEncoder(const QString& name, QObject *parent);
    static Codec *createEncoder(int id, QObject *parent);
    static Volume *createVolume(const QString& mimeType, const QString& memberFile, QObject *parent);

private:
    Registry();
    ~Registry();
    void loadPlugins();
    void registerPlugin(QObject *instance);
    QObject *findArchive(const QString& mimeType);
    QObject *findDecoder(const QString& name);
    QObject *findDecoder(int id);
    QObject *findEncoder(const QString& name);
    QObject *findEncoder(int id);
    QObject *findVolume(const QString& type);

    static Registry *the();

    QMap<QString, QObject *> archivesByMimeType;
    QMap<QString, QObject *> decodersByName;
    QMap<int, QObject *> decodersById;
    QMap<QString, QObject *> encodersByName;
    QMap<int, QObject *> encodersById;
    QMap<QString, QObject *> volumesByMimeType;
};

enum ExportType {
    ExportArchive = 1,
    ExportCodec = 2,
    ExportVolume = 3
};

} // namespace qz7

Q_DECLARE_INTERFACE(qz7::ArchiveFactory, "qz7.ArchiveFactory/1.0");
Q_DECLARE_INTERFACE(qz7::CodecFactory, "qz7.CodecFactory/1.0");
Q_DECLARE_INTERFACE(qz7::VolumeFactory, "qz7.VolumeFactory/1.0");

#ifndef QZ7_DECLARE_EXPORT
#define QZ7_DECLARE_EXPORT(exportType, mime, id, object)
#endif

#endif
