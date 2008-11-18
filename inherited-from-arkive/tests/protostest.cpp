/*
 * protostest.cpp - a test harness to run the PROTOS Genome archive test corpus
 * (http://www.ee.oulu.fi/research/ouspg/protos/genome/). Run this program
 * at the root of the extracted corpus, e.g. "protostest gz" to run the gzip corpus.
 */

#include "kernel/Archive.h"
#include "kernel/HandlerRegistry.h"

#include <QtCore/QByteArray>
#include <QtCore/QDebug>
#include <QtCore/QDirIterator>
#include <QtCore/QFile>
#include <QtCore/QString>
#include <QtCore/QTextStream>
#include <QtCore/QVariant>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define WRITE_STRING(x) write(STDOUT_FILENO, x, sizeof(x) - 1)

static QByteArray CurrentTest;
static int NrOk = 0, NrFailed = 0;
static void segv_handler(int signo)
{
    WRITE_STRING("crashing with signal ");
    write(STDOUT_FILENO, strsignal(signo), strlen(strsignal(signo)));
    WRITE_STRING(" on test ");
    write(STDOUT_FILENO, CurrentTest.constData(), CurrentTest.size());
    WRITE_STRING("\n");

    exit(255);
}

static void qtMessageHandler(QtMsgType type, const char *message)
{
    if (type == QtFatalMsg) {
        CurrentTest += '\0';
        fprintf(stderr, "FATAL: while running '%s': %s\n", CurrentTest.data(), message);
        exit(254);
    }
    fprintf(stderr, "%s\n", message);
}

static struct {
    char dir[16];
    char mimeType[64];
} MimeTypes[] = {
    { "ace", "application/x-ace" },
    { "arj", "application/x-arj" },
    { "bz2", "application/x-bzip" },
    { "cab", "application/vns.ms-cab-compressed" },
    { "gz",  "application/x-gzip" },
    { "lha", "application/x-lha" },
    { "rar", "application/x-rar" },
    { "tar", "application/x-tar" },
    { "zip", "application/zip" },
    { "zoo", "application/x-zoo" }
};

static const int NR_TESTS = 10;

extern "C" int main(int argc, char **argv)
{
    QTextStream out(stdout), err(stderr);
    qInstallMsgHandler(qtMessageHandler);

    if (argc != 2) {
        err << "Usage: protostest directory" << endl;
        exit(1);
    }

    QByteArray dirName(argv[1]);
    QByteArray mimeType;

    for (int i = 0; i < NR_TESTS; i++) {
        if (MimeTypes[i].dir == dirName) {
            mimeType = MimeTypes[i].mimeType;
            break;
        }
    }
    if (mimeType.isEmpty()) {
        err << "Could not associate a mime type with test directory " << dirName << endl;
        exit(2);
    }

    // set up some signal handlers to try to record crashes
    signal(SIGSEGV, segv_handler);
    signal(SIGBUS, segv_handler);
    signal(SIGABRT, segv_handler);
    signal(SIGILL, segv_handler);

    // get a handle to /dev/null so we have somewhere to extract everything to
    QFile devNull("/dev/null");
    if (!devNull.open(QIODevice::WriteOnly)) {
        err << "Could not open /dev/null for writing" << endl;
        exit(3);
    }

    QDirIterator it(dirName);

    bool showFeedback = isatty(STDERR_FILENO);

    if (showFeedback)
        err << "testing... 0/0 ok";
    int backupLen = 6; // strlen("0/0 ok")

    while (it.hasNext()) {
        QString path = it.next();

        // skip the directories
        if (path.endsWith("/.") || path.endsWith("/.."))
            continue;

        QFile archiveFile(path);

        // record our currently-running test
        CurrentTest = path.toLatin1();

        if (!archiveFile.open(QIODevice::ReadOnly)) {
            err << path << " open error:" << archiveFile.errorString() << endl;
            continue;
        }
        Archive *archive = CreateArchiveForMimeType(mimeType, 0);

        if (!archive) {
            err << "Could not create an archive for mime type " << mimeType << endl;
            exit(3);
        }

        if (!archive->open(&archiveFile, 0)) {
            NrFailed++;
            out << path << " listing error" << endl;
            delete archive;
            continue;
        }

        bool anyFailed = false;
        int nItems = archive->size();
        for (int i = 0; i < nItems; i++) {
            ArchiveItem item = archive->itemAt(i);

            if (item.property(ArchiveItem::Type).toInt() == ArchiveItem::ItemTypeFile) {
                if (!archive->extract(i, &devNull)) {
                    anyFailed = true;
                    out << path << " extraction error on item " << i << endl;
                }
            }
        }
        delete archive;

        if (anyFailed)
            NrFailed++;
        else
            NrOk++;

        if (showFeedback) {
            for (int i = 0; i < backupLen; i++)
                err << '\b';
    
            QString s = QString::number(NrOk) + '/' + QString::number(NrFailed + NrOk) + " ok";
            backupLen = s.length();
            err << s;
        }
    }

    return 0;
}

