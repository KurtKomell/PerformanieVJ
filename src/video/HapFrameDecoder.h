#pragma once

#include <QByteArray>
#include <QString>

namespace pvj::video {

// Decodes Vidvox Hap-style frames (DXT payload) to raw BC1/BC3 bytes for GPU upload.
// Supports:
//   - YCoCg DXT5 (HapY / DXV5-class): top-level 0xAF (uncompressed) or 0xCF (complex chunks).
//   - RGB DXT1 (Hap1): top-level 0xAB (uncompressed) or 0xCB (complex).
// Chunk second-stage: none (0xA0), LZ4 (0xC0 — same nibble encoding as FFmpeg Hap, custom value).
// Reference: https://github.com/Vidvox/hap/blob/master/documentation/HapVideoDRAFT.md
struct HapDecodeResult {
    bool        ok = false;
    QString     error;
    QByteArray  dxtBytes;
    bool        isYCoCg = false; // true → BC3 + YCoCg shader; false → BC1
};

HapDecodeResult decodeHapFrame(const QByteArray& packet, int width, int height);

} // namespace pvj::video
