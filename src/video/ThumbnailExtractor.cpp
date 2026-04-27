#include "ThumbnailExtractor.h"

#include "MediaProbe.h"

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

} // namespace

ThumbnailResult ThumbnailExtractor::extract(const QString& filePath,
                                            QSize targetSize,
                                            qint64 timestampMs)
{
    ThumbnailResult out;

    FormatGuard fmt;
    const QByteArray path = filePath.toUtf8();
    if (avformat_open_input(&fmt.ctx, path.constData(), nullptr, nullptr) != 0) {
        out.errorMessage = QStringLiteral("Cannot open %1").arg(filePath);
        return out;
    }
    if (avformat_find_stream_info(fmt.ctx, nullptr) < 0) {
        out.errorMessage = QStringLiteral("No stream info");
        return out;
    }

    int videoIdx = -1;
    for (unsigned int i = 0; i < fmt.ctx->nb_streams; ++i) {
        if (fmt.ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoIdx = int(i);
            break;
        }
    }
    if (videoIdx < 0) {
        out.errorMessage = QStringLiteral("No video stream");
        return out;
    }

    AVStream* stream = fmt.ctx->streams[videoIdx];
    const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) {
        out.errorMessage = QStringLiteral("No decoder for codec id %1").arg(int(stream->codecpar->codec_id));
        return out;
    }

    CodecGuard dec;
    dec.ctx = avcodec_alloc_context3(codec);
    if (!dec.ctx) {
        out.errorMessage = QStringLiteral("Cannot allocate codec context");
        return out;
    }
    if (avcodec_parameters_to_context(dec.ctx, stream->codecpar) < 0) {
        out.errorMessage = QStringLiteral("Failed to copy codec parameters");
        return out;
    }
    {
        const int ideal = QThread::idealThreadCount();
        const int cap   = (ideal > 0) ? std::min(4, ideal) : 2;
        dec.ctx->thread_count = qMax(1, cap);
    }

    if (avcodec_open2(dec.ctx, codec, nullptr) < 0) {
        out.errorMessage = QStringLiteral("Cannot open codec");
        return out;
    }

    // Seek target: explicit ts, else 10% into the clip, else start.
    qint64 seekMs = timestampMs;
    if (seekMs < 0) {
        const qint64 durMs = fmt.ctx->duration > 0
            ? fmt.ctx->duration / (AV_TIME_BASE / 1000) : 0;
        seekMs = (durMs > 2000) ? durMs / 10 : 0;
    }
    if (seekMs > 0) {
        const int64_t ts = av_rescale_q(seekMs, AVRational{1, 1000}, stream->time_base);
        av_seek_frame(fmt.ctx, videoIdx, ts, AVSEEK_FLAG_BACKWARD);
        avcodec_flush_buffers(dec.ctx);
    }

    PacketGuard pkt;
    FrameGuard  frame;

    int attempts = 0;
    constexpr int kMaxAttempts = 256;

    while (attempts++ < kMaxAttempts) {
        const int r = av_read_frame(fmt.ctx, pkt.packet);
        if (r < 0) {
            // Flush decoder.
            avcodec_send_packet(dec.ctx, nullptr);
            if (avcodec_receive_frame(dec.ctx, frame.frame) == 0) {
                out.image = frameToQImage(frame.frame, targetSize);
                out.timestampMs = seekMs;
                out.ok = !out.image.isNull();
            }
            break;
        }
        if (pkt.packet->stream_index != videoIdx) {
            av_packet_unref(pkt.packet);
            continue;
        }
        const int sr = avcodec_send_packet(dec.ctx, pkt.packet);
        av_packet_unref(pkt.packet);
        if (sr < 0) continue;

        while (true) {
            const int rr = avcodec_receive_frame(dec.ctx, frame.frame);
            if (rr == AVERROR(EAGAIN) || rr == AVERROR_EOF) break;
            if (rr < 0) break;
            out.image = frameToQImage(frame.frame, targetSize);
            if (frame.frame->best_effort_timestamp != AV_NOPTS_VALUE) {
                out.timestampMs = av_rescale_q(frame.frame->best_effort_timestamp,
                                               stream->time_base,
                                               AVRational{1, 1000});
            } else {
                out.timestampMs = seekMs;
            }
            out.ok = !out.image.isNull();
            if (out.ok) {
                break;
            }
        }
        if (out.ok) break;
    }

    if (!out.ok && out.errorMessage.isEmpty()) {
        out.errorMessage = QStringLiteral("No decodable frame found within %1 packets")
            .arg(kMaxAttempts);
    }
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
    const MediaProbeResult pr = MediaProbe::probe(filePath);
    if (!pr.ok || !pr.hasVideo) {
        return out;
    }
    const qint64 durMs = pr.durationMs > 250 ? pr.durationMs : 2000;
    for (int i = 0; i < count; ++i) {
        const qint64 seekMs = durMs * (i + 1) / (count + 1);
        const ThumbnailResult tr = extract(filePath, targetSize, seekMs);
        if (tr.ok && !tr.image.isNull()) {
            out.append(tr.image);
        }
    }
    return out;
}

} // namespace pvj::video
