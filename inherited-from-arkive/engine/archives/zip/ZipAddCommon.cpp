// Addkernel.cpp

#include "interfaces/ICoder.h"
#include "interfaces/IPassword.h"
#include "kernel/HandlerRegistry.h"
#include "kernel/7zCrc.h"
#include "kernel/InStreamWithCRC.h"

#include "ZipAddCommon.h"
#include "ZipHeader.h"

#include <QtCore/QVariant>

namespace NArchive
{
namespace NZip {

static const CMethodId kMethodId_ZipBase   = 0x040100;
static const CMethodId kMethodId_BZip2     = 0x040202;

CAddCommon::CAddCommon(const CCompressionMethodMode &options):
        _options(options),
        _copyCoderSpec(NULL),
        _cryptoStreamSpec(0)
{}

static HRESULT GetStreamCRC(ISequentialInStream *inStream, quint32 &resultCRC)
{
    quint32 crc = CRC_INIT_VAL;
    const quint32 kBufferSize = (1 << 14);
    quint8 buffer[kBufferSize];
    for (;;) {
        quint32 realProcessedSize;
        RINOK(inStream->Read(buffer, kBufferSize, &realProcessedSize));
        if (realProcessedSize == 0) {
            resultCRC = CRC_GET_DIGEST(crc);
            return S_OK;
        }
        crc = CrcUpdate(crc, buffer, realProcessedSize);
    }
}

HRESULT CAddCommon::Compress(
    ISequentialInStream *_inStream, IOutStream *outStream,
    CCompressingResult &operationResult)
{
    CSequentialInStreamWithCRC *inSecCrcStreamSpec = 0;
    CInStreamWithCRC *inCrcStreamSpec = 0;
    RefPtr<ISequentialInStream> inCrcStream;
    RefPtr<ISequentialInStream> inStream(_inStream);
    {
        RefPtr<IInStream> inStream2;
        // we don't support stdin, since stream from stdin can require 64-bit size header
        RINOK(inStream.QueryInterface(&inStream2));
        if (inStream2) {
            inCrcStreamSpec = new CInStreamWithCRC;
            inCrcStream = inCrcStreamSpec;
            inCrcStreamSpec->SetStream(inStream2);
            inCrcStreamSpec->Init();
        } else {
            inSecCrcStreamSpec = new CSequentialInStreamWithCRC;
            inCrcStream = inSecCrcStreamSpec;
            inSecCrcStreamSpec->SetStream(inStream);
            inSecCrcStreamSpec->Init();
        }
    }

    int numTestMethods = _options.MethodSequence.size();
    if (numTestMethods > 1 || _options.PasswordIsDefined) {
        if (inCrcStreamSpec == 0) {
            if (_options.PasswordIsDefined)
                return E_NOTIMPL;
            numTestMethods = 1;
        }
    }
    quint8 method = 0;
    COutStreamReleaser outStreamReleaser;
    for (int i = 0; i < numTestMethods; i++) {
        if (inCrcStreamSpec != 0)
            RINOK(inCrcStreamSpec->Seek(0, STREAM_SEEK_SET, NULL));
        RINOK(outStream->Seek(0, STREAM_SEEK_SET, NULL));
        if (_options.PasswordIsDefined) {
            if (!_cryptoStream) {
                _cryptoStreamSpec = new CFilterCoder;
                _cryptoStream = _cryptoStreamSpec;
            }
            if (_options.IsAesMode) {
                 _aesFilter = _filterAesSpec = new NCrypto::NWzAES::CEncoder;
                _cryptoStreamSpec->setFilter(_aesFilter);
                _filterAesSpec->SetKeyMode(_options.AesKeyMode);
                RINOK(_filterAesSpec->CryptoSetPassword(
                          (const quint8 *)(const char *)_options.Password, _options.Password.length()));
                RINOK(_filterAesSpec->WriteHeader(outStream));
            } else {
                _zipCryptoFilter = _filterSpec = new NCrypto::NZip::CEncoder;
                _cryptoStreamSpec->setFilter(_zipCryptoFilter);
                RINOK(_filterSpec->CryptoSetPassword(
                          (const quint8 *)(const char *)_options.Password, _options.Password.length()));
                quint32 crc = 0;
                RINOK(GetStreamCRC(inStream, crc));
                RINOK(inCrcStreamSpec->Seek(0, STREAM_SEEK_SET, NULL));
                RINOK(_filterSpec->CryptoSetCRC(crc));
                RINOK(_filterSpec->WriteHeader(outStream));
            }
            RINOK(_cryptoStreamSpec->SetOutStream(outStream));
            outStreamReleaser.FilterCoder = _cryptoStreamSpec;
        }

        method = _options.MethodSequence[i];
        switch (method) {
        case NFileHeader::NCompressionMethod::kStored: {
            if (_copyCoderSpec == NULL) {
                _copyCoderSpec = new NCompress::CCopyCoder;
                _copyCoder = _copyCoderSpec;
            }
            RefPtr<ISequentialOutStream> outStreamNew;
            if (_options.PasswordIsDefined)
                outStreamNew = _cryptoStream;
            else
                outStreamNew = outStream;
            RINOK(_copyCoder->Code(inCrcStream, outStreamNew, NULL, NULL));
            operationResult.ExtractVersion = NFileHeader::NCompressionMethod::kStoreExtractVersion;
            break;
        }
        default: {
            if (!_compressEncoder) {
                CMethodId methodId;
                switch (method) {
                case NFileHeader::NCompressionMethod::kBZip2:
                    methodId = kMethodId_BZip2;
                    break;
                default:
                    methodId = kMethodId_ZipBase + method;
                    break;
                }
                QObject *enc = CreateCoderForId(methodId);
                if (!enc)
                    return E_NOTIMPL;
                _compressEncoder = qobject_cast<ICompressCoder *>(enc);
                if (!_compressEncoder) {
                    delete enc;
                    return E_NOTIMPL;
                }

                if (method == NFileHeader::NCompressionMethod::kDeflated ||
                        method == NFileHeader::NCompressionMethod::kDeflated64) {
                    QVariant properties[] = {
                        _options.Algo,
                        _options.NumPasses,
                        _options.NumFastBytes,
                        _options.NumMatchFinderCycles
                    };
                    PROPID propIDs[] = {
                        NCoderPropID::kAlgorithm,
                        NCoderPropID::kNumPasses,
                        NCoderPropID::kNumFastBytes,
                        NCoderPropID::kMatchFinderCycles
                    };
                    int numProps = sizeof(propIDs) / sizeof(propIDs[0]);
                    if (!_options.NumMatchFinderCyclesDefined)
                        numProps--;
                    RefPtr<ICompressSetCoderProperties> setCoderProperties;
                    _compressEncoder.QueryInterface(&setCoderProperties);
                    if (setCoderProperties) {
                        RINOK(setCoderProperties->SetCoderProperties(propIDs, properties, numProps));
                    }
                } else if (method == NFileHeader::NCompressionMethod::kBZip2) {
                    QVariant properties[] = {
                        _options.DicSize,
                        _options.NumPasses
#ifdef THREADED
                        , _options.NumThreads
#endif
                    };
                    PROPID propIDs[] = {
                        NCoderPropID::kDictionarySize,
                        NCoderPropID::kNumPasses
#ifdef THREADED
                        , NCoderPropID::kNumThreads
#endif
                    };
                    RefPtr<ICompressSetCoderProperties> setCoderProperties;
                    _compressEncoder.QueryInterface(&setCoderProperties);
                    if (setCoderProperties) {
                        RINOK(setCoderProperties->SetCoderProperties(propIDs, properties, sizeof(propIDs) / sizeof(propIDs[0])));
                    }
                }
            }
            RefPtr<ISequentialOutStream> outStreamNew;
            if (_options.PasswordIsDefined)
                outStreamNew = _cryptoStream;
            else
                outStreamNew = outStream;
            RINOK(_compressEncoder->Code(inCrcStream, outStreamNew, NULL, NULL));
            operationResult.ExtractVersion = NFileHeader::NCompressionMethod::kDeflateExtractVersion;
            break;
        }
        }

        RINOK(outStream->Seek(0, STREAM_SEEK_CUR, &operationResult.PackSize));

        if (inCrcStreamSpec != 0) {
            operationResult.CRC = inCrcStreamSpec->GetCRC();
            operationResult.UnpackSize = inCrcStreamSpec->GetSize();
        } else {
            operationResult.CRC = inSecCrcStreamSpec->GetCRC();
            operationResult.UnpackSize = inSecCrcStreamSpec->GetSize();
        }

        if (_options.PasswordIsDefined) {
            if (operationResult.PackSize < operationResult.UnpackSize +
                    (_options.IsAesMode ? _filterAesSpec->GetHeaderSize() : NCrypto::NZip::kHeaderSize))
                break;
        } else if (operationResult.PackSize < operationResult.UnpackSize)
            break;
    }
    if (_options.IsAesMode) {
        RINOK(_filterAesSpec->WriteFooter(outStream));
        RINOK(outStream->Seek(0, STREAM_SEEK_CUR, &operationResult.PackSize));
    }
    operationResult.Method = method;
    return outStream->SetSize(operationResult.PackSize);
}

}
}
