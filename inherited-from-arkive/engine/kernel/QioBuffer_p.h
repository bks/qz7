#ifndef QIOBUFFER_P_H
#define QIOBUFFER_P_H

#include <QtCore/QByteArray>
#include <QtCore/QIODevice>
#include <QtCore/QMutex>
#include <QtCore/QQueue>
#include <QtCore/QStack>
#include <QtCore/QThread>
#include <QtCore/QWaitCondition>


class QioBufferPrivate : public QThread {
    Q_OBJECT
public:
    // default to a 16K buffer size
    enum Mode { ReadMode, WriteMode };
    QioBufferPrivate(Mode mode);
    ~QioBufferPrivate();

    void setBackingDevice(QIODevice *dev) { mBackingDevice = dev; }
    QIODevice * backingDevice() const { return mBackingDevice; }

    void setBufferSize(uint bufSize) { mBufferSize = bufSize; }
    uint bufferSize() const { return mBufferSize; }

    bool writeBuffer(QByteArray *buf);
    bool readBuffer(QByteArray *buf);
    bool flush();
    bool shutdown();

    void setup();
    virtual void run();

private:
    QIODevice *mBackingDevice;
    uint mBufferSize;

    // store free buffers in a stack to try to maximize cache-hotness
    QStack<QByteArray> mFreeBuffers;
    QQueue<QByteArray> mRead;
    QQueue<QByteArray> mWriteQueue;

    QMutex mLock;
    QWaitCondition mStatusSignal;

    enum Status {
        Ready,
        Flush,
        EndOfFile,
        Shutdown,
        NotWorking = Shutdown,
        NotStarted,
        Terminated,
        ReadError,
        WriteError
    };
    void updateStatus(Status s);
    Status mStatus;
    bool mReadMode;
};

#endif
