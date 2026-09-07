#include "FeedbackLoop.h"

#include "RhiMixerWidget.h"

#include "core/FilterEffectIds.h"
#include "core/Model.h"

#include <rhi/qrhi.h>
#include <rhi/qshader.h>

#include <QFile>
#include <QImage>
#include <QList>
#include <QVector>

#include <cstring>

namespace {

constexpr int kMixerUboFloats = 4 + 3 * 4 * 14 + 4;
constexpr int kMixerUboSize = kMixerUboFloats * int(sizeof(float));
constexpr int kFbUboFloats = 20;
constexpr int kFbUboSize = kFbUboFloats * int(sizeof(float));

QShader loadShader(const QString& resourcePath)
{
    QFile f(resourcePath);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning("FeedbackLoop: cannot open shader %s", qUtf8Printable(resourcePath));
        return {};
    }
    return QShader::fromSerialized(f.readAll());
}

bool createPipeline(QRhi* r, const QString& vsPath, const QString& fsPath,
                    QRhiShaderResourceBindings* srb, QRhiRenderPassDescriptor* rp,
                    std::unique_ptr<QRhiGraphicsPipeline>& out)
{
    QShader vs = loadShader(vsPath);
    QShader fs = loadShader(fsPath);
    if (!vs.isValid() || !fs.isValid()) {
        return false;
    }
    QRhiVertexInputLayout layout;
    layout.setBindings({ { 4 * sizeof(float) } });
    layout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float2, 0 },
        { 0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float) },
    });
    out.reset(r->newGraphicsPipeline());
    out->setTopology(QRhiGraphicsPipeline::TriangleStrip);
    out->setShaderStages({
        { QRhiShaderStage::Vertex, vs },
        { QRhiShaderStage::Fragment, fs },
    });
    out->setVertexInputLayout(layout);
    out->setShaderResourceBindings(srb);
    out->setRenderPassDescriptor(rp);
    if (!out->create()) {
        out.reset();
        return false;
    }
    return true;
}

bool feedbackParamsEqual(const pvj::core::FeedbackParams& a, const pvj::core::FeedbackParams& b)
{
    return a.inSaturation == b.inSaturation && a.inBrightness == b.inBrightness
        && a.inContrast == b.inContrast && a.inHueShift == b.inHueShift && a.inGamma == b.inGamma
        && a.saturation == b.saturation && a.brightness == b.brightness && a.contrast == b.contrast
        && a.hueShift == b.hueShift && a.gamma == b.gamma && a.rotationDeg == b.rotationDeg
        && a.zoom == b.zoom && a.translateX == b.translateX && a.translateY == b.translateY
        && a.retention == b.retention && a.blendMode == b.blendMode && a.frameDelay == b.frameDelay
        && a.inputMode == b.inputMode && a.wrapMode == b.wrapMode;
}

QList<pvj::core::CellFilterNode> filtersBetweenGrades(const QList<pvj::core::CellFilterNode>& chain)
{
    QList<pvj::core::CellFilterNode> out;
    out.reserve(chain.size());
    for (const auto& n : chain) {
        if (!pvj::core::isFeedbackMarkerNode(n.typeId)) {
            out.append(n);
        }
    }
    return out;
}

} // namespace

namespace pvj::render {

FeedbackLoop::FeedbackLoop(RhiMixerWidget* host)
    : m_host(host)
{
    m_isFeedback[BackgroundLayerIndex] = false;
}

FeedbackLoop::~FeedbackLoop() = default;

void FeedbackLoop::setLayer(int layer, bool enabled, const pvj::core::FeedbackParams& p)
{
    if (!m_host || layer == BackgroundLayerIndex) {
        return;
    }
    if (layer < 0 || layer >= LayerCount) {
        return;
    }
    const bool wasEnabled = m_isFeedback[layer];
    const bool changed = wasEnabled != enabled || !feedbackParamsEqual(m_params[layer], p);
    if (!changed) {
        return;
    }
    const bool reset = enabled && !wasEnabled;
    m_isFeedback[layer] = enabled;
    m_params[layer] = p;
    if (reset) {
        softResetRing();
    }
    recomputeActiveLayer();
    m_host->update();
}

bool FeedbackLoop::isLayerFeedback(int layer) const
{
    if (layer < 0 || layer >= LayerCount) {
        return false;
    }
    return m_isFeedback[layer];
}

pvj::core::FeedbackParams FeedbackLoop::layerParams(int layer) const
{
    if (layer < 0 || layer >= LayerCount) {
        return {};
    }
    return m_params[layer];
}

void FeedbackLoop::notifyLayerActiveChanged()
{
    recomputeActiveLayer();
}

void FeedbackLoop::onPreFeedbackTopologyChanged(int layer)
{
    if (layer == m_activeLayer || (layer >= 0 && layer < LayerCount && m_isFeedback[layer])) {
        softResetRing();
    }
}

void FeedbackLoop::recomputeActiveLayer()
{
    const int previous = m_activeLayer;
    m_activeLayer = -1;
    if (m_host) {
        for (int i = UserLayerMin; i < LayerCount; ++i) {
            if (m_isFeedback[i] && m_host->m_layerActive[i]) {
                m_activeLayer = i;
                break;
            }
        }
    }
    if (previous >= 0 && m_activeLayer < 0) {
        releaseGpuResources();
    }
}

bool FeedbackLoop::hasActive() const
{
    if (!m_host) {
        return false;
    }
    const int f = m_activeLayer;
    return f >= 0 && f < LayerCount && m_isFeedback[f] && m_host->m_layerActive[f];
}

int FeedbackLoop::activeLayer() const
{
    return m_activeLayer;
}

pvj::core::FeedbackInputMode FeedbackLoop::inputMode() const
{
    if (!hasActive()) {
        return pvj::core::FeedbackInputMode::BelowOnly;
    }
    return m_params[m_activeLayer].inputMode;
}

QRhiTexture* FeedbackLoop::resultTexture() const
{
    if (!m_primed) {
        return m_pingTex[m_writeIdx].get();
    }
    // Newest written slot is the opposite of the next write index.
    const int readIdx = int(1 - m_writeIdx);
    return m_pingTex[readIdx].get();
}

void FeedbackLoop::softResetRing()
{
    m_writeIdx = 0;
    m_primed = false;
}

void FeedbackLoop::releaseGpuResources()
{
    m_fbPipeline.reset();
    m_fbSrb.reset();
    m_fbUbuf.reset();
    m_mixerBelowPipeline.reset();
    m_belowSrb.reset();
    m_belowUbuf.reset();
    for (auto& rt : m_pingRt) {
        rt.reset();
    }
    for (auto& tex : m_pingTex) {
        tex.reset();
    }
    m_midRt.reset();
    m_midTex.reset();
    m_liveRt.reset();
    m_liveTex.reset();
    m_sharedRp.reset();
    m_blackTex.reset();
    m_blackUploaded = false;
    m_pixelSize = {};
    softResetRing();
}

bool FeedbackLoop::ensureTargets(QRhi* r, const QSize& pixelSize)
{
    if (!r || !m_host || pixelSize.isEmpty() || !m_host->m_sampler) {
        return false;
    }
    if (m_pixelSize == pixelSize && m_liveTex && m_midTex && m_pingTex[0] && m_fbPipeline
        && m_mixerBelowPipeline) {
        return true;
    }

    releaseGpuResources();
    m_pixelSize = pixelSize;

    auto makeColorRt = [&](std::unique_ptr<QRhiTexture>& tex,
                           std::unique_ptr<QRhiTextureRenderTarget>& rt) -> bool {
        tex.reset(r->newTexture(QRhiTexture::RGBA8, pixelSize, 1, QRhiTexture::RenderTarget));
        if (!tex->create()) {
            return false;
        }
        rt.reset(r->newTextureRenderTarget({ QRhiColorAttachment(tex.get()) }));
        if (!m_sharedRp) {
            m_sharedRp.reset(rt->newCompatibleRenderPassDescriptor());
        }
        rt->setRenderPassDescriptor(m_sharedRp.get());
        return rt->create();
    };

    if (!makeColorRt(m_liveTex, m_liveRt) || !makeColorRt(m_midTex, m_midRt)) {
        releaseGpuResources();
        return false;
    }
    for (int i = 0; i < 2; ++i) {
        if (!makeColorRt(m_pingTex[i], m_pingRt[i])) {
            releaseGpuResources();
            return false;
        }
    }

    m_blackTex.reset(r->newTexture(QRhiTexture::RGBA8, QSize(1, 1), 1));
    if (!m_blackTex->create()) {
        releaseGpuResources();
        return false;
    }
    m_blackUploaded = false;

    m_belowUbuf.reset(r->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, kMixerUboSize));
    m_fbUbuf.reset(r->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, kFbUboSize));
    if (!m_belowUbuf->create() || !m_fbUbuf->create()) {
        releaseGpuResources();
        return false;
    }

    m_belowSrb.reset(r->newShaderResourceBindings());
    rebuildBelowMixerBindings(true);
    if (!m_belowSrb->create()) {
        releaseGpuResources();
        return false;
    }
    if (!createPipeline(r, QStringLiteral(":/shaders/mixer.vert.qsb"),
                        QStringLiteral(":/shaders/mixer.frag.qsb"), m_belowSrb.get(),
                        m_sharedRp.get(), m_mixerBelowPipeline)) {
        releaseGpuResources();
        return false;
    }

    m_fbSrb.reset(r->newShaderResourceBindings());
    m_fbSrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::FragmentStage, m_fbUbuf.get()),
        QRhiShaderResourceBinding::sampledTexture(
            1, QRhiShaderResourceBinding::FragmentStage, m_liveTex.get(), m_host->m_sampler.get()),
        QRhiShaderResourceBinding::sampledTexture(
            2, QRhiShaderResourceBinding::FragmentStage, m_blackTex.get(), m_host->m_sampler.get()),
    });
    if (!m_fbSrb->create()) {
        releaseGpuResources();
        return false;
    }
    if (!createPipeline(r, QStringLiteral(":/shaders/layer_feedback.vert.qsb"),
                        QStringLiteral(":/shaders/feedback_loop.frag.qsb"), m_fbSrb.get(),
                        m_sharedRp.get(), m_fbPipeline)) {
        releaseGpuResources();
        return false;
    }

    softResetRing();
    return true;
}

QRhiTexture* FeedbackLoop::historyReadTexture() const
{
    if (!m_primed) {
        return m_blackTex.get();
    }
    return m_pingTex[int(1 - m_writeIdx)].get();
}

QRhiTexture* FeedbackLoop::resultWriteTexture() const
{
    return m_pingTex[m_writeIdx].get();
}

QRhiTextureRenderTarget* FeedbackLoop::resultWriteRt() const
{
    return m_pingRt[m_writeIdx].get();
}

void FeedbackLoop::rebuildBelowMixerBindings(bool useBaseTextures)
{
    if (!m_belowSrb || !m_host || !m_belowUbuf) {
        return;
    }
    QVector<QRhiShaderResourceBinding> binds;
    binds.reserve(2 + (LayerCount - 1) + 2);
    binds.append(QRhiShaderResourceBinding::uniformBuffer(
        0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
        m_belowUbuf.get()));
    for (int gpuLayer = UserLayerMin; gpuLayer < LayerCount; ++gpuLayer) {
        QRhiTexture* src = useBaseTextures ? m_host->layerBaseTextureForInput(gpuLayer)
                                           : m_host->sourceTextureForLayer(gpuLayer);
        if (!src) {
            src = m_host->m_tex[gpuLayer].get();
        }
        binds.append(QRhiShaderResourceBinding::sampledTexture(
            gpuLayer, QRhiShaderResourceBinding::FragmentStage, src, m_host->m_sampler.get()));
    }
    QRhiTexture* placeholder = m_host->m_tex[0].get();
    binds.append(QRhiShaderResourceBinding::sampledTexture(
        LayerCount, QRhiShaderResourceBinding::FragmentStage, placeholder, m_host->m_sampler.get()));
    binds.append(QRhiShaderResourceBinding::sampledTexture(
        LayerCount + 1, QRhiShaderResourceBinding::FragmentStage, placeholder,
        m_host->m_sampler.get()));
    m_belowSrb->setBindings(binds.cbegin(), binds.cend());
    m_belowSrb->create();
}

void FeedbackLoop::updateBelowMixerUbo(QRhiResourceUpdateBatch* batch, int minLayerInclusive,
                                       int maxLayerExclusive)
{
    if (!batch || !m_belowUbuf || !m_host) {
        return;
    }
    QSize dst = m_host->m_stagePixelSize.isValid() && !m_host->m_stagePixelSize.isEmpty()
        ? m_host->m_stagePixelSize
        : (m_host->renderTarget() ? m_host->renderTarget()->pixelSize() : m_host->size());
    if (dst.isEmpty()) {
        dst = QSize(1, 1);
    }

    QSize ref(16, 9);
    int bestArea = 0;
    for (int i = 0; i < LayerCount; ++i) {
        if (!m_host->m_layerActive[i] || m_host->m_sizes[i].isEmpty()) {
            continue;
        }
        const int a = m_host->m_sizes[i].width() * m_host->m_sizes[i].height();
        if (a > bestArea) {
            bestArea = a;
            ref = m_host->m_sizes[i];
        }
    }

    float sx = 1.0f;
    float sy = 1.0f;
    {
        const double srcA = double(ref.width()) / double(ref.height());
        const double dstA = double(dst.width()) / double(dst.height());
        if (srcA > dstA) {
            sy = float(dstA / srcA);
        } else {
            sx = float(srcA / dstA);
        }
    }

    float ubo[kMixerUboFloats];
    std::memset(ubo, 0, sizeof(ubo));
    const float t = float(m_host->m_elapsed.isValid() ? m_host->m_elapsed.elapsed() * 1e-3 : 0.0);
    ubo[0] = sx;
    ubo[1] = sy;
    ubo[2] = t;
    ubo[3] = float(dst.width()) / float(qMax(dst.height(), 1));

    constexpr float kPi = 3.14159265358979323846f;
    const int kLayersBase = 4;
    const int kPicUvABase = kLayersBase + 4 * LayerCount;
    const int kPicColBase = kPicUvABase + 4 * LayerCount;

    for (int i = 0; i < LayerCount; ++i) {
        const int base = kLayersBase + i * 4;
        ubo[base + 0] = m_host->m_opacity[i];
        ubo[base + 1] = float(static_cast<int>(m_host->m_copyMode[i]));
        // Cleared/empty frames must not inject into feedback (avoids dark residual amplify).
        const bool layerOn = m_host->m_layerActive[i] && !m_host->m_sizes[i].isEmpty();
        ubo[base + 2] = layerOn ? 1.0f : 0.0f;
        ubo[base + 3] = float(static_cast<int>(m_host->m_layerMatteRole[i]));

        const auto& pic = m_host->m_layerPicture[i];
        const int pA = kPicUvABase + i * 4;
        ubo[pA + 0] = float(pic.zoom);
        ubo[pA + 1] = float(pic.rotationDeg) * (kPi / 180.0f);
        ubo[pA + 2] = 0.1f * float(pic.circularMotion);
        ubo[pA + 3] = 1.0f;

        const int pC = kPicColBase + i * 4;
        ubo[pC + 0] = float(pic.brightness);
        ubo[pC + 1] = float(pic.contrast);
        ubo[pC + 2] = float(pic.saturation);
        ubo[pC + 3] = float(static_cast<int>(pic.wrapMode));
    }

    const int kMixerCfgBase = kPicColBase + 4 * LayerCount;
    const int minLayer =
        (minLayerInclusive >= 0 && minLayerInclusive < LayerCount) ? minLayerInclusive : 0;
    const int maxLayer =
        (maxLayerExclusive >= 0 && maxLayerExclusive <= LayerCount) ? maxLayerExclusive : LayerCount;
    ubo[kMixerCfgBase + 0] = float(maxLayer);
    ubo[kMixerCfgBase + 1] = float(minLayer);
    ubo[kMixerCfgBase + 2] = 0.0f;
    ubo[kMixerCfgBase + 3] = 0.0f;

    batch->updateDynamicBuffer(m_belowUbuf.get(), 0, kMixerUboSize, ubo);
}

void FeedbackLoop::runPartialMixerPass(QRhi* r, QRhiCommandBuffer* cb, int minLayerInclusive,
                                       int maxLayerExclusive, QRhiTextureRenderTarget* targetRt,
                                       const QSize& stagePx, const QColor& clear,
                                       bool useBaseTextures)
{
    if (!r || !cb || !targetRt || !m_mixerBelowPipeline || !m_belowSrb || !m_host || !m_host->m_vbuf) {
        return;
    }
    rebuildBelowMixerBindings(useBaseTextures);
    QRhiResourceUpdateBatch* batch = r->nextResourceUpdateBatch();
    updateBelowMixerUbo(batch, minLayerInclusive, maxLayerExclusive);
    cb->resourceUpdate(batch);

    cb->beginPass(targetRt, clear, { 1.0f, 0 }, nullptr);
    cb->setGraphicsPipeline(m_mixerBelowPipeline.get());
    cb->setViewport(QRhiViewport(0, 0, stagePx.width(), stagePx.height()));
    cb->setShaderResources(m_belowSrb.get());
    QRhiCommandBuffer::VertexInput vin(m_host->m_vbuf.get(), 0);
    cb->setVertexInput(0, 1, &vin);
    cb->draw(4);
    cb->endPass();
}

void FeedbackLoop::rebuildFeedbackBindings(QRhiTexture* liveTex, QRhiTexture* historyTex)
{
    if (!m_fbSrb || !m_fbUbuf || !m_host || !m_host->m_sampler || !liveTex || !historyTex) {
        return;
    }
    m_fbSrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::FragmentStage, m_fbUbuf.get()),
        QRhiShaderResourceBinding::sampledTexture(
            1, QRhiShaderResourceBinding::FragmentStage, liveTex, m_host->m_sampler.get()),
        QRhiShaderResourceBinding::sampledTexture(
            2, QRhiShaderResourceBinding::FragmentStage, historyTex, m_host->m_sampler.get()),
    });
    m_fbSrb->create();
}

void FeedbackLoop::updateFeedbackUbo(QRhiResourceUpdateBatch* batch, int feedbackLayer,
                                     const QSize& stagePx, int passMode)
{
    if (!batch || !m_fbUbuf || feedbackLayer < 0 || feedbackLayer >= LayerCount) {
        return;
    }
    const auto& p = m_params[feedbackLayer];
    float ubo[kFbUboFloats]{};
    ubo[0] = float(passMode);
    ubo[1] = float(p.saturation);
    ubo[2] = float(p.brightness);
    ubo[3] = float(p.contrast);
    ubo[4] = float(p.hueShift);
    ubo[5] = float(p.gamma);
    ubo[6] = float(p.rotationDeg);
    ubo[7] = float(p.zoom);
    ubo[8] = float(p.translateX);
    ubo[9] = float(p.translateY);
    ubo[10] = float(p.retention);
    ubo[11] = float(static_cast<int>(p.wrapMode));
    ubo[12] = float(p.inBrightness);
    ubo[13] = float(p.inContrast);
    ubo[14] = float(p.inSaturation);
    ubo[15] = float(p.inHueShift);
    ubo[16] = float(p.inGamma);
    ubo[17] = float(static_cast<int>(p.blendMode));
    ubo[18] = float(stagePx.width());
    ubo[19] = float(stagePx.height());
    batch->updateDynamicBuffer(m_fbUbuf.get(), 0, kFbUboSize, ubo);
}

void FeedbackLoop::runFeedbackPass(QRhi* r, QRhiCommandBuffer* cb, int feedbackLayer,
                                   const QSize& stagePx, const QColor& clear, QRhiTexture* liveTex,
                                   QRhiTexture* historyTex, QRhiTextureRenderTarget* destRt,
                                   int passMode)
{
    if (!r || !cb || !liveTex || !historyTex || !destRt || !m_fbPipeline || !m_host
        || !m_host->m_vbuf) {
        return;
    }
    QRhiResourceUpdateBatch* batch = r->nextResourceUpdateBatch();
    if (!m_blackUploaded && m_blackTex) {
        QImage black(1, 1, QImage::Format_RGBA8888);
        black.fill(Qt::black);
        batch->uploadTexture(m_blackTex.get(), black);
        m_blackUploaded = true;
    }
    updateFeedbackUbo(batch, feedbackLayer, stagePx, passMode);
    rebuildFeedbackBindings(liveTex, historyTex);
    cb->beginPass(destRt, clear, { 1.0f, 0 }, batch);
    cb->setGraphicsPipeline(m_fbPipeline.get());
    cb->setViewport(QRhiViewport(0, 0, stagePx.width(), stagePx.height()));
    cb->setShaderResources(m_fbSrb.get());
    QRhiCommandBuffer::VertexInput vin(m_host->m_vbuf.get(), 0);
    cb->setVertexInput(0, 1, &vin);
    cb->draw(4);
    cb->endPass();
}

void FeedbackLoop::clearPingPong(QRhiCommandBuffer* cb)
{
    if (!cb) {
        return;
    }
    const QColor empty(0, 0, 0, 0);
    for (auto& rt : m_pingRt) {
        if (rt) {
            cb->beginPass(rt.get(), empty, { 1.0f, 0 }, nullptr);
            cb->endPass();
        }
    }
    softResetRing();
}

void FeedbackLoop::renderFrame(QRhi* r, QRhiCommandBuffer* cb, const QSize& stagePx,
                               const QColor& clear)
{
    if (!m_host || !r || !cb || !hasActive()) {
        return;
    }
    const int F = m_activeLayer;
    if (!ensureTargets(r, stagePx)) {
        return;
    }

    if (m_host->m_opacity[F] <= 0.02f) {
        clearPingPong(cb);
        return;
    }

    // 1) Live inject = layers below F
    runPartialMixerPass(r, cb, 0, F, m_liveRt.get(), stagePx, clear, /*useBaseTextures*/ false);

    int belowWithSize = 0;
    for (int i = UserLayerMin; i < F; ++i) {
        if (m_host->m_layerActive[i] && !m_host->m_sizes[i].isEmpty()) {
            ++belowWithSize;
        }
    }

    // 2) Input grade
    runFeedbackPass(r, cb, F, stagePx, clear, m_liveTex.get(), m_blackTex.get(), m_midRt.get(),
                    /*passMode*/ 0);

    QRhiTexture* afterInput = m_midTex.get();

    // 3) Filters between grades
    const QList<pvj::core::CellFilterNode> midFilters =
        filtersBetweenGrades(m_host->m_layerFilterChain[F]);
    if (!midFilters.isEmpty()) {
        m_host->ensureLayerFilterTargets(r, stagePx);
        if (m_host->m_layerFilterPingRt[F][0] && m_host->m_layerFilterPingRt[F][1]) {
            m_host->runPerLayerFilterChain(r, cb, F, afterInput, stagePx, clear, &midFilters);
            if (QRhiTexture* filtered = m_host->filterOutputTextureForLayer(F)) {
                afterInput = filtered;
            }
        }
    }
    m_host->m_layerFilterLastOut[F] = -1;

    // 4) Accumulate. Mode 2 = fade-only when no live content below.
    QRhiTexture* hist = historyReadTexture();
    if (!hist) {
        hist = m_blackTex.get();
    }
    const int accumMode = (belowWithSize == 0) ? 2 : 1;
    runFeedbackPass(r, cb, F, stagePx, clear, afterInput, hist, resultWriteRt(), accumMode);

    m_writeIdx = quint8(1 - m_writeIdx);
    m_primed = true;
}

void FeedbackLoop::advanceAfterSceneComposite(QRhi* /*r*/, QRhiCommandBuffer* /*cb*/,
                                              const QSize& /*stagePx*/)
{
    // Classic path advances ping-pong inside renderFrame.
}

} // namespace pvj::render
