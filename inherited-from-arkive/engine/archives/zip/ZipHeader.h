// Archive/Zip/Header.h

#ifndef __ARCHIVE_ZIP_HEADER_H
#define __ARCHIVE_ZIP_HEADER_H

#include <QtCore/qglobal.h>

namespace Zip {

namespace Signature {
extern quint32 LocalFileHeader;
extern quint32 DataDescriptor;
extern quint32 CentralFileHeader;
extern quint32 EndOfCentralDir;
extern quint32 Zip64EndOfCentralDir;
extern quint32 Zip64EndOfCentralDirLocator;

enum { MarkerSize = 4 };
}

enum {
    EcdSize = 22,
    Zip64EcdSize = 44,
    Zip64EcdLocatorSize = 20
};

namespace Header {
/*
struct CVersion
{
  quint8 Version;
  quint8 HostOS;
};
*/

namespace CompressionMethod {
enum Type {
    Stored = 0,
    Shrunk = 1,
    Reduced1 = 2,
    Reduced2 = 3,
    Reduced3 = 4,
    Reduced4 = 5,
    Imploded = 6,
    ReservedTokenizing = 7, // reserved for tokenizing
    Deflated = 8,
    Deflated64 = 9,
    PKImploding = 10,

    BZip2 = 12,
    WzPPMd = 0x62,
    WzAES = 0x63
};
enum { NumCompressionMethods = 11 } ;

const quint8 MadeByProgramVersion = 20;
const quint8 DeflateExtractVersion = 20;
const quint8 StoreExtractVersion = 10;
const quint8 SupportedVersion   = 20;
}

namespace ExtraID {
enum {
    Zip64 = 0x01,
    StrongEncrypt = 0x17,
    WzAES = 0x9901
};
}

const quint32 LocalBlockSize = 26;
const quint32 DataDescriptorSize = 16;
// const quint32 kDataDescriptor64Size = 16 + 8;
const quint32 CentralBlockSize = 42;

namespace Flags {
const int NumUsedBits = 4;
const int UsedBitsMask = (1 << kNumUsedBits) - 1;

const int Encrypted = 1 << 0;
const int DescriptorUsedMask = 1 << 3;
const int StrongEncrypted = 1 << 6;

const int ImplodeDictionarySizeMask = 1 << 1;
const int ImplodeLiteralsOnMask     = 1 << 2;

const int DeflateTypeBitStart = 1;
const int NumDeflateTypeBits = 2;
const int NumDeflateTypes = (1 << NumDeflateTypeBits);
const int DeflateTypeMask = (1 << NumDeflateTypeBits) - 1;
}

namespace HostOS {
enum {
    FAT      = 0,
    AMIGA    = 1,
    VMS      = 2,  // VAX/VMS
    Unix     = 3,
    VM_CMS   = 4,
    Atari    = 5,  // what if it's a minix filesystem? [cjh]
    HPFS     = 6,  // filesystem used by OS/2 (and NT 3.x)
    Mac      = 7,
    Z_System = 8,
    CPM      = 9,
    TOPS20   = 10, // pkzip 2.50 NTFS
    NTFS     = 11, // filesystem used by Windows NT
    QDOS     = 12, // SMS/QDOS
    Acorn    = 13, // Archimedes Acorn RISC OS
    VFAT     = 14, // filesystem used by Windows 95, NT
    MVS      = 15,
    BeOS     = 16, // hybrid POSIX/database filesystem
    Tandem   = 17,
    OS400    = 18,
    OSX      = 19
};
}
namespace UnixAttribute {
const quint32 IFMT   =   0170000;     /* Unix file type mask */

const quint32 IFDIR  =   0040000;     /* Unix directory */
const quint32 IFREG  =   0100000;     /* Unix regular file */
const quint32 IFSOCK =   0140000;     /* Unix socket (BSD, not SysV or Amiga) */
const quint32 IFLNK  =   0120000;     /* Unix symbolic link (not SysV, Amiga) */
const quint32 IFBLK  =   0060000;     /* Unix block special       (not Amiga) */
const quint32 IFCHR  =   0020000;     /* Unix character special   (not Amiga) */
const quint32 IFIFO  =   0010000;     /* Unix fifo    (BCC, not MSC or Amiga) */

const quint32 ISUID  =   04000;       /* Unix set user id on execution */
const quint32 ISGID  =   02000;       /* Unix set group id on execution */
const quint32 ISVTX  =   01000;       /* Unix directory permissions control */
const quint32 ENFMT  =   kISGID;   /* Unix record locking enforcement flag */
const quint32 IRWXU  =   00700;       /* Unix read, write, execute: owner */
const quint32 IRUSR  =   00400;       /* Unix read permission: owner */
const quint32 IWUSR  =   00200;       /* Unix write permission: owner */
const quint32 IXUSR  =   00100;       /* Unix execute permission: owner */
const quint32 IRWXG  =   00070;       /* Unix read, write, execute: group */
const quint32 IRGRP  =   00040;       /* Unix read permission: group */
const quint32 IWGRP  =   00020;       /* Unix write permission: group */
const quint32 IXGRP  =   00010;       /* Unix execute permission: group */
const quint32 IRWXO  =   00007;       /* Unix read, write, execute: other */
const quint32 IROTH  =   00004;       /* Unix read permission: other */
const quint32 IWOTH  =   00002;       /* Unix write permission: other */
const quint32 IXOTH  =   00001;       /* Unix execute permission: other */
}

namespace AmigaAttribute {
const quint32 IFMT     = 06000;       /* Amiga file type mask */
const quint32 IFDIR    = 04000;       /* Amiga directory */
const quint32 IFREG    = 02000;       /* Amiga regular file */
const quint32 IHIDDEN  = 00200;       /* to be supported in AmigaDOS 3.x */
const quint32 ISCRIPT  = 00100;       /* executable script (text command file) */
const quint32 IPURE    = 00040;       /* allow loading into resident memory */
const quint32 IARCHIVE = 00020;       /* not modified since bit was last set */
const quint32 IREAD    = 00010;       /* can be opened for reading */
const quint32 IWRITE   = 00004;       /* can be opened for writing */
const quint32 IEXECUTE = 00002;       /* executable image, a loadable runfile */
const quint32 IDELETE  = 00001;      /* can be deleted */
}
}

}
}

#endif
