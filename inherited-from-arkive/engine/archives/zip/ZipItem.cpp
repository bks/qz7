#include "ZipHeader.h"
#include "ZipItem.h"

namespace Zip {

bool operator==(const Version &v1, const Version &v2)
{
    return (v1.version() == v2.version()) && (v1.hostOS() == v2.hostOS());
}

bool operator!=(const Version &v1, const Version &v2)
{
    return !(v1 == v2);
}

ImplodeFlags implodeFlags(const ArchiveItem& item)
{
    Q_ASSERT(item.hasProperty(Flags));
    quint32 flags = item.property(Flags);
    ImplodeFlags ifl = NoImplodeFlags;

    if (flags & Header::Flags::ImplodeDictionarySizeMask)
        ifl |= ImplodeBigDictionary;
    if (flags & Header::Flags::ImplodeLiteralsOnMask)
        ifl |= ImplodeLiteralsOn;

    return ifl;
}

void checkDirectory(const ArchiveItem& item)
{
    if (item.property(ArchiveItem::Name).toString().endsWith('/'))
        goto isDir;

    if (!item.hasProperty(ExternalAttributes))
        return;

    quint32 highAttributes = quint32((item.property(ExternalAttributes).toUInt() >> 16) & 0xFFFF);
    switch (archive.property(CreatorVersion).value<Version>.hostOS()) {
    case Header::HostOS::AMIGA:
        switch (highAttributes & Header::AmigaAttribute::IFMT) {
        case Header::AmigaAttribute::IFDIR:
            goto isDir;
        case Header::AmigaAttribute::IFREG:
            return;
        default:
            return;
        }
    case Header::HostOS::FAT:
    case Header::HostOS::NTFS:
    case Header::HostOS::HPFS:
    case Header::HostOS::VFAT:
        if ((item.property(ExternalAttributes).toUInt() & FILE_ATTRIBUTE_DIRECTORY) == 0)
            return;
        goto isDir;
    case Header::HostOS::Atari:
    case Header::HostOS::Mac:
    case Header::HostOS::VMS:
    case Header::HostOS::VM_CMS:
    case Header::HostOS::Acorn:
    case Header::HostOS::MVS:
        return;
    default:
        return;
    }

isDir:
    item.setProperty(ArchiveItem::Type, quint32(ArchiveItem::ItemTypeDirectory));
    return;
}

}
