#include "SingleFileVolume.h"

#include "qz7/Stream.h"

#include <QtCore/QFile>

namespace qz7 {

SingleFileVolume::SingleFileVolume(const QString& file, QObject *parent)
    : Volume(file, parent), mFile(file)
{
}

SeekableReadStream * SingleFileVolume::openFile(uint n)
{
    if (n > 0)
        return 0;

    QFile *file = new QFile(mFile, this);

    if (file->open(QIODevice::ReadOnly))
        return new QioSeekableReadStream(file);

    delete file;
    return 0;
}

}
