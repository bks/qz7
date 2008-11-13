#ifndef QZ7_CODEC_H
#define QZ7_CODEC_H

#include <QtCore/QObject>

class QByteArray;
class QIODevice;
class QString;
class QVariant;

namespace qz7 {

class Codec : public QObject {
    Q_OBJECT
public:
    virtual ~Codec() { }

    // this throws an exception on error
    virtual void stream(QIODevice *from, QIODevice *to) = 0;

    // these return false or empty on failure or error
    virtual bool setProperty(const QString& property, const QVariant& value) = 0;
    virtual QVariant property(const QString& property) const = 0;
    virtual QByteArray serializeProperties() const = 0;
    virtual bool applySerializedProperties(const QByteArray& serializedProperties) = 0;

signals:
    void progress(quint64 bytesIn, quint64 bytesOut);
};

};

#endif
