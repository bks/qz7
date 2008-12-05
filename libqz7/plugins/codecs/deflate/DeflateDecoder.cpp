#include "DeflateDecoder.h"
#include "DeflateDecoderST_p.h"
#include "DeflateDecoderMT_p.h"

#include <QtCore/QByteArray>
#include <QtCore/QDataStream>
#include <QtCore/QIODevice>
#include <QtCore/QVariant>

namespace qz7 {
namespace deflate {

BaseDeflateDecoder::BaseDeflateDecoder(DeflateType type, QObject *parent)
    : mBytesExpected(0)
    , mType(type)
    , mKeepHistory(false)
    , mMultiThreaded(QThread::idealThreadCount() > 1)
    , mDecoderST(0)
    , mDecoderMT(0)
{
    if (qgetenv("QZ7_NO_MULTITHREADED") == "true")
        mMultiThreaded = false;
}

bool BaseDeflateDecoder::stream(ReadStream *from, WriteStream *to)
{
    mErrorString = QString();

    if (mMultiThreaded) {
        if (!mDecoderMT) {
            mDecoderMT = new DeflateDecoderMT(mType, this);
            connect(mDecoderMT, SIGNAL(progress(quint64, quint64)),
                this, SIGNAL(progress(quint64, quint64)));
        }
        mDecoderMT->setKeepHistory(mKeepHistory);
        mDecoderMT->setBytesExpected(mBytesExpected);

        try {
            return mDecoderMT->stream(from, to);
        } catch (Error e) {
            mErrorString = e.message();
            return false;
        }
    }

    if (!mDecoderST) {
        mDecoderST = new DeflateDecoderST(mType, this);
        connect(mDecoderST, SIGNAL(progress(quint64, quint64)),
            this, SIGNAL(progress(quint64, quint64)));
    }
    mDecoderST->setKeepHistory(mKeepHistory);
    mDecoderST->setBytesExpected(mBytesExpected);

    try {
        return mDecoderST->stream(from, to);
    } catch (Error e) {
        mErrorString = e.message();
        return false;
    }
}

QString BaseDeflateDecoder::errorString() const
{
    return mErrorString;
}

void BaseDeflateDecoder::interrupt()
{
    if (mMultiThreaded && mDecoderMT)
        mDecoderMT->interrupt();
    else if (mDecoderST)
        mDecoderST->interrupt();
}

bool BaseDeflateDecoder::setProperty(const QString& property, const QVariant& value)
{
    if (property == "multithreaded") {
        mMultiThreaded = value.toBool();
        return true;
    } else if (property == "keepHistory") {
        mKeepHistory = value.toBool();
        return true;
    } else if (property == "bytesExpected") {
        mBytesExpected = value.toULongLong();
        return true;
    } else if (property == "threadCount") {
        if (value.toInt() > 1)
            mMultiThreaded = true;
        else
            mMultiThreaded = false;
        return true;
    }
    return false;
}

QVariant BaseDeflateDecoder::property(const QString& property) const
{
    if (property == "multithreaded")
        return QVariant(mMultiThreaded);
    if (property == "keepHistory")
        return QVariant(mKeepHistory);
    if (property == "bytesExpected")
        return QVariant(mBytesExpected);
    if (property == "threadCount") {
        if (mMultiThreaded)
            return QVariant(2U);
        else
            return QVariant(1U);
    }
    return QVariant();
}

static const quint16 MAGIC = 0xdef1;
static const quint8 VERSION = 0;

QByteArray BaseDeflateDecoder::serializeProperties() const
{
    QByteArray ret;
    QDataStream str(&ret, QIODevice::WriteOnly);

    str.setVersion(QDataStream::Qt_4_3);
    str << MAGIC << VERSION;
    str << mMultiThreaded;
    str << mBytesExpected;
    str << mKeepHistory;
}
    
bool BaseDeflateDecoder::applySerializedProperties(const QByteArray& serializedProperties)
{
    QDataStream str(serializedProperties);
    str.setVersion(QDataStream::Qt_4_3);

    quint16 m;
    str >> m;
    if (m != MAGIC)
        return false;
    quint8 v;
    str >> v;
    if (v != VERSION)
        return false;
    str >> mMultiThreaded;
    str >> mBytesExpected;
    str >> mKeepHistory;
}

}   // namespace deflate
}   // namespace qz7
