#ifndef ZIPARCHIVE_H
#define ZIPARCHIVE_H

#include "interfaces/Archive.h"

namespace Zip {

class ExtraBlock;

class Archive : public ::ArchiveBase {
    Q_OBJECT

public:
    virtual bool open(QIODevice *file, const quint64 maxCheckStartPosition);
    virtual bool close();
    virtual void extract(const quint64 index, ::Archive::Mode mode, QIODevice *target);
    virtual bool canUpdate() const;
    virtual void updateTo(QIODevice *target, const QMap<quint64, ArchiveItem>& updateItems,
        const QList<ArchiveItem>& newItems);

private:
    bool readHeader();
    bool readOneLocalItem(quint64 n);
    bool readOneCentralItem(quint64 n);
    bool readLocalHeaders();
    bool readCentralDirectory();
    ExtraBlock readExtraBlock();

    bool writeOneLocalItem(quint64 n);
    bool writeOneCentralItem(quint64 n);
    bool writeLocalItems();
    bool writeCentralDirectory();
    bool writeExtraBlock(const ExtraBlock&);

    quint32 mCurrentMagic;
    quint64 mStartPosition;
};

}

#endif
