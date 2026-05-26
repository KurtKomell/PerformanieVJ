#include "MaxineFilterBackend.h"

namespace pvj::render {

bool maxineFiltersCompiled()
{
#if defined(VJ_FEATURE_MAXINE) && VJ_FEATURE_MAXINE
    return true;
#else
    return false;
#endif
}

// Catalog/UI visibility does not require runtime SDK; apply() falls back to passthrough.

bool maxineFiltersAvailable()
{
    return false;
}

void initializeMaxineRuntime() {}

void setMaxineModelDirectory(const QString& /*path*/) {}

struct MaxineFilterBackend::Impl {};

MaxineFilterBackend::~MaxineFilterBackend()
{
    shutdown();
}

bool MaxineFilterBackend::ensureInitialized(QRhi* /*rhi*/)
{
    return false;
}

void MaxineFilterBackend::shutdown()
{
    delete m_impl;
    m_impl = nullptr;
}

bool MaxineFilterBackend::apply(QRhi* /*rhi*/, QRhiCommandBuffer* /*cb*/,
                                const pvj::core::CellFilterNode& /*node*/, QRhiTexture* /*sourceTex*/,
                                QRhiTextureRenderTarget* /*targetRt*/, const QSize& /*stagePx*/)
{
    return false;
}

bool MaxineFilterBackend::applyFromImage(QRhi* /*rhi*/, QRhiCommandBuffer* /*cb*/,
                                          const pvj::core::CellFilterNode& /*node*/, const QImage& /*input*/,
                                          QRhiTextureRenderTarget* /*targetRt*/, const QSize& /*stagePx*/)
{
    return false;
}

void MaxineFilterBackend::invalidateSize() {}

} // namespace pvj::render
