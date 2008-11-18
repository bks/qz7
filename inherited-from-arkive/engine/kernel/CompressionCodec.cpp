#include "kernel/CompressionCodec.h"

#include <QtCore/QIODevice>

CompressionCodec::~CompressionCodec()
{
}

CompressionCodecBase::~CompressionCodecBase()
{
}

void CompressionCodecBase::setInputDevice(QIODevice *dev)
{
    mInputDevice = dev;
}

void CompressionCodecBase::setOutputDevice(QIODevice *dev)
{
    mOutputDevice = dev;
}

QString CompressionCodecBase::errorString() const
{
    return mErrorString;
}

void CompressionCodecBase::setErrorString(const QString& str)
{
    mErrorString = str;
}

bool CompressionCodecBase::errorCorrupted()
{
    setErrorString(tr("the archive is corrupted"));
    return false;
}

bool CompressionCodecBase::errorCorrupted(const QString& details)
{
    setErrorString(tr("the archive is corrupted: %1").arg(details));
    return false;
}

bool CompressionCodecBase::errorNoMemory()
{
    setErrorString(tr("not enough memory is available"));
    return false;
}

bool CompressionCodecBase::errorIoDevice(const QIODevice *dev)
{
    setErrorString(tr("I/O error: %1").arg(dev->errorString()));
    return false;
}

#include "CompressionCodec.moc"
