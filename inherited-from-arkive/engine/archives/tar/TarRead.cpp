#include "TarArchive.h"
#include "TarFormat.h"

#include "kernel/HandlerRegistry.h"

#include <QtCore/QByteArray>
#include <QtCore/QDateTime>
#include <QtCore/QDebug>
#include <QtCore/QString>
#include <QtCore/QVariant>

namespace ArchiveEngine {
namespace Tar {

// tar fields are either space (' ') or NUL ('\0') terminated, but also have
// a maximum permissable length
static QByteArray extractField(const QByteArray& buffer, int offset, int maxLen)
{
    QByteArray field;
    field.reserve(maxLen);

    Q_ASSERT(offset + maxLen < buffer.size());

    while (maxLen) {
        char ch = buffer.at(offset++);

        if (ch == ' ' || ch == '\0')
            return field;
        field += ch;
        maxLen--;
    }
    return field;
}

static QByteArray extractZeroTerminatedField(const QByteArray& buffer, int offset, int maxLen)
{
    QByteArray field;
    field.reserve(maxLen);
    Q_ASSERT(offset + maxLen < buffer.size());

    while (maxLen) {
        char ch = buffer.at(offset++);

        if (ch == '\0')
            return field;
        field += ch;
        maxLen--;
    }
    return field;
}

// we have our own string-to-number conversions here because we cannot
// simply use QByteArray::toLongLong() due to it's use

static qint64 extractNumber8(const QByteArray& number)
{
    qint64 ret = 0;

    for (int i = 0; i < number.size(); i++) {
        // stop at the first invalid character...
        if (number[i] < '0' || number[i] > '7')
            return ret;
        ret *= 8;
        ret += number[i] - '0';
    }
    return ret;
}

static qint64 extractNumber256(const QByteArray& number)
{
    qint64 ret = 0;

    if (number[0] & 0x40) {
        // a negative number...
        ret = -1;
    }

    ret = (ret << 6) | (number[0] & 0x3f);
    int bitsLeft = 64 - 7;
    for (int i = 1; i < number.size() && bitsLeft > 0; i++, bitsLeft -= 8) {
        int bits = qMin(bitsLeft, 8);
        ret <<= bits;
        ret |= (number[i] >> (8 - bits)) & ((1 << bits) - 1);
    }

    return ret;
}

static qint64 extractNumber10(const QByteArray& number)
{
    qint64 ret = 0;
    int sign = 1;
    int start = 0;

    if (number.size() > 0 && number[0] == '-') {
        sign = -1;
        start = 1;
    }

    for (int i = start; i < number.size(); i++) {
        // stop at the first invalid character...
        if (number[i] < '0' || number[i] > '9')
            return ret*sign;
        ret *= 10;
        ret += number[i] - '0';
    }

    return ret*sign;
}

// there is an extension to the tar format where if the high bit of the first digit
// is set, the number is actually in big-endian base-256, rather than octal
static qint64 extractNumber(const QByteArray& number)
{
    Q_ASSERT(!number.isEmpty());

    // skip leading spaces
    int start = 0;
    while (start < number.size() && number[start] == ' ')
        start++;
    QByteArray trimmed = number.mid(start);

    if (trimmed.isEmpty())
        return 0;

    if (trimmed[0] & 0x80)
        return extractNumber256(number);

    return extractNumber8(number);
}

// timestamps may be stored as arbitrary-precision decimal numbers,
// i.e. "-?[0-9]+(.[0-9]*)?"; parse that out and create a QDateTime
// for it
static QDateTime fractionalDate(const QByteArray string)
{
    if (string.isEmpty())
        return QDateTime();

    int sign = 1;
    int start = 0;
    int secondPart = 0;
    int fractionPart = 0;
    int fractionDenom = 1;
    bool doFraction = false;

    if (string[0] == '-') {
        sign = -1;
        start = 1;
    }

    for (int i = start; i < string.size(); i++) {
        if (string[i] == '.') {
            // only accept one decimal point
            if (doFraction)
                break;
            doFraction = true;
            continue;
        }
        if (string[i] < '0' || string[i] > '9')
            break;

        if (!doFraction) {
            secondPart *= 10;
            secondPart += string[i] - '0';
        } else {
            fractionDenom *= 10;
            fractionPart *= 10;
            fractionPart += string[i] - '0';
        }
    }

    secondPart *= sign;
    fractionPart *= sign;
    
    // convert the fraction part to milliseconds for QDateTime
    if (fractionDenom < 1000) {
        fractionPart *= 1000 / fractionDenom;
    } else if (fractionDenom > 1000) {
        fractionPart /= fractionDenom / 1000;
    }

    return QDateTime::fromTime_t(secondPart).addMSecs(fractionPart);
}

// the checksum of a tar header block is defined as the byte-by-byte arithmetic
// sum of the block, where the 8-byte checksum field is treated as though it was
// all spaces (0x20, ' ')
static bool checksumOk(const QByteArray& block)
{
    Q_ASSERT(block.size() == BLOCK_SIZE);

    unsigned int csumFromFile = extractNumber(extractField(block, CSumOffset, CSumSize));
    unsigned int csum = 0;

    // first do the normally defined checksum
    for (int i = 0; i < CSumOffset; i++)
        csum += (unsigned char)block[i];
    for (int i = 0; i < CSumSize; i++)
        csum += (unsigned char)' ';
    for (int i = CSumOffset + CSumSize; i < 512; i++)
        csum += (unsigned char)block[i];

    if (csum == csumFromFile)
        return true;

    // if the correct checksum fails, try it again with signed characters
    // (old BSD, Solaris, and HP-UX machines have broken tar implementations that
    // make archives this way...)
    csum = 0;
    for (int i = 0; i < CSumOffset; i++)
        csum += (signed char)block[i];
    for (int i = 0; i < CSumSize; i++)
        csum += (signed char)' ';
    for (int i = CSumOffset + CSumSize; i < BLOCK_SIZE; i++)
        csum += (signed char)block[i];

    if (csum == csumFromFile)
        return true;

    // no match: the archive is bad!
    return false;
}

static bool blockIsEmpty(const QByteArray& block)
{
    Q_ASSERT(block.size() == BLOCK_SIZE);

    for (int i = 0; i < BLOCK_SIZE; i++)
        if (block.at(i) != 0)
            return false;
    return true;
}


TarArchive::ItemStatus TarArchive::readOneItem(QByteArray& buffer)
{
    if (!readBuffer(&buffer, BLOCK_SIZE)) {
        errorIoDevice(inputDevice());
        return Error;
    }

    if (blockIsEmpty(buffer)) {
        if (!readBuffer(&buffer, BLOCK_SIZE)) {
            qWarning("TarArchive: archive is missing its second EOF block");
            return AtEnd;
        }
        if (blockIsEmpty(buffer))
            return AtEnd;
        qWarning("TarArchive: trailing junk after end-of-file mark");
        return AtEnd;
    }

    if (!checksumOk(buffer)) {
        errorBadCrc();
        return Error;
    }

    if (mFormat == TarFormat::Unknown) {
        // look for magic numbers -- we include the version field in our search
        // since the GNU tar magic does so, and we only understand POSIX format 00
        // anyways
        QByteArray magic = QByteArray::fromRawData(buffer.constData() + MagicOffset, 
            MagicSize + VersionSize);

        if (magic == QByteArray::fromRawData("ustar\000", 8)) {
            // looks like something POSIX-ish
            QByteArray magic2 = QByteArray::fromRawData(buffer.constData() + STarTailMagicOffset, 
                STarTailMagicSize);

            // check if maybe it's a Schilling S-Tar archive?
            if (magic2 == QByteArray::fromRawData("tar\0", 4))
                mFormat = TarFormat::STar;
            else
                mFormat = TarFormat::USTar;
        } else if (magic == QByteArray::fromRawData("ustar  ", 8)) {
            // looks like GNU tar
            mFormat = TarFormat::GNU;
        } else if (magic == QByteArray::fromRawData("\0\0\0\0\0\0\0\0", 8)) {
            // no magic at all: old-style tar
            mFormat = TarFormat::Basic;
        } else {
            // gobbledygook: not a tar file
            setErrorString(tr("does not look like a tar file"));
            return Error;
        }
    }

    // okay, now we know the file format: interpret the record
    char type = buffer.at(TypeOffset);

    switch (type) {
        case TarItemGnuVolumeHeader:
            {
                QByteArray volName = extractField(buffer, NameOffset, NameSize);
                archiveInfo().setProperty(ArchiveInfo::Label, QString::fromLatin1(volName));
                break;
            }
        case TarItemAttributes:
        case TarItemAttributesAlternate:
            // we see an attribute section, so it must be PAX, not just USTar
            if (mFormat == TarFormat::USTar)
                mFormat = TarFormat::PAX;

            if (!readAttributes(buffer, &mCurrentItem))
                return Error;
            break;
        case TarItemDefaultAttributes:
            // we see an attribute section, so it must be PAX, not just USTar
            if (mFormat == TarFormat::USTar)
                mFormat = TarFormat::PAX;

            // template items take effect immediately and are active
            // until each specific attribute is superceded
            if (!readAttributes(buffer, &mGlobalTemplateItem))
                return Error;

            // apply the new template to the next item
            // XXX: what if we are in the middle of an item?
            // (I think that is a sufficiently broken archive that I don't care...)
            mCurrentItem = mGlobalTemplateItem;
            break;
        case TarItemGnuLongLink:
            // the data for this object is the link name for the next object
            {
                QByteArray linkName;
                if (!readObjectData(buffer, &linkName))
                    return Error;
                mCurrentItem.setProperty(ArchiveItem::LinkTarget, QString::fromUtf8(linkName));
                break;
            }
        case TarItemGnuLongName:
            // the data for this object is the object name for the next object
            {
                QByteArray objectName;
                if (!readObjectData(buffer, &objectName))
                    return Error;
                mCurrentItem.setProperty(ArchiveItem::Path, QString::fromUtf8(objectName));
                break;
            }
        case TarItemFifo:
            if (!readNamedObject(buffer, ArchiveItem::ItemTypeFifo))
                return Error;
            break;
        case TarItemDirectory:
        case TarItemGnuDumpDirectory:
            // the GNU dump directory extension stores a listing of the
            // directory entries in its data along with a note of whether
            // each entry is present in the archive or not; we just treat it
            // like a normal directory entry and skip its data
            if (!readNamedObject(buffer, ArchiveItem::ItemTypeDirectory))
                return Error;
            break;
        case TarItemBlockDevice:
            if (!readNamedObject(buffer, ArchiveItem::ItemTypeBlockDevice))
                return Error;
            break;
        case TarItemCharacterDevice:
            if (!readNamedObject(buffer, ArchiveItem::ItemTypeCharacterDevice))
                return Error;
            break;
        case TarItemSymbolicLink:
            if (!readNamedObject(buffer, ArchiveItem::ItemTypeSymbolicLink))
                return Error;
            break;
        case TarItemHardLink:
            if (!readNamedObject(buffer, ArchiveItem::ItemTypeHardLink))
                return Error;
            break;
        case TarItemSolarisAccessControlList:
        case TarItemSolarisExtendedAttribute:
        case TarItemGnuMultiVolume:
        case TarItemGnuNameMangleList:
        default:
            // anything unrecognized or unhandled should be treated as a file
            // issue a warning, though
            qWarning("TarArchive: unhandled item type '%c'", type);
            // FALL THROUGH
        case TarItemRegularFile:
        case TarItemRegularFileAlternate:
        case TarItemGnuSparseFile:
            // regular files
        case TarItemSTarInodeOnly:
            // like a file, but with no data...
            if (!readNamedObject(buffer, ArchiveItem::ItemTypeFile))
                return Error;
            break;
    }
    return Ok;
}

#define STRING_FIELD(archiveField, tarField)                                            \
    do {                                                                                \
        if (!mCurrentItem.hasProperty(archiveField)) {                                  \
            QByteArray val = extractZeroTerminatedField(buffer,                         \
                tarField ## Offset, tarField ## Size);                                  \
                                                                                        \
            if (!val.isEmpty())                                                         \
                mCurrentItem.setProperty(archiveField, QString::fromAscii(val));        \
        }                                                                               \
    } while (0)

#define NUMERIC_FIELD(archiveField, tarField)                                           \
    do {                                                                                \
        if (!mCurrentItem.hasProperty(archiveField)) {                                  \
            QByteArray val = extractField(buffer, tarField ## Offset, tarField ## Size);\
                                                                                        \
            if (!val.isEmpty())                                                         \
                mCurrentItem.setProperty(archiveField, extractNumber(val));             \
        }                                                                               \
    } while (0)

#define TEMPORAL_FIELD(archiveField, tarField)                                          \
    do {                                                                                \
        if (!mCurrentItem.hasProperty(archiveField)) {                                  \
            QByteArray val = extractField(buffer, tarField ## Offset, tarField ## Size);\
                                                                                        \
            if (!val.isEmpty())                                                         \
                mCurrentItem.setProperty(archiveField,                                  \
                    QDateTime::fromTime_t(extractNumber(val)));                         \
        }                                                                               \
    } while (0)

bool TarArchive::readNamedObject(const QByteArray& buffer, ArchiveItem::ItemType type)
{
    // first deal with the item's name
    if (!mCurrentItem.hasProperty(ArchiveItem::Path)) {
        // no name yet: extract it from this block
        QByteArray rawName = extractZeroTerminatedField(buffer, NameOffset, NameSize);

        if (mFormat == TarFormat::USTar || mFormat == TarFormat::PAX) {
            QByteArray prefix = extractZeroTerminatedField(buffer, USTarPrefixOffset, USTarPrefixSize);

            if (!prefix.isEmpty()) {
                rawName += '/';
                rawName += prefix;
            }
        } else if (mFormat == TarFormat::STar) {
            QByteArray prefix = extractZeroTerminatedField(buffer, STarPrefixOffset, STarPrefixSize);

            if (!prefix.isEmpty()) {
                rawName += '/';
                rawName += prefix;
            }
        }

        // certain old archives make directories by giving their name a trailing slash:
        // deal with that here
        if (rawName.endsWith('/')) {
            type = ArchiveItem::ItemTypeDirectory;
            rawName.resize(rawName.size() - 1);
        }

        mCurrentItem.setProperty(ArchiveItem::Path, QString::fromAscii(rawName));
    }

    // then the item type
    mCurrentItem.setProperty(ArchiveItem::Type, type);

    // then user and group ids
    STRING_FIELD(ArchiveItem::UserName, UserName);
    STRING_FIELD(ArchiveItem::GroupName, GroupName);
    NUMERIC_FIELD(ArchiveItem::UserId, Uid);
    NUMERIC_FIELD(ArchiveItem::GroupId, Gid);

    // then the item's permissions
    NUMERIC_FIELD(ArchiveItem::UnixMode, Mode);

    // then the item's device numbers, if appropriate
    if (type == ArchiveItem::ItemTypeBlockDevice || type == ArchiveItem::ItemTypeCharacterDevice) {
        NUMERIC_FIELD(ArchiveItem::MajorDeviceNumber, DeviceMajor);
        NUMERIC_FIELD(ArchiveItem::MinorDeviceNumber, DeviceMinor);
    } else {
        // device numbers have no meaning: kill them
        mCurrentItem.removeProperty(ArchiveItem::MajorDeviceNumber);
        mCurrentItem.removeProperty(ArchiveItem::MinorDeviceNumber);
    }


    // then the item's position and size:
    // the item's data starts with the next block (i. e., the current file pointer)
    qint64 itemPos = currentPosition();
    mCurrentItem.setProperty(ArchiveItem::Position, itemPos);

    // we might already have the size from an attribute block; we define the CompressedSize
    // to be the actual size in the archive; sparse files may have 
    // CompressedSize != UncompressedSize...
    if (!mCurrentItem.hasProperty(ArchiveItem::CompressedSize)) {
        qint64 itemSize = extractNumber(extractField(buffer, SizeOffset, SizeSize));
        mCurrentItem.setProperty(ArchiveItem::CompressedSize, itemSize);

        if (!mCurrentItem.hasProperty(ArchiveItem::UncompressedSize))
            mCurrentItem.setProperty(ArchiveItem::UncompressedSize, itemSize);
    }

    // then the item's various time stamps
    if (!mCurrentItem.hasProperty(ArchiveItem::LastWriteTime)) {
        TEMPORAL_FIELD(ArchiveItem::LastWriteTime, MTime);
    }
    if (!mCurrentItem.hasProperty(ArchiveItem::LastAccessTime)) {
        if (mFormat == TarFormat::GNU) {
            TEMPORAL_FIELD(ArchiveItem::LastAccessTime, GnuATime);
        } else if (mFormat == TarFormat::STar) {
            TEMPORAL_FIELD(ArchiveItem::LastAccessTime, STarATime);
        }
    }
    if (!mCurrentItem.hasProperty(ArchiveItem::CreationTime)) {
        if (mFormat == TarFormat::GNU) {
            TEMPORAL_FIELD(ArchiveItem::CreationTime, GnuCTime);
        } else if (mFormat == TarFormat::STar) {
            TEMPORAL_FIELD(ArchiveItem::CreationTime, STarCTime);
        }
    }

    // finally, handle sparse files
    if (buffer.at(TypeOffset) == TarItemGnuSparseFile || 
        mCurrentItem.hasProperty(TarItem::GnuSparseFormat)) {
        if (!readSparseStructure(buffer))
            return false;
    }

    // fast-forward over the item
    qint64 dataSize = mCurrentItem.property(ArchiveItem::CompressedSize).toLongLong();
    qint64 newPos = itemPos + ((dataSize + BLOCK_SIZE - 1) / BLOCK_SIZE) * BLOCK_SIZE;
    seek(newPos, RelativeToBeginning, 0);

    // add the item to our index
    mCurrentItem.setProperty(ArchiveItem::CompressionMethod, ArchiveItem::CompressionMethodStore);
    addItem(mCurrentItem);

    // reset the current item
    mCurrentItem = mGlobalTemplateItem;

    // and rememeber where the first record pertaining to this item is so that we
    // can losslessly transfer it to a new archive if need be
    mCurrentItem.setProperty(TarItem::HeaderStartPosition, currentPosition());
    return true;
}

bool TarArchive::readAttributes(const QByteArray& buffer, ArchiveItem *item)
{
    // read the attribute set
    QByteArray attrs;
    if (!readObjectData(buffer, &attrs))
        return false;

    // and now parse it
    int start = 0, end;

    // process the attributes line-by-line
    while (start < attrs.size() && (end = attrs.indexOf('\n', start)) >= 0) {
        QByteArray line = QByteArray::fromRawData(&attrs.constData()[start], end - start);

        // Attributes are in the form "length key=value\n", where the length is a decimal
        // ASCII number, and includes its own length, the following space, and the terminating
        // '\n'; this allows attributes to contain embedded '\n's, while still having the
        // resulting attribute list be human-readable. (NOTE: I am not sure that embedde
        // '\n's are permitted by POSIX; but paranoia is still justified.) All string-ish 
        // attribute information is required to be stored in UTF-8. (Note that unrecognized 
        // attribute values may conceivably be binary rather than text, however. Again,
        // I'm not sure what POSIX has to say about this, but we'll be paranoid.)

        // start by finding the length and making sure we have the entire attribute record
        int spacePos = line.indexOf(' ');
        int len = extractNumber10(QByteArray::fromRawData(&line.constData()[0], spacePos));

        if (len < line.length() + 1 || (start + len >= attrs.size())) {
            // broken looking attribute: warn and continue
            qWarning() << "TarArchive: broken attribute line" << line.left(80);
            start = end + 1;
            continue;
        }

        if (len > line.length() + 1) {
            // multi-line attribute; grab the rest
            line = QByteArray::fromRawData(&attrs.constData()[start], len);
        }

        // note where the next attribute will start
        start += len;

        // next find the attribute name
        int equalsPos = line.indexOf('=', spacePos + 1);

        if (equalsPos < 0) {
            // broken looking attribute: warn and continue
            qWarning() << "TarArchive: broken attribute line" << line.left(80);
            continue;
        }

        // leave out the ' '
        QByteArray attrName = QByteArray::fromRawData(&line.constData()[spacePos + 1], 
            equalsPos - spacePos - 1);

        // leave out both the '=' and the trailing '\n'
        QByteArray attrValue = QByteArray::fromRawData(&line.constData()[equalsPos + 1], 
            len - equalsPos - 2);

        // and now handle the attributes we know:
        qint64 pendingSize = -1, pendingOffset = -1;

        if (attrName == "size") {
            qint64 size = extractNumber10(attrValue);
            item->setProperty(ArchiveItem::CompressedSize, size);

            // don't override a SCHILY.realsize
            if (!item->hasProperty(ArchiveItem::UncompressedSize))
                item->setProperty(ArchiveItem::UncompressedSize, size);
        } else if (attrName == "path") {
            item->setProperty(ArchiveItem::Path, QString::fromUtf8(attrValue));
        } else if (attrName == "linkpath") {
            item->setProperty(ArchiveItem::LinkTarget, QString::fromUtf8(attrValue));
        } else if (attrName == "uname") {
            item->setProperty(ArchiveItem::UserName, QString::fromUtf8(attrValue));
        } else if (attrName == "gname") {
            item->setProperty(ArchiveItem::GroupName, QString::fromUtf8(attrValue));
        } else if (attrName == "uid") {
            item->setProperty(ArchiveItem::UserId, extractNumber10(attrValue));
        } else if (attrName == "gid") {
            item->setProperty(ArchiveItem::GroupId, extractNumber10(attrValue));
        } else if (attrName == "comment") {
            item->setProperty(ArchiveItem::Comment, QString::fromUtf8(attrValue));
        } else if (attrName == "atime") {
            QDateTime atime = fractionalDate(attrValue);

            if (atime.isValid())
                item->setProperty(ArchiveItem::LastAccessTime, atime);
        } else if (attrName == "mtime") {
            QDateTime mtime = fractionalDate(attrValue);

            if (mtime.isValid())
                item->setProperty(ArchiveItem::LastWriteTime, mtime);
        } else if (attrName == "ctime") {
            QDateTime ctime = fractionalDate(attrValue);

            if (ctime.isValid())
                item->setProperty(ArchiveItem::CreationTime, ctime);
        } else if (attrName == "SCHILY.realsize") {
            item->setProperty(ArchiveItem::UncompressedSize, extractNumber10(attrValue));
        } else if (attrName == "SCHILY.devmajor") {
            item->setProperty(ArchiveItem::MajorDeviceNumber, extractNumber10(attrValue));
        } else if (attrName == "SCHILY.devminor") {
            item->setProperty(ArchiveItem::MinorDeviceNumber, extractNumber10(attrValue));
        } else if (attrName == "GNU.sparse.numblocks") {
            item->setProperty(TarItem::GnuSparseFormat, GnuSparseFormat0p0);
        } else if (attrName == "GNU.sparse.offset") {
            pendingOffset = extractNumber10(attrValue);

            if (pendingSize != -1) {
                SparseFileRecordList l;

                if (item->hasProperty(TarItem::SparseFileStructure))
                    l = item->property(TarItem::SparseFileStructure).value<SparseFileRecordList>();
                l << SparseFileRecord(pendingOffset, pendingSize);
                item->setProperty(TarItem::SparseFileStructure, QVariant::fromValue(l));

                pendingOffset = -1;
                pendingSize = -1;
            }
        } else if (attrName == "GNU.sparse.numbytes") {
            pendingSize = extractNumber10(attrValue);


            if (pendingOffset != -1) {
                SparseFileRecordList l;

                if (item->hasProperty(TarItem::SparseFileStructure))
                    l = item->property(TarItem::SparseFileStructure).value<SparseFileRecordList>();
                l << SparseFileRecord(pendingOffset, pendingSize);
                item->setProperty(TarItem::SparseFileStructure, QVariant::fromValue(l));

                pendingOffset = -1;
                pendingSize = -1;
            }
        } else if (attrName == "GNU.sparse.size" || attrName == "GNU.sparse.realsize") {
            item->setProperty(ArchiveItem::UncompressedSize, extractNumber10(attrValue));
        } else if (attrName == "GNU.sparse.major") {
            uint ver = extractNumber10(attrValue) * 10;

            if (item->hasProperty(TarItem::GnuSparseFormat))
                ver += item->property(TarItem::GnuSparseFormat).toUInt();
            item->setProperty(TarItem::GnuSparseFormat, ver);
        } else if (attrName == "GNU.sparse.major") {
            uint ver = extractNumber10(attrValue);

            if (item->hasProperty(TarItem::GnuSparseFormat))
                ver += item->property(TarItem::GnuSparseFormat).toUInt();
            item->setProperty(TarItem::GnuSparseFormat, ver);
        } else if (attrName == "GNU.sparse.name") {
            item->setProperty(ArchiveItem::Path, QString::fromUtf8(attrValue));
        } else if (attrName == "GNU.sparse.map") {
            item->setProperty(TarItem::GnuSparseFormat, GnuSparseFormat0p1);

            SparseFileRecordList l;

            if (item->hasProperty(TarItem::SparseFileStructure))
                l = item->property(TarItem::SparseFileStructure).value<SparseFileRecordList>();

            // a GNU sparse format 0.1 sparse map is just a list of comma-separated numbers,
            // in the form "offset,size,offset,size,..."
            int start = 0;
            while (true) {
                if (start >= attrValue.size())
                    break;

                int end = attrValue.indexOf(',', start);

                if (end < 0)
                    break;

                qint64 offset = extractNumber10(QByteArray::fromRawData(&attrValue.constData()[start], end - start));

                if (offset < 0)
                    break;
                
                start = end + 1;
                end = attrValue.indexOf(',', start);

                if (end < 0)
                    break;

                qint64 size = extractNumber10(QByteArray::fromRawData(&attrValue.constData()[start], end - start));

                if (size <= 0)
                    break;

                l << SparseFileRecord(offset, size);
                start = end + 1;
            }
            if (!l.isEmpty())
                item->setProperty(TarItem::SparseFileStructure, QVariant::fromValue(l));
        } else {
            qDebug() << "TarArchive: unhandled attribute" << attrValue;
        }
    }

    return true;
}

bool TarArchive::readObjectData(const QByteArray& buffer, QByteArray *data)
{
    QByteArray block;
    block.reserve(BLOCK_SIZE);

    qint64 size = extractNumber(extractField(buffer, SizeOffset, SizeSize));

    int blocks = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    data->clear();
    data->reserve(size);

    while (blocks) {
        if (!readBuffer(&block, BLOCK_SIZE))
            return errorIoDevice(inputDevice());

        if (size < BLOCK_SIZE)
            block.resize(size);
        *data += block;
        size -= 512;
        blocks--;
    }

    return true;
}

bool TarArchive::readSparseStructure(const QByteArray& buffer)
{
    SparseFileRecordList sparseItems;

    if (mCurrentItem.hasProperty(TarItem::SparseFileStructure)) {
        sparseItems = mCurrentItem.property(TarItem::SparseFileStructure)
                        .value<SparseFileRecordList>();
    }

    // an old-style GNU sparse file
    bool isOldGnuFormat = (mFormat == TarFormat::GNU && 
        !mCurrentItem.hasProperty(TarItem::GnuSparseFormat));
    if (isOldGnuFormat) {
        // GNU tar files store up to 4 sparse fragment records in their header
        for (int i = 0; i < 3; i++) {
            QByteArray fragmentRecord = QByteArray::fromRawData(&buffer.constData()[GnuSparseOffset + i*SparseItemSize], SparseItemSize);

            SparseFileRecord record(fragmentRecord);

            if (record.size() <= 0)
                break;
            sparseItems << record;
        }
    }

    if (mFormat == TarFormat::STar || 
        (isOldGnuFormat && buffer.at(GnuHasMoreSparseElementsOffset) != 0)) {
        // following this block is one or more blocks of sparse file records
        QByteArray block;

        if (!readBuffer(&block, BLOCK_SIZE))
            return false;

        // XXX: complete me
    }

    if (mCurrentItem.hasProperty(TarItem::GnuSparseFormat) && 
        mCurrentItem.property(TarItem::GnuSparseFormat).toInt() == GnuSparseFormat1p0) {
        // a more recent form of sparse map! the only one that needs handling here
        // is version 1.0
        // XXX: complete me
    }

    mCurrentItem.setProperty(TarItem::SparseFileStructure, QVariant::fromValue(sparseItems));

    return true;
}

bool TarArchive::extract(const quint64 index, QIODevice *target)
{
    setOutputDevice(target);
    if (index >= size()) {
        setErrorString(tr("index out of bounds"));
        return false;
    }

    ArchiveItem item = itemAt(index);

    Q_ASSERT(item.hasProperty(ArchiveItem::Position));
    Q_ASSERT(item.hasProperty(ArchiveItem::CompressedSize));
    qint64 position = item.property(ArchiveItem::Position).toLongLong();

    // seek to the item's header
    if (!seek(position, RelativeToBeginning, 0))
        return errorIoDevice(inputDevice());

    // dump the data to the target
    // XXX: handle sparse files
    SparseFileRecordList fragments;

    // sparse files have multiple fragments; for non-sparse files,
    // just synthesize a single fragment to cover the entire file
    if (item.hasProperty(TarItem::SparseFileStructure))
        fragments = item.property(TarItem::SparseFileStructure).value<SparseFileRecordList>();
    else
        fragments << SparseFileRecord(0, item.property(ArchiveItem::CompressedSize).toULongLong());

    qint64 currentPosition = 0;
    const qint64 READ_BLOCK_SIZE = 4096*16;
    QByteArray blocks;

    foreach (const SparseFileRecord& fragment, fragments) {
        // for each fragment, seek forward to it's target location
        // and then write it out
        if (fragment.offset() != currentPosition) {
            // seek if we can, otherwise just write zeroes
            if (!outputDevice()->isSequential()) {
                // we can seek
                if (!outputDevice()->seek(fragment.offset()))
                    return errorIoDevice(outputDevice());
                currentPosition = target->pos();
            } else {
                // no seeking, so write zeroes instead
                static quint8 zeroes[512];
                static bool cleared;
    
                if (!cleared) {
                    ::memset(zeroes, 0, sizeof(zeroes));
                    cleared = true;
                }
                while (fragment.offset() != currentPosition) {
                    int size = qMin(fragment.offset() - currentPosition, Q_INT64_C(512));

                    if (!writeBytes(zeroes, size))
                        return errorIoDevice(outputDevice());
                    currentPosition += size;
                }
            }
        }

        // now that we're at the right place in the output file, write out the fragment
        // of data
        qint64 remaining = fragment.size();
        while (remaining) {
            int size = qMin(remaining, READ_BLOCK_SIZE);

            if (!readBuffer(&blocks, size))
                return errorIoDevice(inputDevice());
            if (!writeBuffer(blocks, size))
                return errorIoDevice(outputDevice());
            remaining -= size;
            currentPosition += size;
        }

        // fragments are stored in integral numbers of blocks, so skip
        // any trailing padding
        if (fragment.size() % BLOCK_SIZE)
            skipBytes(BLOCK_SIZE - (fragment.size() % BLOCK_SIZE));
    }

    return true;
}

// writing support for SparseFileRecords is in TarWrite.cpp
SparseFileRecord::SparseFileRecord(const QByteArray& data)
{
    Q_ASSERT(data.size() == SparseItemSize);

    // GNU/STar sparse file records are octal-encoded
    mOffset = extractNumber(extractField(data, SparseOffsetOffset, SparseOffsetSize));
    mSize = extractNumber(extractField(data, SparseSizeOffset, SparseSizeSize));
}

SparseFileRecord::SparseFileRecord(const quint64 offset_, const quint64 size_)
    : mOffset(offset_), mSize(size_)
{
}

// close namespaces
}
}
