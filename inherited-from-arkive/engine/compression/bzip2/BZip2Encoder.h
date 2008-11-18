#ifndef BZIP2ENCODER_H
#define BZIP2ENCODER_H

#include "kernel/CompressionCodec.h"
#include "kernel/CompressionManager.h"
#include "kernel/BitStreamBE.h"
#include "kernel/MemoryBuffer.h"
#include "kernel/QioBuffer.h"

#include "BZip2Const.h"
#include "BZip2CRC.h"

namespace Compress {
namespace BZip2 {

class BZip2EncoderTask;

class BZip2Encoder : public CompressionCodecBase {
    Q_OBJECT

public:
    BZip2Encoder(QObject *parent = 0);
    virtual bool code();
    virtual bool setProperties(const QMap<QString, QVariant>& properties);
    virtual bool setProperties(const QByteArray& serializedProperties);
    virtual QByteArray serializeProperties();

private:
    friend class BZip2EncoderTask;

    QioReadBuffer *mInBuffer;
    int mNrPasses;
    int mBlockSizeMult;
    bool mOptimizeNrOfTables;

    quint8 mCacheLineFill[128];

    BZip2CombinedCRC mCombinedCrc;
    QioWriteBuffer *mOutBuffer;
    BitStreamBE<QioWriteBuffer> mOutBitStream;
};

const int NumPassesMax = 10;

class BZip2EncoderTask : public CompressionTask {
public:
    BZip2EncoderTask(BZip2Encoder *parent);
    virtual ~BZip2EncoderTask();

    virtual qint64 readBlock();
    virtual void processBlock();
    virtual qint64 writeBlock();

private:
    BZip2Encoder const *q;
    quint8 *mBlock;
    quint8 *mMtfArray;
    quint32 *mBlockSorterIndicies;

    quint8 mLens[NumTablesMax][MaxAlphaSize];
    quint32 mFreqs[NumTablesMax][MaxAlphaSize];
    quint32 mCodes[NumTablesMax][MaxAlphaSize];

    quint8 mSelectors[NumSelectorsMax];

    quint32 mCrcs[1 << NumPassesMax];
    quint32 mNrCrcs;

    BitStreamBE<MemoryWriteBuffer> mTrialStream;
    MemoryWriteBuffer mTrialBuffer;
};

#if 0
class CThreadInfo
{
public:
    quint8 *m_Block;
private:
    quint8 *m_MtfArray;
    quint8 *m_TempArray;
    quint32 *m_BlockSorterIndex;

    CMsbfEncoderTemp *m_OutStreamCurrent;

    quint8 Lens[kNumTablesMax][kMaxAlphaSize];
    quint32 Freqs[kNumTablesMax][kMaxAlphaSize];
    quint32 Codes[kNumTablesMax][kMaxAlphaSize];

    quint8 m_Selectors[kNumSelectorsMax];

    quint32 m_CRCs[1 << kNumPassesMax];
    quint32 m_NumCrcs;

    int m_BlockIndex;

    void WriteBits2(quint32 value, quint32 numBits);
    void WriteByte2(quint8 b);
    void WriteBit2(bool v);
    void WriteCRC2(quint32 v);

    void EncodeBlock(const quint8 *block, quint32 blockSize);
    quint32 EncodeBlockWithHeaders(const quint8 *block, quint32 blockSize);
    void EncodeBlock2(const quint8 *block, quint32 blockSize, quint32 numPasses);
public:
    bool m_OptimizeNumTables;
    CEncoder *Encoder;
#ifdef COMPRESS_BZIP2_MT
    NWindows::CThread Thread;

    NWindows::NSynchronization::CAutoResetEvent StreamWasFinishedEvent;
    NWindows::NSynchronization::CAutoResetEvent WaitingWasStartedEvent;

    // it's not member of this thread. We just need one event per thread
    NWindows::NSynchronization::CAutoResetEvent CanWriteEvent;

    quint64 m_PackSize;

    quint8 MtPad[1 << 8]; // It's pad for Multi-Threading. Must be >= Cache_Line_Size.
    HRes Create();
    void FinishStream(bool needLeave);
    DWORD ThreadFunc();
#endif

    CThreadInfo(): m_BlockSorterIndex(0), m_Block(0) {}
    ~CThreadInfo() {
        Free();
    }
    bool Alloc();
    void Free();

    HRESULT EncodeBlock3(quint32 blockSize);
};

class CEncoder :
            public ICompressCoder,
            public ICompressSetCoderProperties,
#ifdef COMPRESS_BZIP2_MT
            public ICompressSetCoderMt,
#endif
            public CMyUnknownImp
{
    quint32 m_BlockSizeMult;
    bool m_OptimizeNumTables;

    quint32 m_NumPassesPrev;

    quint32 m_NumThreadsPrev;
public:
    CInBuffer m_InStream;
    quint8 MtPad[1 << 8]; // It's pad for Multi-Threading. Must be >= Cache_Line_Size.
    NStream::NMSBF::CEncoder<COutBuffer> m_OutStream;
    quint32 NumPasses;
    CBZip2CombinedCRC CombinedCRC;

#ifdef COMPRESS_BZIP2_MT
    CThreadInfo *ThreadsInfo;
    NWindows::NSynchronization::CManualResetEvent CanProcessEvent;
    NWindows::NSynchronization::CCriticalSection CS;
    quint32 NumThreads;
    bool MtMode;
    quint32 NextBlockIndex;

    bool CloseThreads;
    bool StreamWasFinished;
    NWindows::NSynchronization::CManualResetEvent CanStartWaitingEvent;

    HRESULT Result;
    ICompressProgressInfo *Progress;
#else
    CThreadInfo ThreadsInfo;
#endif

    quint32 ReadRleBlock(quint8 *buffer);
    void WriteBytes(const quint8 *data, quint32 sizeInBits, quint8 lastByte);

    void WriteBits(quint32 value, quint32 numBits);
    void WriteByte(quint8 b);
    void WriteBit(bool v);
    void WriteCRC(quint32 v);

#ifdef COMPRESS_BZIP2_MT
    HRes Create();
    void Free();
#endif

public:
    CEncoder();
#ifdef COMPRESS_BZIP2_MT
    ~CEncoder();
#endif

    HRESULT Flush() {
        return m_OutStream.Flush();
    }

    void ReleaseStreams() {
        m_InStream.ReleaseStream();
        m_OutStream.ReleaseStream();
    }

    class CFlusher
    {
        CEncoder *_coder;
    public:
        bool NeedFlush;
        CFlusher(CEncoder *coder): _coder(coder), NeedFlush(true) {}
        ~CFlusher() {
            if (NeedFlush)
                _coder->Flush();
            _coder->ReleaseStreams();
        }
    };

    HRESULT CodeReal(ISequentialInStream *inStream,
                     ISequentialOutStream *outStream, const quint64 *inSize, const quint64 *outSize,
                     ICompressProgressInfo *progress);

    STDMETHOD(Code)(ISequentialInStream *inStream,
                    ISequentialOutStream *outStream, const quint64 *inSize, const quint64 *outSize,
                    ICompressProgressInfo *progress);
    STDMETHOD(SetCoderProperties)(const PROPID *propIDs,
                                  const QVariant *properties, quint32 numProperties);
};
#endif

}
}

#endif
