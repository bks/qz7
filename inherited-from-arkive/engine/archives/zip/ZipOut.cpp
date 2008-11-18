// ZipOut.cpp


#include "ZipOut.h"
#include "kernel/OffsetStream.h"
#include "kernel/StreamUtils.h"

namespace NArchive
{
namespace NZip {

HRESULT COutArchive::Create(IOutStream *outStream)
{
    if (!m_OutBuffer.Create(1 << 16))
        return E_OUTOFMEMORY;
    m_Stream = outStream;
    m_OutBuffer.SetStream(outStream);
    m_OutBuffer.Init();
    m_BasePosition = 0;

    return S_OK;
}

void COutArchive::MoveBasePosition(quint64 distanceToMove)
{
    m_BasePosition += distanceToMove; // test overflow
}

void COutArchive::PrepareWriteCompressedDataZip64(quint16 fileNameLength, bool isZip64, bool aesEncryption)
{
    m_IsZip64 = isZip64;
    m_ExtraSize = isZip64 ? (4 + 8 + 8) : 0;
    if (aesEncryption)
        m_ExtraSize += 4 + 7;
    m_LocalFileHeaderSize = 4 + NFileHeader::kLocalBlockSize + fileNameLength + m_ExtraSize;
}

void COutArchive::PrepareWriteCompressedData(quint16 fileNameLength, quint64 unPackSize, bool aesEncryption)
{
    // We test it to 0xF8000000 to support case when compressed size
    // can be larger than uncompressed size.
    PrepareWriteCompressedDataZip64(fileNameLength, unPackSize >= 0xF8000000, aesEncryption);
}

void COutArchive::PrepareWriteCompressedData2(quint16 fileNameLength, quint64 unPackSize, quint64 packSize, bool aesEncryption)
{
    bool isUnPack64 = unPackSize >= 0xFFFFFFFF;
    bool isPack64 = packSize >= 0xFFFFFFFF;
    bool isZip64 = isPack64 || isUnPack64;
    PrepareWriteCompressedDataZip64(fileNameLength, isZip64, aesEncryption);
}

void COutArchive::WriteBytes(const void *buffer, quint32 size)
{
    m_OutBuffer.WriteBytes(buffer, size);
    m_BasePosition += size;
}

void COutArchive::WriteByte(quint8 b)
{
    WriteBytes(&b, 1);
}

void COutArchive::WriteUInt16(quint16 value)
{
    for (int i = 0; i < 2; i++) {
        WriteByte((quint8)value);
        value >>= 8;
    }
}

void COutArchive::WriteUInt32(quint32 value)
{
    for (int i = 0; i < 4; i++) {
        WriteByte((quint8)value);
        value >>= 8;
    }
}

void COutArchive::WriteUInt64(quint64 value)
{
    for (int i = 0; i < 8; i++) {
        WriteByte((quint8)value);
        value >>= 8;
    }
}

void COutArchive::WriteExtra(const CExtraBlock &extra)
{
    if (extra.SubBlocks.size() != 0) {
        for (int i = 0; i < extra.SubBlocks.size(); i++) {
            const CExtraSubBlock &subBlock = *extra.SubBlocks[i];
            WriteUInt16(subBlock.ID);
            WriteUInt16((quint16)subBlock.Data.GetCapacity());
            WriteBytes(subBlock.Data, (quint32)subBlock.Data.GetCapacity());
        }
    }
}

HRESULT COutArchive::WriteLocalHeader(const CLocalItem &item)
{
    RINOK(m_Stream->Seek(m_BasePosition, STREAM_SEEK_SET, 0));

    bool isZip64 = m_IsZip64 || item.PackSize >= 0xFFFFFFFF || item.UnPackSize >= 0xFFFFFFFF;

    WriteUInt32(NSignature::kLocalFileHeader);
    WriteByte(item.ExtractVersion.Version);
    WriteByte(item.ExtractVersion.HostOS);
    WriteUInt16(item.Flags);
    WriteUInt16(item.CompressionMethod);
    WriteUInt32(item.Time);
    WriteUInt32(item.FileCRC);
    WriteUInt32(isZip64 ? 0xFFFFFFFF : (quint32)item.PackSize);
    WriteUInt32(isZip64 ? 0xFFFFFFFF : (quint32)item.UnPackSize);
    WriteUInt16((quint16)item.Name.length());
    {
        quint16 localExtraSize = (quint16)((isZip64 ? (4 + 16) : 0) + item.LocalExtra.GetSize());
        if (localExtraSize > m_ExtraSize)
            return E_FAIL;
    }
    WriteUInt16((quint16)m_ExtraSize); // test it;
    WriteBytes((const char *)item.Name, item.Name.length());

    quint32 extraPos = 0;
    if (isZip64) {
        extraPos += 4 + 16;
        WriteUInt16(NFileHeader::NExtraID::kZip64);
        WriteUInt16(16);
        WriteUInt64(item.UnPackSize);
        WriteUInt64(item.PackSize);
    }

    WriteExtra(item.LocalExtra);
    extraPos += (quint32)item.LocalExtra.GetSize();
    for (; extraPos < m_ExtraSize; extraPos++)
        WriteByte(0);

    m_OutBuffer.FlushWithCheck();
    MoveBasePosition(item.PackSize);

    return m_Stream->Seek(m_BasePosition, STREAM_SEEK_SET, 0);
}

void COutArchive::WriteCentralHeader(const CItem &item)
{
    bool isUnPack64 = item.UnPackSize >= 0xFFFFFFFF;
    bool isPack64 = item.PackSize >= 0xFFFFFFFF;
    bool isPosition64 = item.LocalHeaderPosition >= 0xFFFFFFFF;
    bool isZip64  = isPack64 || isUnPack64 || isPosition64;

    WriteUInt32(NSignature::kCentralFileHeader);
    WriteByte(item.MadeByVersion.Version);
    WriteByte(item.MadeByVersion.HostOS);
    WriteByte(item.ExtractVersion.Version);
    WriteByte(item.ExtractVersion.HostOS);
    WriteUInt16(item.Flags);
    WriteUInt16(item.CompressionMethod);
    WriteUInt32(item.Time);
    WriteUInt32(item.FileCRC);
    WriteUInt32(isPack64 ? 0xFFFFFFFF : (quint32)item.PackSize);
    WriteUInt32(isUnPack64 ? 0xFFFFFFFF : (quint32)item.UnPackSize);
    WriteUInt16((quint16)item.Name.length());
    quint16 zip64ExtraSize = (quint16)((isUnPack64 ? 8 : 0) + (isPack64 ? 8 : 0) + (isPosition64 ? 8 : 0));
    quint16 centralExtraSize = (quint16)(isZip64 ? (4 + zip64ExtraSize) : 0);
    centralExtraSize = (quint16)(centralExtraSize + item.CentralExtra.GetSize());
    WriteUInt16(centralExtraSize); // test it;
    WriteUInt16((quint16)item.Comment.GetCapacity());
    WriteUInt16(0); // DiskNumberStart;
    WriteUInt16(item.InternalAttributes);
    WriteUInt32(item.ExternalAttributes);
    WriteUInt32(isPosition64 ? 0xFFFFFFFF : (quint32)item.LocalHeaderPosition);
    WriteBytes((const char *)item.Name, item.Name.length());
    if (isZip64) {
        WriteUInt16(NFileHeader::NExtraID::kZip64);
        WriteUInt16(zip64ExtraSize);
        if (isUnPack64)
            WriteUInt64(item.UnPackSize);
        if (isPack64)
            WriteUInt64(item.PackSize);
        if (isPosition64)
            WriteUInt64(item.LocalHeaderPosition);
    }
    WriteExtra(item.CentralExtra);
    if (item.Comment.GetCapacity() > 0)
        WriteBytes(item.Comment, (quint32)item.Comment.GetCapacity());
}

HRESULT COutArchive::WriteCentralDir(const QList<CItem *> &items, const CByteBuffer &comment)
{
    RINOK(m_Stream->Seek(m_BasePosition, STREAM_SEEK_SET, 0));

    quint64 cdOffset = GetCurrentPosition();
    for (int i = 0; i < items.size(); i++)
        WriteCentralHeader(*items[i]);
    quint64 cd64EndOffset = GetCurrentPosition();
    quint64 cdSize = cd64EndOffset - cdOffset;
    bool cdOffset64 = cdOffset >= 0xFFFFFFFF;
    bool cdSize64 = cdSize >= 0xFFFFFFFF;
    bool items64 = items.size() >= 0xFFFF;
    bool isZip64 = (cdOffset64 || cdSize64 || items64);

    if (isZip64) {
        WriteUInt32(NSignature::kZip64EndOfCentralDir);
        WriteUInt64(kZip64EcdSize); // ThisDiskNumber = 0;
        WriteUInt16(45); // version
        WriteUInt16(45); // version
        WriteUInt32(0); // ThisDiskNumber = 0;
        WriteUInt32(0); // StartCentralDirectoryDiskNumber;;
        WriteUInt64((quint64)items.size());
        WriteUInt64((quint64)items.size());
        WriteUInt64((quint64)cdSize);
        WriteUInt64((quint64)cdOffset);

        WriteUInt32(NSignature::kZip64EndOfCentralDirLocator);
        WriteUInt32(0); // number of the disk with the start of the zip64 end of central directory
        WriteUInt64(cd64EndOffset);
        WriteUInt32(1); // total number of disks
    }
    WriteUInt32(NSignature::kEndOfCentralDir);
    WriteUInt16(0); // ThisDiskNumber = 0;
    WriteUInt16(0); // StartCentralDirectoryDiskNumber;
    WriteUInt16((quint16)(items64 ? 0xFFFF : items.size()));
    WriteUInt16((quint16)(items64 ? 0xFFFF : items.size()));
    WriteUInt32(cdSize64 ? 0xFFFFFFFF : (quint32)cdSize);
    WriteUInt32(cdOffset64 ? 0xFFFFFFFF : (quint32)cdOffset);
    quint16 commentSize = (quint16)comment.GetCapacity();
    WriteUInt16(commentSize);
    if (commentSize > 0)
        WriteBytes((const quint8 *)comment, commentSize);
    m_OutBuffer.FlushWithCheck();

    return S_OK;
}

void COutArchive::CreateStreamForCompressing(IOutStream **outStream)
{
    COffsetOutStream *streamSpec = new COffsetOutStream;
    streamSpec->Init(m_Stream, m_BasePosition + m_LocalFileHeaderSize);
    *outStream = streamSpec;
}

HRESULT COutArchive::SeekToPackedDataPosition()
{
    return m_Stream->Seek(m_BasePosition + m_LocalFileHeaderSize, STREAM_SEEK_SET, 0);
}

void COutArchive::CreateStreamForCopying(ISequentialOutStream **outStream)
{
    *outStream = m_Stream;
}

}
}
