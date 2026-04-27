#include <QTest>

#include "audio/AudioRingBuffer.h"

class TestAudioRingBuffer : public QObject
{
    Q_OBJECT
private slots:
    void writeRead_dropsOldestWhenFull();
};

void TestAudioRingBuffer::writeRead_dropsOldestWhenFull()
{
    pvj::audio::AudioRingBuffer rb;
    rb.setCapacityFrames(4);

    float chunk1[8] = {0.25f, 0.25f, 0.25f, 0.25f, 0.25f, 0.25f, 0.25f, 0.25f};
    rb.writeFrames(chunk1, 4);

    float chunk2[8] = {1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f};
    rb.writeFrames(chunk2, 4);

    float out[8] = {};
    rb.readMixAdd(out, 4, 1.f);

    // Oldest frame was dropped; first frame read should be from chunk2.
    QCOMPARE(out[0], 1.f);
    QCOMPARE(out[1], 1.f);
}

QTEST_MAIN(TestAudioRingBuffer)
#include "test_audio_ringbuffer.moc"

