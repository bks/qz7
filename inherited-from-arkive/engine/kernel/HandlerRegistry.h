#ifndef HANDLERREGISTRY_H
#define HANDLERREGISTRY_H

#include <QtCore/QtGlobal>
#include <QtCore/QByteArray>

#include "kernel/arkive_export.h"

class QObject;

class ArchiveHandlerInfo;
class CodecHandlerInfo;

class Archive;
class CompressionCodec;

ARKIVE_EXPORT void RegisterArchive(const ArchiveHandlerInfo* info);
ARKIVE_EXPORT void RegisterCodec(const CodecHandlerInfo* info);

ARKIVE_EXPORT Archive * CreateArchiveForName(const QByteArray& name, QObject *parent);
ARKIVE_EXPORT Archive * CreateArchiveForMimeType(const QByteArray& mimeType, QObject *parent);

typedef quint32 CompressionMethodId;

CompressionCodec * CreateCoderForName(const QByteArray& name, QObject *parent);
CompressionCodec * CreateCoderForId(CompressionMethodId id, QObject *parent);
CompressionCodec * CreateDecoderForName(const QByteArray& name, QObject *parent);
CompressionCodec * CreateDecoderForId(CompressionMethodId id, QObject *parent);

class ArchiveHandlerInfo {
public:
    enum { ReadOnly, ReadWrite };

    ArchiveHandlerInfo(const char *name_, const char *mime_)
        : mName(name_), mMimeType(mime_) { RegisterArchive(this); }
    virtual ~ArchiveHandlerInfo() {}

    QByteArray name() const { return mName; }
    QByteArray mimeType() const { return mMimeType; }
    virtual Archive * createArchive(QObject *parent) const = 0;
    virtual bool canWrite() const = 0;

private:
    QByteArray mName;
    QByteArray mMimeType;
};

class CodecHandlerInfo {
public:
    CodecHandlerInfo(const char *name_, CompressionMethodId methodId_)
        : mName(name_), mMethodId(methodId_) { RegisterCodec(this); }
    virtual ~CodecHandlerInfo() {}

    QByteArray name() const { return mName; }
    quint64 methodId() const { return mMethodId; }
    virtual CompressionCodec * createDecoder(QObject *parent) const = 0;
    virtual CompressionCodec * createEncoder(QObject *parent) const = 0;

private:
    QByteArray mName;
    CompressionMethodId mMethodId;
};

#define EXPORT_ARCHIVE(name, mime, writable, Class)                         \
    class ArchiveHandlerInfo ## name : public ArchiveHandlerInfo {          \
    public:                                                                 \
        ArchiveHandlerInfo ## name() : ArchiveHandlerInfo(#name, mime) {}   \
        virtual Archive * createArchive(QObject *parent) const { return new Class(parent); }  \
        virtual bool canWrite() const { return (writable == ReadWrite); }   \
    };                                                                      \
    static ArchiveHandlerInfo ## name s_export ## name;

#define EXPORT_CODEC(name, id, In, Out)                                     \
    class CodecHandlerInfo ## name : public CodecHandlerInfo {              \
    public:                                                                 \
        CodecHandlerInfo ## name() : CodecHandlerInfo(#name, id) {}         \
        virtual CompressionCodec * createDecoder(QObject *parent) const { return new In(parent); }  \
        virtual CompressionCodec * createEncoder(QObject *parent) const { return new Out(parent); } \
    };                                                                      \
    static CodecHandlerInfo ## name s_export ## name;

#define EXPORT_CODEC_READONLY(name, id, In)                                 \
    class CodecHandlerInfo ## name : public CodecHandlerInfo {              \
    public:                                                                 \
        CodecHandlerInfo ## name() : CodecHandlerInfo(#name, id) {}         \
        virtual CompressionCodec * createDecoder(QObject *parent) const { return new In(parent); }  \
        virtual CompressionCodec * createEncoder(QObject *parent) const { return 0; }               \
    };                                                                      \
    static CodecHandlerInfo ## name s_export ## name;

#endif
