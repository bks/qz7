#include "ZipArchive.h"
#include "ZipItem.h"

namespace Zip {

bool Archive::open(QIODevice *file, const quint64 maxStartPosition)
{
    pushInputStream(file);

    if (!readHeader(maxStartPosition))
        return false;

    if (!readLocalHeaders())
        return false;

    if (!readCentralDirectory())
        return false;

    return true;
}

bool Archive::close()
{
    return true;
}

bool Archive::extract(quint64 index, ::Archive::Mode mode, QIODevice *target)
{
    const ArchiveItem& item = item(index);

    return false;
}

bool Archive::canUpdate()
{
    return true;
}

bool Archive::updateTo(QIODevice *target, const QMap<quint64, ArchiveItem>& replacements, 
    const QList<ArchiveItem>& additions)
{
    return false;
}
