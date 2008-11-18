#include <QtTest/QtTest>
#include <QtCore/QBuffer>

#include "kernel/RingBuffer.h"

class RingBufferTester : public QObject {
    Q_OBJECT
public:
    RingBufferTester();
    ~RingBufferTester();

private slots:
    void testCreation();
    void testSetup();
    void testWrite();
    void testPeek();
    void testCopy();
    void testFlush();

private:
    RingBuffer *mRB;
    QBuffer mBuffer;
};

RingBufferTester::RingBufferTester()
{
    mRB = 0;
    mBuffer.open(QIODevice::WriteOnly);
}

RingBufferTester::~RingBufferTester()
{
    delete mRB;
}

void RingBufferTester::testCreation()
{
    RingBuffer b1;

    mRB = new RingBuffer(&mBuffer);
}

void RingBufferTester::testSetup()
{
    QVERIFY(mRB->setBufferSize(7));
    mRB->setBackingDevice(&mBuffer);
}

void RingBufferTester::testWrite()
{
    QVERIFY(mRB->writeByte('a'));

    QByteArray buf("bcde");
    QVERIFY(mRB->writeBytes((quint8 *)buf.constData(), buf.length()));
    
    // nothing should be pushed out yet...
    QCOMPARE(mBuffer.buffer(), QByteArray());

    QVERIFY(mRB->writeByte('f'));

    // and now everything should
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
    QVERIFY(mRB->copyBytes(5, 1));
    QCOMPARE(mRB->peekByte(0), quint8('a'));
    QVERIFY(mRB->copyBytes(6, 3));
}

void RingBufferTester::testFlush()
{
    QVERIFY(mRB->flush());
    QCOMPARE(mBuffer.buffer(), QByteArray("abcdefaabc"));
}

QTEST_MAIN(RingBufferTester)

#include "RingBufferTest.moc"
