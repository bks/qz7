#ifndef BZIP2ARCHIVE_H
#define BZIP2ARCHIVE_H

#include "kernel/Archive.h"

namespace ArchiveEngine {
namespace BZip2 {

class BZip2Archive : public ArchiveBase {
    Q_OBJECT

public:
    BZip2Archive(QObject *parent = 0);
    virtual ~BZip2Archive();

    virtual bool open(QIODevice *file, const quint64 maxCheckStartPosition);
    virtual bool extract(const quint64 index, QIODevice *target);
    virtual bool canUpdate() const;
    virtual bool setUpdateProperties(const QMap<QByteArray, QByteArray>& props);
    virtual bool updateTo(QIODevice *target,
        const QMap<quint64, ArchiveItem>& updateItems, const QList<ArchiveItem>& newItems);
};

}
}
#endif
