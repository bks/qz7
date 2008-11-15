#ifndef QZ7_ERROR_H
#define QZ7_ERROR_H

#include <QtCore/QCoreApplication>
#include <QtCore/QIODevice>
#include <QtCore/QString>

#include "qz7/Stream.h"

namespace qz7 {

class Error {
public:
    Error(const QString& msg) : mMsg(msg) { }

    QString message() const { return mMsg; }

private:
    QString mMsg;
};

class ReadError : public Error {
public:
    ReadError(const QIODevice *dev) : 
        Error(QCoreApplication::translate("libqz7", "the error \"%1\" occurred when reading")
                .arg(dev->errorString())) { }

    ReadError(const ReadStream *stream) :
        Error(QCoreApplication::translate("libqz7", "the error \"%1\" occurred when reading")
                .arg(stream->errorString())) { }

    ReadError(const ReadStream& stream) :
        Error(QCoreApplication::translate("libqz7", "the error \"%1\" occurred when reading")
                .arg(stream.errorString())) { }
};

class WriteError : public Error {
public:
    WriteError(const QIODevice *dev) : 
        Error(QCoreApplication::translate("libqz7", "the error \"%1\" occured when writing")
                .arg(dev->errorString())) { }

    WriteError(const WriteStream *stream) :
        Error(QCoreApplication::translate("libqz7", "the error \"%1\" occured when writing")
                .arg(stream->errorString())) { }

    WriteError(const WriteStream& stream) :
        Error(QCoreApplication::translate("libqz7", "the error \"%1\" occured when writing")
                .arg(stream.errorString())) { }
};

class CrcError : public Error {
public:
    CrcError() : Error(QCoreApplication::translate("libqz7", "the archive's integrity check failed")) { }
};

class CorruptedError : public Error {
public:
    CorruptedError() : Error(QCoreApplication::translate("libqz7", "the archive is corrupt")) { }
};

class UnrecognizedFormatError : public Error {
public:
    UnrecognizedFormatError() : 
        Error(QCoreApplication::translate("libqz7", "the archive format is not recognized")) { }
};

class TruncatedArchiveError : public Error {
public:
    TruncatedArchiveError() : 
        Error(QCoreApplication::translate("libqz7", "the archive appears to be truncated")) { }
};

class InterruptedError : public Error {
public:
    InterruptedError() :
        Error(QCoreApplication::translate("libqz7", "the operation was interrupted")) { }
};

}

#endif
