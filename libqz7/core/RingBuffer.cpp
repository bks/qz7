#include "RingBuffer.h"

#include "Error.h"
#include "Stream.h"

#include <string.h>

namespace qz7 {
    
void RingBuffer::flush()
{
    if (!mStream->write(mBuffer, mPos))
        throw WriteError(mStream);
    
    // if we're flushing in the middle, we need to rearrange the buffer
    if (mPos != mSize) {
        quint8 temp[mSize];
        
        ::memcpy(temp, &mBuffer[mPos], mSize - mPos);
        ::memcpy(&temp[mSize - mPos], mBuffer, mPos);
        ::memcpy(mBuffer, temp, mSize);
    }
    mPos = 0;
}

}

