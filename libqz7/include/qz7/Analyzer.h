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
template<class AnalyzerType> AnalyzerReadStream : public ReadStream {
public:
    AnalyzerReadStream(ReadStream *stream, AnalyzerType *filter)
        : mStream(stream), mAnalyzer(filter){ }
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

template<class AnalyzerType> inline int AnalyzerReadStream<AnalyzerType>::read(quint8 *buffer, int bytes)
{
    int r = mStream->read(buffer, bytes);
    
    if (r < 0 || r == 0)
        return r;
    
    mAnalyzer->analyze(data, r);
    return r;
};

template<class AnalyzerType> inline int AnalyzerReadStream<AnalyzerType>::readSome(quint8 *buffer, int minBytes, int maxBytes)
{
    int r = mStream->readSome(buffer, minBytes, maxBytes);
    
    if (r < 0 || r == 0)
        return r;
    
    mAnalyzer->analyze(data, r);
    return r;
};

template<class AnalyzerType> inline bool AnalyzerReadStream<AnalyzerType>::skipForward(qint64 bytes)
{
    // we can't actually skip -- we have to read and analyze the bytes in between...
    const int bufferSize = 64 * 1024;
    quint8 buffer[bufferSize];
    
    while (bytes) {
        int toRead = qMin(bytes, qint64(bufferSize));
        int r = mStream->read(buffer, toRead);
        if (r != toRead)
            return false;
        mAnalyzer->analyze(buffer, r);
        bytes -= r;
    }
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
template<class AnalyzerType> AnalyzerWriteStream : public WriteStream {
public:
    AnalyzerWriteStream(WriteStream *stream, AnalyzerType *filter)
        : mStream(stream), mAnalyzer(filter) { }
    ~AnalyzerWriteStream() { delete mAnalyzer; }
    
    const AnalyzerType *analyzer() const { return mAnalyzer; }

    virtual bool write(const quint8 *buffer, int bytes);
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
            : AnalyzerReadStream<type ## Analyzer>(stream, new type) { }        \
    };                                                                          \
                                                                                \
    class type ## WriteStream : public AnalyzerWriteStream<type ## Analyzer> {  \
    public:                                                                     \
        type ## WriteStream(ReadStream *stream)                                 \
            : AnalyzerWriteStream<type ## Analyzer>(stream, new type) { }       \
    };

}

#endif

