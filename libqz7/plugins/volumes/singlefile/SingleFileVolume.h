#ifndef QZ7_SINGLEFILEVOLUME_H
#define QZ7_SINGLEFILEVOLUME_H

#include "qz7/Volume.h"

#include <QtCore/QString>

namespace qz7 {

class SeekableReadStream;

class SingleFileVolume : public Volume {
    Q_OBJECT
public:
    SingleFileVolume(const QString& file, QObject *parent = 0);

    virtual SeekableReadStream * openFile(uint n);

private:
    QString mFile;
};

} // namespace qz7

#endif
