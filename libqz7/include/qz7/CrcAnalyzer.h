#ifndef QZ7_CRC_ANALYZER_H
#define QZ7_CRC_ANALYZER_H

#include "qz7/Analyzer.h"

namespace qz7 {

class Crc32 {
public:
    Crc32() { clear(); }
    void clear() { mVal = 0; }
    quint32 value() const { return (mVal ^ quint32(0xFFFFFFFF)); }
    
    void update(const quint8 *buffer, int bytes);
    void update(quint32 word);
    void update(qint32 word);
    void update(quint16 word);
    void update(qint16 word);
    
private:
    quint32 mVal;
};

class Crc32Analyzer : public Analyzer {
public:
    Crc32Analyzer() { }
    ~Crc32Analyzer() { }
    quint32 value() const { return mCrc.value(); }
    virtual void analyze(const quint8 *data, int length) { mCrc.update(data, length); }
};

QZ7_DECLARE_STREAMS_FOR_ANALYZER(Crc32)

}

#endif

