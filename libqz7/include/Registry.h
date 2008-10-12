#ifndef QZ7_REGISTRY_H
#define QZ7_REGISTRY_H

#include <QtCore/QString>

class QObject;

namespace qz7 {

class Archive;
class Codec;
class Volume;

class VolumeInfo;
class CodecInfo;
class ArchiveInfo;

void RegisterVolume(const VolumeInfo *);
void RegisterCodec(const CodecInfo *);
void RegisterArchive(const ArchiveInfo *);

template<class T> class HandlerInfo {
public:
    HandlerInfo<T>(const QString& name, int id) : mName(name), mId(id) { }
    QString name() const { return mName; }
    int id() const { return mId; }
private:
    QString mName;
    int mId;
};

class VolumeInfo : public HandlerInfo<Volume> {
public:
    VolumeInfo(const QString& mimeType) : HandlerInfo<Volume>(mimeType, 0) { RegisterVolume(this); }
    virtual Volume *createForFile(const QString& file, QObject *parent) const = 0;
};

class CodecInfo : public HandlerInfo<Codec> {
public:
    CodecInfo(const QString& name, int id) : HandlerInfo<Codec>(name, id) { RegisterCodec(this); }
    virtual Codec *createDecoder(QObject *parent) const = 0;
    virtual Codec *createEncoder(QObject *parent) const = 0;
};

class ArchiveInfo : public HandlerInfo<Archive> {
public:
    ArchiveInfo(const QString& name) : HandlerInfo<Archive>(name, 0) { RegisterArchive(this); }
    virtual Archive * createForVolume(Volume * v) const = 0;
};

#define EXPORT_VOLUME(mime, Class)                                                  \
    class VolumeInfo ## Class : public VolumeInfo {                                 \
    public:                                                                         \
        VolumeInfo ## Class(const QString& mimeType) : VolumeInfo(mimeType) { }     \
        virtual Volume *createForFile(const QString& file, QObject *parent) const   \
            { return new Class(file, parent); }                                     \
    };                                                                              \
    static const VolumeInfo ## Class s_export ## Class(QLatin1String(mime));

#define EXPORT_ARCHIVE(name, Class)                                         \
    class ArchiveInfo ## Class : public ArchiveInfo {                       \
    public:                                                                 \
        ArchiveInfo ## Class(const QString& n) : ArchiveInfo(n) { }   \
        virtual Archive *createForVolume(Volume *v) const                   \
            { return new Class(v); }                                        \
    };                                                                      \
    static const ArchiveInfo ## Class s_export ## Class(QLatin1String(name));

#define EXPORT_CODEC_BASE(Class, name, id, CREATE_IN, CREATE_OUT)                           \
    class CodecInfo ## Class : public CodecInfo {                                           \
    public:                                                                                 \
        CodecInfo ## Class(const QString& n, int i) : CodecInfo(n, i) { }                   \
        virtual Codec *createDecoder(QObject *parent) const { return CREATE_IN (parent); }  \
        virtual Codec *createEncoder(QObject *parent) const { return CREATE_OUT (parent); } \
    };                                                                                      \
    static const ArchiveInfo ## Class s_export ## Class(QLatin1String(name), id);

#define EXPORT_CODEC_READONLY(name, id, Out)    \
    EXPORT_CODEC_BASE(Out, name, id, 0; (void), new Out)

#define EXPORT_CODEC(name, id, In, Out)         \
    EXPORT_CODEC_BASE(Out, name, id, new In, new Out)

// the client API for the handler registry
Volume * CreateVolume(const QString& mimeType, const QString& file, QObject *parent = 0);
Archive * CreateArchive(const QString& type, Volume *parent);
Codec * CreateEncoder(const QString& type, QObject *parent = 0);
Codec * CreateEncoder(uint id, QObject *parent = 0);
Codec * CreateDecoder(const QString& type, QObject *parent = 0);
Codec * CreateDecoder(uint id, QObject *parent = 0);

};

#endif
