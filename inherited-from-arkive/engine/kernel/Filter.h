#ifndef FILTER_H
#define FILTER_H

#include <QtCore/QByteArray>
#include <QtCore/QIODevice>
#include <QtCore/QQueue>

/**
 * A Filter is a byte- or block-level transformation of a data stream. It is
 * allowed to change the size of the block, or state that it needs more data
 * (i.e. for a block-level transformation).
 */
class Filter {
public:
    virtual ~Filter();

    /// filterData() may work in-place, @p data should be considered to be modified.
    /// @p data should be of length blockSize().
    virtual void filterData(QByteArray& data, QByteArray *filteredData) = 0;

    /// this is the final block, so the filter must do _something_ with it
    virtual void filterLast(QByteArray& data, QByteArray *filteredData) = 0;

    /// filterData() should be called with blocks of (multiples of) blockSize()
    virtual int blockSize() = 0;
};
Q_DECLARE_INTERFACE(Filter, "org.kde.Arkive.Filter")

/**
 * A FilteringIoDevice allows one to build up Filter chains over
 * a backend QIODevice. It only works as a read-only or write-only device;
 * the filter and the backing device must be set before calling open().
 */
class FilteringIoDevice : public QIODevice {
    Q_OBJECT

public:
    FilteringIoDevice(QObject *parent = 0);
    virtual ~FilteringIoDevice();

    virtual bool open(QIODevice::OpenMode openMode);
    virtual void close();
    virtual bool isSequential() const;

    void setBackingDevice(QIODevice *backingDevice) { mBackingDevice = backingDevice; }
    void setFilter(Filter *filter) { mFilter = filter; }

protected:
    virtual qint64 readData(char *data, qint64 maxSize);
    virtual qint64 writeData(const char *data, qint64 maxSize);

private:
    QQueue<QByteArray> mBlocks;
    QIODevice *mBackingDevice;
    Filter *mFilter;
    qint64 mSize;
};

#endif
