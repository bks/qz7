#include "qz7/StreamTools.h"

namespace qz7 {
namespace tools {

bool copyData(ReadStream& src, WriteStream& dst)
{
    const int BufferSize = 64 * 1024;
    quint8 buffer[BufferSize];

    while (!src.atEnd()) {
        int r = src.readSome(buffer, 0, BufferSize);
        
        if (r < 0)
            return false;
            
        if (!dst.write(buffer, r))
            return false;
    }
    return true;
}

}
}

