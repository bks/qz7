// ZipOut.h

#ifndef __ZIP_OUT_H
#define __ZIP_OUT_H

#include "interfaces/ObjectBasics.h"

#include "interfaces/IStream.h"
#include "kernel/OutBuffer.h"

#include "ZipItem.h"

#include <QtCore/QList>

namespace NArchive
{
namespace NZip {

// can throw CSystemException and COutBufferException

class COutArchive
{
    RefPtr<IOutStream> m_Stream;
    COutBuffer m_OutBuffer;

    quint64 m_BasePosition;
    quint32 m_LocalFileHeaderSize;
    quint32 m_ExtraSize;
    bool m_IsZip64;

    void WriteBytes(const void *buffer, quint32 size);
    void WriteByte(quint8 b);
    void WriteUInt16(quint16 value);
    void WriteUInt32(quint32 value);
    void WriteUInt64(quint64 value);

    void WriteExtraHeader(const CItem &item);
    void WriteCentralHeader(const CItem &item);
    void WriteExtra(const CExtraBlock &extra);
public:
    HRESULT Create(IOutStream *outStream);
    void MoveBasePosition(quint64 distanceToMove);
    quint64 GetCurrentPosition() const {
        return m_BasePosition;
    };
    void PrepareWriteCompressedDataZip64(quint16 fileNameLength, bool isZip64, bool aesEncryption);
    void PrepareWriteCompressedData(quint16 fileNameLength, quint64 unPackSize, bool aesEncryption);
    void PrepareWriteCompressedData2(quint16 fileNameLength, quint64 unPackSize, quint64 packSize, bool aesEncryption);
    HRESULT WriteLocalHeader(const CLocalItem &item);

    HRESULT WriteCentralDir(const QList<CItem *> &items, const CByteBuffer &comment);

    void CreateStreamForCompressing(IOutStream **outStream);
    void CreateStreamForCopying(ISequentialOutStream **outStream);
    HRESULT SeekToPackedDataPosition();
};

}
}

#endif
