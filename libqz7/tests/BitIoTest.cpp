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

#include "qz7/BitIoBE.h"
#include "qz7/BitIoLE.h"
#include "qz7/Error.h"
#include "qz7/Stream.h"

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
    void readReversedLE_data();
    void readReversedLE();
    void writeLE();
    void roundtripLE_data();
    void roundtripLE();
    void readBE_data();
    void readBE();
    void writeBE();
    void roundtripBE_data();
    void roundtripBE();

private:
    BitData randomBits(uint nrBits);

    QByteArray mData;
    QBuffer mBuffer;
    QioReadStream mRS;
    QioWriteStream mWS;
    BitReaderLE mReaderLE;
    BitReaderBE mReaderBE;
};

BitIoTester::BitIoTester(QObject *parent)
    : QObject(parent), mRS(&mBuffer), mWS(&mBuffer), mReaderLE(&mRS), mReaderBE(&mRS)
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
        QCOMPARE(mReaderLE.peekBits(bits), result);
    else
        QCOMPARE(mReaderLE.readBits(bits), result);
}

void BitIoTester::readExceptionLE()
{
    QBuffer b1;
    b1.open(QIODevice::ReadOnly);
    QioReadStream rs(&b1);
    BitReaderLE r1(&rs);
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
    QioReadStream rs(&b1);
    BitReaderLE r1(&rs);

    for (int i = 0; i < 5000 * 8; i += 4) {
        uint off = (i % 40);
        uint expected = (0xbadcfe1032ULL >> (36 - off)) & 0x0f;
        if (r1.peekBits(4) != expected)
            qDebug() << "fail at" << i << "bits";
        QCOMPARE(r1.readBits(4), expected);
    }
}

void BitIoTester::readReversedLE_data()
{
    if (mBuffer.isOpen())
        mBuffer.close();

    // 0000 0001 0011 0011 1111 0000 0001 1010 1000 0011 0001 1000 0011 0010 0000 0001 1101 0110
    mData = "\x01\x33\xf0\x1a\x83\x14\x32\x01\xd6";
    mBuffer.setBuffer(&mData);
    mBuffer.open(QIODevice::ReadOnly);

    QTest::addColumn<uint>("bits");
    QTest::addColumn<bool>("peek");
    QTest::addColumn<uint>("result");

    QTest::newRow("1") << 5U << true << 0U;
    QTest::newRow("2") << 8U << true << 0x80U;
    QTest::newRow("3") << 31U << true << 0x580fcc80U;
    QTest::newRow("4") << 9U << false << 0x80U;
    QTest::newRow("5") << 4U << true << 6U;
    QTest::newRow("6") << 9U << false << 0x1e6U;
    QTest::newRow("7") << 3U << false << 3U;
    QTest::newRow("8") << 5U << false << 0U;
}

void BitIoTester::readReversedLE()
{
    QFETCH(uint, bits);
    QFETCH(bool, peek);
    QFETCH(uint, result);

    QCOMPARE(mReaderLE.peekReversedBits(bits), result);

    if (!peek)
        mReaderLE.consumeBits(bits);
}

void BitIoTester::writeLE()
{
    QBuffer b1;
    b1.open(QIODevice::WriteOnly);
    QioWriteStream ws(&b1);
    BitWriterLE w1(&ws);

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
    QioWriteStream ws(&wb);
    BitWriterLE wr(&ws);

    foreach (const BitDataElt& bitPair, values) {
        wr.writeBits(bitPair.first, bitPair.second);
    }
    wr.flush();
    wb.close();

    QBuffer rb(&backing);
    rb.open(QIODevice::ReadOnly);
    QioReadStream rs(&rb);
    BitReaderLE rd(&rs);

    foreach (const BitDataElt& bitPair, values) {
        QCOMPARE(rd.peekBits(bitPair.second), rd.peekBits(bitPair.second));
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

void BitIoTester::readBE_data()
{
    if (mBuffer.isOpen())
        mBuffer.close();

    // 0000 0001 0011 0011 1111 0000 0001 1010 1000 0011 0001 1000 0011 0010 0000 0001 1101 0110
    mData = "\x01\x33\xf0\x1a\x83\x14\x32\x01\xd6";
    mBuffer.setBuffer(&mData);
    mBuffer.open(QIODevice::ReadOnly);

    QTest::addColumn<uint>("bits");
    QTest::addColumn<bool>("peek");
    QTest::addColumn<uint>("result");

    QTest::newRow("1") << 8U << true << 0x01U;
    QTest::newRow("2") << 8U << true << 0x01U;
    QTest::newRow("3") << 8U << false << 0x01U;
    QTest::newRow("4") << 8U << true << 0x33U;
    QTest::newRow("5") << 1U << true << 0x00U;
    QTest::newRow("6") << 4U << true << 0x03U;
    QTest::newRow("7") << 11U << true << 0x19fU;
    QTest::newRow("8") << 13U << false << 0x67eU;
    QTest::newRow("9") << 7U << false << 0x01U;
    QTest::newRow("10") << 1U << true << 0x01U;
    QTest::newRow("11") << 2U << true << 0x02U;
    QTest::newRow("12") << 5U << true << 0x15U;
    QTest::newRow("13") << 3U << false << 0x05U;
    QTest::newRow("14") << 10U << true << 0x106U;
    QTest::newRow("15") << 11U << true << 0x20cU;
    QTest::newRow("16") << 9U << false << 0x83U;
    QTest::newRow("17") << 30U << true << 0x50c8075U;
    QTest::newRow("18") << 26U << false << 0x50c807U;
    QTest::newRow("19") << 7U << true << 0x2dU;
    QTest::newRow("20") << 6U << false << 0x16U;
}

void BitIoTester::readBE()
{
    QFETCH(uint, bits);
    QFETCH(bool, peek);
    QFETCH(uint, result);

    if (peek)
        QCOMPARE(mReaderBE.peekBits(bits), result);
    else
        QCOMPARE(mReaderBE.readBits(bits), result);
}

void BitIoTester::writeBE()
{
    QBuffer b1;
    b1.open(QIODevice::WriteOnly);
    QioWriteStream ws(&b1);
    BitWriterBE wr(&ws);

    wr.writeBits(0x03, 3); // 011X XXXX
    wr.writeBits(0x101, 13); // 0110 0001 0000 0001
    wr.writeBits(0x7a, 9); // ... 0011 1101 0XXX XXXX
    wr.writeBits(0x01, 2); // ... 001X XXXX
    wr.writeBits(0x00, 3); // ... 0010 00XX
    wr.writeBits(0xee, 8); // ... 0010 0011 1011 10XX
    wr.writeBits(0x02, 5); // ... 1011 1000 010X XXXX
    wr.writeBits(0x04, 7); // ... 0100 0001 00XX XXXX
    wr.writeBits(0x23, 6); // ... 0010 0011
    wr.writeBits(0x11, 2); // ... 01XX XXXX
    wr.writeBits(0x01321e5c, 28); // ... 0100 0100 1100 1000 0111 1001 0111 00XX
    wr.writeBits(0x3f10d428, 30); // ... 0111 0010 1111 0001 0000 1101 0100 0010 1000 XXXX
    wr.writeBits(0x08, 4); // ... 1000 1000
    wr.flush();

    QCOMPARE(b1.buffer(), QByteArray("\x61\x01\x3d\x23\xb8\x41\x23\x44\xc8\x79\x73\xf1\x0d\x42\x88"));
}

void BitIoTester::roundtripBE_data()
{
    roundtripLE_data();
}

void BitIoTester::roundtripBE()
{
    QFETCH(BitData, values);

    QByteArray backing;
    QBuffer wb(&backing);
    wb.open(QIODevice::WriteOnly);
    QioWriteStream ws(&wb);
    BitWriterLE wr(&ws);

    foreach (const BitDataElt& bitPair, values) {
        wr.writeBits(bitPair.first, bitPair.second);
    }
    wr.flush();
    wb.close();

    QBuffer rb(&backing);
    rb.open(QIODevice::ReadOnly);
    QioReadStream rs(&rb);
    BitReaderLE rd(&rs);

    foreach (const BitDataElt& bitPair, values) {
        QCOMPARE(rd.peekBits(bitPair.second), rd.peekBits(bitPair.second));
        uint bits = rd.peekBits(bitPair.second);
        if (bits != bitPair.first) {
            qDebug() << "failure when reading" << bitPair.second << "bits";
            qDebug("e ^ a = 0x%08x", bits ^ bitPair.first);
        }
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
