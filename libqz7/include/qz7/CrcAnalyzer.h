#ifndef QZ7_CRC_ANALYZER_H
#define QZ7_CRC_ANALYZER_H
#include "qz7/Analyzer.h"
#include "qz7/Crc.h"
namespace qz7 {
class Crc32 {
public:
    Crc32() { clear(); }
    void clear() { mVal = CrcInitValue(); }
    quint32 value() const { return CrcValue(mVal); }
    void update(const quint8 *buffer, int bytes) { mVal = CrcUpdate(mVal, buffer, bytes); }
    void update(const char *buffer, int bytes) { mVal = CrcUpdate(mVal, buffer, bytes); }
    void update(quint32 word) { mVal = CrcUpdate(mVal, &word, 4); }
    void update(qint32 word) { mVal = CrcUpdate(mVal, &word, 4); }
    void update(quint16 word) { mVal = CrcUpdate(mVal, &word, 2); }
    void update(qint16 word) { mVal = CrcUpdate(mVal, &word, 2); }
    void update(quint8 byte) { mVal = CrcUpdate(mVal, &byte, 1); }
    void update(qint8 byte) { mVal = CrcUpdate(mVal, &byte, 1); }
private:
    quint32 mVal;
};
class Crc32Analyzer : public Analyzer {
public:
    Crc32Analyzer() : mCrc() { }
    ~Crc32Analyzer() { }
    quint32 value() const { return mCrc.value(); }
    virtual void analyze(const quint8 *data, int length) { mCrc.update(data, length); }
private:
    Crc32 mCrc;
};
QZ7_DECLARE_STREAMS_FOR_ANALYZER(Crc32)
}
#endif
