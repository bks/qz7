#ifndef LIMITEDIODEVICE_H
#define LIMITEDIODEVICE_H

#include <QtCore/QIODevice>

class LimitedIoDevice : public QIODevice {
    Q_OBJECT

public:
    LimitedIoDevice(QObject *parent = 0);

    void setBackingDevice(QIODevice *backingDevice);
    void setLimit(qint64 limit);

    virtual bool open(QIODevice::OpenMode mode);
    virtual bool isSequential() const;
    virtual bool seek(qint64 to);
    virtual qint64 size() const;

protected:
    virtual qint64 readData(char *data, qint64 maxSize);
    virtual qint64 writeData(const char *data, qint64 maxSize);

private:
    QIODevice *mBackingDevice;
    qint64 mLimit;
    qint64 mBase;
};

#endif
