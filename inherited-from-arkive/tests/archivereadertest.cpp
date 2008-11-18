#include "kernel/Archive.h"
#include "kernel/HandlerRegistry.h"

#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QString>
#include <QtCore/QTextStream>
#include <QtCore/QVariant>

#include <KDE/KCmdLineArgs>
#include <KDE/KComponentData>
#include <KDE/KMimeType>

#include <stdio.h>

int main(int argc, char **argv)
{
    KComponentData cdate("archivereadertest");
    KCmdLineArgs::init(argc, argv, "archivereadertest", "archivereadertest", ki18n("archive reader test"), "1.0", ki18n("test program for archive reading"));
    KCmdLineOptions opts;

    opts.add("list", ki18n("list the archive"));
    opts.add("extract <file>", ki18n("extract <file> from the archive to the current directory"));
    opts.add("extractIndex <index>", ki18n("extract item <index> from the archive to stdout"));
    opts.add("+archive", ki18n("the archive to work with"));

    KCmdLineArgs::addCmdLineOptions(opts);
    KCmdLineArgs *args = KCmdLineArgs::parsedArgs();

    if (args->count() != 1)
        KCmdLineArgs::usageError("You must provide an archive path.");

    QFile archiveFile(args->arg(0));

    if (!archiveFile.open(QIODevice::ReadOnly))
        qFatal("Unable to open file %s", args->arg(0).toUtf8().constData());

    KMimeType::Ptr mimeType = KMimeType::findByPath(args->arg(0));

    // XXX: hack alert!
    if (mimeType->is("application/x-compressed-tar"))
        mimeType = KMimeType::mimeType("application/x-gzip");
    if (mimeType->is("application/x-bzip-compressed-tar"))
        mimeType = KMimeType::mimeType("application/x-bzip2");

    QStringList types;
    types << mimeType->name();
    types << mimeType->allParentMimeTypes();
    Archive *archive = 0;

    for (int i = 0; !archive && i < types.size(); i++)
        archive = CreateArchiveForMimeType(types.at(i).toLatin1(), 0);
    
    if (!archive)
        qFatal("No handler found for mime type %s", mimeType->name().toLatin1().constData());

    QTextStream out(stderr);
    
    if (!archive)
        qFatal("No handler found for archive %s", args->arg(0).toUtf8().constData());

    if (!archive->open(&archiveFile, 0))
        qFatal("Unable to open archive %s: %s", args->arg(0).toUtf8().constData(),
            archive->errorString().toUtf8().constData());

    if (!args->isSet("extract") && !args->isSet("extractIndex")) {
        out << "Archive contains " << archive->size() << 
            (archive->size() == 1 ? " item:" : " items:") << endl;
        for (quint64 i = 0; i < archive->size(); i++) {
            QString path = archive->itemAt(i).property(ArchiveItem::Path).toString();

            if (path.isEmpty())
                path = QLatin1String("(unnamed)");

            out << path << endl;
        }
        delete archive;
        return 0;
    } else {
        // extract a file from the archive
        QFile target;
        quint64 ix = 0;

        if (args->isSet("extract")) {
            QString file = args->getOption("extract");
            if (file.lastIndexOf('/') >= 0)
                file = file.right(file.size() - file.lastIndexOf('/') - 1);
            target.setFileName(file);
            bool ok = target.open(QIODevice::WriteOnly);

            if (!ok)
                qFatal("Unable to open %s for writing", file.toUtf8().constData());
            while (true) {
                if (ix >= archive->size())
                    qFatal("Archive %s does not contain item %s", args->arg(0).toUtf8().constData(),
                        file.toUtf8().constData());
                if (archive->itemAt(ix).property(ArchiveItem::Path) == file)
                    break;
                ++ix;
            }
        } else {
            if (!target.open(stdout, QIODevice::WriteOnly))
                qFatal("Unable to connect to stdout");

            bool ok;
            ix = args->getOption("extractIndex").toULongLong(&ok);

            if (!ok)
                KCmdLineArgs::usage();
        }


        bool ok = archive->extract(ix, &target);

        out << "Extraction " << (ok ? QString("suceeded") : QString("failed: %1").arg(archive->errorString())) << endl;

        delete archive;
        return ok ? 0 : 2;
    }
}
