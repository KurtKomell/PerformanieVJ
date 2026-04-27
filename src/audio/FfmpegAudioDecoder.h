#pragma once

#include <QAtomicInt>
#include <QObject>
#include <QString>
#include <QThread>

namespace pvj::audio {

class AudioRingBuffer;

// FFmpeg-based stereo PCM decoder feeding an `AudioRingBuffer` at a fixed
// sample rate (must match `AudioEngine::initialize`). Paces output like
// `VideoDecoder` so A/V stay aligned when both play the same file.
class FfmpegAudioDecoder : public QObject
{
    Q_OBJECT
public:
    explicit FfmpegAudioDecoder(AudioRingBuffer* ring,
                                int            outSampleRateHz,
                                QObject*       parent = nullptr);
    ~FfmpegAudioDecoder() override;

    bool open(const QString& filePath);
    void close();

    bool isOpen() const { return m_isOpen.loadAcquire() != 0; }

    void play();
    void pause();
    void stop();
    void setLooping(bool on);
    bool isLooping() const { return m_loop.loadAcquire() != 0; }

    void setPlaybackSpeed(double speed);
    double playbackSpeed() const;

    void seek(qint64 ms);

signals:
    void errorOccurred(QString message);

private:
    class DecoderThread;
    friend class DecoderThread;

    DecoderThread* m_thread = nullptr;

    AudioRingBuffer* m_ring           = nullptr;
    int              m_outSampleRate  = 48000;

    QAtomicInt m_isOpen { 0 };
    QAtomicInt m_loop   { 1 };
};

} // namespace pvj::audio
