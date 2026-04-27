#include "HapFrameDecoder.h"

#include <lz4.h>

#include <cstring>

namespace pvj::video {

namespace {

constexpr int kBlockW = 4;
constexpr int kBlockH = 4;

enum HapSectionType : uint8_t {
    StDecodeInstructions = 0x01,
    StCompressorTable    = 0x02,
    StSizeTable          = 0x03,
    StOffsetTable        = 0x04,
};

// Top-level texture type (low nibble = texture format).
enum HapTextureFmt : uint8_t {
    FmtRGBDXT1    = 0x0B,
    FmtRGBADXT5   = 0x0E,
    FmtYCoCgDXT5  = 0x0F,
};

// High nibble of top-level section type byte.
enum HapSecondStage : uint8_t {
    CompNone    = 0xA0,
    CompSnappy  = 0xB0, // not implemented here (FFmpeg can decode Hap instead)
    CompComplex = 0xC0,
};

enum HapChunkCompressor : uint8_t {
    ChunkRaw   = 0x0A, // << 4 => 0xA0
    ChunkSnappy = 0x0B,
    ChunkLz4    = 0x0C, // << 4 => 0xC0 (custom; LZ4 second stage)
};

struct ChunkInfo {
    uint32_t compressedOffset = 0;
    uint32_t compressedSize = 0;
    uint32_t uncompressedSize = 0;
    int      compressorNibble = 0; // byte from table << 4
};

bool readSectionHeader(const uint8_t*& p, const uint8_t* end,
                       uint32_t& sectionSize, uint8_t& sectionType)
{
    if (end - p < 4) {
        return false;
    }
    sectionSize = uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16);
    sectionType = p[3];
    p += 4;
    if (sectionSize == 0) {
        if (end - p < 4) {
            return false;
        }
        sectionSize = uint32_t(p[0]) | (uint32_t(p[1]) << 8)
            | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
        p += 4;
    }
    if (sectionSize > static_cast<uint32_t>(end - p)) {
        return false;
    }
    return true;
}

size_t expectedDxtSize(int width, int height, bool bc3)
{
    const int bw = (width + kBlockW - 1) / kBlockW;
    const int bh = (height + kBlockH - 1) / kBlockH;
    const size_t blockBytes = bc3 ? 16u : 8u;
    return static_cast<size_t>(bw) * static_cast<size_t>(bh) * blockBytes;
}

bool parseDecodeInstructions(const uint8_t* containerBegin, size_t containerSize,
                             std::vector<ChunkInfo>& chunks, QString& err)
{
    const uint8_t* p = containerBegin;
    const uint8_t* end = containerBegin + containerSize;

    bool hadCompressors = false;
    bool hadSizes       = false;
    bool hadOffsets     = false;

    while (p < end) {
        uint32_t secSize = 0;
        uint8_t  secType = 0;
        const uint8_t* next = p;
        if (!readSectionHeader(next, end, secSize, secType)) {
            err = QStringLiteral("Invalid Hap decode-instruction section header");
            return false;
        }
        const uint8_t* body = next;
        next += secSize;
        if (next > end) {
            err = QStringLiteral("Hap decode-instruction section overflows container");
            return false;
        }

        switch (secType) {
        case StCompressorTable: {
            if (secSize == 0) {
                err = QStringLiteral("Empty compressor table");
                return false;
            }
            if (!chunks.empty() && chunks.size() != secSize) {
                err = QStringLiteral("Hap compressor table chunk count mismatch");
                return false;
            }
            if (chunks.empty()) {
                chunks.resize(secSize);
            }
            for (uint32_t i = 0; i < secSize; ++i) {
                const int nib = int(body[i]) << 4;
                chunks[i].compressorNibble = nib;
            }
            hadCompressors = true;
            break;
        }
        case StSizeTable: {
            if (secSize % 4 != 0) {
                err = QStringLiteral("Invalid Hap chunk size table");
                return false;
            }
            const int n = int(secSize / 4);
            if (!chunks.empty() && int(chunks.size()) != n) {
                err = QStringLiteral("Hap size table chunk count mismatch");
                return false;
            }
            if (chunks.empty()) {
                chunks.resize(static_cast<size_t>(n));
            }
            for (int i = 0; i < n; ++i) {
                const uint32_t cs = uint32_t(body[i * 4 + 0]) | (uint32_t(body[i * 4 + 1]) << 8)
                    | (uint32_t(body[i * 4 + 2]) << 16) | (uint32_t(body[i * 4 + 3]) << 24);
                chunks[static_cast<size_t>(i)].compressedSize = cs;
            }
            hadSizes = true;
            break;
        }
        case StOffsetTable: {
            if (secSize % 4 != 0) {
                err = QStringLiteral("Invalid Hap chunk offset table");
                return false;
            }
            const int n = int(secSize / 4);
            if (chunks.empty()) {
                chunks.resize(static_cast<size_t>(n));
            }
            if (int(chunks.size()) != n) {
                err = QStringLiteral("Hap offset table chunk count mismatch");
                return false;
            }
            for (int i = 0; i < n; ++i) {
                const uint32_t off = uint32_t(body[i * 4 + 0]) | (uint32_t(body[i * 4 + 1]) << 8)
                    | (uint32_t(body[i * 4 + 2]) << 16) | (uint32_t(body[i * 4 + 3]) << 24);
                chunks[static_cast<size_t>(i)].compressedOffset = off;
            }
            hadOffsets = true;
            break;
        }
        default:
            break;
        }
        p = next;
    }

    if (!hadCompressors || !hadSizes) {
        err = QStringLiteral("Hap decode instructions missing compressor or size table");
        return false;
    }

    if (!hadOffsets) {
        uint32_t run = 0;
        for (auto& c : chunks) {
            c.compressedOffset = run;
            run += c.compressedSize;
        }
    }

    return true;
}

bool decompressChunks(const uint8_t* frameData, size_t frameDataSize,
                      std::vector<ChunkInfo>& chunks, QByteArray& outDxt,
                      size_t expectedTexSize, QString& err)
{
    outDxt.resize(int(expectedTexSize));
    std::memset(outDxt.data(), 0, outDxt.size());

    size_t dstOff = 0;
    for (size_t i = 0; i < chunks.size(); ++i) {
        ChunkInfo& ch = chunks[i];
        if (uint64_t(ch.compressedOffset) + uint64_t(ch.compressedSize) > frameDataSize) {
            err = QStringLiteral("Hap chunk out of range");
            return false;
        }
        const uint8_t* src = frameData + ch.compressedOffset;
        uint8_t* dst = reinterpret_cast<uint8_t*>(outDxt.data()) + dstOff;

        if (ch.compressorNibble == (int(ChunkRaw) << 4)) {
            if (ch.compressedSize > expectedTexSize - dstOff) {
                err = QStringLiteral("Hap raw chunk too large");
                return false;
            }
            std::memcpy(dst, src, ch.compressedSize);
            ch.uncompressedSize = ch.compressedSize;
        } else if (ch.compressorNibble == (int(ChunkLz4) << 4)) {
            const int maxOut = int(expectedTexSize - dstOff);
            const int dec = LZ4_decompress_safe(reinterpret_cast<const char*>(src),
                                                reinterpret_cast<char*>(dst),
                                                int(ch.compressedSize),
                                                maxOut);
            if (dec < 0) {
                err = QStringLiteral("LZ4 decompression failed for Hap chunk");
                return false;
            }
            ch.uncompressedSize = uint32_t(dec);
        } else {
            err = QStringLiteral("Unsupported Hap chunk compressor (only raw and LZ4 implemented)");
            return false;
        }
        dstOff += ch.uncompressedSize;
    }

    if (dstOff != expectedTexSize) {
        err = QStringLiteral("Hap decompressed size mismatch");
        return false;
    }
    return true;
}

} // namespace

HapDecodeResult decodeHapFrame(const QByteArray& packet, int width, int height)
{
    HapDecodeResult r;
    if (width <= 0 || height <= 0) {
        r.error = QStringLiteral("Invalid dimensions");
        return r;
    }

    const uint8_t* p = reinterpret_cast<const uint8_t*>(packet.constData());
    const uint8_t* end = p + packet.size();

    uint32_t topPayloadSize = 0;
    uint8_t  topType = 0;
    if (!readSectionHeader(p, end, topPayloadSize, topType)) {
        r.error = QStringLiteral("Invalid Hap top-level section header");
        return r;
    }

    const uint8_t* texBody = p;
    const uint8_t* texEnd = texBody + topPayloadSize;
    if (texEnd > end) {
        r.error = QStringLiteral("Hap top-level section overflows packet");
        return r;
    }

    const int fmtNibble = topType & 0x0F;
    const int compNibble = topType & 0xF0;

    const bool bc3 = (fmtNibble == int(FmtYCoCgDXT5) || fmtNibble == int(FmtRGBADXT5));
    r.isYCoCg = (fmtNibble == int(FmtYCoCgDXT5));

    const size_t texSize = expectedDxtSize(width, height, bc3);

    if (compNibble == int(CompNone)) {
        if (topPayloadSize != texSize) {
            r.error = QStringLiteral("Hap uncompressed section size mismatch");
            return r;
        }
        r.dxtBytes = QByteArray(reinterpret_cast<const char*>(texBody), int(topPayloadSize));
        r.ok = true;
        return r;
    }

    if (compNibble != int(CompComplex)) {
        r.error = QStringLiteral("Unsupported Hap second-stage compression (use uncompressed or complex+LZ4)");
        return r;
    }

    p = texBody;
    uint32_t diSize = 0;
    uint8_t  diType = 0;
    if (!readSectionHeader(p, texEnd, diSize, diType)) {
        r.error = QStringLiteral("Invalid Hap inner section header");
        return r;
    }
    if (diType != StDecodeInstructions) {
        r.error = QStringLiteral("Expected Hap decode-instructions container");
        return r;
    }
    if (p + diSize > texEnd) {
        r.error = QStringLiteral("Decode-instructions size out of range");
        return r;
    }

    std::vector<ChunkInfo> chunks;
    QString err;
    if (!parseDecodeInstructions(p, diSize, chunks, err)) {
        r.error = err;
        return r;
    }
    p += diSize;

    const size_t frameDataSize = static_cast<size_t>(texEnd - p);
    QByteArray dxt;
    if (!decompressChunks(p, frameDataSize, chunks, dxt, texSize, err)) {
        r.error = err;
        return r;
    }

    r.dxtBytes = std::move(dxt);
    r.ok = true;
    return r;
}

} // namespace pvj::video
