#ifndef QZ7_VOLUME_H
#define QZ7_VOLUME_H

#include <QtCore/QObject>

class QIODevice;
class QString;

namespace qz7 {

class Archive;

class Volume : public QObject {
    Q_OBJECT
public:
    virtual ~Volume() { }
    // Volume(const QString& memberFile)
    virtual Archive *open() = 0;

protected:
    virtual uint filesInVolume() const = 0;
    virtual QIODevice * openFile(uint n) = 0;

    // look for a file in the volume with an id greater than filesInVolume()
    // FIXME: should this be merged with openFile()? should filesInVolume() even exist?
    virtual QIODevice * findFile(uint n) = 0;
};

};

#endif
