#include "VideoDecoder.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

#include <QElapsedTimer>
#include <QThread>

#include <algorithm>
#include <QtGlobal>

namespace pvj::video {

// -----------------------------------------------------------------------------
// Internal decoder worker. Runs on its own QThread and communicates with the
// outer VideoDecoder via atomic commands plus a mutex-protected seek target.
// -----------------------------------------------------------------------------
class VideoDecoder::DecoderThread : public QThread
{
public:
    explicit DecoderThread(VideoDecoder* owner)
        : m_owner(owner)
    {}

    // Called from GUI thread.
    bool openSync(const QString& path)
    {
        closeSync();

        m_filePath = path;
        const QByteArray p = path.toUtf8();

        AVFormatContext* fmt = nullptr;
        if (avformat_open_input(&fmt, p.constData(), nullptr, nullptr) != 0) {
            emitError(QStringLiteral("Cannot open %1").arg(path));
            return false;
        }
        if (avformat_find_stream_info(fmt, nullptr) < 0) {
            avformat_close_input(&fmt);
            emitError(QStringLiteral("No stream info for %1").arg(path));
            return false;
        }

        int vIdx = -1;
        for (unsigned int i = 0; i < fmt->nb_streams; ++i) {
            if (fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                vIdx = int(i);
                break;
            }
        }
        if (vIdx < 0) {
            avformat_close_input(&fmt);
            emitError(QStringLiteral("No video stream in %1").arg(path));
            return false;
        }

        AVStream* st = fmt->streams[vIdx];
        const AVCodec* codec = avcodec_find_decoder(st->codecpar->codec_id);
        AVCodecContext* dec = avcodec_alloc_context3(codec);
        if (!codec || !dec) {
            avformat_close_input(&fmt);
            emitError(QStringLiteral("No decoder for codec %1").arg(int(st->codecpar->codec_id)));
            return false;
        }
        avcodec_parameters_to_context(dec, st->codecpar);
        {
            const int ideal = QThread::idealThreadCount();
            const int cap   = (ideal > 0) ? std::min(8, ideal) : 4;
            dec->thread_count = qMax(1, cap);
        }
        if (avcodec_open2(dec, codec, nullptr) < 0) {
            avcodec_free_context(&dec);
            avformat_close_input(&fmt);
            emitError(QStringLiteral("Cannot open codec"));
            return false;
        }

        m_fmt      = fmt;
        m_dec      = dec;
        m_videoIdx = vIdx;
        m_stream   = st;
        m_timeBase = st->time_base;

        AVRational fr = av_guess_frame_rate(fmt, st, nullptr);
        m_fps = (fr.num > 0 && fr.den > 0) ? double(fr.num) / double(fr.den) : 30.0;
        m_owner->m_fps = m_fps;
        m_owner->m_videoSize = QSize(dec->width, dec->height);
        m_owner->m_durationMs = (fmt->duration > 0)
            ? fmt->duration / (AV_TIME_BASE / 1000) : 0;

        m_sws = nullptr;
        return true;
    }

    void closeSync()
    {
        requestStopAndWait();
        if (m_sws) { sws_freeContext(m_sws); m_sws = nullptr; }
        if (m_dec) avcodec_free_context(&m_dec);
        if (m_fmt) avformat_close_input(&m_fmt);
        m_videoIdx = -1;
        m_stream   = nullptr;
    }

    void requestPlay()  { m_playing.storeRelease(1); m_wait.wakeAll(); }
    void requestPause() { m_playing.storeRelease(0); }
    void requestStop()  { m_abort.storeRelease(1); m_playing.storeRelease(0); m_wait.wakeAll(); }

    void requestSeek(qint64 ms)
    {
        QMutexLocker lk(&m_mutex);
        m_seekTargetMs = ms;
        m_seekPending  = true;
        m_wait.wakeAll();
    }

    void setPlaybackSpeed(double s) { m_speed.storeRelease(int(s * 1000.0)); }
    double playbackSpeed() const    { return double(m_speed.loadAcquire()) / 1000.0; }

protected:
    void run() override
    {
        if (!m_fmt || !m_dec || !m_stream) return;

        AVPacket* pkt = av_packet_alloc();
        AVFrame*  frame = av_frame_alloc();
        if (!pkt || !frame) {
            if (pkt)   av_packet_free(&pkt);
            if (frame) av_frame_free(&frame);
            return;
        }

        QElapsedTimer clock;
        clock.invalidate();
        qint64 firstPtsMs = -1;

        while (!m_abort.loadAcquire()) {
            if (!m_playing.loadAcquire()) {
                QMutexLocker lk(&m_mutex);
                m_wait.wait(&m_mutex, 50);
                continue;
            }

            if (m_seekPending) {
                QMutexLocker lk(&m_mutex);
                const qint64 ts = av_rescale_q(m_seekTargetMs,
                                               AVRational{1, 1000},
                                               m_timeBase);
                av_seek_frame(m_fmt, m_videoIdx, ts, AVSEEK_FLAG_BACKWARD);
                avcodec_flush_buffers(m_dec);
                m_seekPending = false;
                clock.invalidate();
                firstPtsMs = -1;
            }

            const int r = av_read_frame(m_fmt, pkt);
            if (r < 0) {
                if (m_owner->m_loop.loadAcquire()) {
                    av_seek_frame(m_fmt, m_videoIdx, 0, AVSEEK_FLAG_BACKWARD);
                    avcodec_flush_buffers(m_dec);
                    clock.invalidate();
                    firstPtsMs = -1;
                    continue;
                }
                emit m_owner->endOfStream();
                m_playing.storeRelease(0);
                continue;
            }
            if (pkt->stream_index != m_videoIdx) {
                av_packet_unref(pkt);
                continue;
            }

            const int sr = avcodec_send_packet(m_dec, pkt);
            av_packet_unref(pkt);
            if (sr < 0 && sr != AVERROR(EAGAIN)) {
                continue;
            }

            while (!m_abort.loadAcquire()) {
                const int rr = avcodec_receive_frame(m_dec, frame);
                if (rr == AVERROR(EAGAIN) || rr == AVERROR_EOF) break;
                if (rr < 0) break;

                qint64 ptsMs = 0;
                if (frame->best_effort_timestamp != AV_NOPTS_VALUE) {
                    ptsMs = av_rescale_q(frame->best_effort_timestamp,
                                         m_timeBase,
                                         AVRational{1, 1000});
                }

                // Pace output to real-time (respecting playback speed).
                if (firstPtsMs < 0) {
                    firstPtsMs = ptsMs;
                    clock.restart();
                }
                const double speed = qMax(0.01, playbackSpeed());
                const qint64 targetMs = qint64((ptsMs - firstPtsMs) / speed);
                const qint64 elapsed = clock.isValid() ? clock.elapsed() : 0;
                if (targetMs > elapsed) {
                    const qint64 waitMs = qMin<qint64>(targetMs - elapsed, 100);
                    QMutexLocker lk(&m_mutex);
                    m_wait.wait(&m_mutex, static_cast<unsigned long>(waitMs));
                    if (m_seekPending || m_abort.loadAcquire()) break;
                }

                QImage img = convertFrame(frame);
                if (!img.isNull()) {
                    m_owner->m_lastPtsMs.storeRelease(ptsMs);
                    emit m_owner->frameReady(img, ptsMs);
                }
            }
        }

        av_frame_free(&frame);
        av_packet_free(&pkt);
    }

private:
    void requestStopAndWait()
    {
        if (!isRunning()) return;
        requestStop();
        wait();
        m_abort.storeRelease(0);
    }

    void emitError(const QString& msg)
    {
        emit m_owner->errorOccurred(msg);
    }

    QImage convertFrame(AVFrame* frame)
    {
        if (!frame || frame->width <= 0 || frame->height <= 0) return {};

        if (!m_sws || m_swsSrcW != frame->width
                 || m_swsSrcH != frame->height
                 || m_swsSrcFmt != frame->format) {
            if (m_sws) sws_freeContext(m_sws);
            m_swsSrcW   = frame->width;
            m_swsSrcH   = frame->height;
            m_swsSrcFmt = frame->format;
            m_sws = sws_getContext(m_swsSrcW, m_swsSrcH, AVPixelFormat(m_swsSrcFmt),
                                   m_swsSrcW, m_swsSrcH, AV_PIX_FMT_RGBA,
                                   SWS_BILINEAR, nullptr, nullptr, nullptr);
        }
        if (!m_sws) return {};

        QImage img(m_swsSrcW, m_swsSrcH, QImage::Format_RGBA8888);
        uint8_t* dst[4]     = { img.bits(), nullptr, nullptr, nullptr };
        int      dstStride[4] = { int(img.bytesPerLine()), 0, 0, 0 };
        sws_scale(m_sws, frame->data, frame->linesize, 0, frame->height,
                  dst, dstStride);
        return img;
    }

    VideoDecoder*   m_owner = nullptr;
    QString         m_filePath;

    AVFormatContext* m_fmt    = nullptr;
    AVCodecContext*  m_dec    = nullptr;
    AVStream*        m_stream = nullptr;
    int              m_videoIdx = -1;
    AVRational       m_timeBase { 1, 1 };
    double           m_fps      = 30.0;

    SwsContext* m_sws = nullptr;
    int         m_swsSrcW   = 0;
    int         m_swsSrcH   = 0;
    int         m_swsSrcFmt = -1;

    QAtomicInt m_playing { 0 };
    QAtomicInt m_abort   { 0 };
    QAtomicInt m_speed   { 1000 }; // millis per second == 1.0x

    QMutex          m_mutex;
    QWaitCondition  m_wait;
    bool            m_seekPending  = false;
    qint64          m_seekTargetMs = 0;
};

// -----------------------------------------------------------------------------
// VideoDecoder public interface
// -----------------------------------------------------------------------------

VideoDecoder::VideoDecoder(QObject* parent)
    : QObject(parent)
    , m_thread(new DecoderThread(this))
{
}

VideoDecoder::~VideoDecoder()
{
    close();
    delete m_thread;
}

bool VideoDecoder::open(const QString& filePath)
{
    close();
    if (!m_thread->openSync(filePath)) {
        return false;
    }
    m_lastPtsMs.storeRelease(-1);
    m_isOpen.storeRelease(1);
    m_thread->start();
    return true;
}

void VideoDecoder::close()
{
    m_isOpen.storeRelease(0);
    m_lastPtsMs.storeRelease(-1);
    m_thread->closeSync();
}

void VideoDecoder::play()  { if (isOpen()) m_thread->requestPlay(); }
void VideoDecoder::pause() { m_thread->requestPause(); }
void VideoDecoder::stop()  { pause(); seek(0); }

void VideoDecoder::setLooping(bool on) { m_loop.storeRelease(on ? 1 : 0); }

void VideoDecoder::setPlaybackSpeed(double speed) { m_thread->setPlaybackSpeed(speed); }
double VideoDecoder::playbackSpeed() const        { return m_thread->playbackSpeed(); }

void VideoDecoder::seek(qint64 ms) { m_thread->requestSeek(ms); }

void VideoDecoder::seekRelative(qint64 deltaMs)
{
    if (!isOpen()) {
        return;
    }
    const qint64 dur = m_durationMs;
    qint64 cur       = m_lastPtsMs.loadAcquire();
    if (cur < 0) {
        cur = 0;
    }
    qint64 tgt = cur + deltaMs;
    if (dur > 1) {
        tgt = qBound(qint64(0), tgt, dur - 1);
    } else {
        tgt = qMax(qint64(0), tgt);
    }
    seek(tgt);
}

} // namespace pvj::video
