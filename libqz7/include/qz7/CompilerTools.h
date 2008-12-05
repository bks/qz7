#ifndef QZ7_COMPILER_TOOLS_H
#define QZ7_COMPILER_TOOLS_H

#ifdef __GNUC__
#define likely(x) (__builtin_expect(!!(x), 1))
#define unlikely(x) (__builtin_expect(!!(x), 0))
#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif

#endif
