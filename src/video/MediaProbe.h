#pragma once

#include <QSize>
#include <QString>

namespace pvj::video {

struct MediaProbeResult {
    bool    ok = false;
    QString errorMessage;

    QString filePath;
    QString formatName;       // e.g. "mov,mp4,m4a,3gp,3g2,mj2"
    qint64  durationMs = 0;   // 0 if unknown

    // Video
    bool    hasVideo    = false;
    QSize   videoSize;
    double  videoFps    = 0.0;
    QString videoCodec;

    // Audio
    bool    hasAudio      = false;
    int     audioChannels = 0;
    int     audioSampleRate = 0;
    QString audioCodec;
};

// Thin wrapper around libavformat that extracts enough metadata for the
// media library (duration, resolution, codec, audio presence). Cheap enough
// to run synchronously for a single file; callers that probe whole folders
// should dispatch the calls to a worker thread.
class MediaProbe
{
public:
    static MediaProbeResult probe(const QString& filePath);
};

} // namespace pvj::video
