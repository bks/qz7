#ifndef COPYCODEC_H
#define COPYCODEC_H

#include "kernel/CompressionCodec.h"

namespace Compression {
namespace Copy {

class CopyCodec : public CompressionCodecBase {
    Q_OBJECT

public:
    CopyCodec(QObject *parent = 0);
    virtual bool code();
    virtual bool setProperties(const QMap<QString, QVariant>& properties);
    virtual bool setProperties(const QByteArray& serializedProperties);
    virtual QByteArray serializeProperties();
};

}
}

#endif
