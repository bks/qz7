#ifndef QZ7_VOLUME_H
#define QZ7_VOLUME_H

#include <QtCore/QObject>

class QString;

namespace qz7 {

class SeekableReadStream;

class Volume : public QObject {
    Q_OBJECT
public:
    Volume(const QString& memberFile, QObject *parent = 0);
    virtual ~Volume();

    // open the (0-based) n'th file in the volume; this function may need to
    // perform GUI callouts or the like
    virtual SeekableReadStream * openFile(uint n) = 0;
};

};

#endif
