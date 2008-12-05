#ifndef QZ7_CODEC_H
#define QZ7_CODEC_H

#include <QtCore/QObject>

class QByteArray;
class QString;
class QVariant;

namespace qz7 {

class ReadStream;
class WriteStream;

class Codec : public QObject {
    Q_OBJECT
public:
    Codec(QObject *parent = 0);
    virtual ~Codec();

    // returns true on success, false on interrupted or error
    virtual bool stream(ReadStream *from, WriteStream *to) = 0;

    // returns QString() on no error (this can be used to distinguish interruption
    // from error)
    virtual QString errorString() const = 0;

    // interupt() is meant to be called from possibly a different thread than the one
    // the Codec is executing on
    virtual void interrupt() = 0;

    // these return false or empty on failure or error
    virtual bool setProperty(const QString& property, const QVariant& value) = 0;
    virtual QVariant property(const QString& property) const = 0;
    virtual QByteArray serializeProperties() const = 0;
    virtual bool applySerializedProperties(const QByteArray& serializedProperties) = 0;

signals:
    void progress(quint64 bytesIn, quint64 bytesOut);
};

}

#endif
