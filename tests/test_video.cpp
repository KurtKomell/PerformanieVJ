#include <QTest>

#include "video/MediaProbe.h"
#include "video/ThumbnailExtractor.h"

class TestVideo : public QObject
{
    Q_OBJECT

private slots:
    void mediaProbe_missingFile();
    void thumbnailExtractor_missingFile();
};

void TestVideo::mediaProbe_missingFile()
{
    const auto r = pvj::video::MediaProbe::probe(
        QStringLiteral("/nonexistent/path/PerformanieVJ_test_missing.mp4"));
    QVERIFY(!r.ok);
    QVERIFY(!r.errorMessage.isEmpty());
}

void TestVideo::thumbnailExtractor_missingFile()
{
    const auto r = pvj::video::ThumbnailExtractor::extract(
        QStringLiteral("/nonexistent/path/PerformanieVJ_test_missing.mp4"));
    QVERIFY(!r.ok);
    QVERIFY(!r.errorMessage.isEmpty());
}

QTEST_MAIN(TestVideo)
#include "test_video.moc"
