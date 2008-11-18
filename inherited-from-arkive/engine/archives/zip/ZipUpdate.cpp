#include <stdio.h>

#include "ZipUpdate.h"
#include "ZipAddCommon.h"
#include "ZipOut.h"

#ifdef THREADED
#include "kernel/ProgressMt.h"
#endif
#include "kernel/LimitedStreams.h"
#include "kernel/OutMemStream.h"
#include "kernel/CopyCoder.h"
#include "kernel/HandlerRegistry.h"

namespace NArchive
{
namespace NZip {

#ifdef _WIN32
static const quint8 kMadeByHostOS = NFileHeader::NHostOS::kFAT;
static const quint8 kExtractHostOS = NFileHeader::NHostOS::kFAT;
#else
static const quint8 kMadeByHostOS = NFileHeader::NHostOS::kUnix;
static const quint8 kExtractHostOS = NFileHeader::NHostOS::kUnix;
#endif

static const quint8 kMethodForDirectory = NFileHeader::NCompressionMethod::kStored;
static const quint8 kExtractVersionForDirectory = NFileHeader::NCompressionMethod::kStoreExtractVersion;

static HRESULT CopyBlockToArchive(ISequentialInStream *inStream,
                                  COutArchive &outArchive)
{
    ISequentialOutStream *poutStream;
    outArchive.CreateStreamForCopying(&poutStream);
    RefPtr<ISequentialOutStream> outStream(poutStream);
    RefPtr<ICompressCoder> copyCoder = new NCompress::CCopyCoder;
    return copyCoder->Code(inStream, outStream, NULL, NULL);
}

static HRESULT WriteRange(IInStream *inStream, COutArchive &outArchive,
                          const CUpdateRange &range)
{
    quint64 position;
    RINOK(inStream->Seek(range.Position, STREAM_SEEK_SET, &position));

    CLimitedSequentialInStream *streamSpec = new CLimitedSequentialInStream;
    RefPtr<CLimitedSequentialInStream> inStreamLimited(streamSpec);
    streamSpec->SetStream(inStream);
    streamSpec->Init(range.Size);

    RINOK(CopyBlockToArchive(inStreamLimited, outArchive));
}

static void SetFileHeader(
    COutArchive &archive,
    const CCompressionMethodMode &options,
    const CUpdateItem &updateItem,
    CItem &item)
{
    item.UnPackSize = updateItem.Size;
    bool isDirectory;

    if (updateItem.NewProperties) {
        isDirectory = updateItem.IsDirectory;
        item.Name = updateItem.Name;
        item.ExternalAttributes = updateItem.Attributes;
        item.Time = updateItem.Time;
    } else
        isDirectory = item.IsDirectory();

    item.LocalHeaderPosition = archive.GetCurrentPosition();
    item.MadeByVersion.HostOS = kMadeByHostOS;
    item.MadeByVersion.Version = NFileHeader::NCompressionMethod::kMadeByProgramVersion;

    item.ExtractVersion.HostOS = kExtractHostOS;

    item.InternalAttributes = 0; // test it
    item.ClearFlags();
    item.SetEncrypted(!isDirectory && options.PasswordIsDefined);
    if (isDirectory) {
        item.ExtractVersion.Version = kExtractVersionForDirectory;
        item.CompressionMethod = kMethodForDirectory;
        item.PackSize = 0;
        item.FileCRC = 0; // test it
    }
}

static void SetItemInfoFromCompressingResult(const CCompressingResult &compressingResult,
        bool isAesMode, quint8 aesKeyMode, CItem &item)
{
    item.ExtractVersion.Version = compressingResult.ExtractVersion;
    item.CompressionMethod = compressingResult.Method;
    item.FileCRC = compressingResult.CRC;
    item.UnPackSize = compressingResult.UnpackSize;
    item.PackSize = compressingResult.PackSize;

    item.LocalExtra.Clear();
    item.CentralExtra.Clear();

    if (isAesMode) {
        CWzAesExtraField wzAesField;
        wzAesField.Strength = aesKeyMode;
        wzAesField.Method = compressingResult.Method;
        item.CompressionMethod = NFileHeader::NCompressionMethod::kWzAES;
        item.FileCRC = 0;
        CExtraSubBlock sb;
        wzAesField.SetSubBlock(sb);
        item.LocalExtra.SubBlocks.append(new CExtraSubBlock(sb));
        item.CentralExtra.SubBlocks.append(new CExtraSubBlock(sb));
    }
}

#ifdef THREADED

static THREAD_FUNC_DECL CoderThread(void *threadCoderInfo);

struct CThreadInfo {
#ifdef EXTERNAL_CODECS
    RefPtr<ICompressCodecsInfo> _codecsInfo;
    const QList<CCodecInfoEx *> *_externalCodecs;
#endif

    NWindows::CThread Thread;
    NWindows::NSynchronization::CAutoResetEvent CompressEvent;
    NWindows::NSynchronization::CAutoResetEvent CompressionCompletedEvent;
    bool ExitThread;

    CMtCompressProgress *ProgressSpec;
    RefPtr<ICompressProgressInfo> Progress;

    COutMemStream *OutStreamSpec;
    RefPtr<IOutStream> OutStream;
    RefPtr<ISequentialInStream> InStream;

    CAddCommon Coder;
    HRESULT Result;
    CCompressingResult CompressingResult;

    bool IsFree;
    quint32 UpdateIndex;

    CThreadInfo(const CCompressionMethodMode &options):
            ExitThread(false),
            ProgressSpec(0),
            OutStreamSpec(0),
            Coder(options) {}

    HRESULT CreateEvents() {
        RINOK(CompressEvent.CreateIfNotCreated());
        return CompressionCompletedEvent.CreateIfNotCreated();
    }
    HRes CreateThread() {
        return Thread.Create(CoderThread, this);
    }

    void WaitAndCode();
    void StopWaitClose() {
        ExitThread = true;
        if (OutStreamSpec != 0)
            OutStreamSpec->StopWriting(E_ABORT);
        if (CompressEvent.IsCreated())
            CompressEvent.Set();
        Thread.Wait();
        Thread.Close();
    }

};

void CThreadInfo::WaitAndCode()
{
    for (;;) {
        CompressEvent.Lock();
        if (ExitThread)
            return;
        Result = Coder.Compress(
#ifdef EXTERNAL_CODECS
                     _codecsInfo, _externalCodecs,
#endif
                     InStream, OutStream, Progress, CompressingResult);
        if (Result == S_OK && Progress)
            Result = Progress->SetRatioInfo(&CompressingResult.UnpackSize, &CompressingResult.PackSize);
        CompressionCompletedEvent.Set();
    }
}

static THREAD_FUNC_DECL CoderThread(void *threadCoderInfo)
{
    ((CThreadInfo *)threadCoderInfo)->WaitAndCode();
    return 0;
}

class CThreads
{
public:
    CObjectVector<CThreadInfo> Threads;
    ~CThreads() {
        for (int i = 0; i < Threads.Size(); i++)
            Threads[i].StopWaitClose();
    }
};

struct CMemBlocks2: public CMemLockBlocks {
    CCompressingResult CompressingResult;
    bool Defined;
    bool Skip;
    CMemBlocks2(): Defined(false), Skip(false) {}
};

class CMemRefs
{
public:
    CMemBlockManagerMt *Manager;
    CObjectVector<CMemBlocks2> Refs;
    CMemRefs(CMemBlockManagerMt *manager): Manager(manager) {} ;
    ~CMemRefs() {
        for (int i = 0; i < Refs.Size(); i++)
            Refs[i].FreeOpt(Manager);
    }
};

class CMtProgressMixer2:
            public ICompressProgressInfo,
            public CMyUnknownImp
{
    quint64 ProgressOffset;
    quint64 InSizes[2];
    quint64 OutSizes[2];
    RefPtr<IProgress> Progress;
    RefPtr<ICompressProgressInfo> RatioProgress;
    bool _inSizeIsMain;
public:
    NWindows::NSynchronization::CCriticalSection CriticalSection;
    MY_UNKNOWN_IMP
    void Create(IProgress *progress, bool inSizeIsMain);
    void SetProgressOffset(quint64 progressOffset);
    HRESULT SetRatioInfo(int index, const quint64 *inSize, const quint64 *outSize);
    STDMETHOD(SetRatioInfo)(const quint64 *inSize, const quint64 *outSize);
};

void CMtProgressMixer2::Create(IProgress *progress, bool inSizeIsMain)
{
    Progress = progress;
    Progress.QueryInterface(IID_ICompressProgressInfo, &RatioProgress);
    _inSizeIsMain = inSizeIsMain;
    ProgressOffset = InSizes[0] = InSizes[1] = OutSizes[0] = OutSizes[1] = 0;
}

void CMtProgressMixer2::SetProgressOffset(quint64 progressOffset)
{
    CriticalSection.Enter();
    InSizes[1] = OutSizes[1] = 0;
    ProgressOffset = progressOffset;
    CriticalSection.Leave();
}

HRESULT CMtProgressMixer2::SetRatioInfo(int index, const quint64 *inSize, const quint64 *outSize)
{
    NWindows::NSynchronization::CCriticalSectionLock lock(CriticalSection);
    if (index == 0 && RatioProgress) {
        RINOK(RatioProgress->SetRatioInfo(inSize, outSize));
    }
    if (inSize != 0)
        InSizes[index] = *inSize;
    if (outSize != 0)
        OutSizes[index] = *outSize;
    quint64 v = ProgressOffset + (_inSizeIsMain  ?
                                  (InSizes[0] + InSizes[1]) :
                                  (OutSizes[0] + OutSizes[1]));
    return Progress->SetCompleted(&v);
}

STDMETHODIMP CMtProgressMixer2::SetRatioInfo(const quint64 *inSize, const quint64 *outSize)
{
    return SetRatioInfo(0, inSize, outSize);
}

class CMtProgressMixer:
            public ICompressProgressInfo,
            public CMyUnknownImp
{
public:
    CMtProgressMixer2 *Mixer2;
    RefPtr<ICompressProgressInfo> RatioProgress;
    void Create(IProgress *progress, bool inSizeIsMain);
    MY_UNKNOWN_IMP
    STDMETHOD(SetRatioInfo)(const quint64 *inSize, const quint64 *outSize);
};

void CMtProgressMixer::Create(IProgress *progress, bool inSizeIsMain)
{
    Mixer2 = new CMtProgressMixer2;
    RatioProgress = Mixer2;
    Mixer2->Create(progress, inSizeIsMain);
}

STDMETHODIMP CMtProgressMixer::SetRatioInfo(const quint64 *inSize, const quint64 *outSize)
{
    return Mixer2->SetRatioInfo(1, inSize, outSize);
}


#endif


static HRESULT UpdateItemOldData(COutArchive &archive,
                                 IInStream *inStream,
                                 const CUpdateItem &updateItem, CItemEx &item,
                                 /* bool izZip64, */
                                 quint64 &complexity)
{
    if (updateItem.NewProperties) {
        if (item.HasDescriptor())
            return E_NOTIMPL;

        // use old name size.
        // CUpdateRange range(item.GetLocalExtraPosition(), item.LocalExtraSize + item.PackSize);
        CUpdateRange range(item.GetDataPosition(), item.PackSize);

        // item.ExternalAttributes = updateItem.Attributes;
        // Test it
        item.Name = updateItem.Name;
        item.Time = updateItem.Time;
        item.CentralExtra.RemoveUnknownSubBlocks();
        item.LocalExtra.RemoveUnknownSubBlocks();

        archive.PrepareWriteCompressedData2((quint16)item.Name.length(), item.UnPackSize, item.PackSize, item.LocalExtra.HasWzAesField());
        item.LocalHeaderPosition = archive.GetCurrentPosition();
        RINOK(archive.SeekToPackedDataPosition());
        RINOK(WriteRange(inStream, archive, range));
        // emit compressionRatio ?
        complexity += range.Size;
        RINOK(archive.WriteLocalHeader(item));
    } else {
        CUpdateRange range(item.LocalHeaderPosition, item.GetLocalFullSize());

        // set new header position
        item.LocalHeaderPosition = archive.GetCurrentPosition();

        RINOK(WriteRange(inStream, archive, range));
        complexity += range.Size;
        archive.MoveBasePosition(range.Size);
    }
    return S_OK;
}

static HRESULT WriteDirHeader(COutArchive &archive, const CCompressionMethodMode *options,
                           const CUpdateItem &updateItem, CItemEx &item)
{
    SetFileHeader(archive, *options, updateItem, item);
    archive.PrepareWriteCompressedData((quint16)item.Name.length(), updateItem.Size, options->IsAesMode);
    return archive.WriteLocalHeader(item);
}

static HRESULT Update2St(
    COutArchive &archive,
    CInArchive *inArchive,
    IInStream *inStream,
    const QList<CItemEx *> &inputItems,
    const QList<CUpdateItem *> &updateItems,
    const CCompressionMethodMode *options,
    const CByteBuffer &comment,
    IArchiveUpdateCallback *updateCallback)
{
    CAddCommon compressor(*options);

    QList<CItem *> items;
    quint64 unpackSizeTotal = 0, packSizeTotal = 0;

    for (int itemIndex = 0; itemIndex < updateItems.size(); itemIndex++) {
        const CUpdateItem &updateItem = *updateItems[itemIndex];
        CItemEx item;
        if (!updateItem.NewProperties || !updateItem.NewData) {
            item = *inputItems[updateItem.IndexInArchive];
            if (inArchive->ReadLocalItemAfterCdItemFull(item) != S_OK)
                return E_NOTIMPL;
        }

        if (updateItem.NewData) {
            bool isDirectory = ((updateItem.NewProperties) ? updateItem.IsDirectory : item.IsDirectory());
            if (isDirectory) {
                WriteDirHeader(archive, options, updateItem, item);
            } else {
                ISequentialInStream *pfileInStream;
                HRESULT res = updateCallback->GetStream(updateItem.IndexInClient, &pfileInStream);
                RefPtr<ISequentialInStream> fileInStream(pfileInStream);
                if (res == S_FALSE) {
                    RINOK(updateCallback->SetOperationResult(NArchive::NUpdate::NOperationResult::kOK));
                    continue;
                }
                RINOK(res);

                // file Size can be 64-bit !!!
                SetFileHeader(archive, *options, updateItem, item);
                archive.PrepareWriteCompressedData((quint16)item.Name.length(), updateItem.Size, options->IsAesMode);
                CCompressingResult compressingResult;
                IOutStream *poutStream;
                archive.CreateStreamForCompressing(&poutStream);
                RefPtr<IOutStream> outStream(poutStream);
                RINOK(compressor.Compress(
                          fileInStream, outStream, compressingResult));
                SetItemInfoFromCompressingResult(compressingResult, options->IsAesMode, options->AesKeyMode, item);
                RINOK(archive.WriteLocalHeader(item));
                RINOK(updateCallback->SetOperationResult(NArchive::NUpdate::NOperationResult::kOK));
                unpackSizeTotal += item.UnPackSize;
                packSizeTotal += item.PackSize;
            }
        } else {
            quint64 complexity = 0;
            RINOK(UpdateItemOldData(archive, inStream, updateItem, item, complexity));
        }
        items.append(new CItemEx(item));
    }
    return archive.WriteCentralDir(items, comment);
}

static HRESULT Update2(
    COutArchive &archive,
    CInArchive *inArchive,
    IInStream *inStream,
    const QList<CItemEx *> &inputItems,
    const QList<CUpdateItem *> &updateItems,
    const CCompressionMethodMode *options,
    const CByteBuffer &comment,
    IArchiveUpdateCallback *updateCallback)
{
    quint64 complexity = 0;
    quint64 numFilesToCompress = 0;
    quint64 numBytesToCompress = 0;

    int i;
    for (i = 0; i < updateItems.size(); i++) {
        const CUpdateItem &updateItem = *updateItems[i];
        if (updateItem.NewData) {
            complexity += updateItem.Size;
            numBytesToCompress += updateItem.Size;
            numFilesToCompress++;
            /*
            if (updateItem.Commented)
              complexity += updateItem.CommentRange.Size;
            */
        } else {
            CItemEx inputItem = *inputItems[updateItem.IndexInArchive];
            if (inArchive->ReadLocalItemAfterCdItemFull(inputItem) != S_OK)
                return E_NOTIMPL;
            complexity += inputItem.GetLocalFullSize();
            // complexity += inputItem.GetCentralExtraPlusCommentSize();
        }
        complexity += NFileHeader::kLocalBlockSize;
        complexity += NFileHeader::kCentralBlockSize;
    }

    if (comment != 0)
        complexity += comment.GetCapacity();
    complexity++; // end of central
    updateCallback->SetTotal(complexity);

    CAddCommon compressor(*options);

    complexity = 0;

#ifdef THREADED

    const size_t kNumMaxThreads = (1 << 10);
    quint32 numThreads = options->NumThreads;
    if (numThreads > kNumMaxThreads)
        numThreads = kNumMaxThreads;

    const size_t kMemPerThread = (1 << 25);
    const size_t kBlockSize = 1 << 16;

    CCompressionMethodMode options2;
    if (options != 0)
        options2 = *options;

    bool mtMode = ((options != 0) && (numThreads > 1));

    if (numFilesToCompress <= 1)
        mtMode = false;

    if (mtMode) {
        quint8 method = options->MethodSequence.Front();
        if (method == NFileHeader::NCompressionMethod::kStored && !options->PasswordIsDefined)
            mtMode = false;
        if (method == NFileHeader::NCompressionMethod::kBZip2) {
            quint64 averageSize = numBytesToCompress / numFilesToCompress;
            quint32 blockSize = options->DicSize;
            if (blockSize == 0)
                blockSize = 1;
            quint64 averageNumberOfBlocks = averageSize / blockSize;
            quint32 numBZip2Threads = 32;
            if (averageNumberOfBlocks < numBZip2Threads)
                numBZip2Threads = (quint32)averageNumberOfBlocks;
            if (numBZip2Threads < 1)
                numBZip2Threads = 1;
            numThreads = numThreads / numBZip2Threads;
            options2.NumThreads = numBZip2Threads;
            if (numThreads <= 1)
                mtMode = false;
        }
    }

    if (!mtMode)
#endif
        return Update2St(
                   archive, inArchive, inStream,
                   inputItems, updateItems, options, comment, updateCallback);


#ifdef THREADED

    CObjectVector<CItem> items;

    CMtProgressMixer *mtProgressMixerSpec = new CMtProgressMixer;
    RefPtr<ICompressProgressInfo> progress = mtProgressMixerSpec;
    mtProgressMixerSpec->Create(updateCallback, true);

    CMtCompressProgressMixer mtCompressProgressMixer;
    mtCompressProgressMixer.Init(numThreads, mtProgressMixerSpec->RatioProgress);

    CMemBlockManagerMt memManager(kBlockSize);
    CMemRefs refs(&memManager);

    CThreads threads;
    QList<HANDLE> compressingCompletedEvents;
    QList<int> threadIndices;  // list threads in order of updateItems

    {
        RINOK(memManager.AllocateSpaceAlways((size_t)numThreads * (kMemPerThread / kBlockSize)));
        for (i = 0; i < updateItems.Size(); i++)
            refs.Refs.Add(CMemBlocks2());

        quint32 i;
        for (i = 0; i < numThreads; i++)
            threads.Threads.Add(CThreadInfo(options2));

        for (i = 0; i < numThreads; i++) {
            CThreadInfo &threadInfo = threads.Threads[i];
#ifdef EXTERNAL_CODECS
            threadInfo._codecsInfo = codecsInfo;
            threadInfo._externalCodecs = externalCodecs;
#endif
            RINOK(threadInfo.CreateEvents());
            threadInfo.OutStreamSpec = new COutMemStream(&memManager);
            RINOK(threadInfo.OutStreamSpec->CreateEvents());
            threadInfo.OutStream = threadInfo.OutStreamSpec;
            threadInfo.IsFree = true;
            threadInfo.ProgressSpec = new CMtCompressProgress();
            threadInfo.Progress = threadInfo.ProgressSpec;
            threadInfo.ProgressSpec->Init(&mtCompressProgressMixer, (int)i);
            RINOK(threadInfo.CreateThread());
        }
    }
    int mtItemIndex = 0;

    int itemIndex = 0;
    int lastRealStreamItemIndex = -1;

    while (itemIndex < updateItems.Size()) {
        if ((quint32)threadIndices.Size() < numThreads && mtItemIndex < updateItems.Size()) {
            const CUpdateItem &updateItem = updateItems[mtItemIndex++];
            if (!updateItem.NewData)
                continue;
            CItemEx item;
            if (updateItem.NewProperties) {
                if (updateItem.IsDirectory)
                    continue;
            } else {
                item = inputItems[updateItem.IndexInArchive];
                if (inArchive->ReadLocalItemAfterCdItemFull(item) != S_OK)
                    return E_NOTIMPL;
                if (item.IsDirectory())
                    continue;
            }
            RefPtr<ISequentialInStream> fileInStream;
            {
                NWindows::NSynchronization::CCriticalSectionLock lock(mtProgressMixerSpec->Mixer2->CriticalSection);
                HRESULT res = updateCallback->GetStream(updateItem.IndexInClient, &fileInStream);
                if (res == S_FALSE) {
                    complexity += updateItem.Size;
                    complexity += NFileHeader::kLocalBlockSize;
                    mtProgressMixerSpec->Mixer2->SetProgressOffset(complexity);
                    RINOK(updateCallback->SetOperationResult(NArchive::NUpdate::NOperationResult::kOK));
                    refs.Refs[mtItemIndex - 1].Skip = true;
                    continue;
                }
                RINOK(res);
                RINOK(updateCallback->SetOperationResult(NArchive::NUpdate::NOperationResult::kOK));
            }

            for (quint32 i = 0; i < numThreads; i++) {
                CThreadInfo &threadInfo = threads.Threads[i];
                if (threadInfo.IsFree) {
                    threadInfo.IsFree = false;
                    threadInfo.InStream = fileInStream;

                    // !!!!! we must release ref before sending event
                    // BUG was here in v4.43 and v4.44. It could change ref counter in two threads in same time
                    fileInStream.Release();

                    threadInfo.OutStreamSpec->Init();
                    threadInfo.ProgressSpec->Reinit();
                    threadInfo.CompressEvent.Set();
                    threadInfo.UpdateIndex = mtItemIndex - 1;

                    compressingCompletedEvents.Add(threadInfo.CompressionCompletedEvent);
                    threadIndices.Add(i);
                    break;
                }
            }
            continue;
        }

        if (refs.Refs[itemIndex].Skip) {
            itemIndex++;
            continue;
        }

        const CUpdateItem &updateItem = updateItems[itemIndex];

        CItemEx item;
        if (!updateItem.NewProperties || !updateItem.NewData) {
            item = inputItems[updateItem.IndexInArchive];
            if (inArchive->ReadLocalItemAfterCdItemFull(item) != S_OK)
                return E_NOTIMPL;
        }

        if (updateItem.NewData) {
            bool isDirectory = ((updateItem.NewProperties) ? updateItem.IsDirectory : item.IsDirectory());
            if (isDirectory) {
                WriteDirHeader(archive, options, updateItem, item);
            } else {
                if (lastRealStreamItemIndex < itemIndex) {
                    lastRealStreamItemIndex = itemIndex;
                    SetFileHeader(archive, *options, updateItem, item);
                    // file Size can be 64-bit !!!
                    archive.PrepareWriteCompressedData((quint16)item.Name.Length(), updateItem.Size, options->IsAesMode);
                }

                CMemBlocks2 &memRef = refs.Refs[itemIndex];
                if (memRef.Defined) {
                    RefPtr<IOutStream> outStream;
                    archive.CreateStreamForCompressing(&outStream);
                    memRef.WriteToStream(memManager.GetBlockSize(), outStream);
                    SetItemInfoFromCompressingResult(memRef.CompressingResult,
                                                     options->IsAesMode, options->AesKeyMode, item);
                    SetFileHeader(archive, *options, updateItem, item);
                    RINOK(archive.WriteLocalHeader(item));
                    // RINOK(updateCallback->SetOperationResult(NArchive::NUpdate::NOperationResult::kOK));
                    memRef.FreeOpt(&memManager);
                } else {
                    {
                        CThreadInfo &thread = threads.Threads[threadIndices.Front()];
                        if (!thread.OutStreamSpec->WasUnlockEventSent()) {
                            RefPtr<IOutStream> outStream;
                            archive.CreateStreamForCompressing(&outStream);
                            thread.OutStreamSpec->SetOutStream(outStream);
                            thread.OutStreamSpec->SetRealStreamMode();
                        }
                    }

                    DWORD result = ::WaitForMultipleObjects(compressingCompletedEvents.Size(),
                                                            &compressingCompletedEvents.Front(), FALSE, INFINITE);
                    int t = (int)(result - WAIT_OBJECT_0);
                    CThreadInfo &threadInfo = threads.Threads[threadIndices[t]];
                    threadInfo.InStream.Release();
                    threadInfo.IsFree = true;
                    RINOK(threadInfo.Result);
                    threadIndices.Delete(t);
                    compressingCompletedEvents.Delete(t);
                    if (t == 0) {
                        RINOK(threadInfo.OutStreamSpec->WriteToRealStream());
                        threadInfo.OutStreamSpec->ReleaseOutStream();
                        SetItemInfoFromCompressingResult(threadInfo.CompressingResult,
                                                         options->IsAesMode, options->AesKeyMode, item);
                        SetFileHeader(archive, *options, updateItem, item);
                        RINOK(archive.WriteLocalHeader(item));
                    } else {
                        CMemBlocks2 &memRef = refs.Refs[threadInfo.UpdateIndex];
                        threadInfo.OutStreamSpec->DetachData(memRef);
                        memRef.CompressingResult = threadInfo.CompressingResult;
                        memRef.Defined = true;
                        continue;
                    }
                }
            }
        } else {
            RINOK(UpdateItemOldData(archive, inStream, updateItem, item, progress, complexity));
        }
        items.Add(item);
        complexity += NFileHeader::kLocalBlockSize;
        mtProgressMixerSpec->Mixer2->SetProgressOffset(complexity);
        itemIndex++;
    }
    return archive.WriteCentralDir(items, comment);
#endif
}

HRESULT Update(
    const QList<CItemEx *> &inputItems,
    const QList<CUpdateItem *> &updateItems,
    ISequentialOutStream *_seqOutStream,
    CInArchive *inArchive,
    CCompressionMethodMode *compressionMethodMode,
    IArchiveUpdateCallback *updateCallback)
{
    RefPtr<ISequentialOutStream> seqOutStream(_seqOutStream);
    RefPtr<IOutStream> outStream;
    RINOK(seqOutStream.QueryInterface(&outStream));
    if (!outStream)
        return E_NOTIMPL;

    CInArchiveInfo archiveInfo;
    if (inArchive != 0) {
        inArchive->GetArchiveInfo(archiveInfo);
        if (archiveInfo.Base != 0)
            return E_NOTIMPL;
    } else
        archiveInfo.StartPosition = 0;

    COutArchive outArchive;
    outArchive.Create(outStream);
    if (archiveInfo.StartPosition > 0) {
        RefPtr<ISequentialInStream> inStream;
        inStream = inArchive->CreateLimitedStream(0, archiveInfo.StartPosition);
        RINOK(CopyBlockToArchive(inStream, outArchive));
        outArchive.MoveBasePosition(archiveInfo.StartPosition);
    }
    RefPtr<IInStream> inStream;
    if (inArchive != 0)
        inStream = inArchive->CreateStream();

    return Update2(
               outArchive, inArchive, inStream,
               inputItems, updateItems,
               compressionMethodMode,
               archiveInfo.Comment, updateCallback);
}

}
}
