#include "CompressionManager.h"

#include <QtCore/QMutexLocker>
#include <QtCore/QQueue>
#include <QtCore/QThread>
#include <QtCore/QThreadPool>
#include <QtCore/QStack>

CompressionTask::CompressionTask()
    : mStatus(NotStarted), mBytesIn(0), mBytesOut(0), mLastBlock(false), mStreamFinished(0)
{
}

CompressionTask::~CompressionTask()
{
    Q_ASSERT(mStatus == Terminated);
}

void CompressionTask::run()
{
    mLock.lock();
    setStatus(ReadyToRead);

    while (true) {
        switch (mStatus) {
            case ReadyToRead:
            case ReadyToWrite:
                // waiting for clearance: go back to sleep
                mStatusSignal.wait(&mLock);
                break;
            case ClearToRead:
                mLock.unlock();
                mBytesIn = readBlock();
                mLock.lock();
                // no need to call setStatus(), since this is a purely internal state change
                // that the manager doesn't care about; if we had an error, that will be
                // announced below
                if (mStatus == ClearToRead)
                    mStatus = Thinking;
                break;
            case Thinking:
                mLock.unlock();
                if (!mStreamFinished)
                    processBlock();
                mLock.lock();
                if (mStatus == Thinking)
                    setStatus(ReadyToWrite);
                break;
            case ClearToWrite:
                mLock.unlock();
                if (!mStreamFinished)
                    mBytesOut = writeBlock();
                mLock.lock();
                if (mStatus == ClearToWrite) {
                    if (mLastBlock || mStreamFinished)
                        setStatus(Done);
                    else
                        setStatus(ReadyToRead);
                }
                break;
            case Terminate:
                // alright, we've been given permission to disappear: have done with it and go away
                mStatus = Terminated;
                mLock.unlock();
                return;
            case Error:
            case Done:
                // just sit here until we're told to go away
                mStatusSignal.wait(&mLock);
                break;
            case Starting:
            case NotStarted:
            case Terminated:
                // NOTREACHED
                break;
        }

        // if we've finished or picked up an error, notify the manager of it
        if (mStatus == Error || mStatus == Done)
            setStatus(mStatus);
    }
}

void CompressionTask::lastBlock()
{
    mLastBlock = true;
}

void CompressionTask::blockFailed(int error)
{
    mErrorCode = error;
    mStatus = Error;
}

void CompressionTask::streamFinished()
{
    // pretend that we're the last block so that this task flushes the
    // pending write queue, but make a note that we don't actually have to
    // do anything
    mLastBlock = true;
    mStreamFinished = true;
}

void CompressionTask::setStatus(Status s)
{
    // if we don't have a manager, we've nothing to do
    if (!q)
        return;

    // locking order: we are called with our lock held
    QMutexLocker managerLocker(&q->mLock);

    mStatus = s;

    // tell the manager we've changed
    q->mSignaledTasks.append(this);
    q->mStatusSignal.wakeAll();
}

// ------------- CompressionManager -----------------------

CompressionManager::CompressionManager(QObject *parent)
    : QObject(parent), mErrorCode(0)
{
}

// the heart of the matter: run a set of tasks until one of them either succeeds or errors
// out
bool CompressionManager::code(const QList<CompressionTask *>& tasks)
{
    Q_ASSERT(!tasks.isEmpty());
    if (tasks.size() == 1)
        return singleThreadedCode(tasks.first());

    CompressionTask *currentlyReading = 0, *currentlyWriting = 0;

    // release readers in LIFO order to try to keep them cache-hot, but writers must
    // be FIFO, since they need to give out their results in the proper order
    // (i.e., reading is stateless but writing is stateful)
    QStack<CompressionTask *> waitingToRead;
    QQueue<CompressionTask *> waitingToWrite;
    bool lastBlock = false;
    qint64 bytesIn = 0, bytesOut = 0;

    // set up the tasks
    foreach (CompressionTask *t, tasks) {
        t->mStatus = CompressionTask::Starting;
        t->q = this;
        t->setAutoDelete(true);
    }

    // take our lock so we don't miss any notifications
    mLock.lock();

    // start the tasks: at least one, but no more than the thread pool can immediately sustain
    int i = 0;
    int n = QThreadPool::globalInstance()->maxThreadCount() - QThreadPool::globalInstance()->activeThreadCount();
    n = qMax(n, 1);
    foreach (CompressionTask *t, tasks) {
        if (i++ < n)
            QThreadPool::globalInstance()->start(t);
        else {
            t->mStatus = CompressionTask::Terminated;
            delete t;
        }
    }

    while (true) {
        // wait for notifications
        mStatusSignal.wait(&mLock);

        // handle any notifications: we have to release our lock while we do so,
        // because the manager lock nests inside the task locks
        Q_ASSERT(!mSignaledTasks.isEmpty());
        while (!mSignaledTasks.isEmpty()) {
            CompressionTask *signaledTask = mSignaledTasks.takeFirst();
            mLock.unlock();

            signaledTask->mLock.lock();
            switch (signaledTask->mStatus) {
                // if the task announced that it's now waiting,
                // add it to the appropriate queue
                case CompressionTask::ReadyToRead:
                    // we've finished a block or just started a task (in which case both increments
                    // will be zero); either way, announce the (possible) progress bump
                    bytesIn += signaledTask->mBytesIn;
                    bytesOut += signaledTask->mBytesOut;
                    emit progress(bytesIn, bytesOut);

                    waitingToRead.push(signaledTask);
                    break;
                case CompressionTask::ReadyToWrite:
                    waitingToWrite.enqueue(signaledTask);
                    break;
                case CompressionTask::Error:
                    // an error occurred: record it, and order all the tasks to
                    // die; they'll clean up after themselves
                    mErrorCode = signaledTask->errorCode();
                    signaledTask->mLock.unlock();
                    foreach (CompressionTask *t, tasks) {
                        setStatus(t, CompressionTask::Terminate);
                    }
                    return false;
                case CompressionTask::Done:
                    // the last task finished: update the progress one last time,
                    // then order all the tasks to die and return success
                    bytesIn += signaledTask->mBytesIn;
                    bytesOut += signaledTask->mBytesOut;
                    emit progress(bytesIn, bytesOut);

                    signaledTask->mLock.unlock();
                    foreach (CompressionTask *t, tasks) {
                        setStatus(t, CompressionTask::Terminate);
                    }
                    return true;
                default:
                    break;
            }

            // see if we're up to the last block yet
            if (!lastBlock)
                lastBlock = signaledTask->mLastBlock;

            // we're done with this task
            signaledTask->mLock.unlock();

            // and now see if we should release any pending tasks
            if (signaledTask == currentlyReading || !currentlyReading) {
                // if we've read the last block, don't try to start another one...
                if (!lastBlock && !waitingToRead.isEmpty()) {
                    currentlyReading = waitingToRead.pop();
                    setStatus(currentlyReading, CompressionTask::ClearToRead);
                } else {
                    currentlyReading = 0;
                }
            }
            if (signaledTask == currentlyWriting || !currentlyWriting) {
                if (!waitingToWrite.isEmpty()) {
                    currentlyWriting = waitingToWrite.dequeue();
                    setStatus(currentlyWriting, CompressionTask::ClearToWrite);
                } else {
                    currentlyWriting = 0;
                }
            }

            mLock.lock();
        }
    }
}

void CompressionManager::setStatus(CompressionTask *t, CompressionTask::Status s)
{
    QMutexLocker locker(&t->mLock);
    t->mStatus = s;

    if (s == CompressionTask::Terminate)
        t->q = 0;

    t->mStatusSignal.wakeAll();
}

int CompressionManager::errorCode() const
{
    return mErrorCode;
}

int CompressionManager::recommendedNumberOfTasks()
{
    return QThread::idealThreadCount();
}

// basically do the same thing as CompressionManager::code() and CompressionTask::run(),
// but syncronously, in the calling thread
bool CompressionManager::singleThreadedCode(CompressionTask *task)
{
    qint64 bytesIn = 0, bytesOut = 0;

    task->mStatus = CompressionTask::ReadyToRead;
    while (true) {
        if (task->mStreamFinished) {
            // all done!
            task->mStatus = CompressionTask::Done;
        }

        switch (task->mStatus) {
            case CompressionTask::Error:
                mErrorCode = task->errorCode();
                task->mStatus = CompressionTask::Terminated;
                delete task;
                return false;
            case CompressionTask::Done:
                task->mStatus = CompressionTask::Terminated;
                delete task;
                return true;
            case CompressionTask::ReadyToRead:
                emit progress(bytesIn, bytesOut);
                task->mStatus = CompressionTask::ClearToRead;
                bytesIn += task->readBlock();
                if (task->mStatus == CompressionTask::ClearToRead)
                    task->mStatus = CompressionTask::Thinking;
                break;
            case CompressionTask::Thinking:
                if (!task->mStreamFinished)
                    task->processBlock();
                if (task->mStatus == CompressionTask::Thinking)
                    task->mStatus = CompressionTask::ReadyToWrite;
                break;
            case CompressionTask::ReadyToWrite:
                task->mStatus = CompressionTask::ClearToWrite;
                bytesOut += task->writeBlock();

                // if we've written out the last block, we're done here...
                if (task->mLastBlock)
                    task->mStreamFinished = true;
                if (task->mStatus == CompressionTask::ClearToWrite)
                    task->mStatus = CompressionTask::ReadyToRead;
                break;
            default:
                Q_ASSERT_X(false, "CompressionManager::singleThreadedCode", "bad task->mStatus");
                return false;
        }
    }
}
