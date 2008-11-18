#include "kernel/LimitedIoDevice.h"

LimitedIoDevice::LimitedIoDevice(QObject *parent)
    : QIODevice(parent), mBackingDevice(0), mLimit(-1)
{
}

void LimitedIoDevice::setBackingDevice(QIODevice *b)
{
    mBackingDevice = b;
    mBase = b->pos();

    if (mLimit > 0 && !b->isSequential()) {
        // clamp mLimit
        mLimit = qMin(mLimit, b->size() - b->pos());
    }
}

void LimitedIoDevice::setLimit(qint64 limit)
{
    if (mBackingDevice && !mBackingDevice->isSequential())
        limit = qMin(limit, mBackingDevice->size() - mBase);

    mLimit = limit;
}

bool LimitedIoDevice::isSequential() const
{
    return mBackingDevice->isSequential();
}

bool LimitedIoDevice::open(QIODevice::OpenMode mode)
{
    Q_ASSERT(mode == QIODevice::ReadOnly);

    return QIODevice::open(mode);
}

bool LimitedIoDevice::seek(qint64 to)
{
    if (mBackingDevice->isSequential())
        return false;

    if (to >= mLimit || to < 0) {
        setErrorString(tr("attempt to seek past limit"));
        return false;
    }

    bool ok = mBackingDevice->seek(to + mBase);

    if (ok)
        return QIODevice::seek(to);

    return ok;
}

qint64 LimitedIoDevice::size() const
{
    return mLimit;
}

qint64 LimitedIoDevice::readData(char *data, qint64 maxSize)
{
    if (maxSize > mLimit - pos())
        maxSize = mLimit - pos();

    if (!maxSize) {
        setErrorString(tr("attempt to read past limit"));

        // there can never be more data available: return error
        return -1;
    }

    return mBackingDevice->read(data, maxSize);
}

qint64 LimitedIoDevice::writeData(const char *data, qint64 maxSize)
{
    Q_ASSERT_X(false, "LimitedIODevice::writeData", "writing is not supported");
    return -1;
}

#include "LimitedIoDevice.moc"
