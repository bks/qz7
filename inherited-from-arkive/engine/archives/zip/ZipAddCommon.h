// Zip/Addkernel.h

#ifndef __ZIP_ADDCOMMON_H
#define __ZIP_ADDCOMMON_H

#include "interfaces/ICoder.h"
#include "interfaces/IProgress.h"
#include "kernel/CopyCoder.h"
#include "kernel/FilterCoder.h"
#include "../../crypto/Zip/ZipCipher.h"
#include "../../crypto/WzAES/WzAES.h"

#include "ZipCompressionMode.h"

namespace NArchive
{
namespace NZip {

struct CCompressingResult {
    quint64 UnpackSize;
    quint64 PackSize;
    quint32 CRC;
    quint16 Method;
    quint8 ExtractVersion;
};

class CAddCommon
{
    CCompressionMethodMode _options;
    NCompress::CCopyCoder *_copyCoderSpec;
    RefPtr<ICompressCoder> _copyCoder;

    RefPtr<ICompressCoder> _compressEncoder;

    CFilterCoder *_cryptoStreamSpec;
    RefPtr<ISequentialOutStream> _cryptoStream;

    NCrypto::NZip::CEncoder *_filterSpec;
    NCrypto::NWzAES::CEncoder *_filterAesSpec;

    RefPtr<ICompressFilter> _zipCryptoFilter;
    RefPtr<ICompressFilter> _aesFilter;


public:
    CAddCommon(const CCompressionMethodMode &options);
    HRESULT Compress(
        ISequentialInStream *inStream, IOutStream *outStream,
        CCompressingResult &operationResult);
};

}
}

#endif
