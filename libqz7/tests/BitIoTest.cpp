#include <QtTest/QTest>

#include <QtCore/QByteArray>
#include <QtCore/QBuffer>
#include <QtCore/QDebug>
#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QPair>

#include <time.h>
#include <stdlib.h>
#include <unistd.h>

#include "qz7/BitIoLE.h"
#include "qz7/Error.h"

using namespace qz7;

typedef QPair<uint, uint> BitDataElt;
typedef QList<BitDataElt> BitData;

Q_DECLARE_METATYPE(BitData)

class BitIoTester :  public QObject {
    Q_OBJECT

public:
    BitIoTester(QObject *parent = 0);

private slots:
    void readLE_data();
    void readLE();
    void readBigLE();
    void readExceptionLE();
    void writeLE();
    void roundtripLE_data();
    void roundtripLE();

private:
    BitData randomBits(uint nrBits);

    QByteArray mData;
    QBuffer mBuffer;
    BitReaderLE mReader;
};

BitIoTester::BitIoTester(QObject *parent)
    : QObject(parent), mReader(&mBuffer)
{
    ::srand(::getpid() ^ ::time(0));
}

void BitIoTester::readLE_data()
{
    if (mBuffer.isOpen())
        mBuffer.close();

    // 0000 0001 0011 0011 1111 0000 0001 1010 1000 0011
    mData = "\x01\x33\xf0\x1a\x83";
    mBuffer.setBuffer(&mData);
    mBuffer.open(QIODevice::ReadOnly);

    QTest::addColumn<uint>("bits");
    QTest::addColumn<bool>("peek");
    QTest::addColumn<uint>("result");

    QTest::newRow("1") << 8U << true << 0x01U;
    QTest::newRow("2") << 8U << true << 0x01U;
    QTest::newRow("3") << 8U << false << 0x01U;
    QTest::newRow("4") << 8U << true << 0x33U;
    QTest::newRow("5") << 1U << true << 0x01U;
    QTest::newRow("6") << 4U << true << 0x03U;
    QTest::newRow("7") << 11U << true << 0x33U;
    QTest::newRow("8") << 13U << false << 0x1033U;
    QTest::newRow("9") << 7U << false << 0x57U;
    QTest::newRow("10") << 1U << true << 0x01U;
    QTest::newRow("11") << 2U << true << 0x01U;
    QTest::newRow("12") << 5U << true << 0x11U;
    QTest::newRow("13") << 3U << false << 0x01U;
    QTest::newRow("14") << 10U << true << 0x306U;
    QTest::newRow("15") << 11U << true << 0x706U;
    QTest::newRow("16") << 9U << false << 0x106U;
}

void BitIoTester::readLE()
{
    QFETCH(uint, bits);
    QFETCH(bool, peek);
    QFETCH(uint, result);

    if (peek)
        QCOMPARE(mReader.peekBits(bits), result);
    else
        QCOMPARE(mReader.readBits(bits), result);
}

void BitIoTester::readExceptionLE()
{
    QBuffer b1;
    b1.open(QIODevice::ReadOnly);
    BitReaderLE r1(&b1);
    try {
        r1.readBits(1);
        QVERIFY(false);
    } catch (const TruncatedArchiveError&) {
        QVERIFY(true);
    } catch (...) {
        QVERIFY(false);
    }

    try {
        r1.readBits(8);
        QVERIFY(false);
    } catch (const TruncatedArchiveError&) {
        QVERIFY(true);
    } catch (...) {
        QVERIFY(false);
    }

    try {
        r1.readBits(9);
        QVERIFY(false);
    } catch (const TruncatedArchiveError&) {
        QVERIFY(true);
    } catch (...) {
        QVERIFY(false);
    }
}

void BitIoTester::readBigLE()
{
    QByteArray a1;

    for (int i = 0; i < 5000 * 8; i += 40)
        a1 += "\xab\xcd\xef\x01\x23";

    QBuffer b1(&a1);
    b1.open(QIODevice::ReadOnly);
    BitReaderLE r1(&b1);

    for (int i = 0; i < 5000 * 8; i += 4) {
        uint off = (i % 40);
        uint expected = (0xbadcfe1032ULL >> (36 - off)) & 0x0f;
        if (r1.peekBits(4) != expected)
            qDebug() << "fail at" << i << "bits";
        QCOMPARE(r1.readBits(4), expected);
    }
}

void BitIoTester::writeLE()
{
    QBuffer b1;
    b1.open(QIODevice::WriteOnly);
    BitWriterLE w1(&b1);

    w1.writeBits(0x0101, 15); // 0000 0001 X000 0001
    w1.writeBits(0x01, 1); // ... 1000 0001
    w1.writeBits(0x07, 3); // ...XXXX X111
    w1.writeBits(0x02, 2); // ... XXX1 0111
    w1.writeBits(0x02, 4); // ... 0101 0111 XXXX XXX0
    w1.writeBits(0x1d, 7); // ... 0011 1010
    w1.flushByte();
    w1.flush();

    QCOMPARE(b1.buffer(), QByteArray("\x01\x81\x57\x3a"));
}

void BitIoTester::roundtripLE_data()
{
    QTest::addColumn<BitData>("values");

    QList<uint> bitLengths;
    bitLengths << 8 << 16 << 32 << 512 << 8*1000 << 8*4095 << 8 * 4096 << 8 * 4097 << 8*5000 << 8*8191;

    foreach (uint length, bitLengths) {
        QTest::newRow(QByteArray::number(length).constData()) << randomBits(length);
    }
}

void BitIoTester::roundtripLE()
{
    QFETCH(BitData, values);

    QByteArray backing;
    QBuffer wb(&backing);
    wb.open(QIODevice::WriteOnly);
    BitWriterLE wr(&wb);

    foreach (const BitDataElt& bitPair, values) {
        wr.writeBits(bitPair.first, bitPair.second);
    }
    wr.flush();
    wb.close();

    QBuffer rb(&backing);
    rb.open(QIODevice::ReadOnly);
    BitReaderLE rd(&rb);

    foreach (const BitDataElt& bitPair, values) {
        QCOMPARE(rd.readBits(bitPair.second), bitPair.first);
    }

    try {
        rd.readBits(1);
        QVERIFY(false);
    } catch (const TruncatedArchiveError&) {
        QVERIFY(true);
    } catch (...) {
        QVERIFY(false);
    }
}

BitData BitIoTester::randomBits(uint nrBits)
{
    BitData ret;

    uint bits = 0;
    while (bits < nrBits) {
        uint bitsToUse = qMin(uint(::rand() & 31), nrBits - bits);
        uint bitPattern = ::rand() & ((1 << bitsToUse) - 1);

        if (!bitsToUse)
            continue;

        ret << qMakePair(bitPattern, bitsToUse);
        bits += bitsToUse;
    }

    return ret;
}

QTEST_MAIN(BitIoTester)
#include "BitIoTest.moc"
