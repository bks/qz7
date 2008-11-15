#ifndef QZ7_ANALYZER_H
#define QZ7_ANALYZER_H

#include "qz7/Stream.h"

namespace qz7 {

/**
 * An Analyzer is something which is effectively a function over an entire
 * I/O stream.
 */
class Analyzer {
public:
    virtual ~Analyzer() { };
    virtual void analyze(const quint8 *data, int length) = 0;
};

/**
 * AnalyzerReadStream provides a Stream which automatically runs the Analyzer on
 * the data read from it.
 */
template<class AnalyzerType> class AnalyzerReadStream : public ReadStream {
public:
    AnalyzerReadStream(ReadStream *stream, AnalyzerType *filter)
        : mStream(stream), mAnalyzer(filter) { }
    ~AnalyzerReadStream() { delete mAnalyzer; }
    const AnalyzerType *analyzer() const { return mAnalyzer; }
    virtual bool read(quint8 *buffer, int bytes);
    virtual int readSome(quint8 *buffer, int minBytes, int maxBytes);
    virtual bool skipForward(qint64 bytes);
    virtual qint64 bytesRead() const;
    virtual QString errorString() const;

private:
    qint64 mBytesSkipped;
    ReadStream *mStream;
    AnalyzerType *mAnalyzer;
};

template<class AnalyzerType> inline bool AnalyzerReadStream<AnalyzerType>::read(quint8 *buffer, int bytes)
{
    bool ok = mStream->read(buffer, bytes);
    if (!ok)
        return ok;
    mAnalyzer->analyze(buffer, bytes);
    return true;
};

template<class AnalyzerType> inline int AnalyzerReadStream<AnalyzerType>::readSome(quint8 *buffer, int minBytes, int maxBytes)
{
    int r = mStream->readSome(buffer, minBytes, maxBytes);
    if (r < 0 || r == 0)
        return r;
    mAnalyzer->analyze(buffer, r);
    return r;
};

template<class AnalyzerType> inline bool AnalyzerReadStream<AnalyzerType>::skipForward(qint64 bytes)
{
    // we can't actually skip -- we have to read and analyze the bytes in between...
    const int bufferSize = 64 * 1024;
    quint8 buffer[bufferSize];

    while (bytes) {
        int toRead = qMin(bytes, qint64(bufferSize));
        bool ok = mStream->read(buffer, toRead);
        if (!ok)
            return false;
        mAnalyzer->analyze(buffer, toRead);
        bytes -= toRead;
    }
    return true;
};

template<class AnalyzerType> inline qint64 AnalyzerReadStream<AnalyzerType>::bytesRead() const
{
    return mStream->bytesRead();
};

template<class AnalyzerType> inline QString AnalyzerReadStream<AnalyzerType>::errorString() const
{
    return mStream->errorString();
};

/**
 * AnalyzerWriteStream provides a Stream which automatically runs an Analyzer on
 * the data written to it.
 */
template<class AnalyzerType> class AnalyzerWriteStream : public WriteStream {
public:
    AnalyzerWriteStream(WriteStream *stream, AnalyzerType *filter)
        : mStream(stream), mAnalyzer(filter) { }
    ~AnalyzerWriteStream() { delete mAnalyzer; }

    const AnalyzerType *analyzer() const { return mAnalyzer; }
    virtual bool write(const quint8 *buffer, int bytes);
    virtual void flush();
    virtual qint64 bytesWritten() const;
    virtual QString errorString() const;

private:
    WriteStream *mStream;
    AnalyzerType *mAnalyzer;
};

template<class AnalyzerType> inline bool AnalyzerWriteStream<AnalyzerType>::write(const quint8 *buffer, int bytes)
{
    mAnalyzer->analyze(buffer, bytes);
    return mStream->write(buffer, bytes);
};

template<class AnalyzerType> inline void AnalyzerWriteStream<AnalyzerType>::flush()
{
    return mStream->flush();
};

template<class AnalyzerType> inline qint64 AnalyzerWriteStream<AnalyzerType>::bytesWritten() const
{
    return mStream->bytesWritten();
};

template<class AnalyzerType> inline QString AnalyzerWriteStream<AnalyzerType>::errorString() const
{
    return mStream->errorString();
};

/**
 * QZ7_DECLARE_STREAMS_FOR_ANALYZER declares shorthand streams for a give analyzer,
 * e.g. QZ7_DECLARE_STREAMS_FOR_ANALYZER(Foo) declares FooReadStream and FooWriteStream
 * which each wrap a FooAnalyzer.
 */
#define QZ7_DECLARE_STREAMS_FOR_ANALYZER(type)                                  \
    class type ## ReadStream : public AnalyzerReadStream<type ## Analyzer> {    \
    public:                                                                     \
        type ## ReadStream(ReadStream *stream)                                  \
            : AnalyzerReadStream<type ## Analyzer>(stream, new type ## Analyzer) { } \
    };                                                                          \
                                                                                \
    class type ## WriteStream : public AnalyzerWriteStream<type ## Analyzer> {  \
    public:                                                                     \
        type ## WriteStream(WriteStream *stream)                                \
            : AnalyzerWriteStream<type ## Analyzer>(stream, new type ## Analyzer) { } \
    };

}

#endif
