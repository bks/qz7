// Zip/Update.h

#ifndef __ZIP_UPDATE_H
#define __ZIP_UPDATE_H

#include "interfaces/ICoder.h"
#include "interfaces/IArchive.h"

#include "ZipCompressionMode.h"
#include "ZipIn.h"

namespace NArchive
{
namespace NZip {

struct CUpdateRange {
    quint64 Position;
    quint64 Size;
    CUpdateRange() {};
    CUpdateRange(quint64 position, quint64 size): Position(position), Size(size) {};
};

struct CUpdateItem {
    bool NewData;
    bool NewProperties;
    bool IsDirectory;
    int IndexInArchive;
    int IndexInClient;
    quint32 Attributes;
    quint32 Time;
    quint64 Size;
    QByteArray Name;
    // bool Commented;
    // CUpdateRange CommentRange;
    CUpdateItem(): Size(0) {}
};

HRESULT Update(
    const QList<CItemEx *> &inputItems,
    const QList<CUpdateItem *> &updateItems,
    ISequentialOutStream *seqOutStream,
    CInArchive *inArchive,
    CCompressionMethodMode *compressionMethodMode,
    IArchiveUpdateCallback *updateCallback);

}
}

#endif
