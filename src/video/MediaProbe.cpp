#include "MediaProbe.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/rational.h>
}

namespace pvj::video {

namespace {

QString codecName(const AVCodecParameters* par)
{
    if (!par) return {};
    const AVCodec* c = avcodec_find_decoder(par->codec_id);
    return c ? QString::fromLatin1(c->name) : QString::fromLatin1(avcodec_get_name(par->codec_id));
}

int audioChannels(const AVCodecParameters* par)
{
    if (!par) {
        return 0;
    }
    // Prefer struct field: av_channel_layout_nb_channels() is FFmpeg 5.1+ and
    // some MSVC/vcpkg toolchains expose ch_layout but not the helper symbol.
    int n = par->ch_layout.nb_channels;
#if LIBAVCODEC_VERSION_MAJOR < 61
    if (n <= 0 && par->channels > 0) {
        n = par->channels;
    }
#endif
    return n;
}

} // namespace

MediaProbeResult MediaProbe::probe(const QString& filePath)
{
    MediaProbeResult out;
    out.filePath = filePath;

    AVFormatContext* fmt = nullptr;
    const QByteArray path = filePath.toUtf8();
    if (avformat_open_input(&fmt, path.constData(), nullptr, nullptr) != 0) {
        out.errorMessage = QStringLiteral("Cannot open %1").arg(filePath);
        return out;
    }
    if (avformat_find_stream_info(fmt, nullptr) < 0) {
        out.errorMessage = QStringLiteral("No stream info for %1").arg(filePath);
        avformat_close_input(&fmt);
        return out;
    }

    if (fmt->iformat && fmt->iformat->name) {
        out.formatName = QString::fromLatin1(fmt->iformat->name);
    }
    if (fmt->duration > 0) {
        out.durationMs = fmt->duration / (AV_TIME_BASE / 1000);
    }

    for (unsigned int i = 0; i < fmt->nb_streams; ++i) {
        const AVStream* st = fmt->streams[i];
        const AVCodecParameters* par = st->codecpar;
        if (!par) continue;

        if (par->codec_type == AVMEDIA_TYPE_VIDEO && !out.hasVideo) {
            out.hasVideo   = true;
            out.videoSize  = QSize(par->width, par->height);
            out.videoCodec = codecName(par);
            AVRational fr = av_guess_frame_rate(fmt, const_cast<AVStream*>(st), nullptr);
            if (fr.num > 0 && fr.den > 0) {
                out.videoFps = double(fr.num) / double(fr.den);
            }
            if (out.durationMs == 0 && st->duration > 0 && st->time_base.den > 0) {
                out.durationMs = qint64(st->duration) * 1000 * st->time_base.num / st->time_base.den;
            }
        } else if (par->codec_type == AVMEDIA_TYPE_AUDIO && !out.hasAudio) {
            out.hasAudio        = true;
            out.audioChannels   = audioChannels(par);
            out.audioSampleRate = par->sample_rate;
            out.audioCodec      = codecName(par);
        }
    }

    avformat_close_input(&fmt);
    out.ok = out.hasVideo || out.hasAudio;
    if (!out.ok && out.errorMessage.isEmpty()) {
        out.errorMessage = QStringLiteral("No video or audio stream");
    }
    return out;
}

} // namespace pvj::video
