#include "MaxineTextureBridge.h"

#include <rhi/qrhi.h>

#include <QDebug>

#if defined(Q_OS_WIN)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <d3d11.h>
#endif

namespace pvj::render {
namespace {

bool dxgiFormatIsBgra(DXGI_FORMAT format)
{
    switch (format) {
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8X8_UNORM:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
        return true;
    default:
        return false;
    }
}

#if defined(Q_OS_WIN)
bool readRgba8D3D11(QRhi* rhi, QRhiTexture* texture, const QSize& size, QImage& out)
{
    if (!rhi || !texture || size.isEmpty()) {
        return false;
    }
    if (rhi->backend() != QRhi::D3D11) {
        return false;
    }

    const QRhiTexture::NativeTexture ntex = texture->nativeTexture();
    if (!ntex.object) {
        return false;
    }
    auto* srcTex = reinterpret_cast<ID3D11Texture2D*>(static_cast<quintptr>(ntex.object));

    ID3D11Device* device = nullptr;
    srcTex->GetDevice(&device);
    if (!device) {
        return false;
    }
    ID3D11DeviceContext* ctx = nullptr;
    device->GetImmediateContext(&ctx);
    if (!ctx) {
        device->Release();
        return false;
    }

    D3D11_TEXTURE2D_DESC desc{};
    srcTex->GetDesc(&desc);
    desc.BindFlags = 0;
    desc.MiscFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.Usage = D3D11_USAGE_STAGING;

    ID3D11Texture2D* staging = nullptr;
    HRESULT hr = device->CreateTexture2D(&desc, nullptr, &staging);
    if (FAILED(hr) || !staging) {
        ctx->Release();
        device->Release();
        return false;
    }

    // Flush pending QRhi draws so the source texture contains this frame's pixels.
    ctx->Flush();
    ctx->CopyResource(staging, srcTex);

    D3D11_MAPPED_SUBRESOURCE mapped{};
    hr = ctx->Map(staging, 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        staging->Release();
        ctx->Release();
        device->Release();
        return false;
    }

    out = QImage(size.width(), size.height(), QImage::Format_RGBA8888);
    const int w = qMin(size.width(), static_cast<int>(desc.Width));
    const int h = qMin(size.height(), static_cast<int>(desc.Height));
    const bool swizzleBgraToRgba = dxgiFormatIsBgra(desc.Format);
    for (int y = 0; y < h; ++y) {
        const uchar* row = static_cast<const uchar*>(mapped.pData) + y * mapped.RowPitch;
        uchar* dst = out.scanLine(y);
        for (int x = 0; x < w; ++x) {
            if (swizzleBgraToRgba) {
                dst[x * 4 + 0] = row[x * 4 + 2];
                dst[x * 4 + 1] = row[x * 4 + 1];
                dst[x * 4 + 2] = row[x * 4 + 0];
            } else {
                dst[x * 4 + 0] = row[x * 4 + 0];
                dst[x * 4 + 1] = row[x * 4 + 1];
                dst[x * 4 + 2] = row[x * 4 + 2];
            }
            dst[x * 4 + 3] = row[x * 4 + 3];
        }
    }
    ctx->Unmap(staging, 0);
    staging->Release();
    ctx->Release();
    device->Release();
    return true;
}
#endif

} // namespace

bool readRgba8Texture(QRhi* rhi, QRhiCommandBuffer* /*cb*/, QRhiTexture* texture, const QSize& size,
                       QImage& out)
{
#if defined(Q_OS_WIN)
    if (readRgba8D3D11(rhi, texture, size, out)) {
        return true;
    }
#endif
    Q_UNUSED(rhi);
    Q_UNUSED(texture);
    Q_UNUSED(size);
    Q_UNUSED(out);
    return false;
}

bool uploadRgba8ToRenderTarget(QRhi* rhi, QRhiCommandBuffer* cb, const QImage& image,
                               QRhiTextureRenderTarget* targetRt, const QSize& stagePx)
{
    if (!rhi || !cb || !targetRt || image.isNull() || stagePx.isEmpty()) {
        return false;
    }
    if (targetRt->description().colorAttachmentCount() < 1) {
        return false;
    }
    QRhiTexture* dstTex = targetRt->description().colorAttachmentAt(0)->texture();
    if (!dstTex) {
        return false;
    }
    QImage upload = image;
    if (upload.format() != QImage::Format_RGBA8888) {
        upload = upload.convertToFormat(QImage::Format_RGBA8888);
    }
    if (upload.size() != stagePx) {
        upload = upload.scaled(stagePx, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    QRhiResourceUpdateBatch* batch = rhi->nextResourceUpdateBatch();
    batch->uploadTexture(dstTex, upload);
    cb->resourceUpdate(batch);
    return true;
}

} // namespace pvj::render
