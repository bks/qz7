#ifndef ARKIVE_EXPORT_H
#define ARKIVE_EXPORT_H

#include <kdemacros.h>

#ifndef ARKIVE_EXPORT
#  if defined(MAKE_ARKIVEENGINE_LIB)
#    define ARKIVE_EXPORT KDE_EXPORT
#  else
#    define ARKIVE_EXPORT KDE_IMPORT
#  endif
#endif

#endif
