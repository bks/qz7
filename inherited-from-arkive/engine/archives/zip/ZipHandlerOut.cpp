// Zip/HandlerOut.cpp
#include "ZipHandler.h"
#include "ZipUpdate.h"

#include "interfaces/IPassword.h"
#include "kernel/ItemNameUtils.h"
#include "kernel/OutBuffer.h"
#include "kernel/WinAttributes.h"
#include "../../crypto/WzAES/WzAES.h"

namespace NArchive
{
namespace NZip {

static const quint32 kDeflateAlgoX1 = 0;
static const quint32 kDeflateAlgoX5 = 1;

static const quint32 kDeflateNumPassesX1  = 1;
static const quint32 kDeflateNumPassesX7  = 3;
static const quint32 kDeflateNumPassesX9  = 10;

static const quint32 kNumFastBytesX1 = 32;
static const quint32 kNumFastBytesX7 = 64;
static const quint32 kNumFastBytesX9 = 128;

static const quint32 kBZip2NumPassesX1 = 1;
static const quint32 kBZip2NumPassesX7 = 2;
static const quint32 kBZip2NumPassesX9 = 7;

static const quint32 kBZip2DicSizeX1 = 100000;
static const quint32 kBZip2DicSizeX3 = 500000;
static const quint32 kBZip2DicSizeX5 = 900000;

STDMETHODIMP CHandler::GetFileTimeType(quint32 *timeType)
{
    *timeType = NFileTimeType::kDOS;
    return S_OK;
}

static bool IsAsciiString(const QString &s)
{
    for (int i = 0; i < s.length(); i++) {
        QChar c = s[i];
        if (c < 0x20 || c > 0x7F)
            return false;
    }
    return true;
}

STDMETHODIMP CHandler::UpdateItems(ISequentialOutStream *outStream, quint32 numItems,
                                   IArchiveUpdateCallback *updateCallback)
{
//    COM_TRY_BEGIN2
    QList<CUpdateItem *> updateItems;
    for (quint32 i = 0; i < numItems; i++) {
        CUpdateItem updateItem;
        qint32 newData;
        qint32 newProperties;
        quint32 indexInArchive;
        if (!updateCallback)
            return E_FAIL;
        RINOK(updateCallback->GetUpdateItemInfo(i,
                                                &newData, // 1 - compress 0 - copy
                                                &newProperties,
                                                &indexInArchive));
        updateItem.NewProperties = (newProperties != 0);
        updateItem.NewData = (newData != 0);
        updateItem.IndexInArchive = indexInArchive;
        updateItem.IndexInClient = i;
        // bool existInArchive = (indexInArchive != quint32(-1));

        if (updateItem.NewProperties) {
            FILETIME utcFileTime;
            QString name;
            bool isDirectoryStatusDefined;
            {
                QVariant propVariant;
                RINOK(updateCallback->GetProperty(i, kpidAttributes, &propVariant));
                if (!propVariant.isValid())
                    updateItem.Attributes = 0;
                else if (propVariant.type() != QVariant::UInt)
                    return E_INVALIDARG;
                else
                    updateItem.Attributes = propVariant.toUInt();
            }
            {
                QVariant propVariant;
                RINOK(updateCallback->GetProperty(i, kpidLastWriteTime, &propVariant));
                if (propVariant.type() != QVariant::DateTime)
                    return E_INVALIDARG;
#warning port me!
#if 0
                utcFileTime = propVariant.filetime;
#endif
            }
            {
                QVariant propVariant;
                RINOK(updateCallback->GetProperty(i, kpidPath, &propVariant));
                if (!propVariant.isValid())
                    name.clear();
                else if (propVariant.type() != QVariant::String)
                    return E_INVALIDARG;
                else
                    name = propVariant.toString();
            }
            {
                QVariant propVariant;
                RINOK(updateCallback->GetProperty(i, kpidIsFolder, &propVariant));
                if (!propVariant.isValid())
                    isDirectoryStatusDefined = false;
                else if (propVariant.type() != QVariant::Bool)
                    return E_INVALIDARG;
                else {
                    updateItem.IsDirectory = propVariant.toBool();
                    isDirectoryStatusDefined = true;
                }
            }
#warning port me!
#if 0
            FILETIME localFileTime;
            if (!FileTimeToLocalFileTime(&utcFileTime, &localFileTime))
                return E_INVALIDARG;
            if (!FileTimeToDosTime(localFileTime, updateItem.Time)) {
                // return E_INVALIDARG;
            }
#endif

            if (!isDirectoryStatusDefined)
                updateItem.IsDirectory = ((updateItem.Attributes & FILE_ATTRIBUTE_DIRECTORY) != 0);

            name = NItemName::MakeLegalName(name);
            bool needSlash = updateItem.IsDirectory;
            const char kSlash = '/';
            if (!name.isEmpty()) {
                if (name[name.length() - 1] == kSlash) {
                    if (!updateItem.IsDirectory)
                        return E_INVALIDARG;
                    needSlash = false;
                }
            }
            if (needSlash)
                name += kSlash;
            updateItem.Name = name.toLocal8Bit();
            if (updateItem.Name.length() > 0xFFFF)
                return E_INVALIDARG;

            updateItem.IndexInClient = i;
            /*
            if(existInArchive)
            {
              const CItemEx &itemInfo = m_Items[indexInArchive];
              // updateItem.Commented = itemInfo.IsCommented();
              updateItem.Commented = false;
              if(updateItem.Commented)
              {
                updateItem.CommentRange.Position = itemInfo.GetCommentPosition();
                updateItem.CommentRange.Size  = itemInfo.CommentSize;
              }
            }
            else
              updateItem.Commented = false;
            */
        }
        if (updateItem.NewData) {
            quint64 size;
            {
                QVariant propVariant;
                RINOK(updateCallback->GetProperty(i, kpidSize, &propVariant));
                if (propVariant.type() != QVariant::ULongLong)
                    return E_INVALIDARG;
                size = propVariant.toULongLong();
            }
            updateItem.Size = size;
        }
        updateItems.append(new CUpdateItem(updateItem));
    }

    RefPtr<ICryptoGetTextPassword2> getTextPassword;
    if (!getTextPassword) {
        RefPtr<IArchiveUpdateCallback> udateCallBack2(updateCallback);
        udateCallBack2.QueryInterface(&getTextPassword);
    }
    CCompressionMethodMode options;

    if (getTextPassword) {
        QString password;
        qint32 passwordIsDefined;
        RINOK(getTextPassword->CryptoGetTextPassword2(&passwordIsDefined, &password));
        options.PasswordIsDefined = (passwordIsDefined != 0);
        if (options.PasswordIsDefined) {
            if (!IsAsciiString(password))
                return E_INVALIDARG;
            if (m_IsAesMode) {
                if (options.Password.length() > NCrypto::NWzAES::kPasswordSizeMax)
                    return E_INVALIDARG;
            }
            options.Password = password.toLocal8Bit();
            options.IsAesMode = m_IsAesMode;
            options.AesKeyMode = m_AesKeyMode;
        }
    } else
        options.PasswordIsDefined = false;

    int level = m_Level;
    if (level < 0)
        level = 5;

    quint8 mainMethod;
    if (m_MainMethod < 0)
        mainMethod = (quint8)(((level == 0) ?
                               NFileHeader::NCompressionMethod::kStored :
                               NFileHeader::NCompressionMethod::kDeflated));
    else
        mainMethod = (quint8)m_MainMethod;
    options.MethodSequence.append(mainMethod);
    if (mainMethod != NFileHeader::NCompressionMethod::kStored)
        options.MethodSequence.append(NFileHeader::NCompressionMethod::kStored);
    bool isDeflate = (mainMethod == NFileHeader::NCompressionMethod::kDeflated) ||
                     (mainMethod == NFileHeader::NCompressionMethod::kDeflated64);
    bool isBZip2 = (mainMethod == NFileHeader::NCompressionMethod::kBZip2);
    options.NumPasses = m_NumPasses;
    options.DicSize = m_DicSize;
    options.NumFastBytes = m_NumFastBytes;
    options.NumMatchFinderCycles = m_NumMatchFinderCycles;
    options.NumMatchFinderCyclesDefined = m_NumMatchFinderCyclesDefined;
    options.Algo = m_Algo;
#ifdef THREADED
    options.NumThreads = _numThreads;
#endif
    if (isDeflate) {
        if (options.NumPasses == 0xFFFFFFFF)
            options.NumPasses = (level >= 9 ? kDeflateNumPassesX9 :
                                 (level >= 7 ? kDeflateNumPassesX7 :
                                  kDeflateNumPassesX1));
        if (options.NumFastBytes == 0xFFFFFFFF)
            options.NumFastBytes = (level >= 9 ? kNumFastBytesX9 :
                                    (level >= 7 ? kNumFastBytesX7 :
                                     kNumFastBytesX1));
        if (options.Algo == 0xFFFFFFFF)
            options.Algo =
                (level >= 5 ? kDeflateAlgoX5 :
                 kDeflateAlgoX1);
    }
    if (isBZip2) {
        if (options.NumPasses == 0xFFFFFFFF)
            options.NumPasses = (level >= 9 ? kBZip2NumPassesX9 :
                                 (level >= 7 ? kBZip2NumPassesX7 :
                                  kBZip2NumPassesX1));
        if (options.DicSize == 0xFFFFFFFF)
            options.DicSize = (level >= 5 ? kBZip2DicSizeX5 :
                               (level >= 3 ? kBZip2DicSizeX3 :
                                kBZip2DicSizeX1));
    }

    return Update(
              m_Items, updateItems, outStream,
               m_ArchiveIsOpen ? &m_Archive : NULL, &options, updateCallback);
//    COM_TRY_END2
}

STDMETHODIMP CHandler::SetProperties(const QMap<QString, QVariant>& properties)
{
#ifdef THREADED
    const quint32 numProcessors = NSystem::GetNumberOfProcessors();
    _numThreads = numProcessors;
#endif
    InitMethodProperties();
    for (QMap<QString, QVariant>::const_iterator i = properties.constBegin(); i != properties.constEnd(); i++) {
        QString name = i.key();
        if (name.isEmpty())
            return E_INVALIDARG;

        const QVariant& prop = i.value();

        if (name == "CompressionLevel") {
            quint32 level = 9;
            bool ok;
            level = prop.toInt(&ok);
            if (!ok)
                return E_INVALIDARG;
            m_Level = level;
            continue;
        } else if (name == "Method") {
            if (prop.type() == QVariant::String) {
                QString valueString = prop.toString().toUpper();
                if (valueString == QLatin1String("COPY"))
                    m_MainMethod = NFileHeader::NCompressionMethod::kStored;
                else if (valueString == QLatin1String("DEFLATE"))
                    m_MainMethod = NFileHeader::NCompressionMethod::kDeflated;
                else if (valueString == QLatin1String("DEFLATE64"))
                    m_MainMethod = NFileHeader::NCompressionMethod::kDeflated64;
                else if (valueString == QLatin1String("BZIP2"))
                    m_MainMethod = NFileHeader::NCompressionMethod::kBZip2;
                else
                    return E_INVALIDARG;
            } else if (prop.type() == QVariant::UInt) {
                switch (prop.toUInt()) {
                case NFileHeader::NCompressionMethod::kStored:
                case NFileHeader::NCompressionMethod::kDeflated:
                case NFileHeader::NCompressionMethod::kDeflated64:
                case NFileHeader::NCompressionMethod::kBZip2:
                    m_MainMethod = (quint8)prop.toUInt();
                    break;
                default:
                    return E_INVALIDARG;
                }
            } else
                return E_INVALIDARG;
        } else if (name == "Encryption") {
            if (prop.type() == QVariant::String) {
                QString valueString = prop.toString().toUpper();
                if (valueString.startsWith(QLatin1String("AES"))) {
                    valueString = valueString.mid(3);
                    if (valueString == QLatin1String("128"))
                        m_AesKeyMode = 1;
                    else if (valueString == QLatin1String("192"))
                        m_AesKeyMode = 2;
                    else if (valueString == QLatin1String("256") || valueString.isEmpty())
                        m_AesKeyMode = 3;
                    else
                        return E_INVALIDARG;
                    m_IsAesMode = true;
                } else if (valueString == QLatin1String("ZIPCRYPTO"))
                    m_IsAesMode = false;
                else
                    return E_INVALIDARG;
            } else
                return E_INVALIDARG;
        } else if (name == "DictSize") {
            quint32 dicSize = kBZip2DicSizeX5;
            bool ok;
            dicSize = prop.toUInt(&ok);
            if (!ok)
                return E_INVALIDARG;
            m_DicSize = dicSize;
        } else if (name == "NumPasses") {
            quint32 num = kDeflateNumPassesX9;
            bool ok;
            num = prop.toUInt(&ok);
            if (!ok)
                return E_INVALIDARG;
            m_NumPasses = num;
        } else if (name == "FastBytes") {
            quint32 num = kNumFastBytesX9;
            bool ok;
            num = prop.toUInt(&ok);
            if (!ok)
                return E_INVALIDARG;
            m_NumFastBytes = num;
        } else if (name == "MatchCycles") {
            quint32 num = 0xFFFFFFFF;
            bool ok;
            num = prop.toUInt(&ok);
            if (!ok)
                return E_INVALIDARG;
            m_NumMatchFinderCycles = num;
            m_NumMatchFinderCyclesDefined = true;
        } else if (name == "NumThreads") {
#ifdef THREADED
            RINOK(ParseMtProp(name.mid(2), prop, numProcessors, _numThreads));
#endif
        } else if (name == "Algorithm") {
            quint32 num = kDeflateAlgoX5;
            bool ok;
            num = prop.toUInt(&ok);
            if (!ok)
                return E_INVALIDARG;
            m_Algo = num;
        } else
            return E_INVALIDARG;
    }
    return S_OK;
}

}
}
