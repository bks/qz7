#ifndef ARCHIVE_H
#define ARCHIVE_H

#include <QtCore/QObject>
#include <QtCore/QSharedDataPointer>
#include <QtCore/QString>
#include <QtCore/QVariant>

#include "kernel/arkive_export.h"

class QIODevice;
class CompressionCodec;

/**
 * An ArchiveInfo describes an archive. It is a set of properites; which properties are
 * present depends on the archive type.
 */
class ARKIVE_EXPORT ArchiveInfo {
public:
    enum Property {
        Label,
        Comment,
        IsSolid,
        IsVolume,
        IsEncrypted,
        NumBlocks,
        NumVolumes
    };

    ArchiveInfo();

    QVariant property(int p) const;
    bool hasProperty(int p) const;
    void setProperty(int p, const QVariant& val);
    void removeProperty(int p);

private:
    class Private : public QSharedData {
    public:
        Private() : QSharedData(), properties() { }
        Private(const Private& other) : QSharedData(other), properties(other.properties) { }
        ~Private() { }
    
        QMap<int, QVariant> properties;
    };
    QSharedDataPointer<Private> d;
};

/**
 * An ArchiveItem defines an entry in an archive. It is effectively a set of properties;
 * which properties are present depends on the archive type.
 */
class ARKIVE_EXPORT ArchiveItem {
public:
    /// The possible properties available in an ArchiveItem
    enum Property {
        Path,
        Type,
        UncompressedSize,
        CompressedSize,
        CreationTime,
        LastAccessTime,
        LastWriteTime,
        CRC,
        Comment,
        CompressionMethod,
        UserName,
        GroupName,
        UserId,
        GroupId,
        UnixMode,
        LinkTarget,
        MajorDeviceNumber,
        MinorDeviceNumber,
        Win32Attributes,
        SplitBefore,
        SplitAfter,
        IsEncrypted,
        HostOS,
        Position,
        Prefix,
        Links,
        Stream,

#if 0
        kpidTotalSize = 0x1100,
        kpidFreeSpace,
        kpidClusterSize,
        kpidVolumeName,
    
        kpidLocalName = 0x1200,
        kpidProvider,
    
        kpidUserDefined = 0x10000
#endif
        ArchivePrivate = 0x10000,
        UserPrivate = 0x20000,
        Last = 0xffffffff
    };

    enum ItemType {
        ItemTypeFile = 0,
        ItemTypeDirectory,
        ItemTypeSymbolicLink,
        ItemTypeHardLink,
        ItemTypeFifo,
        ItemTypeBlockDevice,
        ItemTypeCharacterDevice
    };

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

        UnknownHostOperatingSystem = 0xffffffff
    };

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

    ArchiveItem();

    bool isValid() const;
    QVariant property(int p) const;
    bool hasProperty(int p) const;
    void setProperty(int p, const QVariant& val);
    void removeProperty(int p);

private:
    class Private : public QSharedData {
    public:
        Private() : QSharedData(), properties() { }
        Private(const Private& other) : QSharedData(other), properties(other.properties) { }
        ~Private() { }
    
        QMap<int, QVariant> properties;
    };
    
    QSharedDataPointer<Private> d;
};

Q_DECLARE_METATYPE(QIODevice *)

/**
 * Archive is the interface class that defines an archive plugin. All archives are additionally
 * expected to provide the same set of signals as ArchiveBase.
 */
class Archive {
public:
    virtual ~Archive() { }

    enum ProgressUnit { UnitBytes, UnitFiles };

    virtual bool open(QIODevice *file, const quint64 maxCheckStartPosition) = 0;
    virtual ArchiveInfo archiveInfo() const = 0;
    virtual quint64 size() const = 0;
    virtual ArchiveItem itemAt(const quint64 index) const = 0;
    virtual bool extract(const quint64 index, QIODevice *target) = 0;

    virtual bool canUpdate() const = 0;
    virtual bool setUpdateProperties(const QMap<QByteArray, QByteArray>& props) = 0;
    /**
     * updateTo will look for each item's data stream at item.property(ArchiveItem::Stream)
     * if it is set: NULL means reuse the old stream or fail if there isn't one;
     * if it is unset, the Archive will emit needStreamForItem(item) and expect it to be filled
     * in by the slot connected to it. The stream will be closed and deleted after the 
     * Archive is done with it. An invalid ArchiveItem in @p updateItems means to delete
     * the item at that index. Note that running updateTo() does NOT change this Archive:
     * the user will have to open an new archive on the target to see the changes.
     */
    virtual bool updateTo(QIODevice *target,
        const QMap<quint64, ArchiveItem>& updateItems, const QList<ArchiveItem>& newItems) = 0;

    /// @return a string describing the last error to occur
    virtual QString errorString() const = 0;
};

Q_DECLARE_INTERFACE(Archive, "org.kde.Arkive.Archive/1.0")

/**
 * A concrete utility base class for Archives.
 */
class ArchiveBase : public QObject, public Archive {
    Q_OBJECT
    Q_INTERFACES(Archive)

public:
    ArchiveBase(QObject *parent = 0);
    virtual ~ArchiveBase();

    virtual bool open(QIODevice *file, const quint64 maxCheckStartPosition) = 0;
    virtual ArchiveInfo archiveInfo() const;
    virtual quint64 size() const;
    virtual ArchiveItem itemAt(const quint64 index) const;
    virtual bool extract(const quint64 index, QIODevice *target) = 0;
    virtual bool canUpdate() const;
    virtual bool setUpdateProperties(const QMap<QByteArray, QByteArray>& props) = 0;
    virtual bool updateTo(QIODevice *target, const QMap<quint64, ArchiveItem>& updateItems,
        const QList<ArchiveItem>& newItems) = 0;
    virtual QString errorString() const;

signals:
    void openProgress(qint64 completed, qint64 total, ProgressUnit unit);
    void extractProgress(qint64 completedBytes, qint64 totalBytes);
    void compressionRatio(qint64 bytesIn, qint64 bytesOut);
    void updateProgress(quint64 index, quint64 completedBytes, quint64 totalBytes);
    void formatWarning(const QString& warningString);
    void unhandledFormatExtension(const QString& archiveFormat, const QString& extension);
    /// the Archive needs a stream for the item @p item -- either fill in *@p stream
    /// with the QIODevice or zero to denote that it should use the preexisting data
    /// for @p item
    /// this must be connected with a Qt::DirectConnection
    void needStreamForItem(const ArchiveItem& item, QIODevice **stream);

protected:
    enum SeekWhence {
        RelativeToPosition,
        RelativeToBeginning,
        RelativeToEnd
    };

    void addItem(const ArchiveItem& item_);
    void setInfo(const ArchiveInfo& info_);
    ArchiveInfo& archiveInfo();
    ArchiveItem& itemAt(quint64 index);
    void pushInputDevice(QIODevice *stream);
    QIODevice * popInputDevice();
    QIODevice *inputDevice() const;

    bool readByte(quint8 *b);
    bool readUInt16LE(quint16 *s);
    bool readUInt32LE(quint32 *l);
    bool readUInt64LE(quint64 *ll);
    bool readUInt16BE(quint16 *s);
    bool readUInt32BE(quint32 *l);
    bool readUInt64BE(quint64 *ll);
    bool readBuffer(QByteArray *buffer, quint32 size);
    bool readOptionalBuffer(QByteArray *buffer, quint32 size);
    bool readString(QByteArray *buffer, quint32 maxSize = 0xffffffffU);
    bool skipBytes(quint32 bytes);

    quint64 currentPosition();
    bool seek(qint64 position, SeekWhence whence, qint64 *resultingPosition);

    void setOutputDevice(QIODevice *stream);
    QIODevice *outputDevice() const;

    QIODevice *streamForItem(const ArchiveItem& item);

    bool writeByte(quint8 b);
    bool writeUInt16LE(quint16 s);
    bool writeUInt32LE(quint32 l);
    bool writeUInt64LE(quint64 ll);
    bool writeUInt16BE(quint16 s);
    bool writeUInt32BE(quint32 l);
    bool writeUInt64BE(quint64 ll);
    bool writeBuffer(const QByteArray& buf);
    bool writeBuffer(const QByteArray& buf, quint32 size);
    bool writeBytes(const quint8* buf, int size);

    void setErrorString(const QString& str);

    /// sets an error string and returns false
    bool errorBadMagic(const QString& type);
    bool errorTruncated();
    bool errorBadCrc();
    bool errorOutOfMemory();
    bool errorIoDevice(QIODevice *dev);
    bool errorCompression(CompressionCodec *codec);
    bool errorDecompression(CompressionCodec *codec);

private:
    ArchiveInfo mInfo;
    QList<ArchiveItem> mItems;
    QList<QIODevice *> mStreams;
    QIODevice *mOutputStream;
    QString mErrorString;
};

#endif
