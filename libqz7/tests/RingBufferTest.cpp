#include <QtTest/QtTest>
#include <QtCore/QBuffer>

#include "RingBuffer.h"
#include "Stream.h"

using namespace qz7;

class RingBufferTester : public QObject {
    Q_OBJECT
public:
    RingBufferTester();
    ~RingBufferTester();

private slots:
    void testCreation();
    void testSetup();
    void testWriteOne();
    void testWriteMulti();
    void testPeek();
    void testCopy();
    void testSelfReferentialCopy();
    void testFlush();

private:
    RingBuffer *mRB;
    QioWriteStream *mStream;
    QBuffer mBuffer;
};

RingBufferTester::RingBufferTester()
    : mRB(0), mStream(0)
{
    mBuffer.open(QIODevice::WriteOnly);
}

RingBufferTester::~RingBufferTester()
{
    delete mRB;
    delete mStream;
}

void RingBufferTester::testCreation()
{
    RingBuffer b1(13);

    mStream = new QioWriteStream(&mBuffer);
    mRB = new RingBuffer();
}

void RingBufferTester::testSetup()
{
    mRB->setBufferSize(6);
    mRB->setBackingStream(mStream);
    mRB->clear();
}

void RingBufferTester::testWriteOne()
{
    mRB->putByte('a');

    // nothing should be pushed out yet...
    QCOMPARE(mBuffer.buffer(), QByteArray());
}

void RingBufferTester::testWriteMulti()
{
    mRB->putBytes((quint8 *)"bcdef", 5);

    // and now everything should
    QCOMPARE(mBuffer.buffer(), QByteArray("abcdef"));

    // and nothing should be left
    mRB->flush();
    QCOMPARE(mBuffer.buffer(), QByteArray("abcdef"));
}

void RingBufferTester::testPeek()
{
    QCOMPARE(mRB->peekByte(0), quint8('f'));
    QCOMPARE(mRB->peekByte(1), quint8('e'));
    QCOMPARE(mRB->peekByte(2), quint8('d'));
    QCOMPARE(mRB->peekByte(3), quint8('c'));
    QCOMPARE(mRB->peekByte(4), quint8('b'));
    QCOMPARE(mRB->peekByte(5), quint8('a'));
}

void RingBufferTester::testCopy()
{
    mRB->repeatBytes(5, 1);
    QCOMPARE(mRB->peekByte(0), quint8('a'));
    mRB->repeatBytes(5, 3);
    QCOMPARE(char(mRB->peekByte(0)), 'd');
    QCOMPARE(char(mRB->peekByte(1)), 'c');
    QCOMPARE(char(mRB->peekByte(2)), 'b');
}

void RingBufferTester::testSelfReferentialCopy()
{
    mRB->repeatBytes(1, 4);
    QCOMPARE(char(mRB->peekByte(0)), 'd');
    QCOMPARE(char(mRB->peekByte(1)), 'c');
    QCOMPARE(char(mRB->peekByte(2)), 'd');
    QCOMPARE(char(mRB->peekByte(3)), 'c');
}

void RingBufferTester::testFlush()
{
    mRB->putBytes((quint8 *)"123", 3);
    mRB->flush();
    QCOMPARE(mBuffer.buffer(), QByteArray("abcdefabcdcdcd123"));
}

QTEST_MAIN(RingBufferTester)

#include "RingBufferTest.moc"
