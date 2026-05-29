#include "FfmpegAudioDecoder.h"
#include "AudioRingBuffer.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

#include <QElapsedTimer>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>

#include <QtGlobal>

#include <algorithm>
#include <vector>

namespace pvj::audio {

namespace {

constexpr qint64 kPacingWaitCapMs = 30;
constexpr qint64 kAnchorUnset     = -1;

} // namespace

class FfmpegAudioDecoder::DecoderThread : public QThread
{
public:
    DecoderThread(FfmpegAudioDecoder* owner, AudioRingBuffer* ring, int outRate)
        : m_owner(owner)
        , m_ring(ring)
        , m_outSampleRate(outRate)
    {}

    bool openSync(const QString& path)
    {
        closeSync();

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

        int aIdx = -1;
        for (unsigned int i = 0; i < fmt->nb_streams; ++i) {
            if (fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                aIdx = int(i);
                break;
            }
        }
        if (aIdx < 0) {
            avformat_close_input(&fmt);
            emitError(QStringLiteral("No audio stream in %1").arg(path));
            return false;
        }

        AVStream* st = fmt->streams[aIdx];
        const AVCodec* codec = avcodec_find_decoder(st->codecpar->codec_id);
        AVCodecContext* dec = avcodec_alloc_context3(codec);
        if (!codec || !dec) {
            avformat_close_input(&fmt);
            emitError(QStringLiteral("No decoder for audio codec %1")
                          .arg(int(st->codecpar->codec_id)));
            return false;
        }
        if (avcodec_parameters_to_context(dec, st->codecpar) < 0) {
            avcodec_free_context(&dec);
            avformat_close_input(&fmt);
            emitError(QStringLiteral("Cannot copy audio codec parameters"));
            return false;
        }
        if (avcodec_open2(dec, codec, nullptr) < 0) {
            avcodec_free_context(&dec);
            avformat_close_input(&fmt);
            emitError(QStringLiteral("Cannot open audio codec"));
            return false;
        }

        if (!buildSwr(dec)) {
            avcodec_free_context(&dec);
            avformat_close_input(&fmt);
            return false;
        }

        m_fmt      = fmt;
        m_dec      = dec;
        m_audioIdx = aIdx;
        m_stream   = st;
        m_timeBase = st->time_base;

        m_frame = av_frame_alloc();
        m_pkt   = av_packet_alloc();
        if (!m_frame || !m_pkt) {
            closeSync();
            emitError(QStringLiteral("Out of memory"));
            return false;
        }

        return true;
    }

    void closeSync()
    {
        requestStopAndWait();
        freeSwr();
        if (m_frame) {
            av_frame_free(&m_frame);
        }
        if (m_pkt) {
            av_packet_free(&m_pkt);
        }
        if (m_dec) {
            avcodec_free_context(&m_dec);
        }
        if (m_fmt) {
            avformat_close_input(&m_fmt);
        }
        m_audioIdx = -1;
        m_stream   = nullptr;
    }

    void requestPlay()  { m_playing.storeRelease(1); m_wait.wakeAll(); }
    void requestPause() { m_playing.storeRelease(0); }
    void requestStop()
    {
        m_abort.storeRelease(1);
        m_playing.storeRelease(0);
        m_wait.wakeAll();
    }

    void requestSeek(qint64 ms)
    {
        QMutexLocker lk(&m_mutex);
        m_seekTargetMs = ms;
        m_seekPending  = true;
        m_wait.wakeAll();
    }

    void setPlaybackSpeed(double s)
    {
        m_speed.storeRelease(int(qRound(qBound(0.0, s, 4.0) * 1000.0)));
        m_wait.wakeAll();
    }
    double playbackSpeed() const { return double(m_speed.loadAcquire()) / 1000.0; }

protected:
    void run() override
    {
        if (!m_fmt || !m_dec || !m_stream || !m_swr || !m_ring) return;

        QElapsedTimer clock;
        clock.invalidate();
        qint64 anchorPtsMs  = kAnchorUnset;
        qint64 anchorWallMs = 0;
        double lastSpeed    = -1.0;

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
                av_seek_frame(m_fmt, m_audioIdx, ts, AVSEEK_FLAG_BACKWARD);
                avcodec_flush_buffers(m_dec);
                freeSwr();
                if (!buildSwr(m_dec)) {
                    emitError(QStringLiteral("Audio resampler reinit failed"));
                    m_seekPending = false;
                    continue;
                }
                m_ring->clear();
                m_seekPending = false;
                clock.invalidate();
                anchorPtsMs = kAnchorUnset;
                lastSpeed   = -1.0;
            }

            const double holdSpeed = qBound(0.0, playbackSpeed(), 4.0);
            if (holdSpeed <= 0.0 && anchorPtsMs >= 0) {
                QMutexLocker lk(&m_mutex);
                m_wait.wait(&m_mutex, static_cast<unsigned long>(kPacingWaitCapMs));
                continue;
            }

            const int r = av_read_frame(m_fmt, m_pkt);
            if (r < 0) {
                if (m_owner->m_loop.loadAcquire()) {
                    av_seek_frame(m_fmt, m_audioIdx, 0, AVSEEK_FLAG_BACKWARD);
                    avcodec_flush_buffers(m_dec);
                    freeSwr();
                    if (!buildSwr(m_dec)) {
                        emitError(QStringLiteral("Audio resampler reinit failed"));
                        continue;
                    }
                    m_ring->clear();
                    clock.invalidate();
                    anchorPtsMs = kAnchorUnset;
                    lastSpeed   = -1.0;
                    continue;
                }
                m_playing.storeRelease(0);
                continue;
            }
            if (m_pkt->stream_index != m_audioIdx) {
                av_packet_unref(m_pkt);
                continue;
            }

            const int sr = avcodec_send_packet(m_dec, m_pkt);
            av_packet_unref(m_pkt);
            if (sr < 0 && sr != AVERROR(EAGAIN)) {
                continue;
            }

            while (!m_abort.loadAcquire()) {
                const int rr = avcodec_receive_frame(m_dec, m_frame);
                if (rr == AVERROR(EAGAIN) || rr == AVERROR_EOF) break;
                if (rr < 0) break;

                qint64 ptsMs = 0;
                if (m_frame->best_effort_timestamp != AV_NOPTS_VALUE) {
                    ptsMs = av_rescale_q(m_frame->best_effort_timestamp,
                                         m_timeBase,
                                         AVRational{1, 1000});
                }

                const double speed = qBound(0.0, playbackSpeed(), 4.0);
                if (speed <= 0.0) {
                    if (anchorPtsMs < 0 || speed != lastSpeed) {
                        if (!clock.isValid()) {
                            clock.start();
                        }
                        anchorPtsMs  = ptsMs;
                        anchorWallMs = clock.elapsed();
                        lastSpeed    = speed;
                    }
                    break;
                }

                if (anchorPtsMs < 0) {
                    if (!clock.isValid()) {
                        clock.start();
                    }
                    anchorPtsMs  = ptsMs;
                    anchorWallMs = 0;
                    lastSpeed    = speed;
                } else if (speed != lastSpeed) {
                    anchorPtsMs  = ptsMs;
                    anchorWallMs = clock.elapsed();
                    lastSpeed    = speed;
                }

                const qint64 targetMs = anchorWallMs
                    + qint64(double(ptsMs - anchorPtsMs) / speed);
                qint64 elapsed = clock.isValid() ? clock.elapsed() : 0;
                while (targetMs > elapsed) {
                    const qint64 waitMs = qMin<qint64>(targetMs - elapsed, kPacingWaitCapMs);
                    QMutexLocker lk(&m_mutex);
                    m_wait.wait(&m_mutex, static_cast<unsigned long>(waitMs));
                    if (m_seekPending || m_abort.loadAcquire()) {
                        break;
                    }
                    elapsed = clock.elapsed();
                }
                if (m_seekPending || m_abort.loadAcquire()) {
                    break;
                }

                if (!convertAndWrite(m_frame)) break;
            }
        }
    }

private:
    void requestStopAndWait()
    {
        if (!isRunning()) return;
        requestStop();
        wait();
        m_abort.storeRelease(0);
    }

    void emitError(const QString& msg) { emit m_owner->errorOccurred(msg); }

    bool buildSwr(AVCodecContext* dec)
    {
        AVChannelLayout outLayout;
        av_channel_layout_default(&outLayout, 2);
        int err = swr_alloc_set_opts2(&m_swr,
                                      &outLayout,
                                      AV_SAMPLE_FMT_FLT,
                                      m_outSampleRate,
                                      &dec->ch_layout,
                                      dec->sample_fmt,
                                      dec->sample_rate,
                                      0,
                                      nullptr);
        if (err < 0 || !m_swr) {
            av_channel_layout_uninit(&outLayout);
            emitError(QStringLiteral("Cannot create audio resampler"));
            return false;
        }
        if (swr_init(m_swr) < 0) {
            swr_free(&m_swr);
            av_channel_layout_uninit(&outLayout);
            emitError(QStringLiteral("Cannot init audio resampler"));
            return false;
        }
        av_channel_layout_uninit(&outLayout);
        return true;
    }

    void freeSwr()
    {
        if (m_swr) {
            swr_free(&m_swr);
        }
    }

    bool convertAndWrite(AVFrame* frame)
    {
        if (!frame || frame->nb_samples <= 0) return true;

        const int64_t delay = swr_get_delay(m_swr, m_dec->sample_rate);
        const int64_t outCount =
            av_rescale_rnd(delay + int64_t(frame->nb_samples),
                           int64_t(m_outSampleRate),
                           int64_t(m_dec->sample_rate),
                           AV_ROUND_UP);

        std::vector<float> interleaved(static_cast<std::size_t>(std::max<int64_t>(outCount, 1)) * 2);

        uint8_t* outPtr[1] = {reinterpret_cast<uint8_t*>(interleaved.data())};

        const uint8_t* inPlanes[AV_NUM_DATA_POINTERS] = {};
        for (int i = 0; i < AV_NUM_DATA_POINTERS; ++i) {
            inPlanes[i] = frame->extended_data[i];
        }

        const int converted = swr_convert(m_swr,
                                          outPtr,
                                          int(outCount),
                                          inPlanes,
                                          frame->nb_samples);
        if (converted < 0) {
            return false;
        }

        if (converted > 0) {
            m_ring->writeFrames(interleaved.data(),
                                static_cast<std::size_t>(converted));
        }
        return true;
    }

    FfmpegAudioDecoder* m_owner = nullptr;
    AudioRingBuffer*    m_ring  = nullptr;
    int                 m_outSampleRate = 48000;
    AVFormatContext* m_fmt      = nullptr;
    AVCodecContext*  m_dec      = nullptr;
    AVStream*        m_stream   = nullptr;
    int              m_audioIdx = -1;
    AVRational       m_timeBase {1, 1};

    SwrContext* m_swr = nullptr;

    AVFrame*  m_frame = nullptr;
    AVPacket* m_pkt   = nullptr;

    QAtomicInt m_playing {0};
    QAtomicInt m_abort {0};
    QAtomicInt m_speed {1000};

    QMutex         m_mutex;
    QWaitCondition m_wait;
    bool           m_seekPending  = false;
    qint64         m_seekTargetMs = 0;
};

FfmpegAudioDecoder::FfmpegAudioDecoder(AudioRingBuffer* ring,
                                       int            outSampleRateHz,
                                       QObject*       parent)
    : QObject(parent)
    , m_thread(new DecoderThread(this, ring, outSampleRateHz > 0 ? outSampleRateHz : 48000))
    , m_ring(ring)
    , m_outSampleRate(outSampleRateHz > 0 ? outSampleRateHz : 48000)
{
}

FfmpegAudioDecoder::~FfmpegAudioDecoder()
{
    close();
    delete m_thread;
}

bool FfmpegAudioDecoder::open(const QString& filePath)
{
    close();
    if (!m_thread->openSync(filePath)) {
        return false;
    }
    m_isOpen.storeRelease(1);
    m_thread->start();
    return true;
}

void FfmpegAudioDecoder::close()
{
    m_isOpen.storeRelease(0);
    m_thread->closeSync();
}

void FfmpegAudioDecoder::play()
{
    if (isOpen()) {
        m_thread->requestPlay();
    }
}
void FfmpegAudioDecoder::pause() { m_thread->requestPause(); }
void FfmpegAudioDecoder::stop()
{
    pause();
    seek(0);
}

void FfmpegAudioDecoder::setLooping(bool on) { m_loop.storeRelease(on ? 1 : 0); }

void FfmpegAudioDecoder::setPlaybackSpeed(double speed) { m_thread->setPlaybackSpeed(speed); }
double FfmpegAudioDecoder::playbackSpeed() const { return m_thread->playbackSpeed(); }

void FfmpegAudioDecoder::seek(qint64 ms) { m_thread->requestSeek(ms); }

} // namespace pvj::audio
