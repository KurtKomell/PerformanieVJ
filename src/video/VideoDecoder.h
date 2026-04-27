#pragma once

#include <QAtomicInt>
#include <QAtomicInteger>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QSize>
#include <QString>
#include <QThread>
#include <QWaitCondition>

namespace pvj::video {

// Threaded video decoder based on libavformat/libavcodec. Owns its own
// decoder thread and emits `frameReady` on the caller's thread via a queued
// connection whenever a new frame is ready to display.
//
// Audio is decoded separately by `pvj::audio::FfmpegAudioDecoder` and mixed
// in `pvj::audio::AudioEngine` (M7). VideoDecoder remains video-only.
//
// Typical use (owned by the GUI thread):
//
//     auto* dec = new VideoDecoder(parent);
//     connect(dec, &VideoDecoder::frameReady, preview, &PreviewWidget::setFrame);
//     dec->open("C:/clip.mp4");
//     dec->play();
class VideoDecoder : public QObject
{
    Q_OBJECT
public:
    explicit VideoDecoder(QObject* parent = nullptr);
    ~VideoDecoder() override;

    // Opens a new file; replaces any previous source. Returns true on success.
    // Safe to call from the GUI thread.
    bool open(const QString& filePath);
    void close();

    bool   isOpen()      const { return m_isOpen.loadAcquire() != 0; }
    QSize  videoSize()   const { return m_videoSize; }
    qint64 durationMs()  const { return m_durationMs; }
    double fps()         const { return m_fps; }

    void play();
    void pause();
    void stop();
    void setLooping(bool on);
    bool isLooping() const { return m_loop.loadAcquire() != 0; }

    void setPlaybackSpeed(double speed);  // 1.0 = realtime
    double playbackSpeed() const;

    // Seek (approximate) to `ms`; decoder keeps playing state.
    void seek(qint64 ms);
    void seekRelative(qint64 deltaMs);

    /// Last presented frame PTS in milliseconds (-1 if unknown).
    qint64 lastPresentedPtsMs() const { return m_lastPtsMs.loadAcquire(); }

signals:
    // Emitted from the worker thread; connect slots with Qt::QueuedConnection
    // (default when receiver lives in the GUI thread).
    void frameReady(QImage frame, qint64 pts);
    void endOfStream();
    void errorOccurred(QString message);

private:
    class DecoderThread;
    friend class DecoderThread;

    DecoderThread*  m_thread = nullptr;

    // These are read from the GUI thread after open() returns.
    QSize  m_videoSize;
    qint64 m_durationMs = 0;
    double m_fps        = 0.0;

    QAtomicInt m_isOpen { 0 };
    QAtomicInt m_loop   { 1 };

    QAtomicInteger<qint64> m_lastPtsMs{-1};
};

} // namespace pvj::video
