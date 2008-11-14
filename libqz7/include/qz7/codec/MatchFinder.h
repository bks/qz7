#ifndef MATCHFINDER_H
#define MATCHFINDER_H

#include <QtCore/QtGlobal>

typedef quint32 LzRef;

class QIODevice;

struct MatchFinder
{
  quint8 *buffer;
  quint32 pos;
  quint32 posLimit;
  quint32 streamPos;
  quint32 lenLimit;

  quint32 cyclicBufferPos;
  quint32 cyclicBufferSize; /* it must be = (historySize + 1) */

  quint32 matchMaxLen;
  LzRef *hash;
  LzRef *son;
  quint32 hashMask;
  quint32 cutValue;

  quint8 *bufferBase;
  int streamEndWasReached;

  quint32 blockSize;
  quint32 keepSizeBefore;
  quint32 keepSizeAfter;

  quint32 numHashBytes;
  int directInput;
  int btMode;
  /* int skipModeBits; */
  int bigHash;
  quint32 historySize;
  quint32 fixedHashSize;
  quint32 hashSizeSum;
  quint32 numSons;
  QIODevice *backingDevice;

  bool result;
};

#define Inline_MatchFinder_GetPointerToCurrentPos(p) ((p)->buffer)
#define Inline_MatchFinder_GetIndexByte(p, index) ((p)->buffer[(qint32)(index)])

#define Inline_MatchFinder_GetNumAvailableBytes(p) ((p)->streamPos - (p)->pos)

int MatchFinder_NeedMove(MatchFinder *p);
quint8 *MatchFinder_GetPointerToCurrentPos(MatchFinder *p);
void MatchFinder_MoveBlock(MatchFinder *p);
void MatchFinder_ReadIfRequired(MatchFinder *p);

void MatchFinder_Construct(MatchFinder *p);

/* Conditions:
     historySize <= 3 GB
     keepAddBufferBefore + matchMaxLen + keepAddBufferAfter < 511MB
*/
int MatchFinder_Create(MatchFinder *p, quint32 historySize, 
    quint32 keepAddBufferBefore, quint32 matchMaxLen, quint32 keepAddBufferAfter);
void MatchFinder_Free(MatchFinder *p);
void MatchFinder_Normalize3(quint32 subValue, LzRef *items, quint32 numItems);
void MatchFinder_ReduceOffsets(MatchFinder *p, quint32 subValue);

quint32 * GetMatchesSpec1(quint32 lenLimit, quint32 curMatch, quint32 pos, const quint8 *buffer, LzRef *son, 
    quint32 _cyclicBufferPos, quint32 _cyclicBufferSize, quint32 _cutValue, 
    quint32 *distances, quint32 maxLen);

/* 
Conditions:
  Mf_GetNumAvailableBytes_Func must be called before each Mf_GetMatchLen_Func.
  Mf_GetPointerToCurrentPos_Func's result must be used only before any other function
*/

typedef void (*Mf_Init_Func)(void *object);
typedef quint8 (*Mf_GetIndexByte_Func)(void *object, qint32 index);
typedef quint32 (*Mf_GetNumAvailableBytes_Func)(void *object);
typedef const quint8 * (*Mf_GetPointerToCurrentPos_Func)(void *object);
typedef quint32 (*Mf_GetMatches_Func)(void *object, quint32 *distances);
typedef void (*Mf_Skip_Func)(void *object, quint32);

typedef struct _IMatchFinder
{
  Mf_Init_Func Init;
  Mf_GetIndexByte_Func GetIndexByte;
  Mf_GetNumAvailableBytes_Func GetNumAvailableBytes;
  Mf_GetPointerToCurrentPos_Func GetPointerToCurrentPos;
  Mf_GetMatches_Func GetMatches;
  Mf_Skip_Func Skip;
} IMatchFinder;

void MatchFinder_CreateVTable(MatchFinder *p, IMatchFinder *vTable);

void MatchFinder_Init(MatchFinder *p);
quint32 Bt3Zip_MatchFinder_GetMatches(MatchFinder *p, quint32 *distances);
quint32 Hc3Zip_MatchFinder_GetMatches(MatchFinder *p, quint32 *distances);
void Bt3Zip_MatchFinder_Skip(MatchFinder *p, quint32 num);
void Hc3Zip_MatchFinder_Skip(MatchFinder *p, quint32 num);

#endif
