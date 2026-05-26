#pragma once

#include <QImage>
#include <QSize>

QT_BEGIN_NAMESPACE
class QRhi;
class QRhiCommandBuffer;
class QRhiTexture;
class QRhiTextureRenderTarget;
QT_END_NAMESPACE

namespace pvj::render {

/// Read RGBA8 from a QRhi texture into \a out (Format_RGBA8888). Returns false if unsupported.
bool readRgba8Texture(QRhi* rhi, QRhiCommandBuffer* cb, QRhiTexture* texture, const QSize& size,
                       QImage& out);

/// Upload RGBA8888 image into a render target (full viewport).
bool uploadRgba8ToRenderTarget(QRhi* rhi, QRhiCommandBuffer* cb, const QImage& image,
                               QRhiTextureRenderTarget* targetRt, const QSize& stagePx);

} // namespace pvj::render
