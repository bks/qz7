// Archive/ZipIn.cpp
#include "ZipIn.h"
#include "kernel/DynamicBuffer.h"
#include "kernel/LimitedStreams.h"
#include "kernel/StreamUtils.h"

namespace NZip {

// static const char kEndOfString = '\0';

bool CInArchive::Open(IInStream *inStream, const quint64 *searchHeaderSizeLimit)
{
    m_Stream = inStream;
    if (m_Stream->Seek(0, STREAM_SEEK_CUR, &m_StreamStartPosition) != S_OK)
        return false;
    m_Position = m_StreamStartPosition;
    return FindAndReadMarker(searchHeaderSizeLimit);
}

void CInArchive::Close()
{
    m_Stream = 0;
}

HRESULT CInArchive::Seek(quint64 offset)
{
    return m_Stream->Seek(offset, STREAM_SEEK_SET, NULL);
}

//////////////////////////////////////
// Markers

static inline bool TestMarkerCandidate(const quint8 *p, quint32 &value)
{
    value = p[0] | (((quint32)p[1]) << 8) | (((quint32)p[2]) << 16) | (((quint32)p[3]) << 24);
    return (value == NSignature::kLocalFileHeader) ||
           (value == NSignature::kEndOfCentralDir);
}

static const quint32 kNumMarkerAddtionalBytes = 2;
static inline bool TestMarkerCandidate2(const quint8 *p, quint32 &value)
{
    value = p[0] | (((quint32)p[1]) << 8) | (((quint32)p[2]) << 16) | (((quint32)p[3]) << 24);
    if (value == NSignature::kEndOfCentralDir) {
        quint16 nextWord = p[0] | (((quint16)p[1]) << 8);
        return (nextWord == 0);
    }
    if (value != NSignature::kLocalFileHeader)
        return false;
    if (p[0] > 128)
        return false;
    return true;
}

bool CInArchive::FindAndReadMarker(const quint64 *searchHeaderSizeLimit)
{
    m_ArchiveInfo.Clear();
    m_Position = m_StreamStartPosition;
    if (Seek(m_StreamStartPosition) != S_OK)
        return false;

    quint8 marker[NSignature::kMarkerSize];
    quint32 processedSize;
    ReadBytes(marker, NSignature::kMarkerSize, &processedSize);
    if (processedSize != NSignature::kMarkerSize)
        return false;
    if (TestMarkerCandidate(marker, m_Signature))
        return true;

    CByteDynamicBuffer dynamicBuffer;
    static const quint32 kSearchMarkerBufferSize = 0x10000;
    dynamicBuffer.EnsureCapacity(kSearchMarkerBufferSize);
    quint8 *buffer = dynamicBuffer;
    quint32 numBytesPrev = NSignature::kMarkerSize - 1;
    memmove(buffer, marker + 1, numBytesPrev);
    quint64 curTestPos = m_StreamStartPosition + 1;
    for (;;) {
        if (searchHeaderSizeLimit != NULL)
            if (curTestPos - m_StreamStartPosition > *searchHeaderSizeLimit)
                break;
        quint32 numReadBytes = kSearchMarkerBufferSize - numBytesPrev;
        ReadBytes(buffer + numBytesPrev, numReadBytes, &processedSize);
        quint32 numBytesInBuffer = numBytesPrev + processedSize;
        const quint32 kMarker2Size = NSignature::kMarkerSize + kNumMarkerAddtionalBytes;
        if (numBytesInBuffer < kMarker2Size)
            break;
        quint32 numTests = numBytesInBuffer - kMarker2Size + 1;
        for (quint32 pos = 0; pos < numTests; pos++, curTestPos++) {
            if (TestMarkerCandidate2(buffer + pos, m_Signature)) {
                m_ArchiveInfo.StartPosition = curTestPos;
                // m_ArchiveInfo.Base = m_ArchiveInfo.StartPosition;
                // m_ArchiveInfo.Base = 0;
                m_Position = curTestPos + NSignature::kMarkerSize;
                if (Seek(m_Position) != S_OK)
                    return false;
                return true;
            }
        }
        numBytesPrev = numBytesInBuffer - numTests;
        memmove(buffer, buffer + numTests, numBytesPrev);
    }
    return false;
}

HRESULT CInArchive::ReadBytes(void *data, quint32 size, quint32 *processedSize)
{
    quint32 realProcessedSize;
    HRESULT result = ReadStream(m_Stream, data, size, &realProcessedSize);
    if (processedSize != NULL)
        *processedSize = realProcessedSize;
    m_Position += realProcessedSize;
    return result;
}

HRESULT CInArchive::ReadBytesRequired(void *data, quint32 size)
{
    quint32 realProcessedSize;
    HRESULT ret;
    if ((ret = ReadBytes(data, size, &realProcessedSize)) != S_OK)
        return ret;
    return (realProcessedSize == size) ? S_OK : E_UNEXPECTED_END;
}

HRESULT CInArchive::ReadBuffer(CByteBuffer &buffer, quint32 size)
{
    buffer.SetCapacity(size);
    if (size > 0)
        return ReadBytesRequired(buffer, size);

    return S_OK;
}

HRESULT CInArchive::ReadByte(quint8 *p)
{
    return ReadBytesRequired(p, 1);
}

HRESULT CInArchive::ReadUInt16(quint16 *p)
{
    quint16 value = 0;
    for (int i = 0; i < 2; i++) {
        quint8 b;
        RINOK(ReadByte(&b));
        value |= (((quint16)b) << (8 * i));
    }

    *p = value;
    return S_OK;
}

HRESULT CInArchive::ReadUInt32(quint32 *p)
{
    quint32 value = 0;
    for (int i = 0; i < 4; i++) {
        quint8 b;
        RINOK(ReadByte(&b));
        value |= (((quint32)b) << (8 * i));
    }

    *p = value;
    return S_OK;
}

HRESULT CInArchive::ReadUInt64(quint64 *p)
{
    quint64 value = 0;
    for (int i = 0; i < 8; i++) {
        quint8 b;
        RINOK(ReadByte(&b));
        value |= (((quint64)b) << (8 * i));
    }

    *p = value;
    return S_OK;
}

HRESULT CInArchive::ReadFileName(quint32 nameSize, QByteArray *name)
{
    if (nameSize == 0) {
        *name = QByteArray();
        return S_OK;
    }
    m_NameBuffer.resize(nameSize);
    RINOK(ReadBytesRequired(m_NameBuffer.data(), nameSize));
    *name = m_NameBuffer;
    return S_OK;
}

void CInArchive::GetArchiveInfo(CInArchiveInfo &archiveInfo) const
{
    archiveInfo = m_ArchiveInfo;
}

static quint32 GetUInt32(const quint8 *data)
{
    return
        ((quint32)(quint8)data[0]) |
        (((quint32)(quint8)data[1]) << 8) |
        (((quint32)(quint8)data[2]) << 16) |
        (((quint32)(quint8)data[3]) << 24);
}

static quint64 GetUInt64(const quint8 *data)
{
    return GetUInt32(data) | ((quint64)GetUInt32(data + 4) << 32);
}

HRESULT CInArchive::ReadExtra(quint32 extraSize, CExtraBlock &extraBlock,
                           quint64 &unpackSize, quint64 &packSize, quint64 &localHeaderOffset, quint32 &diskStartNumber)
{
    extraBlock.Clear();
    quint32 remain = extraSize;
    while (remain >= 4) {
        CExtraSubBlock subBlock;
        quint16 id;
        RINOK(ReadUInt16(&id));
        subBlock.ID = id;
        quint16 d;
        RINOK(ReadUInt16(&d));
        quint32 dataSize = d;
        remain -= 4;
        if (dataSize > remain) // it's bug
            dataSize = remain;
        if (subBlock.ID == NFileHeader::NExtraID::kZip64) {
            if (unpackSize == 0xFFFFFFFF) {
                if (dataSize < 8)
                    break;
                RINOK(ReadUInt64(&unpackSize));
                remain -= 8;
                dataSize -= 8;
            }
            if (packSize == 0xFFFFFFFF) {
                if (dataSize < 8)
                    break;
                RINOK(ReadUInt64(&packSize));
                remain -= 8;
                dataSize -= 8;
            }
            if (localHeaderOffset == 0xFFFFFFFF) {
                if (dataSize < 8)
                    break;
                RINOK(ReadUInt64(&localHeaderOffset));
                remain -= 8;
                dataSize -= 8;
            }
            if (diskStartNumber == 0xFFFF) {
                if (dataSize < 4)
                    break;
                quint32 d;
                RINOK(ReadUInt32(&d));
                diskStartNumber = d;
                remain -= 4;
                dataSize -= 4;
            }
            for (quint32 i = 0; i < dataSize; i++) {
                quint8 dummy;
                RINOK(ReadByte(&dummy));
            }
        } else {
            RINOK(ReadBuffer(subBlock.Data, dataSize));
            extraBlock.SubBlocks.append(new CExtraSubBlock(subBlock));
        }
        remain -= dataSize;
    }
    return m_Stream->Seek(remain, STREAM_SEEK_CUR, &m_Position);
}

HRESULT CInArchive::ReadLocalItem(CItemEx &item)
{
    RINOK(ReadByte(&item.ExtractVersion.Version));
    RINOK(ReadByte(&item.ExtractVersion.HostOS));
    RINOK(ReadUInt16(&item.Flags)); // & NFileHeader::NFlags::kUsedBitsMask;
    RINOK(ReadUInt16(&item.CompressionMethod));
    RINOK(ReadUInt32(&item.Time));
    RINOK(ReadUInt32(&item.FileCRC));
    quint32 packSize, unpackSize;
    RINOK(ReadUInt32(&packSize));
    RINOK(ReadUInt32(&unpackSize));
    item.PackSize = packSize;
    item.UnPackSize = unpackSize;
    quint16 fileNameSize;
    RINOK(ReadUInt16(&fileNameSize));
    RINOK(ReadUInt16(&item.LocalExtraSize));
    RINOK(ReadFileName(fileNameSize, &item.Name));
    item.FileHeaderWithNameSize = 4 + NFileHeader::kLocalBlockSize + fileNameSize;
    if (item.LocalExtraSize > 0) {
        quint64 localHeaderOffset = 0;
        quint32 diskStartNumber = 0;
        RINOK(ReadExtra(item.LocalExtraSize, item.LocalExtra, item.UnPackSize, item.PackSize,
                        localHeaderOffset, diskStartNumber));
    }
    /*
    if (item.IsDirectory())
      item.UnPackSize = 0;       // check It
    */
    return S_OK;
}

HRESULT CInArchive::ReadLocalItemAfterCdItem(CItemEx &item)
{
    if (item.FromLocal)
        return S_OK;
    RINOK(Seek(m_ArchiveInfo.Base + item.LocalHeaderPosition));
    CItemEx localItem;
    quint32 signature;
    RINOK(ReadUInt32(&signature));
    if (signature != NSignature::kLocalFileHeader)
        return E_BAD_SIGNATURE;
    RINOK(ReadLocalItem(localItem));
    if (item.Flags != localItem.Flags) {
        if (
            (item.CompressionMethod != NFileHeader::NCompressionMethod::kDeflated ||
                (item.Flags & 0x7FF9) != (localItem.Flags & 0x7FF9)) &&
            (item.CompressionMethod != NFileHeader::NCompressionMethod::kStored ||
                (item.Flags & 0x7FFF) != (localItem.Flags & 0x7FFF)) &&
            (item.CompressionMethod != NFileHeader::NCompressionMethod::kImploded ||
                (item.Flags & 0x7FFF) != (localItem.Flags & 0x7FFF))
        )
            return E_UNKNOWN_COMPRESSION;
    }

    if (item.CompressionMethod != localItem.CompressionMethod ||
            // item.Time != localItem.Time ||
            (!localItem.HasDescriptor() &&
                (
                    item.FileCRC != localItem.FileCRC ||
                    item.PackSize != localItem.PackSize ||
                    item.UnPackSize != localItem.UnPackSize
                )
            ) ||
            item.Name.length() != localItem.Name.length()
        ) {
        return S_FALSE;
    }
    item.FileHeaderWithNameSize = localItem.FileHeaderWithNameSize;
    item.LocalExtraSize = localItem.LocalExtraSize;
    item.LocalExtra = localItem.LocalExtra;
    item.FromLocal = true;

    return S_OK;
}

HRESULT CInArchive::ReadLocalItemDescriptor(CItemEx &item)
{
    if (item.HasDescriptor()) {
        const int kBufferSize = (1 << 12);
        quint8 buffer[kBufferSize];

        quint32 numBytesInBuffer = 0;
        quint32 packedSize = 0;

        bool descriptorWasFound = false;
        for (;;) {
            quint32 processedSize;
            RINOK(ReadBytes(buffer + numBytesInBuffer, kBufferSize - numBytesInBuffer, &processedSize));
            numBytesInBuffer += processedSize;
            if (numBytesInBuffer < NFileHeader::kDataDescriptorSize)
                return S_FALSE;
            quint32 i;
            for (i = 0; i <= numBytesInBuffer - NFileHeader::kDataDescriptorSize; i++) {
                // descriptorSignature field is Info-ZIP's extension
                // to Zip specification.
                quint32 descriptorSignature = GetUInt32(buffer + i);

                // !!!! It must be fixed for Zip64 archives
                quint32 descriptorPackSize = GetUInt32(buffer + i + 8);
                if (descriptorSignature == NSignature::kDataDescriptor && descriptorPackSize == packedSize + i) {
                    descriptorWasFound = true;
                    item.FileCRC = GetUInt32(buffer + i + 4);
                    item.PackSize = descriptorPackSize;
                    item.UnPackSize = GetUInt32(buffer + i + 12);
                    qint64 offset = qint64(qint32(0 - (numBytesInBuffer - i - NFileHeader::kDataDescriptorSize)));
                    RINOK(m_Stream->Seek(offset, STREAM_SEEK_CUR, &m_Position));
                    break;
                }
            }
            if (descriptorWasFound)
                break;
            packedSize += i;
            int j;
            for (j = 0; i < numBytesInBuffer; i++, j++)
                buffer[j] = buffer[i];
            numBytesInBuffer = j;
        }
    } else {
        RINOK(m_Stream->Seek(item.PackSize, STREAM_SEEK_CUR, &m_Position));
    }
    return S_OK;
}

HRESULT CInArchive::ReadLocalItemAfterCdItemFull(CItemEx &item)
{
    if (item.FromLocal)
        return S_OK;

    RINOK(ReadLocalItemAfterCdItem(item));
    if (item.HasDescriptor()) {
        RINOK(Seek(m_ArchiveInfo.Base + item.GetDataPosition() + item.PackSize));
        quint32 signature;
        RINOK(ReadUInt32(&signature));
        if (signature != NSignature::kDataDescriptor)
            return S_FALSE;
        quint32 crc;
        RINOK(ReadUInt32(&crc));
        quint64 packSize, unpackSize;

        /*
        if (IsZip64)
        {
            packSize = ReadUInt64();
            unpackSize = ReadUInt64();
        }
        else
        */
        {
            quint32 p, u;
            RINOK(ReadUInt32(&p));
            RINOK(ReadUInt32(&u));
            packSize = p;
            unpackSize = u;
        }

        if (crc != item.FileCRC || item.PackSize != packSize || item.UnPackSize != unpackSize)
            return S_FALSE;
    }

    return S_OK;
}

HRESULT CInArchive::ReadCdItem(CItemEx &item)
{
    item.FromCentral = true;
    RINOK(ReadByte(&item.MadeByVersion.Version));
    RINOK(ReadByte(&item.MadeByVersion.HostOS));
    RINOK(ReadByte(&item.ExtractVersion.Version));
    RINOK(ReadByte(&item.ExtractVersion.HostOS));
    RINOK(ReadUInt16(&item.Flags)); //  & NFileHeader::NFlags::kUsedBitsMask;
    RINOK(ReadUInt16(&item.CompressionMethod));
    RINOK(ReadUInt32(&item.Time));
    quint32 crc, packSize, unpackSize;
    RINOK(ReadUInt32(&crc));
    RINOK(ReadUInt32(&packSize));
    RINOK(ReadUInt32(&unpackSize));
    item.FileCRC = crc;
    item.PackSize = packSize;
    item.UnPackSize = unpackSize;
    quint16 headerNameSize, headerExtraSize, headerCommentSize, hds;
    RINOK(ReadUInt16(&headerNameSize));
    RINOK(ReadUInt16(&headerExtraSize));
    RINOK(ReadUInt16(&headerCommentSize));
    RINOK(ReadUInt16(&hds));
    quint32 headerDiskNumberStart = hds;
    RINOK(ReadUInt16(&item.InternalAttributes));
    RINOK(ReadUInt32(&item.ExternalAttributes));
    quint32 lhp;
    RINOK(ReadUInt32(&lhp));
    item.LocalHeaderPosition = lhp;
    RINOK(ReadFileName(headerNameSize, &item.Name));

    if (headerExtraSize > 0) {
        RINOK(ReadExtra(headerExtraSize, item.CentralExtra, item.UnPackSize, item.PackSize,
                        item.LocalHeaderPosition, headerDiskNumberStart));
    }

    if (headerDiskNumberStart != 0)
        return E_NO_MULTIVOLUME_ARCHIVES;

    // May be these strings must be deleted
    /*
    if (item.IsDirectory())
      item.UnPackSize = 0;
    */

    return ReadBuffer(item.Comment, headerCommentSize);
}

HRESULT CInArchive::TryEcd64(quint64 offset, CCdInfo &cdInfo)
{
    RINOK(Seek(offset));
    const quint32 kEcd64Size = 56;
    quint8 buf[kEcd64Size];
    RINOK(ReadBytesRequired(buf, kEcd64Size));
    if (GetUInt32(buf) != NSignature::kZip64EndOfCentralDir)
        return S_FALSE;
    // cdInfo.NumEntries = GetUInt64(buf + 24);
    cdInfo.Size = GetUInt64(buf + 40);
    cdInfo.Offset = GetUInt64(buf + 48);
    return S_OK;
}

HRESULT CInArchive::FindCd(CCdInfo &cdInfo)
{
    quint64 endPosition;
    RINOK(m_Stream->Seek(0, STREAM_SEEK_END, &endPosition));
    const quint32 kBufSizeMax = (1 << 16) + kEcdSize + kZip64EcdLocatorSize;
    quint8 buf[kBufSizeMax];
    quint32 bufSize = (endPosition < kBufSizeMax) ? (quint32)endPosition : kBufSizeMax;
    if (bufSize < kEcdSize)
        return S_FALSE;
    quint64 startPosition = endPosition - bufSize;
    RINOK(m_Stream->Seek(startPosition, STREAM_SEEK_SET, &m_Position));
    if (m_Position != startPosition)
        return S_FALSE;
    RINOK(ReadBytesRequired(buf, bufSize));
    for (int i = (int)(bufSize - kEcdSize); i >= 0; i--) {
        if (GetUInt32(buf + i) == NSignature::kEndOfCentralDir) {
            if (i >= kZip64EcdLocatorSize) {
                const quint8 *locator = buf + i - kZip64EcdLocatorSize;
                if (GetUInt32(locator) == NSignature::kZip64EndOfCentralDirLocator) {
                    quint64 ecd64Offset = GetUInt64(locator + 8);
                    if (TryEcd64(ecd64Offset, cdInfo) == S_OK)
                        return S_OK;
                    if (TryEcd64(m_ArchiveInfo.StartPosition + ecd64Offset, cdInfo) == S_OK) {
                        m_ArchiveInfo.Base = m_ArchiveInfo.StartPosition;
                        return S_OK;
                    }
                }
            }
            if (GetUInt32(buf + i + 4) == 0) {
                // cdInfo.NumEntries = Getquint16(buf + i + 10);
                cdInfo.Size = GetUInt32(buf + i + 12);
                cdInfo.Offset = GetUInt32(buf + i + 16);
                quint64 curPos = endPosition - bufSize + i;
                quint64 cdEnd = cdInfo.Size + cdInfo.Offset;
                if (curPos > cdEnd)
                    m_ArchiveInfo.Base = curPos - cdEnd;
                return S_OK;
            }
        }
    }
    return S_FALSE;
}

HRESULT CInArchive::TryReadCd(QList<CItemEx *> &items, quint64 cdOffset, quint64 cdSize)
{
    items.clear();
    RINOK(m_Stream->Seek(cdOffset, STREAM_SEEK_SET, &m_Position));
    if (m_Position != cdOffset)
        return S_FALSE;
    while (m_Position - cdOffset < cdSize) {
        quint32 signature;
        RINOK(ReadUInt32(&signature));
        if (signature != NSignature::kCentralFileHeader)
            return S_FALSE;
        CItemEx cdItem;
        RINOK(ReadCdItem(cdItem));
        items.append(new CItemEx(cdItem));
    }
    return (m_Position - cdOffset == cdSize) ? S_OK : S_FALSE;
}

HRESULT CInArchive::ReadCd(QList<CItemEx *> &items, quint64 &cdOffset, quint64 &cdSize)
{
    m_ArchiveInfo.Base = 0;
    CCdInfo cdInfo;
    RINOK(FindCd(cdInfo));
    HRESULT res = S_FALSE;
    cdSize = cdInfo.Size;
    cdOffset = cdInfo.Offset;
    res = TryReadCd(items, m_ArchiveInfo.Base + cdOffset, cdSize);
    if (res == S_FALSE && m_ArchiveInfo.Base == 0) {
        res = TryReadCd(items, cdInfo.Offset + m_ArchiveInfo.StartPosition, cdSize);
        if (res == S_OK)
            m_ArchiveInfo.Base = m_ArchiveInfo.StartPosition;
    }
    if (ReadUInt32(&m_Signature) != S_OK)
        return S_FALSE;
    return res;
}

HRESULT CInArchive::ReadLocalsAndCd(QList<CItemEx *> &items, CProgressVirt *progress, quint64 &cdOffset)
{
    items.clear();
    while (m_Signature == NSignature::kLocalFileHeader) {
        // FSeek points to next byte after signature
        // NFileHeader::CLocalBlock localHeader;
        CItemEx item;
        item.LocalHeaderPosition = m_Position - m_StreamStartPosition - 4; // points to signature;
        RINOK(ReadLocalItem(item));
        item.FromLocal = true;
        ReadLocalItemDescriptor(item);
        items.append(new CItemEx(item));
        if (progress != 0) {
            quint64 numItems = items.size();
            RINOK(progress->SetCompleted(&numItems));
        }
        if (ReadUInt32(&m_Signature) != S_OK)
            break;
    }
    cdOffset = m_Position - 4;
    for (int i = 0; i < items.size(); i++) {
        if (progress != 0) {
            quint64 numItems = items.size();
            RINOK(progress->SetCompleted(&numItems));
        }
        if (m_Signature != NSignature::kCentralFileHeader)
            return S_FALSE;

        CItemEx cdItem;
        RINOK(ReadCdItem(cdItem));

        if (i == 0) {
            if (cdItem.LocalHeaderPosition == 0)
                m_ArchiveInfo.Base = m_ArchiveInfo.StartPosition;
        }

        int index;
        int left = 0, right = items.size();
        for (;;) {
            if (left >= right)
                return S_FALSE;
            index = (left + right) / 2;
            quint64 position = items[index]->LocalHeaderPosition - m_ArchiveInfo.Base;
            if (cdItem.LocalHeaderPosition == position)
                break;
            if (cdItem.LocalHeaderPosition < position)
                right = index;
            else
                left = index + 1;
        }
        CItemEx &item = *items[index];
        item.LocalHeaderPosition = cdItem.LocalHeaderPosition;
        item.MadeByVersion = cdItem.MadeByVersion;
        item.CentralExtra = cdItem.CentralExtra;

        if (
            // item.ExtractVersion != cdItem.ExtractVersion ||
            item.Flags != cdItem.Flags ||
            item.CompressionMethod != cdItem.CompressionMethod ||
            // item.Time != cdItem.Time ||
            item.FileCRC != cdItem.FileCRC)
            return S_FALSE;

        if (item.Name.length() != cdItem.Name.length() ||
                item.PackSize != cdItem.PackSize ||
                item.UnPackSize != cdItem.UnPackSize
           )
            return S_FALSE;
        item.Name = cdItem.Name;
        item.InternalAttributes = cdItem.InternalAttributes;
        item.ExternalAttributes = cdItem.ExternalAttributes;
        item.Comment = cdItem.Comment;
        item.FromCentral = cdItem.FromCentral;
        if (ReadUInt32(&m_Signature) != S_OK)
            return S_FALSE;
    }
    return S_OK;
}

HRESULT CInArchive::ReadHeaders(QList<CItemEx *> &items, CProgressVirt *progress)
{
    // m_Signature must be kLocalFileHeaderSignature or
    // kEndOfCentralDirSignature
    // m_Position points to next byte after signature

    IsZip64 = false;
    items.clear();
    if (progress != 0) {
        quint64 numItems = items.size();
        RINOK(progress->SetCompleted(&numItems));
    }

    quint64 cdSize, cdStartOffset;
    HRESULT res = ReadCd(items, cdStartOffset, cdSize);
    if (res != S_FALSE && res != S_OK)
        return res;

    /*
    if (res != S_OK)
      return res;
    res = S_FALSE;
    */

    if (res == S_FALSE) {
        m_ArchiveInfo.Base = 0;
        RINOK(m_Stream->Seek(m_ArchiveInfo.StartPosition, STREAM_SEEK_SET, &m_Position));
        if (m_Position != m_ArchiveInfo.StartPosition)
            return S_FALSE;
        if (!ReadUInt32(&m_Signature))
            return S_FALSE;
        RINOK(ReadLocalsAndCd(items, progress, cdStartOffset));
        cdSize = (m_Position - 4) - cdStartOffset;
        cdStartOffset -= m_ArchiveInfo.Base;
    }

    quint32 thisDiskNumber = 0;
    quint32 startCDDiskNumber = 0;
    quint64 numEntriesInCDOnThisDisk = 0;
    quint64 numEntriesInCD = 0;
    quint64 cdSizeFromRecord = 0;
    quint64 cdStartOffsetFromRecord = 0;
    bool isZip64 = false;
    quint64 zip64EcdStartOffset = m_Position - 4 - m_ArchiveInfo.Base;
    if (m_Signature == NSignature::kZip64EndOfCentralDir) {
        IsZip64 = isZip64 = true;
        quint64 recordSize;
        RINOK(ReadUInt64(&recordSize));
        quint16 versionMade, versionNeedExtract;
        RINOK(ReadUInt16(&versionMade));
        RINOK(ReadUInt16(&versionNeedExtract));
        RINOK(ReadUInt32(&thisDiskNumber));
        RINOK(ReadUInt32(&startCDDiskNumber));
        RINOK(ReadUInt64(&numEntriesInCDOnThisDisk));
        RINOK(ReadUInt64(&numEntriesInCD));
        RINOK(ReadUInt64(&cdSizeFromRecord));
        RINOK(ReadUInt64(&cdStartOffsetFromRecord));
        RINOK(m_Stream->Seek(recordSize - kZip64EcdSize, STREAM_SEEK_CUR, &m_Position));
        RINOK(ReadUInt32(&m_Signature));
        if (thisDiskNumber != 0 || startCDDiskNumber != 0)
            return E_NO_MULTIVOLUME_ARCHIVES;
        if (numEntriesInCDOnThisDisk != items.size() ||
                numEntriesInCD != items.size() ||
                cdSizeFromRecord != cdSize ||
                (cdStartOffsetFromRecord != cdStartOffset &&
                 (!items.isEmpty())))
            return S_FALSE;
    }
    if (m_Signature == NSignature::kZip64EndOfCentralDirLocator) {
        quint32 startEndCDDiskNumber;
        RINOK(ReadUInt32(&startEndCDDiskNumber));
        quint64 endCDStartOffset;
        RINOK(ReadUInt64(&endCDStartOffset));
        quint32 numberOfDisks;
        RINOK(ReadUInt32(&numberOfDisks));
        if (zip64EcdStartOffset != endCDStartOffset)
            return S_FALSE;
        RINOK(ReadUInt32(&m_Signature));
    }
    if (m_Signature != NSignature::kEndOfCentralDir)
        return S_FALSE;

    quint16 thisDiskNumber16;
    RINOK(ReadUInt16(&thisDiskNumber16));
    if (!isZip64 || thisDiskNumber16)
        thisDiskNumber = thisDiskNumber16;

    quint16 startCDDiskNumber16;
    RINOK(ReadUInt16(&startCDDiskNumber16));
    if (!isZip64 || startCDDiskNumber16 != 0xFFFF)
        startCDDiskNumber = startCDDiskNumber16;

    quint16 numEntriesInCDOnThisDisk16;
    RINOK(ReadUInt16(&numEntriesInCDOnThisDisk16));
    if (!isZip64 || numEntriesInCDOnThisDisk16 != 0xFFFF)
        numEntriesInCDOnThisDisk = numEntriesInCDOnThisDisk16;

    quint16 numEntriesInCD16;
    RINOK(ReadUInt16(&numEntriesInCD16));
    if (!isZip64 || numEntriesInCD16 != 0xFFFF)
        numEntriesInCD = numEntriesInCD16;

    quint32 cdSizeFromRecord32;
    RINOK(ReadUInt32(&cdSizeFromRecord32));
    if (!isZip64 || cdSizeFromRecord32 != 0xFFFFFFFF)
        cdSizeFromRecord = cdSizeFromRecord32;

    quint32 cdStartOffsetFromRecord32;
    RINOK(ReadUInt32(&cdStartOffsetFromRecord32));
    if (!isZip64 || cdStartOffsetFromRecord32 != 0xFFFFFFFF)
        cdStartOffsetFromRecord = cdStartOffsetFromRecord32;

    quint16 commentSize;
    RINOK(ReadUInt16(&commentSize));
    ReadBuffer(m_ArchiveInfo.Comment, commentSize);

    if (thisDiskNumber != 0 || startCDDiskNumber != 0)
        return E_NO_MULTIVOLUME_ARCHIVES;
    if ((quint16)numEntriesInCDOnThisDisk != ((quint16)items.size()) ||
            (quint16)numEntriesInCD != ((quint16)items.size()) ||
            (quint32)cdSizeFromRecord != (quint32)cdSize ||
            ((quint32)(cdStartOffsetFromRecord) != (quint32)cdStartOffset &&
             (!items.isEmpty())))
        return S_FALSE;

    return S_OK;
}

ISequentialInStream* CInArchive::CreateLimitedStream(quint64 position, quint64 size)
{
    // create and return it with 0 refcount: ownership passes to the caller
    CLimitedSequentialInStream *streamSpec = new CLimitedSequentialInStream;
    SeekInArchive(m_ArchiveInfo.Base + position);
    streamSpec->SetStream(m_Stream);
    streamSpec->Init(size);
    return streamSpec;
}

IInStream* CInArchive::CreateStream()
{
    return m_Stream;
}

bool CInArchive::SeekInArchive(quint64 position)
{
    quint64 newPosition;
    if (m_Stream->Seek(position, STREAM_SEEK_SET, &newPosition) != S_OK)
        return false;
    return (newPosition == position);
}

}
}
