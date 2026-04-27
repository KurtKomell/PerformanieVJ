#include <QTest>

#include "video/HapFrameDecoder.h"

class TestHapDecode : public QObject
{
    Q_OBJECT

private slots:
    void hapYCoCg_uncompressed_4x4();
};

void TestHapDecode::hapYCoCg_uncompressed_4x4()
{
    // One top-level section: 0xAF (YCoCg DXT5, no second-stage compression).
    // Body = 16 bytes (one BC3 block for 4×4).
    QByteArray pkt;
    pkt.append(char(0x10));
    pkt.append(char(0x00));
    pkt.append(char(0x00));
    pkt.append(char(0xAF));
    pkt.append(QByteArray(16, char(0)));

    const auto r = pvj::video::decodeHapFrame(pkt, 4, 4);
    QVERIFY(r.ok);
    QVERIFY(r.isYCoCg);
    QCOMPARE(r.dxtBytes.size(), 16);
}

QTEST_MAIN(TestHapDecode)
#include "test_hap_decode.moc"
