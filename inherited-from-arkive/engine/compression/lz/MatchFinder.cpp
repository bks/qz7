/* MatchFinder.c */

#include <string.h>

#include "MatchFinder.h"
#include "LzHash.h"

#include <QtCore/QIODevice>

#define kEmptyHashValue 0
#define kMaxValForNormalize ((quint32)0xFFFFFFFF)
#define kNormalizeStepMin (1 << 10) /* it must be power of 2 */
#define kNormalizeMask (~(kNormalizeStepMin - 1))
#define kMaxHistorySize ((quint32)3 << 30)

#define kStartMaxLen 3

// the LZ hash code table is just the Crc32IeeeLe CRC constant table
static quint32 LzHashTable[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
    0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
    0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
    0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
    0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
    0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
    0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
    0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
    0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
    0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
    0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
    0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
    0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
    0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
    0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
    0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
    0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
    0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
    0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
    0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
    0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
    0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

void LzInWindow_Free(MatchFinder *p)
{
  if (!p->directInput)
  {
    delete[] p->bufferBase;
    p->bufferBase = 0;
  }
}

/* keepSizeBefore + keepSizeAfter + keepSizeReserv must be < 4G) */

int LzInWindow_Create(MatchFinder *p, quint32 keepSizeReserv)
{
  quint32 blockSize = p->keepSizeBefore + p->keepSizeAfter + keepSizeReserv;
  if (p->directInput)
  {
    p->blockSize = blockSize;
    return 1;
  }
  if (p->bufferBase == 0 || p->blockSize != blockSize)
  {
    LzInWindow_Free(p);
    p->blockSize = blockSize;
    p->bufferBase = new quint8[blockSize];
  }
  return (p->bufferBase != 0);
}

quint8 *MatchFinder_GetPointerToCurrentPos(MatchFinder *p) { return p->buffer; }
quint8 MatchFinder_GetIndexByte(MatchFinder *p, qint32 index) { return p->buffer[index]; }

quint32 MatchFinder_GetNumAvailableBytes(MatchFinder *p) { return p->streamPos - p->pos; }

void MatchFinder_ReduceOffsets(MatchFinder *p, quint32 subValue)
{
  p->posLimit -= subValue;
  p->pos -= subValue;
  p->streamPos -= subValue;
}

void MatchFinder_ReadBlock(MatchFinder *p)
{
  if (p->streamEndWasReached || !p->result)
    return;
  for (;;)
  {
    quint8 *dest = p->buffer + (p->streamPos - p->pos);
    quint32 size = (quint32)(p->bufferBase + p->blockSize - dest);
    if (size == 0)
      return;

    qint32 numReadBytes = p->backingDevice->read((char *)dest, size);

    if (numReadBytes < 0) {
        p->result = false;
        return;
    }

    if (numReadBytes == 0 && !p->backingDevice->waitForReadyRead(-1))
    {
      p->streamEndWasReached = 1;
      return;
    }
    p->streamPos += numReadBytes;
    if (p->streamPos - p->pos > p->keepSizeAfter)
      return;
  }
}

void MatchFinder_MoveBlock(MatchFinder *p)
{
  memmove(p->bufferBase, 
    p->buffer - p->keepSizeBefore, 
    p->streamPos - p->pos + p->keepSizeBefore);
  p->buffer = p->bufferBase + p->keepSizeBefore;
}

int MatchFinder_NeedMove(MatchFinder *p)
{
  /* if (p->streamEndWasReached) return 0; */
  return ((size_t)(p->bufferBase + p->blockSize - p->buffer) <= p->keepSizeAfter);
}

void MatchFinder_ReadIfRequired(MatchFinder *p)
{
  if (p->streamEndWasReached) 
    return;
  if (p->keepSizeAfter >= p->streamPos - p->pos)
    MatchFinder_ReadBlock(p);
}

void MatchFinder_CheckAndMoveAndRead(MatchFinder *p)
{
  if (MatchFinder_NeedMove(p))
    MatchFinder_MoveBlock(p);
  MatchFinder_ReadBlock(p);
}

void MatchFinder_SetDefaultSettings(MatchFinder *p)
{
  p->cutValue = 32;
  p->btMode = 1;
  p->numHashBytes = 4;
  /* p->skipModeBits = 0; */
  p->directInput = 0;
  p->bigHash = 0;
}

void MatchFinder_Construct(MatchFinder *p)
{
  p->bufferBase = 0;
  p->directInput = 0;
  p->hash = 0;
  MatchFinder_SetDefaultSettings(p);
}

void MatchFinder_FreeThisClassMemory(MatchFinder *p)
{
  delete[] p->hash;
  p->hash = 0;
}

void MatchFinder_Free(MatchFinder *p)
{
  MatchFinder_FreeThisClassMemory(p);
  LzInWindow_Free(p);
}

int MatchFinder_Create(MatchFinder *p, quint32 historySize, 
    quint32 keepAddBufferBefore, quint32 matchMaxLen, quint32 keepAddBufferAfter)
{
  quint32 sizeReserv;
  if (historySize > kMaxHistorySize)
  {
    MatchFinder_Free(p);
    return 0;
  }
  sizeReserv = historySize >> 1;
  if (historySize > ((quint32)2 << 30))
    sizeReserv = historySize >> 2;
  sizeReserv += (keepAddBufferBefore + matchMaxLen + keepAddBufferAfter) / 2 + (1 << 19);

  p->keepSizeBefore = historySize + keepAddBufferBefore + 1; 
  p->keepSizeAfter = matchMaxLen + keepAddBufferAfter;
  /* we need one additional byte, since we use MoveBlock after pos++ and before dictionary using */
  if (LzInWindow_Create(p, sizeReserv))
  {
    quint32 newCyclicBufferSize = (historySize /* >> p->skipModeBits */) + 1;
    quint32 hs;
    p->matchMaxLen = matchMaxLen;
    {
      p->fixedHashSize = 0;
      if (p->numHashBytes == 2)
        hs = (1 << 16) - 1;
      else
      {
        hs = historySize - 1;
        hs |= (hs >> 1);
        hs |= (hs >> 2);
        hs |= (hs >> 4);
        hs |= (hs >> 8);
        hs >>= 1;
        /* hs >>= p->skipModeBits; */
        hs |= 0xFFFF; /* don't change it! It's required for Deflate */
        if (hs > (1 << 24))
        {
          if (p->numHashBytes == 3)
            hs = (1 << 24) - 1;
          else
            hs >>= 1;
        }
      }
      p->hashMask = hs;
      hs++;
      if (p->numHashBytes > 2) p->fixedHashSize += kHash2Size;
      if (p->numHashBytes > 3) p->fixedHashSize += kHash3Size;
      if (p->numHashBytes > 4) p->fixedHashSize += kHash4Size;
      hs += p->fixedHashSize;
    }

    {
      quint32 prevSize = p->hashSizeSum + p->numSons;
      quint32 newSize;
      p->historySize = historySize;
      p->hashSizeSum = hs;
      p->cyclicBufferSize = newCyclicBufferSize;
      p->numSons = (p->btMode ? newCyclicBufferSize * 2 : newCyclicBufferSize);
      newSize = p->hashSizeSum + p->numSons;
      if (p->hash != 0 && prevSize == newSize)
        return 1;
      MatchFinder_FreeThisClassMemory(p);
      p->hash = new LzRef[newSize];
      if (p->hash != 0)
      {
        p->son = p->hash + p->hashSizeSum;
        return 1;
      }
    }
  }
  MatchFinder_Free(p);
  return 0;
}

void MatchFinder_SetLimits(MatchFinder *p)
{
  quint32 limit = kMaxValForNormalize - p->pos;
  quint32 limit2 = p->cyclicBufferSize - p->cyclicBufferPos;
  if (limit2 < limit) 
    limit = limit2;
  limit2 = p->streamPos - p->pos;
  if (limit2 <= p->keepSizeAfter)
  {
    if (limit2 > 0)
      limit2 = 1;
  }
  else
    limit2 -= p->keepSizeAfter;
  if (limit2 < limit) 
    limit = limit2;
  {
    quint32 lenLimit = p->streamPos - p->pos;
    if (lenLimit > p->matchMaxLen)
      lenLimit = p->matchMaxLen;
    p->lenLimit = lenLimit;
  }
  p->posLimit = p->pos + limit;
}

void MatchFinder_Init(MatchFinder *p)
{
  quint32 i;
  for(i = 0; i < p->hashSizeSum; i++)
    p->hash[i] = kEmptyHashValue;
  p->cyclicBufferPos = 0;
  p->buffer = p->bufferBase;
  p->pos = p->streamPos = p->cyclicBufferSize;
  p->result = true;
  p->streamEndWasReached = 0;
  MatchFinder_ReadBlock(p);
  MatchFinder_SetLimits(p);
}

quint32 MatchFinder_GetSubValue(MatchFinder *p) 
{ 
  return (p->pos - p->historySize - 1) & kNormalizeMask; 
}

void MatchFinder_Normalize3(quint32 subValue, LzRef *items, quint32 numItems)
{
  quint32 i;
  for (i = 0; i < numItems; i++)
  {
    quint32 value = items[i];
    if (value <= subValue)
      value = kEmptyHashValue;
    else
      value -= subValue;
    items[i] = value;
  }
}

void MatchFinder_Normalize(MatchFinder *p)
{
  quint32 subValue = MatchFinder_GetSubValue(p);
  MatchFinder_Normalize3(subValue, p->hash, p->hashSizeSum + p->numSons);
  MatchFinder_ReduceOffsets(p, subValue);
}

void MatchFinder_CheckLimits(MatchFinder *p)
{
  if (p->pos == kMaxValForNormalize)
    MatchFinder_Normalize(p);
  if (!p->streamEndWasReached && p->keepSizeAfter == p->streamPos - p->pos)
    MatchFinder_CheckAndMoveAndRead(p);
  if (p->cyclicBufferPos == p->cyclicBufferSize)
    p->cyclicBufferPos = 0;
  MatchFinder_SetLimits(p);
}

quint32 * Hc_GetMatchesSpec(quint32 lenLimit, quint32 curMatch, quint32 pos, const quint8 *cur, LzRef *son, 
    quint32 _cyclicBufferPos, quint32 _cyclicBufferSize, quint32 cutValue, 
    quint32 *distances, quint32 maxLen)
{
  son[_cyclicBufferPos] = curMatch;
  for (;;)
  {
    quint32 delta = pos - curMatch;
    if (cutValue-- == 0 || delta >= _cyclicBufferSize)
      return distances;
    {
      const quint8 *pb = cur - delta;
      curMatch = son[_cyclicBufferPos - delta + ((delta > _cyclicBufferPos) ? _cyclicBufferSize : 0)];
      if (pb[maxLen] == cur[maxLen] && *pb == *cur)
      {
        quint32 len = 0;
        while(++len != lenLimit)
          if (pb[len] != cur[len])
            break;
        if (maxLen < len)
        {
          *distances++ = maxLen = len;
          *distances++ = delta - 1;
          if (len == lenLimit)
            return distances;
        }
      }
    }
  }
}

quint32 * GetMatchesSpec1(quint32 lenLimit, quint32 curMatch, quint32 pos, const quint8 *cur, LzRef *son, 
    quint32 _cyclicBufferPos, quint32 _cyclicBufferSize, quint32 cutValue, 
    quint32 *distances, quint32 maxLen)
{
  LzRef *ptr0 = son + (_cyclicBufferPos << 1) + 1;
  LzRef *ptr1 = son + (_cyclicBufferPos << 1);
  quint32 len0 = 0, len1 = 0;
  for (;;)
  {
    quint32 delta = pos - curMatch;
    if (cutValue-- == 0 || delta >= _cyclicBufferSize)
    {
      *ptr0 = *ptr1 = kEmptyHashValue;
      return distances;
    }
    {
      LzRef *pair = son + ((_cyclicBufferPos - delta + ((delta > _cyclicBufferPos) ? _cyclicBufferSize : 0)) << 1);
      const quint8 *pb = cur - delta;
      quint32 len = (len0 < len1 ? len0 : len1);
      if (pb[len] == cur[len])
      {
        if (++len != lenLimit && pb[len] == cur[len])
          while(++len != lenLimit)
            if (pb[len] != cur[len])
              break;
        if (maxLen < len)
        {
          *distances++ = maxLen = len;
          *distances++ = delta - 1;
          if (len == lenLimit)
          {
            *ptr1 = pair[0];
            *ptr0 = pair[1];
            return distances;
          }
        }
      }
      if (pb[len] < cur[len])
      {
        *ptr1 = curMatch;
        ptr1 = pair + 1;
        curMatch = *ptr1;
        len1 = len;
      }
      else
      {
        *ptr0 = curMatch;
        ptr0 = pair;
        curMatch = *ptr0;
        len0 = len;
      }
    }
  }
}

void SkipMatchesSpec(quint32 lenLimit, quint32 curMatch, quint32 pos, const quint8 *cur, LzRef *son, 
    quint32 _cyclicBufferPos, quint32 _cyclicBufferSize, quint32 cutValue)
{
  LzRef *ptr0 = son + (_cyclicBufferPos << 1) + 1;
  LzRef *ptr1 = son + (_cyclicBufferPos << 1);
  quint32 len0 = 0, len1 = 0;
  for (;;)
  {
    quint32 delta = pos - curMatch;
    if (cutValue-- == 0 || delta >= _cyclicBufferSize)
    {
      *ptr0 = *ptr1 = kEmptyHashValue;
      return;
    }
    {
      LzRef *pair = son + ((_cyclicBufferPos - delta + ((delta > _cyclicBufferPos) ? _cyclicBufferSize : 0)) << 1);
      const quint8 *pb = cur - delta;
      quint32 len = (len0 < len1 ? len0 : len1);
      if (pb[len] == cur[len])
      {
        while(++len != lenLimit)
          if (pb[len] != cur[len])
            break;
        {
          if (len == lenLimit)
          {
            *ptr1 = pair[0];
            *ptr0 = pair[1];
            return;
          }
        }
      }
      if (pb[len] < cur[len])
      {
        *ptr1 = curMatch;
        ptr1 = pair + 1;
        curMatch = *ptr1;
        len1 = len;
      }
      else
      {
        *ptr0 = curMatch;
        ptr0 = pair;
        curMatch = *ptr0;
        len0 = len;
      }
    }
  }
}

#define MOVE_POS \
  ++p->cyclicBufferPos; \
  p->buffer++; \
  if (++p->pos == p->posLimit) MatchFinder_CheckLimits(p);

#define MOVE_POS_RET MOVE_POS return offset;

void MatchFinder_MovePos(MatchFinder *p) { MOVE_POS; }

#define GET_MATCHES_HEADER2(minLen, ret_op) \
  quint32 lenLimit; quint32 hashValue; const quint8 *cur; quint32 curMatch; \
  lenLimit = p->lenLimit; { if (lenLimit < minLen) { MatchFinder_MovePos(p); ret_op; }} \
  cur = p->buffer;

#define GET_MATCHES_HEADER(minLen) GET_MATCHES_HEADER2(minLen, return 0)
#define SKIP_HEADER(minLen)        GET_MATCHES_HEADER2(minLen, continue)

#define MF_PARAMS(p) p->pos, p->buffer, p->son, p->cyclicBufferPos, p->cyclicBufferSize, p->cutValue

#define GET_MATCHES_FOOTER(offset, maxLen) \
  offset = (quint32)(GetMatchesSpec1(lenLimit, curMatch, MF_PARAMS(p), \
  distances + offset, maxLen) - distances); MOVE_POS_RET;

#define SKIP_FOOTER \
  SkipMatchesSpec(lenLimit, curMatch, MF_PARAMS(p)); MOVE_POS;

quint32 Bt2_MatchFinder_GetMatches(MatchFinder *p, quint32 *distances)
{
  quint32 offset;
  GET_MATCHES_HEADER(2)
  HASH2_CALC;
  curMatch = p->hash[hashValue];
  p->hash[hashValue] = p->pos;
  offset = 0;
  GET_MATCHES_FOOTER(offset, 1)
}

quint32 Bt3Zip_MatchFinder_GetMatches(MatchFinder *p, quint32 *distances)
{
  quint32 offset;
  GET_MATCHES_HEADER(3)
  HASH_ZIP_CALC;
  curMatch = p->hash[hashValue];
  p->hash[hashValue] = p->pos;
  offset = 0;
  GET_MATCHES_FOOTER(offset, 2)
}

quint32 Bt3_MatchFinder_GetMatches(MatchFinder *p, quint32 *distances)
{
  quint32 hash2Value, delta2, maxLen, offset;
  GET_MATCHES_HEADER(3)

  HASH3_CALC;

  delta2 = p->pos - p->hash[hash2Value];
  curMatch = p->hash[kFix3HashSize + hashValue];
  
  p->hash[hash2Value] = 
  p->hash[kFix3HashSize + hashValue] = p->pos;


  maxLen = 2;
  offset = 0;
  if (delta2 < p->cyclicBufferSize && *(cur - delta2) == *cur)
  {
    for (; maxLen != lenLimit; maxLen++)
      if (cur[(ptrdiff_t)maxLen - delta2] != cur[maxLen])
        break;
    distances[0] = maxLen;
    distances[1] = delta2 - 1;
    offset = 2;
    if (maxLen == lenLimit)
    {
      SkipMatchesSpec(lenLimit, curMatch, MF_PARAMS(p));
      MOVE_POS_RET; 
    }
  }
  GET_MATCHES_FOOTER(offset, maxLen)
}

quint32 Bt4_MatchFinder_GetMatches(MatchFinder *p, quint32 *distances)
{
  quint32 hash2Value, hash3Value, delta2, delta3, maxLen, offset;
  GET_MATCHES_HEADER(4)

  HASH4_CALC;

  delta2 = p->pos - p->hash[                hash2Value];
  delta3 = p->pos - p->hash[kFix3HashSize + hash3Value];
  curMatch = p->hash[kFix4HashSize + hashValue];
  
  p->hash[                hash2Value] =
  p->hash[kFix3HashSize + hash3Value] =
  p->hash[kFix4HashSize + hashValue] = p->pos;

  maxLen = 1;
  offset = 0;
  if (delta2 < p->cyclicBufferSize && *(cur - delta2) == *cur)
  {
    distances[0] = maxLen = 2;
    distances[1] = delta2 - 1;
    offset = 2;
  }
  if (delta2 != delta3 && delta3 < p->cyclicBufferSize && *(cur - delta3) == *cur)
  {
    maxLen = 3;
    distances[offset + 1] = delta3 - 1;
    offset += 2;
    delta2 = delta3;
  }
  if (offset != 0)
  {
    for (; maxLen != lenLimit; maxLen++)
      if (cur[(ptrdiff_t)maxLen - delta2] != cur[maxLen])
        break;
    distances[offset - 2] = maxLen;
    if (maxLen == lenLimit)
    {
      SkipMatchesSpec(lenLimit, curMatch, MF_PARAMS(p));
      MOVE_POS_RET; 
    }
  }
  if (maxLen < 3)
    maxLen = 3;
  GET_MATCHES_FOOTER(offset, maxLen)
}

quint32 Hc4_MatchFinder_GetMatches(MatchFinder *p, quint32 *distances)
{
  quint32 hash2Value, hash3Value, delta2, delta3, maxLen, offset;
  GET_MATCHES_HEADER(4)

  HASH4_CALC;

  delta2 = p->pos - p->hash[                hash2Value];
  delta3 = p->pos - p->hash[kFix3HashSize + hash3Value];
  curMatch = p->hash[kFix4HashSize + hashValue];

  p->hash[                hash2Value] =
  p->hash[kFix3HashSize + hash3Value] =
  p->hash[kFix4HashSize + hashValue] = p->pos;

  maxLen = 1;
  offset = 0;
  if (delta2 < p->cyclicBufferSize && *(cur - delta2) == *cur)
  {
    distances[0] = maxLen = 2;
    distances[1] = delta2 - 1;
    offset = 2;
  }
  if (delta2 != delta3 && delta3 < p->cyclicBufferSize && *(cur - delta3) == *cur)
  {
    maxLen = 3;
    distances[offset + 1] = delta3 - 1;
    offset += 2;
    delta2 = delta3;
  }
  if (offset != 0)
  {
    for (; maxLen != lenLimit; maxLen++)
      if (cur[(ptrdiff_t)maxLen - delta2] != cur[maxLen])
        break;
    distances[offset - 2] = maxLen;
    if (maxLen == lenLimit)
    {
      p->son[p->cyclicBufferPos] = curMatch;
      MOVE_POS_RET; 
    }
  }
  if (maxLen < 3)
    maxLen = 3;
  offset = (quint32)(Hc_GetMatchesSpec(lenLimit, curMatch, MF_PARAMS(p),
    distances + offset, maxLen) - (distances));
  MOVE_POS_RET
}

quint32 Hc3Zip_MatchFinder_GetMatches(MatchFinder *p, quint32 *distances)
{
  quint32 offset;
  GET_MATCHES_HEADER(3)
  HASH_ZIP_CALC;
  curMatch = p->hash[hashValue];
  p->hash[hashValue] = p->pos;
  offset = (quint32)(Hc_GetMatchesSpec(lenLimit, curMatch, MF_PARAMS(p),
    distances, 2) - (distances));
  MOVE_POS_RET
}

void Bt2_MatchFinder_Skip(MatchFinder *p, quint32 num)
{
  do
  {
    SKIP_HEADER(2) 
    HASH2_CALC;
    curMatch = p->hash[hashValue];
    p->hash[hashValue] = p->pos;
    SKIP_FOOTER
  }
  while (--num != 0);
}

void Bt3Zip_MatchFinder_Skip(MatchFinder *p, quint32 num)
{
  do
  {
    SKIP_HEADER(3)
    HASH_ZIP_CALC;
    curMatch = p->hash[hashValue];
    p->hash[hashValue] = p->pos;
    SKIP_FOOTER
  }
  while (--num != 0);
}

void Bt3_MatchFinder_Skip(MatchFinder *p, quint32 num)
{
  do
  {
    quint32 hash2Value;
    SKIP_HEADER(3)
    HASH3_CALC;
    curMatch = p->hash[kFix3HashSize + hashValue];
    p->hash[hash2Value] =
    p->hash[kFix3HashSize + hashValue] = p->pos;
    SKIP_FOOTER
  }
  while (--num != 0);
}

void Bt4_MatchFinder_Skip(MatchFinder *p, quint32 num)
{
  do
  {
    quint32 hash2Value, hash3Value;
    SKIP_HEADER(4) 
    HASH4_CALC;
    curMatch = p->hash[kFix4HashSize + hashValue];
    p->hash[                hash2Value] =
    p->hash[kFix3HashSize + hash3Value] = p->pos;
    p->hash[kFix4HashSize + hashValue] = p->pos;
    SKIP_FOOTER
  }
  while (--num != 0);
}

void Hc4_MatchFinder_Skip(MatchFinder *p, quint32 num)
{
  do
  {
    quint32 hash2Value, hash3Value;
    SKIP_HEADER(4)
    HASH4_CALC;
    curMatch = p->hash[kFix4HashSize + hashValue];
    p->hash[                hash2Value] =
    p->hash[kFix3HashSize + hash3Value] =
    p->hash[kFix4HashSize + hashValue] = p->pos;
    p->son[p->cyclicBufferPos] = curMatch;
    MOVE_POS
  }
  while (--num != 0);
}

void Hc3Zip_MatchFinder_Skip(MatchFinder *p, quint32 num)
{
  do
  {
    SKIP_HEADER(3)
    HASH_ZIP_CALC;
    curMatch = p->hash[hashValue];
    p->hash[hashValue] = p->pos;
    p->son[p->cyclicBufferPos] = curMatch;
    MOVE_POS
  }
  while (--num != 0);
}

void MatchFinder_CreateVTable(MatchFinder *p, IMatchFinder *vTable)
{
  vTable->Init = (Mf_Init_Func)MatchFinder_Init;
  vTable->GetIndexByte = (Mf_GetIndexByte_Func)MatchFinder_GetIndexByte;
  vTable->GetNumAvailableBytes = (Mf_GetNumAvailableBytes_Func)MatchFinder_GetNumAvailableBytes;
  vTable->GetPointerToCurrentPos = (Mf_GetPointerToCurrentPos_Func)MatchFinder_GetPointerToCurrentPos;
  if (!p->btMode)
  {
    vTable->GetMatches = (Mf_GetMatches_Func)Hc4_MatchFinder_GetMatches;
    vTable->Skip = (Mf_Skip_Func)Hc4_MatchFinder_Skip;
  }
  else if (p->numHashBytes == 2)
  {
    vTable->GetMatches = (Mf_GetMatches_Func)Bt2_MatchFinder_GetMatches;
    vTable->Skip = (Mf_Skip_Func)Bt2_MatchFinder_Skip;
  }
  else if (p->numHashBytes == 3)
  {
    vTable->GetMatches = (Mf_GetMatches_Func)Bt3_MatchFinder_GetMatches;
    vTable->Skip = (Mf_Skip_Func)Bt3_MatchFinder_Skip;
  }
  else
  {
    vTable->GetMatches = (Mf_GetMatches_Func)Bt4_MatchFinder_GetMatches;
    vTable->Skip = (Mf_Skip_Func)Bt4_MatchFinder_Skip;
  }
}
