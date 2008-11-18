#ifndef COMPRESSIONMANAGER_H
#define COMPRESSIONMANAGER_H

#include <QtCore/QList>
#include <QtCore/QMutex>
#include <QtCore/QObject>
#include <QtCore/QRunnable>
#include <QtCore/QString>
#include <QtCore/QWaitCondition>

class CompressionManager;

/**
 * The multi-thread framework is built around the basic idea of multiple threads,
 * each reading a block, (de)?compressing it, and writing it out, where the ordering
 * of the data is maintained by only allowing the threads to read or write one at a time.
 */
class CompressionTask : public QRunnable {
public:
    CompressionTask();
    virtual ~CompressionTask();
    virtual void run();

    virtual qint64 readBlock() = 0;
    virtual void processBlock() = 0;
    virtual qint64 writeBlock() = 0;

    void lastBlock();
    void blockFailed(int err);
    void streamFinished();

    // some standard values for errorCode()
    enum { ReadError, WriteError, CrcMismatchError, CorruptArchiveError };

    void setErrorCode(int code) { mErrorCode = code; }
    int errorCode() const { return mErrorCode; }

private:
    friend class CompressionManager;
    enum Status {
        Starting,
        ReadyToRead,
        ClearToRead,
        Thinking,
        ReadyToWrite,
        ClearToWrite,
        Terminate,
        Terminated,
        Done,
        Error,
        NotStarted
    };
    void setStatus(Status s);

    Status mStatus;
    qint64 mBytesIn;
    qint64 mBytesOut;
    QMutex mLock;
    QWaitCondition mStatusSignal;
    CompressionManager *q;
    int mErrorCode;
    bool mLastBlock;
    bool mStreamFinished;
};

class CompressionManager : public QObject {
    Q_OBJECT

public:
    CompressionManager(QObject *parent = 0);
    bool code(const QList<CompressionTask *>& tasks);
    int errorCode() const;

    static int recommendedNumberOfTasks();

signals:
    void progress(qint64 bytesIn, qint64 bytesOut);

private:
    bool singleThreadedCode(CompressionTask *task);
    void setStatus(CompressionTask *t, CompressionTask::Status s);

    friend class CompressionTask;
    QMutex mLock;
    QWaitCondition mStatusSignal;
    QList<CompressionTask *> mSignaledTasks;
    int mErrorCode;
};

#endif
