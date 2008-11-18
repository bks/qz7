#include "kernel/Filter.h"

Filter::~Filter()
{
}

FilteringIoDevice::FilteringIoDevice(QObject *parent)
    : QIODevice(parent), mBlocks(), mBackingDevice(0), mFilter(0), mSize(0)
{
}

FilteringIoDevice::~FilteringIoDevice()
{
}

bool FilteringIoDevice::isSequential() const
{
    return true;
}

bool FilteringIoDevice::open(QIODevice::OpenMode mode)
{
    if (mode != QIODevice::ReadOnly && mode != QIODevice::WriteOnly)
        return false;

    return QIODevice::open(mode);
}

void FilteringIoDevice::close()
{
    if (openMode() == QIODevice::WriteOnly && !mBlocks.isEmpty()) {
        // flush the tail block
        QByteArray unified;

        while (!mBlocks.isEmpty())
            unified.append(mBlocks.dequeue());

        QByteArray filtered;
        mFilter->filterLast(unified, &filtered);

        int bytes = filtered.size();
        int base = 0;
        while (bytes) {
            int written = mBackingDevice->write(&filtered.constData()[base], bytes);

            if (written < 0)
                break;

            bytes -= written;
            base += written;

            if (bytes && !mBackingDevice->waitForBytesWritten(-1))
                break;
        }
    }

    return QIODevice::close();
}

qint64 FilteringIoDevice::readData(char *data, qint64 maxSize)
{
    if (mSize < maxSize) {
        // try to read some more
        QByteArray buf;
        int blocksize = mFilter->blockSize();
        int block = blocksize;

        // try to work with decent block sizes
        while (block < 4096 || block < (maxSize - mSize))
            block *= 2;

        buf.resize(block);
        int base = 0;

        while (block) {
            int read = mBackingDevice->read(&buf.data()[base], buf.size());

            if (read < 0) {
                if (!mSize)
                    return -1;

                buf.resize(base);
                break;
            }

            mSize += read;
            base += read;
            block -= read;

            if (!block)
                break;

            if (!mBackingDevice->waitForReadyRead(-1)) {
                buf.resize(base);
                break;
            }
        }
        
        int fullblocks = (buf.size() / blocksize) * blocksize;

        if (fullblocks) {
            QByteArray buf1;

            // if buf is a full set of blocks, we don't need to copy it...
            if (buf.size() != fullblocks)
                buf1 = buf.left(fullblocks);
            else
                buf1 = buf;
            QByteArray filtered;
            mFilter->filterData(buf1, &filtered);
            mBlocks.enqueue(filtered);
        }

        if (buf.size() != fullblocks) {
            // if we have a non-full-block, then we must have hit EOF
            QByteArray tail = buf.right(buf.size() - fullblocks);
            QByteArray filteredTail;
            mFilter->filterLast(tail, &filteredTail);
            mBlocks.enqueue(filteredTail);
        }
    }

    int base = 0;
    while (maxSize && !mBlocks.isEmpty()) {
        int s = mBlocks.head().size();

        if (s > maxSize) {
            QByteArray& block = mBlocks.head();
            ::memcpy(&data[base], block.constData(), maxSize);
            mBlocks.head() = block.right(block.size() - maxSize);
            base = maxSize;
            maxSize = 0;
        } else {
            QByteArray buf = mBlocks.dequeue();
            ::memcpy(&data[base], buf.constData(), buf.size());
            maxSize -= s;
            base += s;
        }
    }

    static int test = 0;
    for (int i = 0; i < base; i++)
        test += data[i];

    return base;
}

qint64 FilteringIoDevice::writeData(const char *data, qint64 maxSize)
{
    int blocksize = mFilter->blockSize();

    if (mSize + maxSize >= blocksize) {
        // we are guaranteed to either use the entire submitted buffer
        // or copy what's left of it into our own storage...
        mBlocks.enqueue(QByteArray::fromRawData(data, maxSize));
    } else {
        // we won't get a full block, so copy the data into our
        // own storage now
        mBlocks.enqueue(QByteArray(data, maxSize));
    }

    mSize += maxSize;

    QByteArray unfiltered;
    while (mSize >= blocksize) {
        int s = mBlocks.head().size();

        if (s + unfiltered.size() < blocksize) {
            // while QByteArray normally optimizes appends() to empty
            // arrays into assignments, it cannot do that if the source
            // was from QByteArray::fromRawData()... so do it manually,
            // since we know its safe to do so
            if (unfiltered.isEmpty())
                unfiltered = mBlocks.dequeue();
            else
                unfiltered.append(mBlocks.dequeue());
            mSize -= s;
            continue;
        } else if (((s + unfiltered.size()) % blocksize) == 0) {
            if (unfiltered.isEmpty())
                unfiltered = mBlocks.dequeue();
            else
                unfiltered.append(mBlocks.dequeue());
            mSize -= s;
        } else {
            // the next block will give us n + a fraction of a block
            int remain = (s + unfiltered.size()) % blocksize;
            QByteArray& block = mBlocks.head();
            unfiltered.append(block.left(s - remain));
            block = block.right(remain);
            mSize -= s - remain;
        }

        // unfiltered is now some number of blocks long
        QByteArray filtered;
        mFilter->filterData(unfiltered, &filtered);

        // write out the filtered data
        int base = 0, bytes = filtered.size();

        while (bytes) {
            int written = mBackingDevice->write(&filtered.constData()[base], bytes);

            if (written < 0)
                return -1;

            base += written;
            bytes -= written;

            if (bytes && !mBackingDevice->waitForBytesWritten(-1))
                return -1;
        }
    }

    return maxSize;
}

#include "Filter.moc"
