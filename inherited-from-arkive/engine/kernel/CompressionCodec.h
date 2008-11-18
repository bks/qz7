#ifndef CODER_H
#define CODER_H

#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QVariant>
#include <QtCore/QString>

class QIODevice;

class CompressionCodec {
public:
    CompressionCodec() { }
    virtual ~CompressionCodec();

    virtual void setInputDevice(QIODevice *) = 0;
    virtual void setOutputDevice(QIODevice *) = 0;
    virtual bool code() = 0;
    virtual bool setProperties(const QMap<QString, QVariant>& properties) = 0;
    virtual bool setProperties(const QByteArray& serializedProperties) = 0;
    virtual QByteArray serializeProperties() = 0;
    virtual QString errorString() const = 0;
};

Q_DECLARE_INTERFACE(CompressionCodec, "org.kde.Arkive.CompressionCodec/1.0")

class CompressionCodecBase : public QObject, public CompressionCodec {
    Q_OBJECT
    Q_INTERFACES(CompressionCodec)

public:
    CompressionCodecBase(QObject *parent = 0) : QObject(parent), CompressionCodec() {}
    virtual ~CompressionCodecBase();
    virtual void setInputDevice(QIODevice *dev);
    virtual void setOutputDevice(QIODevice *dev);
    virtual bool code() = 0;
    virtual bool setProperties(const QMap<QString, QVariant>& properties) = 0;
    virtual bool setProperties(const QByteArray& serializedProperties) = 0;
    virtual QByteArray serializeProperties() = 0;
    virtual QString errorString() const;

    QIODevice * inputDevice() { return mInputDevice; }
    QIODevice * outputDevice() { return mOutputDevice; }

signals:
    void compressionRatio(qint64 bytesIn, qint64 bytesOut);

protected:
    void setErrorString(const QString& str);
    bool errorCorrupted();
    bool errorCorrupted(const QString& details);
    bool errorNoMemory();
    bool errorIoDevice(const QIODevice *dev);

private:
    QString mErrorString;
    QIODevice *mInputDevice;
    QIODevice *mOutputDevice;
};

#endif
