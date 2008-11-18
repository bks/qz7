#ifndef TARFORMAT_H
#define TARFORMAT_H

namespace ArchiveEngine {
namespace Tar {

enum {
    BLOCK_SIZE = 512
};

// the various field offsets in a tar block header
enum {
    NameOffset = 0,
    ModeOffset = 100,
    UidOffset = 108,
    GidOffset = 116,
    SizeOffset = 124,
    MTimeOffset = 136,
    CSumOffset = 148,
    TypeOffset = 156,
    LinkNameOffset = 157,
    MagicOffset = 257,
    VersionOffset = 263,
    UserNameOffset = 265,
    GroupNameOffset = 297,
    DeviceMajorOffset = 329,
    DeviceMinorOffset = 337,
    USTarPrefixOffset = 345,
    GnuATimeOffset = 345,
    GnuCTimeOffset = 357,
    GnuMultiVolumeOffsetOffset = 369,
    GnuSparseOffset = 386,
    GnuHasMoreSparseElementsOffset = 482,
    GnuRealSizeOffset = 483,
    STarPrefixOffset = 345,
    STarATimeOffset = 476,
    STarCTimeOffset = 488,
    STarTailMagicOffset = 508
};

// the sizes of the various fields in a tar header
enum {
    NameSize = 100,
    ModeSize = 8,
    UidSize = 8,
    GidSize = 8,
    SizeSize = 12,
    MTimeSize = 12,
    CSumSize = 8,
    TypeSize = 1,
    LinkNameSize = 100,
    MagicSize = 6,
    VersionSize = 2,
    UserNameSize = 32,
    GroupNameSize = 32,
    DeviceMajorSize = 8,
    DeviceMinorSize = 8,
    USTarPrefixSize = 155,
    GnuATimeSize = 12,
    GnuCTimeSize = 12,
    GnuMultiVolumeOffsetSize = 12,
    GnuSparseSize = 24*4,
    GnuHasMoreSparseElementsSize = 1,
    GnuRealSizeSize = 12,
    STarPrefixSize = 131,
    STarATimeSize = 12,
    STarCTimeSize = 12,
    STarTailMagicSize = 4
};

// the fields of a sparse file record
enum {
    SparseOffsetOffset = 0,
    SparseOffsetSize = 12,
    SparseSizeOffset = 12,
    SparseSizeSize = 12,
    SparseItemSize = 24,
    SparseHasMoreElementsOffset = 21*24,
    SparseHasMoreElementsSize = 1
};

// the defined block types for a tar file
enum {
    TarItemRegularFile = '0',
    TarItemRegularFileAlternate = '\0',
    TarItemHardLink = '1',
    TarItemSymbolicLink = '2',
    TarItemCharacterDevice = '3',
    TarItemBlockDevice = '4',
    TarItemDirectory = '5',
    TarItemFifo = '6',
    TarItemDefaultAttributes = 'g',
    TarItemAttributes = 'x',
    TarItemAttributesAlternate = 'X',
    TarItemGnuSparseFile = 'S',
    TarItemGnuDumpDirectory = 'D',
    TarItemGnuLongLink = 'K',
    TarItemGnuLongName = 'L',
    TarItemGnuMultiVolume = 'M',
    TarItemGnuNameMangleList = 'N',
    TarItemGnuVolumeHeader = 'V',
    TarItemSolarisAccessControlList = 'A',
    TarItemSolarisExtendedAttribute = 'E',
    TarItemSTarInodeOnly = 'I'
};

enum {
    GnuSparseFormat0p0 = 0,
    GnuSparseFormat0p1 = 1,
    GnuSparseFormat1p0 = 10
};

}
}

#endif
