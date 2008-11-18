#include "kernel/HandlerRegistry.h"
#include "ZipHandler.h"

EXPORT_ARCHIVE(Zip, "application/zip",
    new NArchive::NZip::CHandler,
    new NArchive::NZip::CHandler
)
