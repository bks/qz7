#ifndef TARARCHIVE_H
#define TARARCHIVE_H

#include "kernel/Archive.h"

#include <QtCore/QList>

namespace ArchiveEngine {
namespace Tar {

namespace TarItem {
enum {
    HeaderStartPosition = ArchiveItem::ArchivePrivate + 1,
    SparseFileStructure,
    GnuSparseFormat
};
}

namespace TarFormat {
enum Format {
    Unknown,    //< the tar format hasn't been determined yet
    Basic,      //< a traditional Unix V7 format file
    GNU,        //< GNU tar format
    STar,       //< Schilling 'star' format
    USTar,      //< POSIX.1-1988 Unix Standard Tar format
    PAX         //< POSIX.1-2001 'pax interchange format' extension of USTar
};
}

class TarArchive : public ArchiveBase {
    Q_OBJECT

public:
    TarArchive(QObject *parent = 0);
    virtual ~TarArchive();

    virtual bool open(QIODevice *file, const quint64 maxCheckStartPosition);
    virtual bool extract(const quint64 index, QIODevice *target);
    virtual bool canUpdate() const;
    virtual bool setUpdateProperties(const QMap<QByteArray, QByteArray>& props);
    virtual bool updateTo(QIODevice *target,
        const QMap<quint64, ArchiveItem>& updateItems, const QList<ArchiveItem>& newItems);

private:
    enum ItemStatus { Ok, Error, AtEnd };

    // readOneItem() takes a buffer as an input so that we can reuse
    // a single buffer many times, without tying it up in a member variable
    ItemStatus readOneItem(QByteArray& buffer);

    // read a single object with a name in the filesystem
    bool readNamedObject(const QByteArray& buffer, ArchiveItem::ItemType type);

    // read a block of PAX-style attributes to the target item
    bool readAttributes(const QByteArray& buffer, ArchiveItem *item);

    // read out the data for a given header block
    bool readObjectData(const QByteArray& buffer, QByteArray *data);

    // read the sparse file structure of an item
    bool readSparseStructure(const QByteArray& buffer);

    // write out a single archive item
    bool writeOneItem(const ArchiveItem& item);

    // tar files are a stream of records, each of which can either be an archive
    // item itself or be a chunk of data that applies either to the next item or
    // until revoked
    ArchiveItem mGlobalTemplateItem;
    ArchiveItem mCurrentItem;
    TarFormat::Format mFormat;
    TarFormat::Format mWriteFormat;
};

class SparseFileRecord {
public:
    SparseFileRecord(const QByteArray& data);
    SparseFileRecord(const quint64 offset_, const quint64 size_);
    qint64 offset() const { return mOffset; }
    qint64 size() const { return mSize; }
    QByteArray toByteArray() const;

private:
    qint64 mOffset, mSize;
};

}
}

Q_DECLARE_TYPEINFO(ArchiveEngine::Tar::SparseFileRecord, Q_MOVABLE_TYPE);
typedef QList<ArchiveEngine::Tar::SparseFileRecord> SparseFileRecordList;
Q_DECLARE_METATYPE(SparseFileRecordList)

#endif
