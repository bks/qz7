// Archive/ZipItem.h

#ifndef __ARCHIVE_ZIP_ITEM_H
#define __ARCHIVE_ZIP_ITEM_H

#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QString>

#include "interfaces/Archive.h"

#include "ZipHeader.h"

namespace Zip {

class Version {
public:
    Version(quint8 host, quint8 version) : mHostOS(hos), mVersion(version) { }
    quint8 version() { return mVersion; }
    quint8 hostOS() { return mHostOS; }

private:
    quint8 mHostOS;
    quint8 mVersion;
};

bool operator==(const Version &v1, const Version &v2);
bool operator!=(const Version &v1, const Version &v2);

struct ExtraSubBlock {
    quint16 id;
    QByteArray data;
};

struct WzAesExtraField {
    quint16 VendorVersion; // 0x0001 - AE-1, 0x0002 - AE-2,
    // quint16 VendorId; // "AE"
    quint8 Strength; // 1 - 128-bit , 2 - 192-bit , 3 - 256-bit
    quint16 Method;

    WzAesExtraField(): VendorVersion(2), Strength(3), Method(0) {}

    bool NeedCrc() const {
        return (VendorVersion == 1);
    }

    bool ParseFromSubBlock(const ExtraSubBlock &sb) {
        if (sb.id != Zip::Header::ExtraID::WzAES)
            return false;
        if (sb.data.size() < 7)
            return false;
        VendorVersion = (((quint16)sb.data[1]) << 8) | sb.data[0];
        if (sb.data[2] != 'A' || sb.data[3] != 'E')
            return false;
        Strength = sb.data[4];
        Method = (((quint16)sb.data[6]) << 16) | sb.data[5];
        return true;
    }
    void SetSubBlock(ExtraSubBlock &sb) const {
        QByteArray d;
        d.reserve(7);
        d.append(char(VendorVersion));
        d.append(char(VendorVersion >> 8));
        d.append('A');
        d.append('E');
        d.append(char(Strength);
        d.append(char(Method));
        d.append(char(Method >> 8));
        sb.id = NFileHeader::NExtraID::kWzAES;
        sb.data = d;
    }
};

namespace StrongCryptoFlags {
const quint16 DES = 0x6601;
const quint16 RC2old = 0x6602;
const quint16 3DES168 = 0x6603;
const quint16 3DES112 = 0x6609;
const quint16 AES128 = 0x660E;
const quint16 AES192 = 0x660F;
const quint16 AES256 = 0x6610;
const quint16 RC2 = 0x6702;
const quint16 Blowfish = 0x6720;
const quint16 Twofish = 0x6721;
const quint16 RC4 = 0x6801;
}

struct StrongCryptoField {
    quint16 Format;
    quint16 AlgId;
    quint16 BitLen;
    quint16 Flags;

    bool ParseFromSubBlock(const ExtraSubBlock& sb) {
        if (sb.id != Header::ExtraID::StrongEncrypt)
            return false;
        const QByteArray p = sb.data;
        if (sb.data.size() < 8)
            return false;
        Format = (((quint16)p[1]) << 8) | p[0];
        AlgId  = (((quint16)p[3]) << 8) | p[2];
        BitLen = (((quint16)p[5]) << 8) | p[4];
        Flags  = (((quint16)p[7]) << 8) | p[6];
        return (Format == 2);
    }
};

struct ExtraBlock {
    QList<ExtraSubBlock> SubBlocks;
    void clear() {
        SubBlocks.clear();
    }
    size_t byteSize() const {
        size_t res = 0;
        for (int i = 0; i < SubBlocks.size(); i++)
            res += SubBlocks[i]->data.size() + 2 + 2;
        return res;
    }
    bool GetWzAesField(WzAesExtraField &aesField) const {
        for (int i = 0; i < SubBlocks.size(); i++)
            if (aesField.ParseFromSubBlock(SubBlocks[i]))
                return true;
        return false;
    }

    bool GetStrongCryptoField(StrongCryptoField &f) const {
        for (int i = 0; i < SubBlocks.size(); i++)
            if (f.ParseFromSubBlock(SubBlocks[i]))
                return true;
        return false;
    }

    bool HasWzAesField() const {
        WzAesExtraField aesField;
        return GetWzAesField(aesField);
    }

    /*
    bool HasStrongCryptoField() const
    {
      CStrongCryptoField f;
      return GetStrongCryptoField(f);
    }
    */

    void RemoveUnknownSubBlocks() {
        for (int i = SubBlocks.size() - 1; i >= 0;) {
            const ExtraSubBlock subBlock = SubBlocks[i];
            if (subBlock->id != FileHeader::ExtraID::WzAES) {
                delete subBlock;
                SubBlocks.removeAt(i);
            } else
                i--;
        }
    }
};

namespace Item {
enum {
    ExtraDataSize,
    ExtraData,
    Flags,
    RequiredVersion,
    CreatorVersion,
    FileHeaderSize,
    LocalExtraSize,
    InternalAttributes,
    ExternalAttributes
}

enum ImplodeFlag {
    NoImplodeFlags = 0,
    ImplodeBigDictionary = 1,
    ImplodeLiteralsOn = 2
};
Q_DECLARE_FLAGS(ImplodeFlag, ImplodeFlags)

ImplodeFlags implodeFlags(const ArchiveItem&);

}
}

Q_DECLARE_METATYPE(Zip::Version)
Q_DECLARE_TYPEINFO(Zip::Version, Q_MOVABLE_TYPE)
Q_DECLARE_METATYPE(Zip::ExtraBlock)
Q_DECLARE_OPERATORS_FOR_FLAGS(Zip::ImplodeFlags)

#endif


