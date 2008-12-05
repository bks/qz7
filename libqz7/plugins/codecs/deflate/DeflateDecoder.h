#ifndef QZ7_DEFLATEDECODER_H
#define QZ7_DEFLATEDECODER_H

#include "qz7/Codec.h"

namespace qz7 {
namespace deflate {

class DeflateDecoderST;
class DeflateDecoderMT;

enum DeflateType {
    BasicDeflate,
    Deflate64,
    DeflateNSIS
};

class BaseDeflateDecoder : public Codec {
    Q_OBJECT

public:
    BaseDeflateDecoder(DeflateType type, QObject *parent);

    virtual bool stream(ReadStream *from, WriteStream *to);
    virtual QString errorString() const;
    virtual void interrupt();
    virtual bool setProperty(const QString& property, const QVariant& value);
    virtual QVariant property(const QString& property) const;
    virtual QByteArray serializeProperties() const;
    virtual bool applySerializedProperties(const QByteArray& serializedProperties);

private:
    quint64 mBytesExpected;
    DeflateType mType;
    bool mKeepHistory;
    bool mMultiThreaded;

    QString mErrorString;
    DeflateDecoderST *mDecoderST;
    DeflateDecoderMT *mDecoderMT;
};

class DeflateDecoder : public BaseDeflateDecoder
{
    Q_OBJECT
public:
    DeflateDecoder(QObject *parent = 0): BaseDeflateDecoder(BasicDeflate, parent) {}
};

class Deflate64Decoder : public BaseDeflateDecoder
{
    Q_OBJECT
public:
    Deflate64Decoder(QObject *parent = 0): BaseDeflateDecoder(Deflate64, parent) {}
};

class DeflateNSISDecoder : public BaseDeflateDecoder
{
    Q_OBJECT
public:
    DeflateNSISDecoder(QObject *parent = 0): BaseDeflateDecoder(DeflateNSIS, parent) {}
};

}
}

#endif
