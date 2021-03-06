#include "BlockSort.h"
#include "kernel/Sort.h"

// use BLOCK_SORT_EXTERNAL_FLAGS if blockSize > 1M
// #define BLOCK_SORT_USE_HEAP_SORT

#if _MSC_VER >= 1300
#define NO_INLINE __declspec(noinline) __fastcall
#else
#ifdef _MSC_VER
#define NO_INLINE __fastcall
#else
#define NO_INLINE
#endif
#endif

// Don't change it !!
static const int kNumHashBytes = 2;
static const quint32 kNumHashValues = 1 << (kNumHashBytes * 8);

static const int kNumRefBitsMax = 12; // must be < (kNumHashBytes * 8) = 16

#define BS_TEMP_SIZE kNumHashValues

#ifdef BLOCK_SORT_EXTERNAL_FLAGS

static const int kNumFlagsBits = 5; // 32 Flags in quint32 word
static const quint32 kNumFlagsInWord = (1 << kNumFlagsBits);
static const quint32 kFlagsMask = kNumFlagsInWord - 1;
static const quint32 kAllFlags = 0xFFFFFFFF;

#else

const int kNumBitsMax = 20;
const quint32 kIndexMask = (1 << kNumBitsMax) - 1;
const int kNumExtraBits = 32 - kNumBitsMax;
const int kNumExtra0Bits = kNumExtraBits - 2;
const quint32 kNumExtra0Mask = (1 << kNumExtra0Bits) - 1;

#define SetFinishedGroupSize(p, size) \
    {  *(p) |= ((((size) - 1) & kNumExtra0Mask) << kNumBitsMax); \
        if ((size) > (1 << kNumExtra0Bits)) { \
            *(p) |= 0x40000000;  *((p) + 1) |= ((((size) - 1)>> kNumExtra0Bits) << kNumBitsMax); } } \
     
inline void SetGroupSize(quint32 *p, quint32 size)
{
    if (--size == 0)
        return;
    *p |= 0x80000000 | ((size & kNumExtra0Mask) << kNumBitsMax);
    if (size >= (1 << kNumExtra0Bits)) {
        *p |= 0x40000000;
        p[1] |= ((size >> kNumExtra0Bits) << kNumBitsMax);
    }
}

#endif

// SortGroup - is recursive Range-Sort function with HeapSort optimization for small blocks
// "range" is not real range. It's only for optimization.
// returns: 1 - if there are groups, 0 - no more groups

quint32 NO_INLINE SortGroup(quint32 BlockSize, quint32 NumSortedBytes, quint32 groupOffset, quint32 groupSize, int NumRefBits, quint32 *Indices
#ifndef BLOCK_SORT_USE_HEAP_SORT
                            , quint32 left, quint32 range
#endif
                           )
{
    quint32 *ind2 = Indices + groupOffset;
    if (groupSize <= 1) {
        /*
        #ifndef BLOCK_SORT_EXTERNAL_FLAGS
        SetFinishedGroupSize(ind2, 1);
        #endif
        */
        return 0;
    }
    quint32 *Groups = Indices + BlockSize + BS_TEMP_SIZE;
    if (groupSize <= ((quint32)1 << NumRefBits)
#ifndef BLOCK_SORT_USE_HEAP_SORT
            && groupSize <= range
#endif
       ) {
        quint32 *temp = Indices + BlockSize;
        quint32 j;
        {
            quint32 gPrev;
            quint32 gRes = 0;
            {
                quint32 sp = ind2[0] + NumSortedBytes;
                if (sp >= BlockSize) sp -= BlockSize;
                gPrev = Groups[sp];
                temp[0] = (gPrev << NumRefBits);
            }

            for (j = 1; j < groupSize; j++) {
                quint32 sp = ind2[j] + NumSortedBytes;
                if (sp >= BlockSize) sp -= BlockSize;
                quint32 g = Groups[sp];
                temp[j] = (g << NumRefBits) | j;
                gRes |= (gPrev ^ g);
            }
            if (gRes == 0) {
#ifndef BLOCK_SORT_EXTERNAL_FLAGS
                SetGroupSize(ind2, groupSize);
#endif
                return 1;
            }
        }

        HeapSort(temp, groupSize);
        const quint32 mask = ((1 << NumRefBits) - 1);
        quint32 thereAreGroups = 0;

        quint32 group = groupOffset;
        quint32 cg = (temp[0] >> NumRefBits);
        temp[0] = ind2[temp[0] & mask];

#ifdef BLOCK_SORT_EXTERNAL_FLAGS
        quint32 *Flags = Groups + BlockSize;
#else
        quint32 prevGroupStart = 0;
#endif

        for (j = 1; j < groupSize; j++) {
            quint32 val = temp[j];
            quint32 cgCur = (val >> NumRefBits);

            if (cgCur != cg) {
                cg = cgCur;
                group = groupOffset + j;

#ifdef BLOCK_SORT_EXTERNAL_FLAGS
                quint32 t = group - 1;
                Flags[t >> kNumFlagsBits] &= ~(1 << (t & kFlagsMask));
#else
                SetGroupSize(temp + prevGroupStart, j - prevGroupStart);
                prevGroupStart = j;
#endif
            } else
                thereAreGroups = 1;
            quint32 ind = ind2[val & mask];
            temp[j] = ind;
            Groups[ind] = group;
        }

#ifndef BLOCK_SORT_EXTERNAL_FLAGS
        SetGroupSize(temp + prevGroupStart, j - prevGroupStart);
#endif

        for (j = 0; j < groupSize; j++)
            ind2[j] = temp[j];
        return thereAreGroups;
    }

    // Check that all strings are in one group (cannot sort)
    {
        quint32 sp = ind2[0] + NumSortedBytes; if (sp >= BlockSize) sp -= BlockSize;
        quint32 group = Groups[sp];
        quint32 j;
        for (j = 1; j < groupSize; j++) {
            sp = ind2[j] + NumSortedBytes; if (sp >= BlockSize) sp -= BlockSize;
            if (Groups[sp] != group)
                break;
        }
        if (j == groupSize) {
#ifndef BLOCK_SORT_EXTERNAL_FLAGS
            SetGroupSize(ind2, groupSize);
#endif
            return 1;
        }
    }

#ifndef BLOCK_SORT_USE_HEAP_SORT
    //--------------------------------------
    // Range Sort
    quint32 i;
    quint32 mid;
    for (;;) {
        if (range <= 1) {
#ifndef BLOCK_SORT_EXTERNAL_FLAGS
            SetGroupSize(ind2, groupSize);
#endif
            return 1;
        }
        mid = left + ((range + 1) >> 1);
        quint32 j = groupSize;
        i = 0;
        do {
            quint32 sp = ind2[i] + NumSortedBytes; if (sp >= BlockSize) sp -= BlockSize;
            if (Groups[sp] >= mid) {
                for (j--; j > i; j--) {
                    sp = ind2[j] + NumSortedBytes; if (sp >= BlockSize) sp -= BlockSize;
                    if (Groups[sp] < mid) {
                        quint32 temp = ind2[i]; ind2[i] = ind2[j]; ind2[j] = temp;
                        break;
                    }
                }
                if (i >= j)
                    break;
            }
        } while (++i < j);
        if (i == 0) {
            range = range - (mid - left);
            left = mid;
        } else if (i == groupSize)
            range = (mid - left);
        else
            break;
    }

#ifdef BLOCK_SORT_EXTERNAL_FLAGS {
    quint32 t = (groupOffset + i - 1);
    quint32 *Flags = Groups + BlockSize;
    Flags[t >> kNumFlagsBits] &= ~(1 << (t & kFlagsMask));
}
#endif

for (quint32 j = i; j < groupSize; j++)
    Groups[ind2[j]] = groupOffset + i;

quint32 res = SortGroup(BlockSize, NumSortedBytes, groupOffset, i, NumRefBits, Indices, left, mid - left);
return res | SortGroup(BlockSize, NumSortedBytes, groupOffset + i, groupSize - i, NumRefBits, Indices, mid, range - (mid - left));

#else

    //--------------------------------------
    // Heap Sort

    {
        quint32 j;
        for (j = 0; j < groupSize; j++) {
            quint32 sp = ind2[j] + NumSortedBytes; if (sp >= BlockSize) sp -= BlockSize;
            ind2[j] = sp;
        }

        HeapSortRef(ind2, Groups, groupSize);

        // Write Flags
        quint32 sp = ind2[0];
        quint32 group = Groups[sp];

#ifdef BLOCK_SORT_EXTERNAL_FLAGS
        quint32 *Flags = Groups + BlockSize;
#else
quint32 prevGroupStart = 0;
#endif

        for (j = 1; j < groupSize; j++) {
            sp = ind2[j];
            if (Groups[sp] != group) {
                group = Groups[sp];
#ifdef BLOCK_SORT_EXTERNAL_FLAGS
                quint32 t = groupOffset + j - 1;
                Flags[t >> kNumFlagsBits] &= ~(1 << (t & kFlagsMask));
#else
SetGroupSize(ind2 + prevGroupStart, j - prevGroupStart);
prevGroupStart = j;
#endif
            }
        }

#ifndef BLOCK_SORT_EXTERNAL_FLAGS
        SetGroupSize(ind2 + prevGroupStart, j - prevGroupStart);
#endif

        // Write new Groups values and Check that there are groups
        quint32 thereAreGroups = 0;
        for (j = 0; j < groupSize; j++) {
            quint32 group = groupOffset + j;
#ifndef BLOCK_SORT_EXTERNAL_FLAGS
            quint32 subGroupSize = ((ind2[j] & ~0xC0000000) >> kNumBitsMax);
            if ((ind2[j] & 0x40000000) != 0)
                subGroupSize += ((ind2[j + 1] >> kNumBitsMax) << kNumExtra0Bits);
            subGroupSize++;
            for (;;) {
                quint32 original = ind2[j];
                quint32 sp = original & kIndexMask;
                if (sp < NumSortedBytes) sp += BlockSize; sp -= NumSortedBytes;
                ind2[j] = sp | (original & ~kIndexMask);
                Groups[sp] = group;
                if (--subGroupSize == 0)
                    break;
                j++;
                thereAreGroups = 1;
            }
#else
for (;;) {
    quint32 sp = ind2[j]; if (sp < NumSortedBytes) sp += BlockSize; sp -= NumSortedBytes;
    ind2[j] = sp;
    Groups[sp] = group;
    if ((Flags[(groupOffset + j) >> kNumFlagsBits] & (1 << ((groupOffset + j) & kFlagsMask))) == 0)
        break;
    j++;
    thereAreGroups = 1;
}
#endif
        }
        return thereAreGroups;
    }
#endif
}

// conditions: blockSize > 0
quint32 BlockSort(quint32 *Indices, const quint8 *data, quint32 blockSize)
{
    quint32 *counters = Indices + blockSize;
    quint32 i;

    // Radix-Sort for 2 bytes
    for (i = 0; i < kNumHashValues; i++)
        counters[i] = 0;
    for (i = 0; i < blockSize - 1; i++)
        counters[((quint32)data[i] << 8) | data[i + 1]]++;
    counters[((quint32)data[i] << 8) | data[0]]++;

    quint32 *Groups = counters + BS_TEMP_SIZE;
#ifdef BLOCK_SORT_EXTERNAL_FLAGS
    quint32 *Flags = Groups + blockSize;
    {
        quint32 numWords = (blockSize + kFlagsMask) >> kNumFlagsBits;
        for (i = 0; i < numWords; i++)
            Flags[i] = kAllFlags;
    }
#endif
    {
        quint32 sum = 0;
        for (i = 0; i < kNumHashValues; i++) {
            quint32 groupSize = counters[i];
            if (groupSize > 0) {
#ifdef BLOCK_SORT_EXTERNAL_FLAGS
                quint32 t = sum + groupSize - 1;
                Flags[t >> kNumFlagsBits] &= ~(1 << (t & kFlagsMask));
#endif
                sum += groupSize;
            }
            counters[i] = sum - groupSize;
        }

        for (i = 0; i < blockSize - 1; i++)
            Groups[i] = counters[((quint32)data[i] << 8) | data[i + 1]];
        Groups[i] = counters[((quint32)data[i] << 8) | data[0]];

        for (i = 0; i < blockSize - 1; i++)
            Indices[counters[((quint32)data[i] << 8) | data[i + 1]]++] = i;
        Indices[counters[((quint32)data[i] << 8) | data[0]]++] = i;

#ifndef BLOCK_SORT_EXTERNAL_FLAGS
        quint32 prev = 0;
        for (i = 0; i < kNumHashValues; i++) {
            quint32 prevGroupSize = counters[i] - prev;
            if (prevGroupSize == 0)
                continue;
            SetGroupSize(Indices + prev, prevGroupSize);
            prev = counters[i];
        }
#endif
    }

    int NumRefBits;
    for (NumRefBits = 0; ((blockSize - 1) >> NumRefBits) != 0; NumRefBits++);
    NumRefBits = 32 - NumRefBits;
    if (NumRefBits > kNumRefBitsMax)
        NumRefBits = kNumRefBitsMax;

    for (quint32 NumSortedBytes = kNumHashBytes; ; NumSortedBytes <<= 1) {
#ifndef BLOCK_SORT_EXTERNAL_FLAGS
        quint32 finishedGroupSize = 0;
#endif
        quint32 newLimit = 0;
        for (i = 0; i < blockSize;) {
#ifdef BLOCK_SORT_EXTERNAL_FLAGS

            if ((Flags[i >> kNumFlagsBits] & (1 << (i & kFlagsMask))) == 0) {
                i++;
                continue;
            }
            quint32 groupSize;
            for (groupSize = 1;
                    (Flags[(i + groupSize) >> kNumFlagsBits] & (1 << ((i + groupSize) & kFlagsMask))) != 0;
                    groupSize++);

            groupSize++;

#else

            quint32 groupSize = ((Indices[i] & ~0xC0000000) >> kNumBitsMax);
            bool finishedGroup = ((Indices[i] & 0x80000000) == 0);
            if ((Indices[i] & 0x40000000) != 0) {
                groupSize += ((Indices[i + 1] >> kNumBitsMax) << kNumExtra0Bits);
                Indices[i + 1] &= kIndexMask;
            }
            Indices[i] &= kIndexMask;
            groupSize++;
            if (finishedGroup || groupSize == 1) {
                Indices[i - finishedGroupSize] &= kIndexMask;
                if (finishedGroupSize > 1)
                    Indices[i - finishedGroupSize + 1] &= kIndexMask;
                quint32 newGroupSize = groupSize + finishedGroupSize;
                SetFinishedGroupSize(Indices + i - finishedGroupSize, newGroupSize);
                finishedGroupSize = newGroupSize;
                i += groupSize;
                continue;
            }
            finishedGroupSize = 0;

#endif

            if (NumSortedBytes >= blockSize)
                for (quint32 j = 0; j < groupSize; j++) {
                    quint32 t = (i + j);
                    // Flags[t >> kNumFlagsBits] &= ~(1 << (t & kFlagsMask));
                    Groups[Indices[t]] = t;
                }
            else
                if (SortGroup(blockSize, NumSortedBytes, i, groupSize, NumRefBits, Indices
#ifndef BLOCK_SORT_USE_HEAP_SORT
                              , 0, blockSize
#endif
                             ) != 0)
                    newLimit = i + groupSize;
            i += groupSize;
        }
        if (newLimit == 0)
            break;
    }
#ifndef BLOCK_SORT_EXTERNAL_FLAGS
    for (i = 0; i < blockSize;) {
        quint32 groupSize = ((Indices[i] & ~0xC0000000) >> kNumBitsMax);
        if ((Indices[i] & 0x40000000) != 0) {
            groupSize += ((Indices[i + 1] >> kNumBitsMax) << kNumExtra0Bits);
            Indices[i + 1] &= kIndexMask;
        }
        Indices[i] &= kIndexMask;
        groupSize++;
        i += groupSize;
    }
#endif
    return Groups[0];
}

