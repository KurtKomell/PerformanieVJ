#pragma once

#include "core/Model.h"

#include <QImage>
#include <QSize>
#include <QString>

QT_BEGIN_NAMESPACE
class QRhi;
class QRhiCommandBuffer;
class QRhiTexture;
class QRhiTextureRenderTarget;
QT_END_NAMESPACE

namespace pvj::render {

/// True when this build includes Maxine support (may still be unavailable at runtime).
bool maxineFiltersCompiled();

/// True when NvVFX initialized successfully and filters can run.
bool maxineFiltersAvailable();

/// Call once at startup: SetDllDirectory, model path, and probe NVVideoEffects.dll.
void initializeMaxineRuntime();

/// Optional override for NVVFX_MODEL_DIRECTORY (empty = SDK default search).
void setMaxineModelDirectory(const QString& path);

class MaxineFilterBackend {
public:
    MaxineFilterBackend() = default;
    ~MaxineFilterBackend();

    MaxineFilterBackend(const MaxineFilterBackend&) = delete;
    MaxineFilterBackend& operator=(const MaxineFilterBackend&) = delete;

    bool ensureInitialized(QRhi* rhi);
    void shutdown();

    /// Apply one Maxine catalog filter node (GPU texture readback — prefer applyFromImage).
    bool apply(QRhi* rhi, QRhiCommandBuffer* cb, const pvj::core::CellFilterNode& node,
               QRhiTexture* sourceTex, QRhiTextureRenderTarget* targetRt, const QSize& stagePx);

    /// Apply from a CPU image (used after deferred readback when the GPU has finished the prior frame).
    bool applyFromImage(QRhi* rhi, QRhiCommandBuffer* cb, const pvj::core::CellFilterNode& node,
                        const QImage& input, QRhiTextureRenderTarget* targetRt, const QSize& stagePx);

    void invalidateSize();

private:
    struct Impl;
    Impl* m_impl = nullptr;
    /// \a slotIndex matches internal Maxine effect slot (0–3).
    bool loadMaxineEffect(int slotIndex);
};

} // namespace pvj::render
