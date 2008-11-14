#ifndef QZ7_ARCHIVE_H
#define QZ7_ARCHIVE_H

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QSharedDataPointer>
#include <QtCore/QString>
#include <QtCore/QVariant>

namespace qz7 {

class ReadStream;
class SeekableReadStream;
class WriteStream;

}

Q_DECLARE_METATYPE(qz7::ReadStream *)

namespace qz7 {

class ArchiveItem {
public:
    ArchiveItem() : d(0) { }
    ArchiveItem(const QString& path, const QString& name) : d(new Private) { d->path = path; d->name = name; }
    ArchiveItem(const ArchiveItem& other) : d(other.d) { }

    const ArchiveItem& operator=(const ArchiveItem& other) { if (&other == this) return *this; d = other.d; }

    bool isValid() const { return (d != 0); }

    enum ItemType {
        ItemTypeFile = 0,
        ItemTypeDirectory,
        ItemTypeSymbolicLink,
        ItemTypeHardLink,
        ItemTypeFifo,
        ItemTypeBlockDevice,
        ItemTypeCharacterDevice
    };
    ItemType itemType() const { return d->type; }
    void setItemType(ItemType t) { d->type = t; }

    enum CompressionMethod {
        // CompressionMethod < CompressionMethodStore is the literal value from
        // the file, and it means the Archive doesn't understand it...
        CompressionMethodStore = 0x10000,
        CompressionMethodDeflate,
        CompressionMethodImplode,
        CompressionMethodShrink,
        CompressionMethodLzma,
        CompressionMethodBzip2,
        CompressionMethodZ,
        CompressionMethodArj,
        CompressionMethodPpmd,
        CompressionMethodQuantum,
        CompressionMethodRarV1,
        CompressionMethodRarV2,
        CompressionMethodRarV3
    };
    CompressionMethod compressionMethod() const { return d->compressionMethod; }
    void setCompressionMethod(CompressionMethod m) { d->compressionMethod = m; }

    enum HostOperatingSystem {
        Acorn,
        Amiga,
        Atari,
        BeOS,
        CPM,
        MacClassic,
        MacOSX,
        MsDos,
        MVS,
        OS_2,
        OS400,
        QDos,
        Tandem,
        THEOS,
        Tops20,
        Unix,
        VM_CMS,
        VMS,
        Windows9x,
        WindowsNT,
        Z_System,
        UnknownHostOperatingSystem = 0x1f
    };
    HostOperatingSystem hostOs() const { return d->hostOs; }
    void setHostOs(HostOperatingSystem h) { d->hostOs = h; }

    quint64 uncompressedSize() const { return d->uncompressedSize; }
    void setUncompressedSize(quint64 sz) { d->uncompressedSize = sz; }

    quint64 compressedSize() const { return d->compressedSize; }
    void setCompressedSize(quint64 sz) { d->compressedSize = sz; }

    quint64 position() const { return d->position; }
    void setPosition(quint64 pos) { d->position = pos; }

    QString path() const { return d->path; }
    QString name() const { return d->name; }

    quint32 crc() const { return d->crc; }
    void setCrc(quint32 crc) { d->crc = crc; }

    bool isWhiteout() const { return d->whiteout; }
    void setWhiteout(bool w = true) { d->whiteout = w; }

    bool isEncrypted() const { return d->encrypted; }
    void setEncrypted(bool w = true) { d->encrypted = w; }

    bool hasProperty(const QString& prop) const { return d->properties.contains(prop); }
    QVariant property(const QString& prop) const {
        QHash<QString, QVariant>::const_iterator it = d->properties.find(prop);
        return (it != d->properties.end()) ? *it : QVariant();
    }
    void setProperty(const QString& prop, const QVariant& val) { d->properties[prop] = val; }

    ReadStream *stream() const {
        QVariant v = property(QLatin1String("stream"));

        if (v.isValid())
            return v.value<ReadStream *>();
        return 0;
    }

    void setStream(ReadStream *stream) {
        setProperty(QLatin1String("stream"), QVariant::fromValue(stream));
    }

    QString userName() const {
        QVariant v = property(QLatin1String("username"));

        if (v.isValid())
            return v.toString();
        return QString();
    }

    uint uid() const {
        QVariant v = property(QLatin1String("uid"));

        if (v.isValid())
            return v.toUInt();
        return (uint)(-1);
    }

    QString groupName() const {
        QVariant v = property(QLatin1String("groupname"));

        if (v.isValid())
            return v.toString();
        return QString();
    }

    uint gid() const {
        QVariant v = property(QLatin1String("gid"));

        if (v.isValid())
            return v.toUInt();
        return (uint)(-1);
    }

    void setIds(uint uid, uint gid) {
        setProperty(QLatin1String("uid"), QVariant::fromValue(uid));
        setProperty(QLatin1String("gid"), QVariant::fromValue(gid));
    }

    void setIds(QString userName, uint uid, QString groupName, uint gid) {
        setProperty(QLatin1String("username"), QVariant::fromValue(userName));
        setProperty(QLatin1String("uid"), QVariant::fromValue(uid));
        setProperty(QLatin1String("groupname"), QVariant::fromValue(groupName));
        setProperty(QLatin1String("gid"), QVariant::fromValue(gid));
    }

private:
    struct Private : QSharedData {
        Private() : QSharedData(), whiteout(0), encrypted(0) { }
        Private(const Private &other) : QSharedData(other),
            uncompressedSize(other.uncompressedSize), compressedSize(other.compressedSize),
            position(other.position), path(other.path), properties(other.properties),
            compressionMethod(other.compressionMethod), hostOs(other.hostOs), type(other.type),
            whiteout(other.whiteout), encrypted(other.encrypted)
            { }
        ~Private() { }

        quint64 uncompressedSize;
        quint64 compressedSize;
        quint64 position;
        QString path;
        QString name;
        QHash<QString, QVariant> properties;
        quint32 crc;
        CompressionMethod compressionMethod : 17;
        HostOperatingSystem hostOs : 5;
        ItemType type : 4;
        uint whiteout : 1;
        uint encrypted : 1;
    };

    QSharedDataPointer<Private> d;
};

class Archive : public QObject {
    Q_OBJECT
public:
    Archive(Volume *parent) { }
    virtual ~Archive() { }

    // the following return false on error
    virtual bool open() = 0;
    bool extractTo(uint id, QIODevice *target);
    virtual bool extractTo(uint id, WriteStream *target) = 0;

    // these only work if canWrite() is true, and only take effect on writeTo()
    virtual bool canWrite() const = 0;
    bool writeTo(QIODevice *target);
    virtual bool writeTo(WriteStream *target) = 0;
    void replaceItem(uint id, const ArchiveItem& item);
    void deleteItem(uint id);
    uint appendItem(const ArchiveItem& item);

    QString errorString() const;

    uint count() const;
    ArchiveItem item(uint id) const;
    QVariant property(const QString& prop);

signals:
    void totalItems(uint nr);
    void itemLoaded(uint id, const ArchiveItem& item);
    void progress(quint64 bytesIn, quint64 bytesOut);
    void itemWritten(uint id, const ArchiveItem& item, qint64 bytesInt, quint64 bytesOut);
    void archiveFormatWarning(const QString& archiveFormat, const QString& text);
    void unhandledFormatExtension(const QString& archiveFormat, const QString& extension);

protected:
    void addItem(const ArchiveItem& item);
    void setProperty(const QString& prop, const QVariant& val);
    void setErrorString(const QString& str);

    QList<ArchiveItem> normalizedItems() const;

private:
    QList<ArchiveItem> mItems;
    QMap<uint, ArchiveItem> mModifications;
    QHash<QString, QVariant> mProperties;
    QString mErrorString;
};

}

#endif
