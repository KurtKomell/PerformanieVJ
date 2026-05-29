#include "ThumbnailExtractor.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

#include <QByteArray>
#include <QThread>

#include <algorithm>
#include <QtGlobal>

namespace pvj::video {

namespace {

struct FormatGuard {
    AVFormatContext* ctx = nullptr;
    ~FormatGuard() { if (ctx) avformat_close_input(&ctx); }
};

struct CodecGuard {
    AVCodecContext* ctx = nullptr;
    ~CodecGuard() { if (ctx) avcodec_free_context(&ctx); }
};

struct FrameGuard {
    AVFrame* frame = nullptr;
    FrameGuard() : frame(av_frame_alloc()) {}
    ~FrameGuard() { if (frame) av_frame_free(&frame); }
};

struct PacketGuard {
    AVPacket* packet = nullptr;
    PacketGuard() : packet(av_packet_alloc()) {}
    ~PacketGuard() { if (packet) av_packet_free(&packet); }
};

struct SwsGuard {
    SwsContext* ctx = nullptr;
    ~SwsGuard() { if (ctx) sws_freeContext(ctx); }
};

// Convert an AVFrame to a QImage (RGBA8888). Scales to fit into `bounds`
// while preserving aspect ratio.
QImage frameToQImage(const AVFrame* src, QSize bounds)
{
    if (!src || src->width <= 0 || src->height <= 0) {
        return {};
    }

    const double srcAspect = double(src->width) / double(src->height);
    int dstW = bounds.width();
    int dstH = bounds.height();
    if (dstW <= 0 || dstH <= 0) {
        dstW = src->width;
        dstH = src->height;
    } else {
        const double boxAspect = double(dstW) / double(dstH);
        if (srcAspect > boxAspect) {
            dstH = qMax(1, int(dstW / srcAspect));
        } else {
            dstW = qMax(1, int(dstH * srcAspect));
        }
    }

    SwsGuard sws;
    sws.ctx = sws_getContext(src->width, src->height, AVPixelFormat(src->format),
                             dstW, dstH, AV_PIX_FMT_RGBA,
                             SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!sws.ctx) {
        return {};
    }

    QImage img(dstW, dstH, QImage::Format_RGBA8888);
    uint8_t* dstData[4]  = { img.bits(), nullptr, nullptr, nullptr };
    int      dstStride[4] = { int(img.bytesPerLine()), 0, 0, 0 };
    sws_scale(sws.ctx, src->data, src->linesize, 0, src->height,
              dstData, dstStride);
    return img;
}

struct VideoDecoderSession {
    AVFormatContext* fmt     = nullptr;
    AVCodecContext*  dec     = nullptr;
    AVStream*        stream  = nullptr;
    int              videoIdx = -1;
};

bool openVideoDecoderSession(const QString& filePath, VideoDecoderSession& session, QString* errorOut)
{
    const QByteArray path = filePath.toUtf8();
    if (avformat_open_input(&session.fmt, path.constData(), nullptr, nullptr) != 0) {
        if (errorOut) {
            *errorOut = QStringLiteral("Cannot open %1").arg(filePath);
        }
        return false;
    }
    if (avformat_find_stream_info(session.fmt, nullptr) < 0) {
        if (errorOut) {
            *errorOut = QStringLiteral("No stream info");
        }
        return false;
    }

    for (unsigned int i = 0; i < session.fmt->nb_streams; ++i) {
        if (session.fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            session.videoIdx = int(i);
            break;
        }
    }
    if (session.videoIdx < 0) {
        if (errorOut) {
            *errorOut = QStringLiteral("No video stream");
        }
        return false;
    }

    session.stream = session.fmt->streams[session.videoIdx];
    const AVCodec* codec = avcodec_find_decoder(session.stream->codecpar->codec_id);
    if (!codec) {
        if (errorOut) {
            *errorOut = QStringLiteral("No decoder for codec id %1")
                            .arg(int(session.stream->codecpar->codec_id));
        }
        return false;
    }

    session.dec = avcodec_alloc_context3(codec);
    if (!session.dec) {
        if (errorOut) {
            *errorOut = QStringLiteral("Cannot allocate codec context");
        }
        return false;
    }
    if (avcodec_parameters_to_context(session.dec, session.stream->codecpar) < 0) {
        if (errorOut) {
            *errorOut = QStringLiteral("Failed to copy codec parameters");
        }
        return false;
    }
    {
        const int ideal = QThread::idealThreadCount();
        const int cap   = (ideal > 0) ? std::min(4, ideal) : 2;
        session.dec->thread_count = qMax(1, cap);
    }
    if (avcodec_open2(session.dec, codec, nullptr) < 0) {
        if (errorOut) {
            *errorOut = QStringLiteral("Cannot open codec");
        }
        return false;
    }
    return true;
}

void closeVideoDecoderSession(VideoDecoderSession& session)
{
    if (session.dec) {
        avcodec_free_context(&session.dec);
        session.dec = nullptr;
    }
    if (session.fmt) {
        avformat_close_input(&session.fmt);
        session.fmt = nullptr;
    }
    session.stream  = nullptr;
    session.videoIdx = -1;
}

ThumbnailResult decodeFrameAtMs(VideoDecoderSession& session,
                              qint64 seekMs,
                              QSize targetSize)
{
    ThumbnailResult out;
    if (!session.fmt || !session.dec || !session.stream || session.videoIdx < 0) {
        out.errorMessage = QStringLiteral("Decoder not open");
        return out;
    }

    if (seekMs > 0) {
        const int64_t ts = av_rescale_q(seekMs, AVRational{1, 1000}, session.stream->time_base);
        av_seek_frame(session.fmt, session.videoIdx, ts, AVSEEK_FLAG_BACKWARD);
        avcodec_flush_buffers(session.dec);
    }

    PacketGuard pkt;
    FrameGuard  frame;

    int attempts = 0;
    constexpr int kMaxAttempts = 256;

    while (attempts++ < kMaxAttempts) {
        const int r = av_read_frame(session.fmt, pkt.packet);
        if (r < 0) {
            avcodec_send_packet(session.dec, nullptr);
            if (avcodec_receive_frame(session.dec, frame.frame) == 0) {
                out.image = frameToQImage(frame.frame, targetSize);
                out.timestampMs = seekMs;
                out.ok = !out.image.isNull();
            }
            break;
        }
        if (pkt.packet->stream_index != session.videoIdx) {
            av_packet_unref(pkt.packet);
            continue;
        }
        const int sr = avcodec_send_packet(session.dec, pkt.packet);
        av_packet_unref(pkt.packet);
        if (sr < 0) {
            continue;
        }

        while (true) {
            const int rr = avcodec_receive_frame(session.dec, frame.frame);
            if (rr == AVERROR(EAGAIN) || rr == AVERROR_EOF) {
                break;
            }
            if (rr < 0) {
                break;
            }
            out.image = frameToQImage(frame.frame, targetSize);
            if (frame.frame->best_effort_timestamp != AV_NOPTS_VALUE) {
                out.timestampMs = av_rescale_q(frame.frame->best_effort_timestamp,
                                               session.stream->time_base,
                                               AVRational{1, 1000});
            } else {
                out.timestampMs = seekMs;
            }
            out.ok = !out.image.isNull();
            if (out.ok) {
                break;
            }
        }
        if (out.ok) {
            break;
        }
    }

    if (!out.ok && out.errorMessage.isEmpty()) {
        out.errorMessage = QStringLiteral("No decodable frame found within %1 packets")
                               .arg(kMaxAttempts);
    }
    return out;
}

} // namespace

ThumbnailResult ThumbnailExtractor::extract(const QString& filePath,
                                            QSize targetSize,
                                            qint64 timestampMs)
{
    ThumbnailResult out;
    VideoDecoderSession session;
    if (!openVideoDecoderSession(filePath, session, &out.errorMessage)) {
        closeVideoDecoderSession(session);
        return out;
    }

    qint64 seekMs = timestampMs;
    if (seekMs < 0) {
        const qint64 durMs = session.fmt->duration > 0
            ? session.fmt->duration / (AV_TIME_BASE / 1000) : 0;
        seekMs = (durMs > 2000) ? durMs / 10 : 0;
    }

    out = decodeFrameAtMs(session, seekMs, targetSize);
    closeVideoDecoderSession(session);
    return out;
}

QList<QImage> ThumbnailExtractor::extractPreviewKeyframes(const QString& filePath,
                                                          int count,
                                                          QSize targetSize)
{
    QList<QImage> out;
    if (count <= 0 || filePath.isEmpty()) {
        return out;
    }

    VideoDecoderSession session;
    QString error;
    if (!openVideoDecoderSession(filePath, session, &error)) {
        closeVideoDecoderSession(session);
        return out;
    }

    qint64 durMs = session.fmt->duration > 0
        ? session.fmt->duration / (AV_TIME_BASE / 1000) : 0;
    if (durMs <= 250) {
        durMs = 2000;
    }

    for (int i = 0; i < count; ++i) {
        const qint64 seekMs = durMs * (i + 1) / (count + 1);
        const ThumbnailResult tr = decodeFrameAtMs(session, seekMs, targetSize);
        if (tr.ok && !tr.image.isNull()) {
            out.append(tr.image);
        }
    }

    closeVideoDecoderSession(session);
    return out;
}

} // namespace pvj::video
