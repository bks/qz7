#ifndef QZ7_STREAM_H
#define QZ7_STREAM_H

#include <QtCore/QtGlobal>

class QString;
class QIODevice;

namespace qz7 {

class ReadStream {
public:
    virtual ~ReadStream();
    virtual bool read(quint8 *buffer, int bytes) = 0;
    virtual int readSome(quint8 *buffer, int minBytes, int maxBytes) = 0;
    virtual bool skipForward(qint64 bytes) = 0;
    virtual bool atEnd() const = 0;
    virtual qint64 bytesRead() const = 0;   // meant for I/O statistics: doesn't include actually skipped bytes
    virtual QString errorString() const = 0;
};

class WriteStream {
public:
    virtual ~WriteStream();
    virtual bool write(const quint8 *buffer, int bytes) = 0;
    virtual void flush() = 0;
    virtual qint64 bytesWritten() const = 0;
    virtual QString errorString() const = 0;
};

class SeekableReadStream : public ReadStream {
public:
    virtual qint64 size() const = 0;
    virtual qint64 pos() const = 0;
    virtual bool setPos(qint64 pos) = 0;
};

class QioReadStream : public ReadStream {
public:
    QioReadStream(QIODevice *dev);
    virtual ~QioReadStream();
    virtual bool read(quint8 *buffer, int bytes);
    virtual int readSome(quint8 *buffer, int minBytes, int maxBytes);
    virtual bool skipForward(qint64 bytes);
    virtual bool atEnd() const;
    virtual qint64 bytesRead() const;
    virtual QString errorString() const;

protected:
    QIODevice *device() { return mDevice; }
    const QIODevice *device() const { return mDevice; }

private:
    qint64 mBytesRead;
    QIODevice *mDevice;
};

class QioSeekableReadStream : public QioReadStream, public SeekableReadStream {
public:
    QioSeekableReadStream(QIODevice *dev);
    virtual ~QioSeekableReadStream();
    virtual bool read(quint8 *buffer, int bytes);
    virtual int readSome(quint8 *buffer, int minBytes, int maxBytes);
    virtual bool skipForward(qint64 bytes);
    virtual bool atEnd() const;
    virtual qint64 bytesRead() const;
    virtual QString errorString() const;
    virtual qint64 size() const;
    virtual qint64 pos() const;
    virtual bool setPos(qint64 pos);
};

class QioWriteStream : public WriteStream {
public:
    QioWriteStream(QIODevice *dev);
    virtual ~QioWriteStream();
    virtual bool write(const quint8 *buffer, int bytes);
    virtual void flush();
    virtual qint64 bytesWritten() const;
    virtual QString errorString() const;

protected:
    QIODevice *device() { return mDevice; }
    const QIODevice *device() const { return mDevice; }

private:
    qint64 mBytesWritten;
    QIODevice *mDevice;
};

class LimitedReadStream : public ReadStream {
public:
    LimitedReadStream(ReadStream *source, qint64 byteLimit);
    virtual bool read(quint8 *buffer, int bytes);
    virtual int readSome(quint8 *buffer, int minBytes, int maxBytes);
    virtual bool skipForward(qint64 bytes);
    virtual bool atEnd() const;
    virtual qint64 bytesRead() const;
    virtual QString errorString() const;

private:
    qint64 mBytesLeft;
    qint64 mBytesInitial;
    ReadStream *mStream;
};

}

#endif
