#ifndef QZ7_PLUGIN_H
#define QZ7_PLUGIN_H

template<typename T> class QList;
template<typename K, typename V> class QMap;
class QObject;
class QString;
class QStringList;

namespace qz7 {

class Archive;
class Codec;
class Volume;

class ArchiveFactory {
public:
    virtual QStringList archiveMimeTypes() const;
    virtual Archive *createArchive(const QString& mimeType, Volume *volume) const;
};

class CodecFactory {
public:
    virtual QStringList decoderNames() const;
    virtual QList<int> decoderIds() const;
    virtual QStringList encoderNames() const;
    virtual QList<int> encoderIds() const;
    virtual Codec *createDecoder(const QString& name, QObject *parent) const;
    virtual Codec *createDecoder(int id, QObject *parent) const;
    virtual Codec *createEncoder(const QString& name, QObject *parent) const;
    virtual Codec *createEncoder(int id, QObject *parent) const;
};

class VolumeFactory {
public:
    virtual QStringList volumeMimeTypes() const;
    virtual Volume *createVolume(const QString& type, QObject *parent) const;
};

class QZ7_EXPORT Registry {
public:
    static Archive *createArchive(const QString& mimeType, QObject *volume);
    static Codec *createCodec(const QString& name, QObject *parent);
    static Codec *createCodec(int id, QObject *parent);
    static Volume *createVolume(const QString& mimeType, QObject *parent);

private:
    Registry();
    ~Registry();
    void loadPlugins();
    void registerPlugin(QObject *instance);
    QObject *findArchive(const QString& mimeType);
    QObject *findCodec(const QString& name);
    QObject *findCodec(int id);
    QObject *findVolume(const QString& type);

    static Registry *the();

    QMap<QString, Object *> archivesByMimeType;
    QMap<QString, QObject *> codecsByName;
    QMap<int, QObject *> codecsById;
    QMape<QString, QObject *> volumesByMimeType;
};

enum {
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
