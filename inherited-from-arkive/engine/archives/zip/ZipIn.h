// Archive/ZipIn.h

#ifndef __ZIP_IN_H
#define __ZIP_IN_H

#include "interfaces/ObjectBasics.h"
#include "interfaces/IStream.h"

#include <QtCore/qglobal.h>
#include <QtCore/QList>

#include "ZipHeader.h"
#include "ZipItemEx.h"

namespace NArchive
{
namespace NZip {

class CInArchiveInfo
{
public:
    quint64 Base;
    quint64 StartPosition;
    CByteBuffer Comment;
    CInArchiveInfo(): Base(0), StartPosition(0) {}
    void Clear() {
        Base = 0;
        StartPosition = 0;
        Comment.SetCapacity(0);
    }
};

class CProgressVirt
{
public:
    virtual ~CProgressVirt() {}
    STDMETHOD(SetCompleted)(const quint64 *numFiles) PURE;
};

struct CCdInfo {
    // quint64 NumEntries;
    quint64 Size;
    quint64 Offset;
};

class CInArchive
{
    RefPtr<IInStream> m_Stream;
    quint32 m_Signature;
    quint64 m_StreamStartPosition;
    quint64 m_Position;
    QByteArray m_NameBuffer;

    HRESULT Seek(quint64 offset);

    bool FindAndReadMarker(const quint64 *searchHeaderSizeLimit);
    HRESULT ReadFileName(quint32 nameSize, QByteArray *name);

    HRESULT ReadBytes(void *data, quint32 size, quint32 *processedSize);
    HRESULT ReadBytesRequired(void *data, quint32 size);
    HRESULT ReadBuffer(CByteBuffer &buffer, quint32 size);
    HRESULT ReadByte(quint8 *p);
    HRESULT ReadUInt16(quint16 *);
    HRESULT ReadUInt32(quint32 *);
    HRESULT ReadUInt64(quint64 *);

    HRESULT ReadExtra(quint32 extraSize, CExtraBlock &extraBlock,
                   quint64 &unpackSize, quint64 &packSize, quint64 &localHeaderOffset, quint32 &diskStartNumber);
    HRESULT ReadLocalItem(CItemEx &item);
    HRESULT ReadLocalItemDescriptor(CItemEx &item);
    HRESULT ReadCdItem(CItemEx &item);
    HRESULT TryEcd64(quint64 offset, CCdInfo &cdInfo);
    HRESULT FindCd(CCdInfo &cdInfo);
    HRESULT TryReadCd(QList<CItemEx *> &items, quint64 cdOffset, quint64 cdSize);
    HRESULT ReadCd(QList<CItemEx *> &items, quint64 &cdOffset, quint64 &cdSize);
    HRESULT ReadLocalsAndCd(QList<CItemEx *> &items, CProgressVirt *progress, quint64 &cdOffset);
public:
    CInArchiveInfo m_ArchiveInfo;
    bool IsZip64;

    HRESULT ReadHeaders(QList<CItemEx *> &items, CProgressVirt *progress);
    HRESULT ReadLocalItemAfterCdItem(CItemEx &item);
    HRESULT ReadLocalItemAfterCdItemFull(CItemEx &item);
    bool Open(IInStream *inStream, const quint64 *searchHeaderSizeLimit);
    void Close();
    void GetArchiveInfo(CInArchiveInfo &archiveInfo) const;
    bool SeekInArchive(quint64 position);
    ISequentialInStream *CreateLimitedStream(quint64 position, quint64 size);
    IInStream* CreateStream();
};

}
}

#endif
