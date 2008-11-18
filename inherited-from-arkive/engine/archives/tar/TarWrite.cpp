#include "TarArchive.h"
#include "TarFormat.h"

#include "kernel/HandlerRegistry.h"

#include <QtCore/QByteArray>
#include <QtCore/QDateTime>
#include <QtCore/QDebug>
#include <QtCore/QString>
#include <QtCore/QVariant>

namespace ArchiveEngine {
namespace Tar {

bool TarArchive::updateTo(QIODevice *target, const QMap<quint64, ArchiveItem>& updateItems,
        const QList<ArchiveItem>& newItems)
{
    for (quint32 index = 0; index < size(); index++) {
    }

    return false;
}

// reading support (i.e. constructors) for SparseFileRecord is in TarRead.cpp
QByteArray SparseFileRecord::toByteArray() const
{
    return QByteArray();
}

// close namespaces
}
}
