#include "qz7/Archive.h"
#include "qz7/Plugin.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDateTime>
#include <QtCore/QFile>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QTextStream>

#include <stdio.h>

using namespace qz7;

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    QTextStream err(stderr);

    if (argc < 2 || argc > 3) {
usage:
        err << "usage: " + app.arguments()[0] + " [--extract] filename.gz" << endl;
        return 1;
    }

    bool extract = (app.arguments()[1] == "--extract");

    if (extract && argc != 3)
        goto usage;

    QString file = app.arguments()[extract ? 2 : 1];

    Volume *vol = Registry::createVolume("application/octet-stream", file, &app);

    if (!vol) {
        err << "Unable to create Volume for " + file << endl;
        return 2;
    }

    Archive *archive = Registry::createArchive("application/x-gzip", vol);

    if (!archive) {
        err << "Unable to create Archive for " + file << endl;
        return 3;
    }

    if (!archive->open()) {
        err << "Unable to open archive: " + archive->errorString() << endl;
        return 4;
    }

    if (archive->count() < 1) {
        err << "Archive contains no items?!" << endl;
        return 5;
    }

    if (extract) {
        QFile extractOut;
        extractOut.open(stdout, QIODevice::WriteOnly);

        if (!archive->extractTo(0, &extractOut)) {
            err << "Extraction error: " + archive->errorString() << endl;
            return 6;
        }
    } else {
        ArchiveItem item = archive->item(0);

        if (!item.name().isEmpty())
            out << item.name() << ':' << endl;
        else
            out << "(unnamed):" << endl;
        out << "  uncompressed size:" << item.uncompressedSize() << endl;
        out << "  compressed size:  " << item.compressedSize() << endl;
        out << "  CRC: ";
        hex(out) << item.crc() << endl;
        out << "  mtime: " << item.mtime().toString(Qt::ISODate) << endl;
    }

    return 0;
}