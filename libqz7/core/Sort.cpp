/* Sort.c */

#include <QtCore/QObject>

#include "qz7/Sort.h"

#define HeapSortDown(p, k, size, temp) \
  { for (;;) { \
    quint32 s = (k << 1); \
    if (s > size) break; \
    if (s < size && p[s + 1] > p[s]) s++; \
    if (temp >= p[s]) break; \
    p[k] = p[s]; k = s; \
  } p[k] = temp; }

void HeapSort(quint32 *p, quint32 size)
{
  if (size <= 1)
    return;
  p--;
  {
    quint32 i = size / 2;
    do
    {
      quint32 temp = p[i];
      quint32 k = i;
      HeapSortDown(p, k, size, temp)
    }
    while(--i != 0);
  }
  /*
  do
  {
    quint32 k = 1;
    quint32 temp = p[size];
    p[size--] = p[1];
    HeapSortDown(p, k, size, temp)
  }
  while (size > 1);
  */
  while (size > 3)
  {
    quint32 temp = p[size];
    quint32 k = (p[3] > p[2]) ? 3 : 2;
    p[size--] = p[1];
    p[1] = p[k]; 
    HeapSortDown(p, k, size, temp)
  }
  {
    quint32 temp = p[size];
    p[size] = p[1];
    if (size > 2 && p[2] < temp)
    {
      p[1] = p[2];
      p[2] = temp;
    }
    else
      p[1] = temp;
  }
}

/*
#define HeapSortRefDown(p, vals, n, size, temp) \
  { quint32 k = n; quint32 val = vals[temp]; for (;;) { \
    quint32 s = (k << 1); \
    if (s > size) break; \
    if (s < size && vals[p[s + 1]] > vals[p[s]]) s++; \
    if (val >= vals[p[s]]) break; \
    p[k] = p[s]; k = s; \
  } p[k] = temp; }

void HeapSortRef(quint32 *p, quint32 *vals, quint32 size)
{
  if (size <= 1)
    return;
  p--;
  {
    quint32 i = size / 2;
    do
    {
      quint32 temp = p[i];
      HeapSortRefDown(p, vals, i, size, temp);
    }
    while(--i != 0);
  }
  do
  {
    quint32 temp = p[size];
    p[size--] = p[1];
    HeapSortRefDown(p, vals, 1, size, temp);
  }
  while (size > 1);
}
*/
