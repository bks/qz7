// CompressionMode.h

#ifndef __ZIP_COMPRESSIONMETHOD_H
#define __ZIP_COMPRESSIONMETHOD_H

#include <QtCore/QString>
#include <QtCore/QByteArray>


namespace NArchive
{
namespace NZip {

struct CCompressionMethodMode {
    QList<quint8> MethodSequence;
    // bool MaximizeRatio;
    quint32 Algo;
    quint32 NumPasses;
    quint32 NumFastBytes;
    bool NumMatchFinderCyclesDefined;
    quint32 NumMatchFinderCycles;
    quint32 DicSize;
#ifdef THREADED
    quint32 NumThreads;
#endif
    bool PasswordIsDefined;
    QByteArray Password;
    bool IsAesMode;
    quint8 AesKeyMode;

    CCompressionMethodMode():
            NumMatchFinderCyclesDefined(false),
            PasswordIsDefined(false),
            IsAesMode(false),
            AesKeyMode(3) {}
};

}
}

#endif
