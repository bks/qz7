// Zip/Handler.h

#ifndef __ZIP_HANDLER_H
#define __ZIP_HANDLER_H

#include "interfaces/ObjectBasics.h"
#include "interfaces/ICoder.h"
#include "interfaces/IArchive.h"

#include "kernel/DynamicBuffer.h"

#include "ZipIn.h"
#include "ZipCompressionMode.h"

#ifdef THREADED
#include <QtCore/QThread>
#include <QtCore/QMutex>
#include <QtCore/QSemaphore>
#endif

namespace NArchive {
namespace NZip {

class CHandler:
    public RefCountedQObject,
    public IInArchive,
    public IOutArchive,
    public ISetProperties
{
    Q_OBJECT
    Q_INTERFACES(IInArchive
                 IOutArchive
                 ISetProperties
                )

public:
    USING_RefCountedQObject

    INTERFACE_IInArchive(;)
    INTERFACE_IOutArchive(;)

    virtual HRESULT SetProperties(const QMap<QString, QVariant>& properties);

    CHandler();

private:
    QList<CItemEx *> m_Items;
    CInArchive m_Archive;
    bool m_ArchiveIsOpen;

    int m_Level;
    int m_MainMethod;
    quint32 m_DicSize;
    quint32 m_Algo;
    quint32 m_NumPasses;
    quint32 m_NumFastBytes;
    quint32 m_NumMatchFinderCycles;
    bool m_NumMatchFinderCyclesDefined;

    bool m_IsAesMode;
    quint8 m_AesKeyMode;

    quint32 _numThreads;

    void InitMethodProperties() {
        m_Level = -1;
        m_MainMethod = -1;
        m_Algo =
            m_DicSize =
                m_NumPasses =
                    m_NumFastBytes =
                        m_NumMatchFinderCycles = 0xFFFFFFFF;
        m_NumMatchFinderCyclesDefined = false;
        m_IsAesMode = false;
        m_AesKeyMode = 3; // aes-256
#ifdef THREADED
        _numThreads = NWindows::NSystem::GetNumberOfProcessors();
#else
        _numThreads = 1;
#endif
    }
};

}
}

#endif
