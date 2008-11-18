#ifndef GZIPHEADER_H
#define GZIPHEADER_H

namespace ArchiveEngine {
namespace Gzip {

static const quint16 Signature = 0x8B1F;
static const quint32 SignatureSize = 2;

enum {
    FlagDataIsText = 1 << 0,
    FlagHeaderCrcIsPresent = 1 << 1,
    FlagExtraIsPresent = 1 << 2,
    FlagNameIsPresent = 1 << 3,
    FlagCommentIsPresent = 1 << 4
};

enum {
    ExtraFlagMaximum = 2,
    ExtraFlagFastest = 4
};

enum {
    CompressionMethodDeflate = 8
};

enum GzipHost {
    HostFAT      = 0,  // filesystem used by MS-DOS, OS/2, Win32
    // pkzip 2.50 (FAT / VFAT / FAT32 file systems)
    HostAMIGA    = 1,
    HostVMS      = 2,  // VAX/VMS
    HostUnix     = 3,
    HostVM_CMS   = 4,
    HostAtari    = 5,  // what if it's a minix filesystem? [cjh]
    HostHPFS     = 6,  // filesystem used by OS/2 (and NT 3.x)
    HostMac      = 7,
    HostZ_System = 8,
    HostCPM      = 9,
    HostTOPS20   = 10, // pkzip 2.50 NTFS
    HostNTFS     = 11, // filesystem used by Windows NT
    HostQDOS     = 12, // SMS/QDOS
    HostAcorn    = 13, // Archimedes Acorn RISC OS
    HostVFAT     = 14, // filesystem used by Windows 95, NT
    HostMVS      = 15,
    HostBeOS     = 16, // hybrid POSIX/database filesystem
    // BeBOX or PowerMac
    HostTandem   = 17,
    HostTHEOS    = 18,

    HostUnknown = 255
};
const int NumHostSystems = 19;

}
}

#endif
