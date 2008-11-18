// ZipHandler.cpp
#include "ZipHandler.h"

#include "interfaces/IPassword.h"

#include "kernel/StreamObjects.h"
#include "kernel/HandlerRegistry.h"
#include "kernel/FilterCoder.h"
#include "kernel/CopyCoder.h"
#include "kernel/ItemNameUtils.h"
#include "kernel/OutStreamWithCRC.h"
#include "kernel/TimeUtils.h"

#include "../../compression/Shrink/ShrinkDecoder.h"
#include "../../compression/Implode/ImplodeDecoder.h"

#include "../../crypto/Zip/ZipCipher.h"
#include "../../crypto/WzAES/WzAES.h"

#ifdef ZIP_STRONG_SUPORT
#include "../../crypto/ZipStrong/ZipStrong.h"
#endif

#include <QtCore/QDateTime>

namespace NArchive
{
namespace NZip {

// static const CMethodId kMethodId_Store = 0;
static const CMethodId kMethodId_ZipBase = 0x040100;
static const CMethodId kMethodId_BZip2 = 0x040202;

const char *kHostOS[] = {
    "FAT",
    "AMIGA",
    "VMS",
    "Unix",
    "VM/CMS",
    "Atari",
    "HPFS",
    "Macintosh",
    "Z-System",
    "CP/M",
    "TOPS-20",
    "NTFS",
    "SMS/QDOS",
    "Acorn",
    "VFAT",
    "MVS",
    "BeOS",
    "Tandem",
    "OS/400",
    "OS/X"
};


static const int kNumHostOSes = sizeof(kHostOS) / sizeof(kHostOS[0]);

static const char *kUnknownOS = "Unknown";

STATPROPSTG kProps[] = {
    { NULL, kpidPath, QVariant::String},
    { NULL, kpidIsFolder, QVariant::Bool},
    { NULL, kpidSize, QVariant::ULongLong},
    { NULL, kpidPackedSize, QVariant::ULongLong},
    { NULL, kpidLastWriteTime, QVariant::DateTime},
    { NULL, kpidAttributes, QVariant::UInt},

    { NULL, kpidEncrypted, QVariant::Bool},
    { NULL, kpidComment, QVariant::String},

    { NULL, kpidCRC, QVariant::UInt},

    { NULL, kpidMethod, QVariant::String},
    { NULL, kpidHostOS, QVariant::String}

    // { NULL, kpidUnpackVer, QVariant::Char},
};

const char *kMethods[] = {
    "Store",
    "Shrink",
    "Reduced1",
    "Reduced2",
    "Reduced2",
    "Reduced3",
    "Implode",

    "Tokenizing",
    "Deflate",
    "Deflate64",
    "PKImploding",
    "Unknown",
    "BZip2"
};

const int kNumMethods = sizeof(kMethods) / sizeof(kMethods[0]);
// const wchar_t *kUnknownMethod = L"Unknown";
const char *kPPMdMethod = "PPMd";
const char *kAESMethod = "AES";
const char *kZipCryptoMethod = "ZipCrypto";
const char *kStrongCryptoMethod = "StrongCrypto";

struct CStrongCryptoPair {
    quint16 Id;
    const char *Name;
};

CStrongCryptoPair g_StrongCryptoPairs[] = {
    { NStrongCryptoFlags::kDES, "DES" },
    { NStrongCryptoFlags::kRC2old, "RC2a" },
    { NStrongCryptoFlags::k3DES168, "3DES-168" },
    { NStrongCryptoFlags::k3DES112, "3DES-112" },
    { NStrongCryptoFlags::kAES128, "pkAES-128" },
    { NStrongCryptoFlags::kAES192, "pkAES-192" },
    { NStrongCryptoFlags::kAES256, "pkAES-256" },
    { NStrongCryptoFlags::kRC2, "RC2" },
    { NStrongCryptoFlags::kBlowfish, "Blowfish" },
    { NStrongCryptoFlags::kTwofish, "Twofish" },
    { NStrongCryptoFlags::kRC4, "RC4" }
};

STATPROPSTG kArcProps[] = {
    { NULL, kpidComment, QVariant::String}
};

CHandler::CHandler():
        RefCountedQObject(), m_ArchiveIsOpen(false)
{
    InitMethodProperties();
}

static QByteArray bytearray(const CByteBuffer &data)
{
    int size = (int)data.GetCapacity();
    if (size <= 0)
        return QByteArray();

    QByteArray s;
    s.resize(size + 1);
    memcpy(s.data(), (const quint8 *)data, size);
    s.data()[size] = '\0';
    return s;
}

IMP_IInArchive_Props
IMP_IInArchive_ArcProps

HRESULT CHandler::GetArchiveProperty(PROPID propID, QVariant *value)
{
    QVariant prop;
    switch (propID) {
    case kpidComment: {
        prop = QString::fromAscii(bytearray(m_Archive.m_ArchiveInfo.Comment));
        break;
    }
    }
    *value = prop;
    return S_OK;
}

HRESULT CHandler::GetNumberOfItems(quint32 *numItems)
{
    *numItems = m_Items.size();
    return S_OK;
}

HRESULT CHandler::GetProperty(quint32 index, PROPID propID, QVariant *value)
{
    QVariant prop;
    const CItemEx &item = *m_Items[index];
    switch (propID) {
    case kpidPath:
        prop = NItemName::GetOSName2(item.Name);
        break;
    case kpidIsFolder:
        prop = item.IsDirectory();
        break;
    case kpidSize:
        prop = item.UnPackSize;
        break;
    case kpidPackedSize:
        prop = item.PackSize;
        break;
    case kpidLastWriteTime: {
        prop = fromDosTime(QByteArray((char *)&item.Time, 4));
        break;
    }
    case kpidAttributes:
        prop = item.GetWinAttributes();
        break;
    case kpidEncrypted:
        prop = item.IsEncrypted();
        break;
    case kpidComment: {
        prop = QString::fromLocal8Bit(bytearray(item.Comment)); // item.GetCodePage()
        break;
    }
    case kpidCRC:
        if (item.IsThereCrc())
            prop = item.FileCRC;
        break;
    case kpidMethod: {
        quint16 methodId = item.CompressionMethod;
        QString method;
        if (item.IsEncrypted()) {
            if (methodId == NFileHeader::NCompressionMethod::kWzAES) {
                method = kAESMethod;
                CWzAesExtraField aesField;
                if (item.CentralExtra.GetWzAesField(aesField)) {
                    method += "-";
                    method += QString::number((aesField.Strength + 1) * 64);
                    method += " ";
                    methodId = aesField.Method;
                }
            } else {
                if (item.IsStrongEncrypted()) {
                    CStrongCryptoField f;
                    bool finded = false;
                    if (item.CentralExtra.GetStrongCryptoField(f)) {
                        for (int i = 0; i < sizeof(g_StrongCryptoPairs) / sizeof(g_StrongCryptoPairs[0]); i++) {
                            const CStrongCryptoPair &pair = g_StrongCryptoPairs[i];
                            if (f.AlgId == pair.Id) {
                                method += pair.Name;
                                finded = true;
                                break;
                            }
                        }
                    }
                    if (!finded)
                        method += kStrongCryptoMethod;
                } else
                    method += kZipCryptoMethod;
                method += " ";
            }
        }
        if (methodId < kNumMethods)
            method += kMethods[methodId];
        else if (methodId == NFileHeader::NCompressionMethod::kWzPPMd)
            method += kPPMdMethod;
        else {
            method += QString::number(methodId);
        }
        prop = method;
        break;
    }
    case kpidHostOS:
        prop = QLatin1String((item.MadeByVersion.HostOS < kNumHostOSes) ?
               (kHostOS[item.MadeByVersion.HostOS]) : kUnknownOS);
        break;
    }
    *value = prop;
    return S_OK;
//    COM_TRY_END
}

class CPropgressImp: public CProgressVirt
{
    RefPtr<IArchiveOpenCallback> m_OpenArchiveCallback;
public:
    STDMETHOD(SetCompleted)(const quint64 *numFiles);
    void Init(IArchiveOpenCallback *openArchiveCallback) {
        m_OpenArchiveCallback = openArchiveCallback;
    }
};

STDMETHODIMP CPropgressImp::SetCompleted(const quint64 *numFiles)
{
    if (m_OpenArchiveCallback)
        return m_OpenArchiveCallback->SetCompleted(numFiles, NULL);
    return S_OK;
}

STDMETHODIMP CHandler::Open(IInStream *inStream,
                            const quint64 *maxCheckStartPosition, IArchiveOpenCallback *openArchiveCallback)
{
//    COM_TRY_BEGIN
    // try
    {
        if (!m_Archive.Open(inStream, maxCheckStartPosition))
            return S_FALSE;
        m_ArchiveIsOpen = true;
        m_Items.clear();
        if (openArchiveCallback != NULL) {
            RINOK(openArchiveCallback->SetTotal(NULL, NULL));
        }
        CPropgressImp propgressImp;
        propgressImp.Init(openArchiveCallback);
        RINOK(m_Archive.ReadHeaders(m_Items, &propgressImp));
    }
    /*
    catch(...)
    {
      return S_FALSE;
    }
    */
//    COM_TRY_END
    return S_OK;
}

STDMETHODIMP CHandler::Close()
{
    m_Items.clear();
    m_Archive.Close();
    m_ArchiveIsOpen = false;
    return S_OK;
}

//////////////////////////////////////
// CHandler::DecompressItems

struct CMethodItem {
    quint16 ZipMethod;
    RefPtr<ICompressCoder> Coder;
};

class CZipDecoder
{
    NCrypto::NZip::CDecoder *_zipCryptoDecoderSpec;
    NCrypto::NWzAES::CDecoder *_aesDecoderSpec;
    RefPtr<ICompressFilter> _zipCryptoDecoder;
    RefPtr<ICompressFilter> _aesDecoder;
#ifdef ZIP_STRONG_SUPORT
    NCrypto::NZipStrong::CDecoder *_zsDecoderSpec;
    RefPtr<ICompressFilter> _zsDecoder;
#endif
    CFilterCoder *filterStreamSpec;
    RefPtr<ISequentialInStream> filterStream;
    RefPtr<ICryptoGetTextPassword> getTextPassword;
    QList<CMethodItem *> methodItems;

public:
    CZipDecoder(): _zipCryptoDecoderSpec(0), _aesDecoderSpec(0), filterStreamSpec(0) {}

    HRESULT Decode(
        CInArchive &archive, const CItemEx &item,
        ISequentialOutStream *realOutStream,
        IArchiveExtractCallback *extractCallback,
        quint32 numThreads, qint32 &res);
};

HRESULT CZipDecoder::Decode(
    CInArchive &archive, const CItemEx &item,
    ISequentialOutStream *realOutStream,
    IArchiveExtractCallback *_extractCallback,
    quint32 numThreads, qint32 &res)
{
    RefPtr<IArchiveExtractCallback> extractCallback(_extractCallback);

    res = NArchive::NExtract::NOperationResult::kDataError;
    CInStreamReleaser inStreamReleaser;

    bool needCRC = true;
    bool aesMode = false;
#ifdef ZIP_STRONG_SUPORT
    bool pkAesMode = false;
#endif
    quint16 methodId = item.CompressionMethod;
    if (item.IsEncrypted()) {
        if (item.IsStrongEncrypted()) {
#ifdef ZIP_STRONG_SUPORT
            CStrongCryptoField f;
            if (item.CentralExtra.GetStrongCryptoField(f)) {
                pkAesMode = true;
            }
            if (!pkAesMode)
#endif
            {
                res = NArchive::NExtract::NOperationResult::kUnSupportedMethod;
                return S_OK;
            }
        }
        if (methodId == NFileHeader::NCompressionMethod::kWzAES) {
            CWzAesExtraField aesField;
            if (item.CentralExtra.GetWzAesField(aesField)) {
                aesMode = true;
                needCRC = aesField.NeedCrc();
            }
        }
    }

    COutStreamWithCRC *outStreamSpec = new COutStreamWithCRC;
    RefPtr<ISequentialOutStream> outStream = outStreamSpec;
    outStreamSpec->SetStream(realOutStream);
    outStreamSpec->Init(needCRC);

    quint64 authenticationPos;

    RefPtr<ISequentialInStream> inStream;
    {
        quint64 packSize = item.PackSize;
        if (aesMode) {
            if (packSize < NCrypto::NWzAES::kMacSize)
                return S_OK;
            packSize -= NCrypto::NWzAES::kMacSize;
        }
        quint64 dataPos = item.GetDataPosition();
        inStream = archive.CreateLimitedStream(dataPos, packSize);
        authenticationPos = dataPos + packSize;
    }

    RefPtr<ICompressFilter> cryptoFilter;
    if (item.IsEncrypted()) {
        if (aesMode) {
            CWzAesExtraField aesField;
            if (!item.CentralExtra.GetWzAesField(aesField))
                return S_OK;
            methodId = aesField.Method;
            if (!_aesDecoder) {
                _aesDecoderSpec = new NCrypto::NWzAES::CDecoder;
                _aesDecoder = _aesDecoderSpec;
            }
            cryptoFilter = _aesDecoder;
            quint8 properties = aesField.Strength;
            RINOK(_aesDecoderSpec->SetDecoderProperties2(&properties, 1));
        }
#ifdef ZIP_STRONG_SUPORT
        else if (pkAesMode) {
            if (!_zsDecoder) {
                _zsDecoderSpec = new NCrypto::NZipStrong::CDecoder;
                _zsDecoder = _zsDecoderSpec;
            }
            cryptoFilter = _zsDecoder;
        }
#endif
        else {
            if (!_zipCryptoDecoder) {
                _zipCryptoDecoderSpec = new NCrypto::NZip::CDecoder;
                _zipCryptoDecoder = _zipCryptoDecoderSpec;
            }
            cryptoFilter = _zipCryptoDecoder;
        }
        RefPtr<ICryptoSetPassword> cryptoSetPassword;
        RINOK(cryptoFilter.QueryInterface(&cryptoSetPassword));

        if (!getTextPassword)
            extractCallback.QueryInterface(&getTextPassword);

        if (getTextPassword) {
            QString password;
            RINOK(getTextPassword->CryptoGetTextPassword(&password));
            QByteArray charPassword;
            if (aesMode
#ifdef ZIP_STRONG_SUPORT
                    || pkAesMode
#endif
               ) {
                charPassword = password.toUtf8();
                /*
                for (int i = 0;; i++)
                {
                  wchar_t c = password[i];
                  if (c == 0)
                    break;
                  if (c >= 0x80)
                  {
                    res = NArchive::NExtract::NOperationResult::kDataError;
                    return S_OK;
                  }
                  charPassword += (char)c;
                }
                */
            } else {
                // we use OEM. WinZip/Windows probably use ANSI for some files
                charPassword = password.toLocal8Bit();
            }
            HRESULT res = cryptoSetPassword->CryptoSetPassword(
                              (const quint8 *)charPassword.constData(), charPassword.length());
            if (res != S_OK)
                return S_OK;
        } else {
            RINOK(cryptoSetPassword->CryptoSetPassword(0, 0));
        }
    }

    int m;
    for (m = 0; m < methodItems.size(); m++)
        if (methodItems[m]->ZipMethod == methodId)
            break;

    if (m == methodItems.size()) {
        CMethodItem mi;
        mi.ZipMethod = methodId;
        if (methodId == NFileHeader::NCompressionMethod::kStored)
            mi.Coder = new NCompress::CCopyCoder;
        else if (methodId == NFileHeader::NCompressionMethod::kShrunk)
            mi.Coder = new NCompress::NShrink::CDecoder;
        else if (methodId == NFileHeader::NCompressionMethod::kImploded)
            mi.Coder = new NCompress::NImplode::NDecoder::CCoder;
        else {
            CMethodId szMethodID;
            if (methodId == NFileHeader::NCompressionMethod::kBZip2)
                szMethodID = kMethodId_BZip2;
            else {
                if (methodId > 0xFF) {
                    res = NArchive::NExtract::NOperationResult::kUnSupportedMethod;
                    return S_OK;
                }
                szMethodID = kMethodId_ZipBase + (quint8)methodId;
            }

            QObject *dec = CreateDecoderForId(szMethodID);
            if (dec)
                mi.Coder = qobject_cast<ICompressCoder *>(dec);

            if (mi.Coder == 0) {
                delete dec;
                res = NArchive::NExtract::NOperationResult::kUnSupportedMethod;
                return S_OK;
            }
        }
        methodItems.append(new CMethodItem(mi));
        m = methodItems.size() - 1;
    }
    RefPtr<ICompressCoder> coder(methodItems[m]->Coder);

    {
        RefPtr<ICompressSetDecoderProperties2> setDecoderProperties;
        coder.QueryInterface(&setDecoderProperties);
        if (setDecoderProperties) {
            quint8 properties = (quint8)item.Flags;
            RINOK(setDecoderProperties->SetDecoderProperties2(&properties, 1));
        }
    }

#ifdef THREADED
    {
        RefPtr<ICompressSetCoderMt> setCoderMt;
        coder.QueryInterface(&setCoderMt);
        if (setCoderMt) {
            RINOK(setCoderMt->SetNumberOfThreads(numThreads));
        }
    }
#endif
    {
        HRESULT result;
        RefPtr<ISequentialInStream> inStreamNew;
        if (item.IsEncrypted()) {
            if (!filterStream) {
                filterStreamSpec = new CFilterCoder;
                filterStream = filterStreamSpec;
            }
            filterStreamSpec->setFilter(cryptoFilter);
            if (aesMode) {
                RINOK(_aesDecoderSpec->ReadHeader(inStream));
            }
#ifdef ZIP_STRONG_SUPORT
            else if (pkAesMode) {
                RINOK(_zsDecoderSpec->ReadHeader(inStream));
            }
#endif
            else {
                RINOK(_zipCryptoDecoderSpec->ReadHeader(inStream));
            }
            RINOK(filterStreamSpec->SetInStream(inStream));
            inStreamReleaser.FilterCoder = filterStreamSpec;
            inStreamNew = filterStream;

            if (aesMode) {
                if (!_aesDecoderSpec->CheckPasswordVerifyCode())
                    return S_OK;
            }
        } else
            inStreamNew = inStream;
        result = coder->Code(inStreamNew, outStream, NULL, &item.UnPackSize);
        if (result == S_FALSE)
            return S_OK;
        RINOK(result);
    }
    bool crcOK = true;
    bool authOk = true;
    if (needCRC)
        crcOK = (outStreamSpec->GetCRC() == item.FileCRC);
    if (aesMode) {
        inStream = archive.CreateLimitedStream(authenticationPos, NCrypto::NWzAES::kMacSize);
        if (_aesDecoderSpec->CheckMac(inStream, authOk) != S_OK)
            authOk = false;
    }

    res = ((crcOK && authOk) ?
           NArchive::NExtract::NOperationResult::kOK :
           NArchive::NExtract::NOperationResult::kCRCError);
    return S_OK;
}


STDMETHODIMP CHandler::Extract(const quint32* indices, quint32 numItems,
                               qint32 _aTestMode, IArchiveExtractCallback *extractCallback)
{
    RefPtr<IArchiveExtractCallback> kungFuGrip(extractCallback);

    CZipDecoder myDecoder;
    bool testMode = (_aTestMode != 0);
    quint64 totalUnPacked = 0, totalPacked = 0;
    bool allFilesMode = (numItems == quint32(-1));
    if (allFilesMode)
        numItems = m_Items.size();
    if (numItems == 0)
        return S_OK;
    quint32 i;
    for (i = 0; i < numItems; i++) {
        const CItemEx &item = *m_Items[allFilesMode ? i : indices[i]];
        totalUnPacked += item.UnPackSize;
        totalPacked += item.PackSize;
    }
    RINOK(extractCallback->SetTotal(totalUnPacked));

    quint64 currentTotalUnPacked = 0, currentTotalPacked = 0;
    quint64 currentItemUnPacked, currentItemPacked;

    for (i = 0; i < numItems; i++, currentTotalUnPacked += currentItemUnPacked,
        currentTotalPacked += currentItemPacked) {
        currentItemUnPacked = 0;
        currentItemPacked = 0;

        qint32 askMode = testMode ?
                         NArchive::NExtract::NAskMode::kTest :
                         NArchive::NExtract::NAskMode::kExtract;
        qint32 index = allFilesMode ? i : indices[i];

        ISequentialOutStream *prealOutStream;
        RINOK(extractCallback->GetStream(index, &prealOutStream, askMode));
        RefPtr<ISequentialOutStream> realOutStream(prealOutStream);

        CItemEx item = *m_Items[index];
        if (!item.FromLocal) {
            HRESULT res = m_Archive.ReadLocalItemAfterCdItem(item);
            if (res == S_FALSE) {
                if (item.IsDirectory() || realOutStream || testMode) {
                    RINOK(extractCallback->PrepareOperation(askMode));
                    realOutStream = 0;
                    RINOK(extractCallback->SetOperationResult(NArchive::NExtract::NOperationResult::kUnSupportedMethod));
                }
                continue;
            }
            RINOK(res);
        }

        if (item.IsDirectory() || item.IgnoreItem()) {
            // if (!testMode)
            {
                RINOK(extractCallback->PrepareOperation(askMode));
                RINOK(extractCallback->SetOperationResult(NArchive::NExtract::NOperationResult::kOK));
            }
            continue;
        }

        currentItemUnPacked = item.UnPackSize;
        currentItemPacked = item.PackSize;

        if (!testMode && (!realOutStream))
            continue;

        RINOK(extractCallback->PrepareOperation(askMode));

        qint32 res;
        RINOK(myDecoder.Decode(
                  m_Archive, item, realOutStream, extractCallback,
                  _numThreads, res));

        RINOK(extractCallback->SetOperationResult(res))
    }
    return S_OK;
}

}
}

#include "ZipHandler.moc"
