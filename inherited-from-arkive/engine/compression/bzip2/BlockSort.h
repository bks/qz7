// BlockSort.h

#ifndef __BLOCKSORT_H
#define __BLOCKSORT_H

#include <QtCore/QtGlobal>

// use BLOCK_SORT_EXTERNAL_FLAGS if blockSize can be > 1M
// #define BLOCK_SORT_EXTERNAL_FLAGS

#ifdef BLOCK_SORT_EXTERNAL_FLAGS
#define BLOCK_SORT_EXTERNAL_SIZE(blockSize) ((((blockSize) + 31) >> 5))
#else
#define BLOCK_SORT_EXTERNAL_SIZE(blockSize) 0
#endif

#define BLOCK_SORT_BUF_SIZE(blockSize) ((blockSize) * 2 + BLOCK_SORT_EXTERNAL_SIZE(blockSize) + (1 << 16))

quint32 BlockSort(quint32 *indices, const quint8 *data, quint32 blockSize);

#endif
