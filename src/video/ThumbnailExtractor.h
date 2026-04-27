#pragma once

#include <QImage>
#include <QList>
#include <QSize>
#include <QString>

namespace pvj::video {

struct ThumbnailResult {
    bool    ok = false;
    QString errorMessage;
    QImage  image;
    qint64  timestampMs = 0;
};

// Decodes a single representative frame from a media file and returns it as a
// QImage (Format_RGBA8888). The decoder seeks to either the provided
// timestamp or to 1/10 of the file's duration so the frame is past any
// black intro frames.
class ThumbnailExtractor
{
public:
    // If timestampMs == -1 (default) the extractor picks a sensible
    // automatic position. `targetSize` is a bounding box; the image keeps
    // its aspect ratio.
    static ThumbnailResult extract(const QString& filePath,
                                   QSize targetSize = QSize(320, 180),
                                   qint64 timestampMs = -1);

    /// Up to `count` frames spread across the clip (by duration), for filmstrip / grid preview.
    /// Empty list on failure or no video. Images are RGBA8888, scaled to `targetSize` bounding box.
    static QList<QImage> extractPreviewKeyframes(const QString& filePath,
                                                 int count,
                                                 QSize targetSize);
};

} // namespace pvj::video
