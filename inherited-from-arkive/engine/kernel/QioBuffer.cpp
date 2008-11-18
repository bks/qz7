#include "QioBuffer.h"
#include "QioBuffer_p.h"

#include <QtCore/QByteArray>
#include <QtCore/QDebug>
#include <QtCore/QIODevice>
#include <QtCore/QQueue>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QWaitCondition>

QioBufferPrivate::QioBufferPrivate(Mode mode)
    : QThread(), mBackingDevice(0), mBufferSize(0), mStatus(NotStarted), mReadMode(mode == ReadMode)
{
}

QioBufferPrivate::~QioBufferPrivate()
{
    // we require that we not be running before we are destroyed
    Q_ASSERT(mStatus == Terminated || mStatus == NotStarted);
}
    

bool QioBufferPrivate::writeBuffer(QByteArray *buf)
{
    Q_ASSERT(mBackingDevice->isWritable());
    
    if (mStatus >= NotWorking)
        return false;
    
    {
        QMutexLocker locker(&mLock);
        
        // queue the new buffer
        mWriteQueue.enqueue(*buf);
        
        // tell the worker thread that something's changed
        mStatusSignal.wakeAll();
        
        // try to reuse a buffer
        if (!mFreeBuffers.isEmpty()) {
            *buf = mFreeBuffers.pop();
            return true;
        }
    }
    
    // no buffers available for reuse, give back a new one
    *buf = QByteArray();
    buf->resize(mBufferSize);
    return true;
}

bool QioBufferPrivate::readBuffer(QByteArray *buf)
{
    Q_ASSERT(mBackingDevice->isReadable());
    
    if (mStatus >= NotWorking)
        return false;
    
    // release the provided buffer, and then try to dequeue a buffer
    {
        QMutexLocker locker(&mLock);
        
        if (!buf->isEmpty()) {
            // reclaim the buffer
            mFreeBuffers.push(*buf);
        }
        
        // wait until we have a buffer available
        while (mRead.isEmpty()) {
            // error out if needed
            if (mStatus >= NotWorking)
                return false;
                
            // on EOF, just return empty
            if (mStatus == EndOfFile) {
                *buf = QByteArray();
                return true;
            }
    
            // keep waking up the worker thread
            mStatusSignal.wakeAll();
            
            // and wait until we get a buffer
            mStatusSignal.wait(locker.mutex());
        }
        
        // dequeue the buffer
        *buf = mRead.dequeue();
        
        // and wake up the worker thread
        mStatusSignal.wakeAll();
    }

    return true;
}


bool QioBufferPrivate::flush()
{
    Q_ASSERT(mBackingDevice->isWritable());
    
    QMutexLocker locker(&mLock);
    
    if (mStatus >= NotWorking)
        return false;
    
    // tell the worker thread to do a flush
    updateStatus(Flush);
    
    while (mStatus != Ready) {
        if (mStatus >= NotWorking)
            return false;
        
        mStatusSignal.wait(locker.mutex());
    }
    return true;
}

bool QioBufferPrivate::shutdown()
{
    QMutexLocker locker(&mLock);

    if (mStatus == NotStarted)
        return true;

    Status oldStatus = mStatus;    
    updateStatus(Shutdown);
    
    while (mStatus != Terminated) {
        mStatusSignal.wait(locker.mutex());
    }
    
    if (oldStatus == ReadError || oldStatus == WriteError)
        return false;
    
    return true;
}

void QioBufferPrivate::setup()
{
    // set our new status before we start the thread, since who knows if the
    // thread will get scheduled before we query the status again?
    mStatus = Ready;
    start();
}
    
void QioBufferPrivate::run()
{
    const int NR_BUFFERS_READY = 2;
    
    mLock.lock();
    
    while (mStatus != Shutdown) {
        // errors are sticky, as is EOF
give_up:
        if (mStatus >= NotWorking || mStatus == EOF) {
            mStatusSignal.wait(&mLock);
            continue;
        }
            
        if (mReadMode) {
            // keep reading buffers until we have a sufficient backlog
            while (mRead.size() < NR_BUFFERS_READY) {
                if (mBackingDevice->atEnd()) {
                    updateStatus(EndOfFile);
                    break;
                }

                QByteArray target;
                
                // grab a free buffer
                if (!mFreeBuffers.isEmpty())
                    target = mFreeBuffers.pop();
                
                // unlock us
                mLock.unlock();
                
                // make sure we actually have a buffer
                if (uint(target.capacity()) < mBufferSize)
                    target.resize(mBufferSize);
                
                // now do the read
retry_read:
                int read = mBackingDevice->read(target.data(), target.size());

                // if we had a read error, note it and its error string,
                // wake up anyone who might care, and give up
                if (read < 0) {
                    mLock.lock();
                    updateStatus(ReadError);
                    goto give_up;
                }
                
                // if we didn't get any data, try to wait for it
                if (!read) {
                    if (!mBackingDevice->waitForReadyRead(-1)) {
                        // if there's no data to be had, we're at EOF;
                        // give up
                        mLock.lock();
                        updateStatus(EndOfFile);
                        goto give_up;
                    }
                    // there should be some data now, retry the read
                    goto retry_read;
                }
                
                // store the block we got
                target.resize(read);
                mLock.lock();
                mRead.enqueue(target);
                
                // and announce the block's presence
                mStatusSignal.wakeAll();
            }
        } else {
            // write out any pending buffers
            while (!mWriteQueue.isEmpty()) {
                QByteArray toWrite = mWriteQueue.dequeue();
                mLock.unlock();
                
                int bytes = toWrite.size();
                int base = 0;

                while (bytes) {
                    int written = mBackingDevice->write(&toWrite.constData()[base], bytes);
                    
                    if (written < 0) {
                        // we got a write error: note it down and give up
                        mLock.lock();
                        updateStatus(WriteError);
                        goto give_up;
                    }
                    
                    // we've finished this block
                    if (written == bytes)
                        break;
                    
                    // wait for more buffer space to write to
                    if (!mBackingDevice->waitForBytesWritten(-1)) {
                        // the write failed: note it and give up
                        mLock.lock();
                        goto give_up;
                    }
                    
                    bytes -= written;
                    base += written;
                }
                
                // we're done with this buffer: release it
                mLock.lock();
                mFreeBuffers.push(toWrite);
            }
            
            // announce that we've successfully flushed the queue
            updateStatus(Ready);
        }
        
        // wait until we have something to do
        if (mStatus != Shutdown)
            mStatusSignal.wait(&mLock);
    }
    
    updateStatus(Terminated);
    mLock.unlock();
    return;
}

void QioBufferPrivate::updateStatus(Status s)
{
    if (mStatus != Shutdown || s == Terminated)
        mStatus = s;
    mStatusSignal.wakeAll();
}

QioReadBuffer::QioReadBuffer()
    : mBacking(0), mPos(0), mAvailable(0), d(new QioBufferPrivate(QioBufferPrivate::ReadMode))
{
    d->setBufferSize(QIO_DEFAULT_BUFFER_SIZE);
}

QioReadBuffer::~QioReadBuffer()
{
    d->shutdown();
    delete d;
}

void QioReadBuffer::setBackingDevice(QIODevice *dev)
{
    d->setBackingDevice(dev);
}

void QioReadBuffer::setBufferSize(uint bufferSize)
{
    d->setBufferSize(bufferSize);
}

bool QioReadBuffer::refresh()
{
    if (!d->isRunning()) {
        // we only want to start the thread if it will read more than one full buffer,
        // otherwise its just a waste of time
        if (!d->backingDevice()->isSequential() && d->backingDevice()->size() <= d->bufferSize()) {
            // if we already have a buffer, there's nothing more to be had!
            if (!mBacking.isEmpty())
                return false;

            int size = d->backingDevice()->size();
            uint base = 0;

            // synchonously read the entire file
            mBacking.resize(size);
            while (size) {
                int read = d->backingDevice()->read(&mBacking.data()[base], size);

                if (read < 0)
                    return false;
                if (read != size && !d->backingDevice()->waitForReadyRead(-1))
                    return false;
                size -= read;
                base += read;
            }

            // we've got the data, so load it up and we're done
            mBuffer = (quint8 *)mBacking.data();
            mAvailable = mBacking.size();
            mPos = 0;
            return true;
        }

        // there's (probably) more than one buffer of data to read: start the thread and do
        // it asynchronously
        d->setup();
    }

    // get a buffer from the reader thread, waiting if needed
    if (!d->readBuffer(&mBacking) || mBacking.isEmpty())
        return false;

    mBuffer = (quint8 *)mBacking.data();
    mAvailable = mBacking.size();
    mPos = 0;
    mBytesRead += mBacking.size();

    return true;
}

QioWriteBuffer::QioWriteBuffer()
    : mBuffer(0), mPos(0), mSize(0), d(new QioBufferPrivate(QioBufferPrivate::WriteMode))
{
    d->setBufferSize(QIO_DEFAULT_BUFFER_SIZE);
}

QioWriteBuffer::~QioWriteBuffer()
{
    d->shutdown();
    delete d;
}

void QioWriteBuffer::setBackingDevice(QIODevice *dev)
{
    d->setBackingDevice(dev);
}

void QioWriteBuffer::setBufferSize(uint bufferSize)
{
    d->setBufferSize(bufferSize);
}

bool QioWriteBuffer::flush()
{
    // set ourselves up if needed
    if (!d->isRunning()) {
        d->setup();

        // no data yet: just allocate a fresh buffer
        mBacking.resize(d->bufferSize());
        mBuffer = reinterpret_cast<quint8 *>(mBacking.data());
        mPos = 0;
        mSize = mBacking.size();
        return true;
    }

    // nothing to flush
    if (!mPos)
        return true;

    // trim the buffer to what's actually been written
    mBacking.resize(mPos);
    mBytesWritten += mPos;

    // submit this buffer and get a new one
    if (!d->writeBuffer(&mBacking))
        return false;

    // we'll try to use the whole buffer, so resize it to its current allocation size
    mBacking.resize(mBacking.capacity());
    mBuffer = reinterpret_cast<quint8 *>(mBacking.data());
    mPos = 0;
    mSize = mBacking.size();

    return true;
}
