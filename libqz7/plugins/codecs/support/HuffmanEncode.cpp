#include "qz7/codec/HuffmanEncode.h"
#include "qz7/Sort.h"

#define MaxLen 16
#define NUM_BITS 10
#define MASK ((1 << NUM_BITS) - 1)

#define NUM_COUNTERS 64

/* use BLOCK_SORT_EXTERNAL_FLAGS if blockSize > 1M */
#define HUFFMAN_SPEED_OPT

void Huffman_Generate(const quint32 *freqs, quint32 *p, quint8 *lens, quint32 numSymbols, quint32 maxLen)
{
  quint32 num = 0;
  /* if (maxLen > 10) maxLen = 10; */
  {
    quint32 i;
    
#ifdef HUFFMAN_SPEED_OPT
    
    quint32 counters[NUM_COUNTERS];
    for (i = 0; i < NUM_COUNTERS; i++) 
      counters[i] = 0;
    for (i = 0; i < numSymbols; i++) 
    {
      quint32 freq = freqs[i];
      counters[(freq < NUM_COUNTERS - 1) ? freq : NUM_COUNTERS - 1]++;
    }
 
    for (i = 1; i < NUM_COUNTERS; i++) 
    {
      quint32 temp = counters[i];
      counters[i] = num;
      num += temp;
    }

    for (i = 0; i < numSymbols; i++) 
    {
      quint32 freq = freqs[i];
      if (freq == 0)
        lens[i] = 0;
      else
        p[counters[((freq < NUM_COUNTERS - 1) ? freq : NUM_COUNTERS - 1)]++] = i | (freq << NUM_BITS);
    }
    counters[0] = 0;
    HeapSort(p + counters[NUM_COUNTERS - 2], counters[NUM_COUNTERS - 1] - counters[NUM_COUNTERS - 2]);
    
#else

    for (i = 0; i < numSymbols; i++) 
    {
      quint32 freq = freqs[i];
      if (freq == 0)
        lens[i] = 0;
      else
        p[num++] = i | (freq << NUM_BITS);
    }
    HeapSort(p, num);

#endif
  }

  if (num < 2) 
  {
    int minCode = 0;
    int maxCode = 1;
    if (num == 1)
    {
      maxCode = p[0] & MASK;
      if (maxCode == 0)
        maxCode++;
    }
    p[minCode] = 0;
    p[maxCode] = 1;
    lens[minCode] = lens[maxCode] = 1;
    return;
  }
  
  {
    quint32 b, e, i;
  
    i = b = e = 0;
    do 
    {
      quint32 n, m, freq;
      n = (i != num && (b == e || (p[i] >> NUM_BITS) <= (p[b] >> NUM_BITS))) ? i++ : b++;
      freq = (p[n] & ~MASK);
      p[n] = (p[n] & MASK) | (e << NUM_BITS);
      m = (i != num && (b == e || (p[i] >> NUM_BITS) <= (p[b] >> NUM_BITS))) ? i++ : b++;
      freq += (p[m] & ~MASK);
      p[m] = (p[m] & MASK) | (e << NUM_BITS);
      p[e] = (p[e] & MASK) | freq;
      e++;
    } 
    while (num - e > 1);
    
    {
      quint32 lenCounters[MaxLen + 1];
      for (i = 0; i <= MaxLen; i++) 
        lenCounters[i] = 0;
      
      p[--e] &= MASK;
      lenCounters[1] = 2;
      while (e > 0) 
      {
        quint32 len = (p[p[--e] >> NUM_BITS] >> NUM_BITS) + 1;
        p[e] = (p[e] & MASK) | (len << NUM_BITS);
        if (len >= maxLen) 
          for (len = maxLen - 1; lenCounters[len] == 0; len--);
        lenCounters[len]--;
        lenCounters[len + 1] += 2;
      }
      
      {
        quint32 len;
        i = 0;
        for (len = maxLen; len != 0; len--) 
        {
          quint32 num;
          for (num = lenCounters[len]; num != 0; num--) 
            lens[p[i++] & MASK] = (quint8)len;
        }
      }
      
      {
        quint32 nextCodes[MaxLen + 1];
        {
          quint32 code = 0;
          quint32 len;
          for (len = 1; len <= MaxLen; len++) 
            nextCodes[len] = code = (code + lenCounters[len - 1]) << 1;
        }
        /* if (code + lenCounters[MaxLen] - 1 != (1 << MaxLen) - 1) throw 1; */

        {
          quint32 i;
          for (i = 0; i < numSymbols; i++) 
            p[i] = nextCodes[lens[i]]++;
        }
      }
    }
  }
}
