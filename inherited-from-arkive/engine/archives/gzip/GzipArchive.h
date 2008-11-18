#ifndef GZIPARCHIVE_H
#define GZIPARCHIVE_H

#include "kernel/Archive.h"

namespace ArchiveEngine {
namespace Gzip {

class GzipArchive : public ArchiveBase {
    Q_OBJECT

public:
    GzipArchive(QObject *parent = 0);
    virtual ~GzipArchive();

    virtual bool open(QIODevice *file, const quint64 maxCheckStartPosition);
    virtual bool extract(const quint64 index, QIODevice *target);
    virtual bool canUpdate() const;
    virtual bool setUpdateProperties(const QMap<QByteArray, QByteArray>& props);
    virtual bool updateTo(QIODevice *target, const QMap<quint64, ArchiveItem>& updateItems,
        const QList<ArchiveItem>& newItems);

private:
    bool readHeader();
    bool writeHeader(const ArchiveItem& theItem);
};

namespace GzipItem {
    enum ItemProperties {
        ItemExtra = ArchiveItem::ArchivePrivate + 1
    };
}

}
}

#endif
