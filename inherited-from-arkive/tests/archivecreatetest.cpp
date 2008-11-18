#include "kernel/Archive.h"
#include "kernel/HandlerRegistry.h"

#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QTextStream>
#include <QtCore/QVariant>

#include <KDE/KCmdLineArgs>
#include <KDE/KComponentData>
#include <KDE/KMimeType>

#include <stdio.h>

int main(int argc, char **argv)
{
    KComponentData cdata("archivereadertest");
    KCmdLineArgs::init(argc, argv, "archivereadertest", "archivereadertest", ki18n("archive reader test"), "1.0", ki18n("test program for archive reading"));
    KCmdLineOptions opts;

    opts.add("type <type>", ki18n("the type of the archive to create"));
    opts.add("archive <name>", ki18n("the name of the archive to create"));
    opts.add("+files", ki18n("the source files to add to the archive"));

    KCmdLineArgs::addCmdLineOptions(opts);
    KCmdLineArgs *args = KCmdLineArgs::parsedArgs();

    if (args->count() < 1)
        KCmdLineArgs::usageError(i18n("No source files specified."));

    if (!args->isSet("type"))
        KCmdLineArgs::usageError(i18n("Archive type not specified."));

    if (!args->isSet("archive"))
        KCmdLineArgs::usageError(i18n("No archive name specified."));

    Archive *archive = CreateArchiveForName(args->getOption("type").toAscii(), NULL);

    if (!archive || !archive->canUpdate())
        qFatal("No writing support for archive type %s", qPrintable(args->getOption("type")));

    QList<ArchiveItem> items;

    for (int i = 0; i < args->count(); i++) {
        QString path = args->arg(i);

        QFile *file = new QFile(path);

        if (!file->open(QFile::ReadOnly))
            qFatal("unable to open %s for reading", qPrintable(path));

        ArchiveItem item;
        item.setProperty(ArchiveItem::Stream, QVariant::fromValue<QIODevice *>(file));
        item.setProperty(ArchiveItem::Path, path);

        items.append(item);
    }

    QFile archiveFile(args->getOption("archive"));

    if (!archiveFile.open(QIODevice::WriteOnly))
        qFatal("unable to open archive file %s for writing", qPrintable(args->getOption("archive")));

    if (archive->updateTo(&archiveFile, QMap<quint64, ArchiveItem>(), items)) {
        QTextStream(stderr) << "Archive update suceeded!" << endl;
    } else {
        QTextStream(stderr) << "Archive update failed: " << archive->errorString() << endl;
    }
    delete archive;

    return 0;
}

