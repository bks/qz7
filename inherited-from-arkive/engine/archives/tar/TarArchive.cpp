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

TarArchive::TarArchive(QObject *parent)
    : ArchiveBase(parent), mFormat(TarFormat::Unknown), mWriteFormat(TarFormat::PAX)
{
}

TarArchive::~TarArchive()
{
}

bool TarArchive::open(QIODevice *file, const quint64 maxCheckStartPosition)
{
    pushInputDevice(file);
    mCurrentItem.setProperty(TarItem::HeaderStartPosition, qint64(0));

    while (true) {
        QByteArray buffer;
        ItemStatus status = readOneItem(buffer);

        if (status == Error)
            return false;
        if (status == AtEnd)
            break;
    }
    return true;
}

bool TarArchive::canUpdate() const
{
    return true;
}

bool TarArchive::setUpdateProperties(const QMap<QByteArray, QByteArray>& props)
{
    for (QMap<QByteArray, QByteArray>::const_iterator it = props.constBegin(); 
        it != props.constEnd(); it++) {
        if (it.key() == "TarFormat") {
            QByteArray f = it.value();
            if (f == "PAX") {
                mWriteFormat = TarFormat::PAX;
            } else if (f == "UStar") {
                mWriteFormat = TarFormat::USTar;
            } else {
                setErrorString(tr("unsupported TAR format '%1'").arg(QString::fromAscii(f)));
                return false;
            }
        } else {
            setErrorString(tr("unknown update property '%1'").arg(QString::fromAscii(it.key())));
            return false;
        }
    }

    return true;
}

// close namespaces
}
}

EXPORT_ARCHIVE(tar, "application/x-tar", ReadOnly, ArchiveEngine::Tar::TarArchive);
