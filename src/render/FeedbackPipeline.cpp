#include "FeedbackPipeline.h"

#include "RhiMixerWidget.h"

#include "core/FilterEffectIds.h"
#include "core/Model.h"

#include <rhi/qrhi.h>
#include <rhi/qshader.h>

#include <QFile>
#include <QList>
#include <QVector>

#include <algorithm>
#include <cstring>

namespace {

constexpr int kLayerCount = 14;
constexpr int kUboFloats = 4 + 3 * 4 * kLayerCount + 4;
constexpr int kUboSize = kUboFloats * int(sizeof(float));

QShader loadShader(const QString& resourcePath)
{
    QFile f(resourcePath);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning("FeedbackPipeline: cannot open shader %s", qUtf8Printable(resourcePath));
        return {};
    }
    return QShader::fromSerialized(f.readAll());
}

bool createMixerGraphicsPipeline(QRhi* r, QRhiShaderResourceBindings* srb,
                                 QRhiRenderPassDescriptor* rp,
                                 std::unique_ptr<QRhiGraphicsPipeline>& out,
                                 const QString& fragmentResource
                                 = QStringLiteral(":/shaders/mixer.frag.qsb"))
{
    QShader vs = loadShader(QStringLiteral(":/shaders/mixer.vert.qsb"));
    QShader fs = loadShader(fragmentResource);
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

bool createEffectPipeline(QRhi* r, const QString& fragmentResource, const QString& vertexResource,
                          QRhiShaderResourceBindings* srb, QRhiRenderPassDescriptor* rp,
                          std::unique_ptr<QRhiGraphicsPipeline>& out)
{
    QShader vs = loadShader(vertexResource);
    QShader fs = loadShader(fragmentResource);
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

bool createEffectPipeline(QRhi* r, const QString& fragmentResource, QRhiShaderResourceBindings* srb,
                          QRhiRenderPassDescriptor* rp, std::unique_ptr<QRhiGraphicsPipeline>& out)
{
    return createEffectPipeline(r, fragmentResource, QStringLiteral(":/shaders/textured_quad.vert.qsb"),
                                srb, rp, out);
}

} // namespace

namespace pvj::render {

FeedbackPipeline::FeedbackPipeline(RhiMixerWidget* host)
    : m_host(host)
{
    m_isFeedback[BackgroundLayerIndex] = false;
}

FeedbackPipeline::~FeedbackPipeline() = default;

namespace {

bool feedbackParamsEqual(const pvj::core::FeedbackParams& a, const pvj::core::FeedbackParams& b)
{
    return a.loopRetention == b.loopRetention && a.liveInject == b.liveInject
        && a.inSaturation == b.inSaturation && a.inBrightness == b.inBrightness
        && a.inContrast == b.inContrast && a.inHueShift == b.inHueShift && a.inGamma == b.inGamma
        && a.saturation == b.saturation && a.brightness == b.brightness && a.contrast == b.contrast
        && a.hueShift == b.hueShift && a.gamma == b.gamma && a.rotationDeg == b.rotationDeg
        && a.zoom == b.zoom && a.frameDelay == b.frameDelay && a.inputMode == b.inputMode
        && a.wrapMode == b.wrapMode;
}

} // namespace

void FeedbackPipeline::setLayer(int layer, bool enabled, const pvj::core::FeedbackParams& p)
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
    const bool resetRing = enabled && !wasEnabled;
    m_isFeedback[layer] = enabled;
    m_params[layer] = p;
    if (resetRing) {
        softResetRing();
    }
    recomputeActiveLayer();
    m_host->update();
}

bool FeedbackPipeline::isLayerFeedback(int layer) const
{
    if (layer < 0 || layer >= LayerCount) {
        return false;
    }
    return m_isFeedback[layer];
}

pvj::core::FeedbackParams FeedbackPipeline::layerParams(int layer) const
{
    if (layer < 0 || layer >= LayerCount) {
        return {};
    }
    return m_params[layer];
}

void FeedbackPipeline::notifyLayerActiveChanged()
{
    recomputeActiveLayer();
}

void FeedbackPipeline::onPreFeedbackTopologyChanged(int layer)
{
    if (layer == m_activeLayer || m_isFeedback[layer]) {
        softResetRing();
    }
}

void FeedbackPipeline::recomputeActiveLayer()
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

bool FeedbackPipeline::hasActive() const
{
    if (!m_host) {
        return false;
    }
    const int f = m_activeLayer;
    return f >= 0 && f < LayerCount && m_isFeedback[f] && m_host->m_layerActive[f];
}

int FeedbackPipeline::activeLayer() const
{
    return m_activeLayer;
}

pvj::core::FeedbackInputMode FeedbackPipeline::inputMode() const
{
    if (!hasActive()) {
        return pvj::core::FeedbackInputMode::SceneLoopback;
    }
    return m_params[m_activeLayer].inputMode;
}

bool FeedbackPipeline::keyFromAboveActive() const
{
    if (!hasActive() || !m_host) {
        return false;
    }
    const int f = m_activeLayer;
    for (int i = f + 1; i < LayerCount; ++i) {
        if (m_host->m_layerActive[i]) {
            return true;
        }
    }
    return false;
}

QRhiTexture* FeedbackPipeline::resultTexture() const
{
    return m_feedbackCompositeTex.get();
}

void FeedbackPipeline::softResetRing()
{
    m_feedbackWriteIdx = 0;
    m_feedbackRingFilled = 0;
    m_sceneHistPrimed = false;
}

void FeedbackPipeline::releaseGpuResources()
{
    m_feedbackPipeline.reset();
    m_feedbackHistPipeline.reset();
    m_feedbackHistSrb.reset();
    m_feedbackSrb.reset();
    m_feedbackUbuf.reset();
    m_feedbackRp.reset();
    for (auto& rt : m_feedbackRt) {
        rt.reset();
    }
    for (auto& tex : m_feedbackTex) {
        tex.reset();
    }

    m_mixerBelowPipeline.reset();
    m_belowSrb.reset();
    m_belowUbuf.reset();
    m_belowRt.reset();
    m_belowTex.reset();
    m_belowRp.reset();
    m_aboveRt.reset();
    m_aboveTex.reset();
    m_stackCombinePipeline.reset();
    m_stackCombineSrb.reset();
    m_stackRt.reset();
    m_stackTex.reset();

    for (auto& rt : m_sceneHistRt) {
        rt.reset();
    }
    for (auto& tex : m_sceneHistTex) {
        tex.reset();
    }

    m_feedbackAboveCoveragePipeline.reset();
    m_feedbackCoverageRt.reset();
    m_feedbackCoverageRp.reset();
    m_feedbackCoverageTex.reset();
    m_feedbackCompositePipeline.reset();
    m_feedbackCompositeRt.reset();
    m_feedbackCompositeRp.reset();
    m_feedbackCompositeSrb.reset();
    m_feedbackCompositeUbuf.reset();
    m_feedbackCompositeTex.reset();

    m_feedbackPixelSize = {};
    m_maskPixelSize = {};
    softResetRing();
    m_sceneHistWriteIdx = 0;
}

QRhiTexture* FeedbackPipeline::feedbackWriteTexture() const
{
    return m_feedbackTex[m_feedbackWriteIdx].get();
}

QRhiTexture* FeedbackPipeline::feedbackReadTexture() const
{
    if (!hasActive()) {
        return nullptr;
    }
    const int F = m_activeLayer;
    const int delay =
        qBound(0, m_params[F].frameDelay, pvj::core::kFeedbackMaxFrameDelay);
    const int maxSafeDelay = qMax(0, int(m_feedbackRingFilled) - 1);
    const int effectiveDelay = qMin(delay, maxSafeDelay);
    const int readIdx = (m_feedbackWriteIdx + kRingSize - 1 - effectiveDelay) % kRingSize;
    return m_feedbackTex[readIdx].get();
}

QRhiTextureRenderTarget* FeedbackPipeline::feedbackWriteRenderTarget() const
{
    return m_feedbackRt[m_feedbackWriteIdx].get();
}

QRhiTexture* FeedbackPipeline::sceneHistReadTexture() const
{
    return m_sceneHistTex[1 - m_sceneHistWriteIdx].get();
}

QRhiTextureRenderTarget* FeedbackPipeline::sceneHistWriteRenderTarget() const
{
    return m_sceneHistRt[m_sceneHistWriteIdx].get();
}

bool FeedbackPipeline::ensureTargets(QRhi* r, const QSize& pixelSize)
{
    if (!m_host || !r || pixelSize.isEmpty() || !hasActive()) {
        return false;
    }
    if (m_feedbackPixelSize != pixelSize) {
        releaseGpuResources();
        m_feedbackPixelSize = pixelSize;
    }

    if (!m_belowTex) {
        m_belowTex.reset(r->newTexture(QRhiTexture::RGBA8, pixelSize, 1, QRhiTexture::RenderTarget));
        if (!m_belowTex->create()) {
            return false;
        }
    }
    if (!m_belowRt) {
        m_belowRt.reset(r->newTextureRenderTarget(
            QRhiTextureRenderTargetDescription(QRhiColorAttachment(m_belowTex.get()))));
        if (!m_belowRp) {
            m_belowRp.reset(m_belowRt->newCompatibleRenderPassDescriptor());
        }
        m_belowRt->setRenderPassDescriptor(m_belowRp.get());
        if (!m_belowRt->create()) {
            return false;
        }
    }

    if (!m_aboveTex) {
        m_aboveTex.reset(r->newTexture(QRhiTexture::RGBA8, pixelSize, 1, QRhiTexture::RenderTarget));
        if (!m_aboveTex->create()) {
            return false;
        }
    }
    if (!m_aboveRt) {
        m_aboveRt.reset(r->newTextureRenderTarget(
            QRhiTextureRenderTargetDescription(QRhiColorAttachment(m_aboveTex.get()))));
        if (!m_belowRp) {
            m_belowRp.reset(m_aboveRt->newCompatibleRenderPassDescriptor());
        }
        m_aboveRt->setRenderPassDescriptor(m_belowRp.get());
        if (!m_aboveRt->create()) {
            return false;
        }
    }

    if (!m_stackTex) {
        m_stackTex.reset(r->newTexture(QRhiTexture::RGBA8, pixelSize, 1, QRhiTexture::RenderTarget));
        if (!m_stackTex->create()) {
            return false;
        }
    }
    if (!m_stackRt) {
        m_stackRt.reset(r->newTextureRenderTarget(
            QRhiTextureRenderTargetDescription(QRhiColorAttachment(m_stackTex.get()))));
        m_stackRt->setRenderPassDescriptor(m_belowRp.get());
        if (!m_stackRt->create()) {
            return false;
        }
    }

    for (int k = 0; k < kRingSize; ++k) {
        if (!m_feedbackTex[k]) {
            m_feedbackTex[k].reset(
                r->newTexture(QRhiTexture::RGBA8, pixelSize, 1, QRhiTexture::RenderTarget));
            if (!m_feedbackTex[k]->create()) {
                return false;
            }
        }
        if (!m_feedbackRt[k]) {
            m_feedbackRt[k].reset(r->newTextureRenderTarget(
                QRhiTextureRenderTargetDescription(QRhiColorAttachment(m_feedbackTex[k].get()))));
            if (!m_feedbackRp) {
                m_feedbackRp.reset(m_feedbackRt[k]->newCompatibleRenderPassDescriptor());
            }
            m_feedbackRt[k]->setRenderPassDescriptor(m_feedbackRp.get());
            if (!m_feedbackRt[k]->create()) {
                return false;
            }
        }
    }

    for (int k = 0; k < 2; ++k) {
        if (!m_sceneHistTex[k]) {
            m_sceneHistTex[k].reset(
                r->newTexture(QRhiTexture::RGBA8, pixelSize, 1, QRhiTexture::RenderTarget));
            if (!m_sceneHistTex[k]->create()) {
                return false;
            }
        }
        if (!m_sceneHistRt[k]) {
            m_sceneHistRt[k].reset(r->newTextureRenderTarget(
                QRhiTextureRenderTargetDescription(QRhiColorAttachment(m_sceneHistTex[k].get()))));
            m_sceneHistRt[k]->setRenderPassDescriptor(m_belowRp.get());
            if (!m_sceneHistRt[k]->create()) {
                return false;
            }
        }
    }

    if (!m_belowUbuf) {
        m_belowUbuf.reset(r->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, kUboSize));
        if (!m_belowUbuf->create()) {
            return false;
        }
    }
    if (!m_belowSrb) {
        m_belowSrb.reset(r->newShaderResourceBindings());
    }
    if (!m_stackCombineSrb) {
        m_stackCombineSrb.reset(r->newShaderResourceBindings());
    }
    if (!m_feedbackUbuf) {
        m_feedbackUbuf.reset(r->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 80));
        if (!m_feedbackUbuf->create()) {
            return false;
        }
    }
    if (!m_feedbackSrb) {
        m_feedbackSrb.reset(r->newShaderResourceBindings());
    }
    if (!m_feedbackHistSrb) {
        m_feedbackHistSrb.reset(r->newShaderResourceBindings());
    }

    rebuildBelowMixerShaderResourceBindings();
    rebuildStackCombineShaderResourceBindings();

    if (!m_mixerBelowPipeline) {
        if (!createMixerGraphicsPipeline(r, m_belowSrb.get(), m_belowRp.get(),
                                         m_mixerBelowPipeline)) {
            return false;
        }
    }
    if (!m_stackCombinePipeline) {
        if (!createEffectPipeline(r, QStringLiteral(":/shaders/stack_combine.frag.qsb"),
                                  QStringLiteral(":/shaders/layer_feedback.vert.qsb"),
                                  m_stackCombineSrb.get(), m_belowRp.get(),
                                  m_stackCombinePipeline)) {
            return false;
        }
    }
    if (!m_feedbackPipeline) {
        if (!createEffectPipeline(r, QStringLiteral(":/shaders/layer_feedback.frag.qsb"),
                                  QStringLiteral(":/shaders/layer_feedback.vert.qsb"),
                                  m_feedbackSrb.get(), m_feedbackRp.get(), m_feedbackPipeline)) {
            return false;
        }
    }
    if (!m_feedbackHistPipeline) {
        QRhiRenderPassDescriptor* histRp = m_belowRp ? m_belowRp.get() : m_feedbackRp.get();
        if (!createEffectPipeline(r, QStringLiteral(":/shaders/layer_feedback_hist.frag.qsb"),
                                  QStringLiteral(":/shaders/layer_feedback.vert.qsb"),
                                  m_feedbackHistSrb.get(), histRp, m_feedbackHistPipeline)) {
            return false;
        }
    }

    return true;
}

void FeedbackPipeline::rebuildBelowMixerShaderResourceBindings()
{
    if (!m_belowSrb || !m_host) {
        return;
    }

    QVector<QRhiShaderResourceBinding> binds;
    binds.reserve(2 + (LayerCount - 1) + 2);
    binds.append(QRhiShaderResourceBinding::uniformBuffer(
        0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
        m_belowUbuf.get()));
    for (int gpuLayer = RhiMixerWidget::UserLayerMin; gpuLayer < LayerCount; ++gpuLayer) {
        QRhiTexture* src = m_host->sourceTextureForLayer(gpuLayer);
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

void FeedbackPipeline::rebuildStackCombineShaderResourceBindings()
{
    if (!m_stackCombineSrb || !m_belowTex || !m_aboveTex || !m_host) {
        return;
    }
    m_stackCombineSrb->setBindings({
        QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage,
                                                  m_belowTex.get(), m_host->m_sampler.get()),
        QRhiShaderResourceBinding::sampledTexture(2, QRhiShaderResourceBinding::FragmentStage,
                                                  m_aboveTex.get(), m_host->m_sampler.get()),
    });
    m_stackCombineSrb->create();
}

void FeedbackPipeline::updateBelowMixerUniformBuffer(QRhiResourceUpdateBatch* batch,
                                                     int minLayerInclusive, int maxLayerExclusive)
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
        if (!m_host->m_layerActive[i]) {
            continue;
        }
        if (m_host->m_sizes[i].isEmpty()) {
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

    float ubo[kUboFloats];
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
        ubo[base + 2] = m_host->m_layerActive[i] ? 1.0f : 0.0f;
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

    batch->updateDynamicBuffer(m_belowUbuf.get(), 0, sizeof(ubo), ubo);
}

void FeedbackPipeline::runPartialMixerPass(QRhi* r, QRhiCommandBuffer* cb, int minLayerInclusive,
                                           int maxLayerExclusive, QRhiTextureRenderTarget* targetRt,
                                           const QSize& stagePx, const QColor& clear)
{
    if (!r || !cb || !targetRt || !m_mixerBelowPipeline || !m_belowSrb || !m_host) {
        return;
    }
    rebuildBelowMixerShaderResourceBindings();
    QRhiResourceUpdateBatch* batch = r->nextResourceUpdateBatch();
    updateBelowMixerUniformBuffer(batch, minLayerInclusive, maxLayerExclusive);
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

void FeedbackPipeline::runStackCombinePass(QRhi* r, QRhiCommandBuffer* cb, const QSize& stagePx,
                                           const QColor& clear)
{
    if (!r || !cb || !m_stackCombinePipeline || !m_stackRt || !m_host) {
        return;
    }
    rebuildStackCombineShaderResourceBindings();
    cb->beginPass(m_stackRt.get(), clear, { 1.0f, 0 }, nullptr);
    cb->setGraphicsPipeline(m_stackCombinePipeline.get());
    cb->setViewport(QRhiViewport(0, 0, stagePx.width(), stagePx.height()));
    cb->setShaderResources(m_stackCombineSrb.get());
    QRhiCommandBuffer::VertexInput vin(m_host->m_vbuf.get(), 0);
    cb->setVertexInput(0, 1, &vin);
    cb->draw(4);
    cb->endPass();
}

QRhiTexture* FeedbackPipeline::feedbackInjectSourceTexture(QRhi* r, QRhiCommandBuffer* cb,
                                                           int feedbackLayer, const QSize& stagePx,
                                                           const QColor& clear)
{
    if (!r || !cb || !m_belowRt || !m_host || feedbackLayer < 0 || feedbackLayer >= LayerCount) {
        return nullptr;
    }

    const auto mode = inputMode();
    const bool hasLayersBelow = std::any_of(
        m_host->m_layerActive.cbegin(), m_host->m_layerActive.cbegin() + feedbackLayer,
        [](bool active) { return active; });
    const bool hasLayersAbove = std::any_of(
        m_host->m_layerActive.cbegin() + feedbackLayer + 1, m_host->m_layerActive.cend(),
        [](bool active) { return active; });

    if (mode == pvj::core::FeedbackInputMode::BelowOnly) {
        runPartialMixerPass(r, cb, 0, feedbackLayer, m_belowRt.get(), stagePx, clear);
        return m_belowTex.get();
    }

    if (hasLayersBelow || hasLayersAbove) {
        runPartialMixerPass(r, cb, 0, feedbackLayer, m_belowRt.get(), stagePx, clear);
        if (hasLayersAbove && m_aboveRt) {
            runPartialMixerPass(r, cb, feedbackLayer + 1, LayerCount, m_aboveRt.get(), stagePx,
                                clear);
            runStackCombinePass(r, cb, stagePx, clear);
            return m_stackTex.get();
        }
        return m_belowTex.get();
    }

    if (mode == pvj::core::FeedbackInputMode::SceneLoopback && m_sceneHistPrimed) {
        if (QRhiTexture* sceneRead = sceneHistReadTexture()) {
            return sceneRead;
        }
    }

    runPartialMixerPass(r, cb, 0, feedbackLayer, m_belowRt.get(), stagePx, clear);
    return m_belowTex.get();
}

void FeedbackPipeline::rebuildFeedbackShaderResourceBindings(QRhiTexture* belowTex,
                                                             QRhiTexture* historyRead)
{
    if (!m_feedbackSrb || !belowTex || !historyRead || !m_host) {
        return;
    }
    m_feedbackSrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::FragmentStage,
                                                 m_feedbackUbuf.get()),
        QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage,
                                                  belowTex, m_host->m_sampler.get()),
        QRhiShaderResourceBinding::sampledTexture(2, QRhiShaderResourceBinding::FragmentStage,
                                                  historyRead, m_host->m_sampler.get()),
    });
    m_feedbackSrb->create();
}

void FeedbackPipeline::rebuildFeedbackHistShaderResourceBindings(QRhiTexture* historyRead)
{
    if (!m_feedbackHistSrb || !historyRead || !m_host) {
        return;
    }
    m_feedbackHistSrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::FragmentStage,
                                                 m_feedbackUbuf.get()),
        QRhiShaderResourceBinding::sampledTexture(2, QRhiShaderResourceBinding::FragmentStage,
                                                  historyRead, m_host->m_sampler.get()),
    });
    m_feedbackHistSrb->create();
}

void FeedbackPipeline::updateFeedbackUniformBuffer(QRhiResourceUpdateBatch* batch, int feedbackLayer)
{
    if (!batch || !m_feedbackUbuf || !m_host || feedbackLayer < 0 || feedbackLayer >= LayerCount) {
        return;
    }
    const auto& fb = m_params[feedbackLayer];
    const float ubo[20] = {
        float(fb.loopRetention),
        float(fb.saturation),
        float(fb.brightness),
        float(fb.contrast),
        float(fb.hueShift),
        float(fb.gamma),
        float(fb.rotationDeg),
        float(fb.zoom),
        0.5f,
        0.5f,
        float(fb.liveInject),
        float(static_cast<int>(fb.wrapMode)),
        float(fb.inBrightness),
        float(fb.inContrast),
        float(fb.inSaturation),
        float(fb.inHueShift),
        float(fb.inGamma),
        m_host->m_opacity[feedbackLayer],
        0.f,
        0.f,
    };
    batch->updateDynamicBuffer(m_feedbackUbuf.get(), 0, sizeof(ubo), ubo);
}

void FeedbackPipeline::runFeedbackPass(QRhi* r, QRhiCommandBuffer* cb, int feedbackLayer,
                                       const QSize& stagePx, const QColor& clear,
                                       QRhiTexture* belowTex, QRhiTexture* historyRead,
                                       QRhiTextureRenderTarget* writeRt)
{
    if (!r || !cb || !m_feedbackPipeline || !belowTex || !historyRead || !writeRt || !m_host) {
        return;
    }

    rebuildFeedbackShaderResourceBindings(belowTex, historyRead);

    QRhiResourceUpdateBatch* batch = r->nextResourceUpdateBatch();
    updateFeedbackUniformBuffer(batch, feedbackLayer);
    cb->resourceUpdate(batch);

    cb->beginPass(writeRt, clear, { 1.0f, 0 }, nullptr);
    cb->setGraphicsPipeline(m_feedbackPipeline.get());
    cb->setViewport(QRhiViewport(0, 0, stagePx.width(), stagePx.height()));
    cb->setShaderResources(m_feedbackSrb.get());
    QRhiCommandBuffer::VertexInput vin(m_host->m_vbuf.get(), 0);
    cb->setVertexInput(0, 1, &vin);
    cb->draw(4);
    cb->endPass();
}

void FeedbackPipeline::runFeedbackHistDisplayPass(QRhi* r, QRhiCommandBuffer* cb, int feedbackLayer,
                                                  const QSize& stagePx, const QColor& clear,
                                                  QRhiTexture* historyRead)
{
    if (!r || !cb || !m_feedbackHistPipeline || !historyRead || !m_stackRt || !m_host) {
        return;
    }

    rebuildFeedbackHistShaderResourceBindings(historyRead);

    QRhiResourceUpdateBatch* batch = r->nextResourceUpdateBatch();
    updateFeedbackUniformBuffer(batch, feedbackLayer);
    cb->resourceUpdate(batch);

    cb->beginPass(m_stackRt.get(), clear, { 1.0f, 0 }, nullptr);
    cb->setGraphicsPipeline(m_feedbackHistPipeline.get());
    cb->setViewport(QRhiViewport(0, 0, stagePx.width(), stagePx.height()));
    cb->setShaderResources(m_feedbackHistSrb.get());
    QRhiCommandBuffer::VertexInput vin(m_host->m_vbuf.get(), 0);
    cb->setVertexInput(0, 1, &vin);
    cb->draw(4);
    cb->endPass();
}

void FeedbackPipeline::runFeedbackAccumulationStep(QRhi* r, QRhiCommandBuffer* cb, int feedbackLayer,
                                                   const QSize& stagePx, const QColor& clear,
                                                   QRhiTexture* filteredBelow)
{
    if (!r || !cb || !filteredBelow || !m_host) {
        return;
    }
    QRhiTexture* historyRead = feedbackReadTexture();
    QRhiTextureRenderTarget* writeRt = feedbackWriteRenderTarget();
    if (!historyRead || !writeRt) {
        return;
    }
    runFeedbackPass(r, cb, feedbackLayer, stagePx, clear, filteredBelow, historyRead, writeRt);
    m_host->m_layerFilterLastOut[feedbackLayer] = -1;
}

void FeedbackPipeline::applyFeedbackPostFilters(QRhi* r, QRhiCommandBuffer* cb, int feedbackLayer,
                                                const QSize& stagePx, const QColor& clear,
                                                const QList<pvj::core::CellFilterNode>& postChain)
{
    if (!r || !cb || !m_host || postChain.isEmpty() || !m_stackTex || !m_stackRt) {
        return;
    }
    if (!m_host->m_layerFilterPingRt[feedbackLayer][0] || !m_host->m_layerFilterPingRt[feedbackLayer][1]
        || !m_host->m_layerFilterPingTex[feedbackLayer][1]) {
        return;
    }
    m_host->runTextureCopyPass(r, cb, m_stackTex.get(),
                               m_host->m_layerFilterPingRt[feedbackLayer][1].get(), stagePx);
    m_host->runPerLayerFilterChain(r, cb, feedbackLayer,
                                   m_host->m_layerFilterPingTex[feedbackLayer][1].get(), stagePx,
                                   clear, &postChain);
    if (QRhiTexture* filtered = m_host->filterOutputTextureForLayer(feedbackLayer)) {
        m_host->runTextureCopyPass(r, cb, filtered, m_stackRt.get(), stagePx);
    }
    m_host->m_layerFilterLastOut[feedbackLayer] = -1;
}

void FeedbackPipeline::primeFeedbackRingForDelay(QRhi* r, QRhiCommandBuffer* cb, int feedbackLayer,
                                                 const QSize& stagePx, const QColor& clear,
                                                 QRhiTexture* filteredBelow)
{
    const int targetFilled = pvj::core::kFeedbackRingPrimeCount;
    while (m_feedbackRingFilled < targetFilled) {
        runFeedbackAccumulationStep(r, cb, feedbackLayer, stagePx, clear, filteredBelow);
        advanceFeedbackRingSlot();
    }
}

void FeedbackPipeline::advanceFeedbackRingSlot()
{
    m_feedbackWriteIdx = quint8((m_feedbackWriteIdx + 1) % kRingSize);
    if (m_feedbackRingFilled < kRingSize) {
        ++m_feedbackRingFilled;
    }
}

void FeedbackPipeline::clearFeedbackHistoryRing(QRhi* r, QRhiCommandBuffer* cb, const QSize& stagePx)
{
    softResetRing();
    if (!r || !cb || stagePx.isEmpty()) {
        return;
    }
    if (!ensureTargets(r, stagePx)) {
        return;
    }
    const QColor black(0, 0, 0);
    for (int k = 0; k < kRingSize; ++k) {
        if (m_feedbackRt[k]) {
            cb->beginPass(m_feedbackRt[k].get(), black, { 1.0f, 0 }, nullptr);
            cb->endPass();
        }
    }
    if (m_stackRt) {
        cb->beginPass(m_stackRt.get(), black, { 1.0f, 0 }, nullptr);
        cb->endPass();
    }
}

void FeedbackPipeline::copySceneToHistory(QRhi* r, QRhiCommandBuffer* cb, const QSize& stagePx)
{
    if (!m_host || !m_host->m_sceneTex || !sceneHistWriteRenderTarget()) {
        return;
    }
    m_host->runTextureCopyPass(r, cb, m_host->m_sceneTex.get(), sceneHistWriteRenderTarget(),
                               stagePx);
    m_sceneHistPrimed = true;
}

void FeedbackPipeline::renderFrame(QRhi* r, QRhiCommandBuffer* cb, const QSize& stagePx,
                                   const QColor& clear)
{
    if (!m_host || !r || !cb) {
        return;
    }
    if (!hasActive()) {
        return;
    }

    const int F = m_activeLayer;
    if (!ensureTargets(r, stagePx)) {
        return;
    }
    ensureMaskResources(r, stagePx);

    QRhiTexture* feedbackBelow = feedbackInjectSourceTexture(r, cb, F, stagePx, clear);

    QList<pvj::core::CellFilterNode> preChain;
    QList<pvj::core::CellFilterNode> postChain;
    pvj::core::splitFilterChainAtFeedbackMarker(m_host->m_layerFilterChain[F], &preChain, &postChain);
    if (!preChain.isEmpty() || !postChain.isEmpty()) {
        m_host->ensureLayerFilterTargets(r, stagePx);
    }

    const float feedbackOpacity = m_host->m_opacity[F];
    const bool accumulateFeedback = feedbackOpacity > 1e-4f;

    if (!accumulateFeedback) {
        clearFeedbackHistoryRing(r, cb, stagePx);
        m_host->m_layerFilterLastOut[F] = -1;
        clearMaskTextures(r, cb, stagePx);
        return;
    }

    QRhiTexture* filteredBelow = feedbackBelow;
    if (feedbackBelow && !preChain.isEmpty() && m_host->m_layerFilterPingRt[F][0]
        && m_host->m_layerFilterPingRt[F][1]) {
        m_host->runPerLayerFilterChain(r, cb, F, feedbackBelow, stagePx, clear, &preChain);
        if (QRhiTexture* preFiltered = m_host->filterOutputTextureForLayer(F)) {
            filteredBelow = preFiltered;
        }
        m_host->m_layerFilterLastOut[F] = -1;
    }

    if (!filteredBelow) {
        return;
    }

    primeFeedbackRingForDelay(r, cb, F, stagePx, clear, filteredBelow);
    runFeedbackAccumulationStep(r, cb, F, stagePx, clear, filteredBelow);

    if (QRhiTexture* histSrc = feedbackWriteTexture()) {
        runFeedbackHistDisplayPass(r, cb, F, stagePx, clear, histSrc);
    }
    if (!postChain.isEmpty()) {
        applyFeedbackPostFilters(r, cb, F, stagePx, clear, postChain);
    }

    const bool applyMask =
        inputMode() == pvj::core::FeedbackInputMode::BelowOnly && keyFromAboveActive();
    if (applyMask) {
        runFeedbackAboveCoveragePass(r, cb, F, stagePx, clear);
    }
    runFeedbackLayerCompositePass(r, cb, F, stagePx, clear, applyMask);
}

void FeedbackPipeline::advanceAfterSceneComposite(QRhi* r, QRhiCommandBuffer* cb,
                                                  const QSize& stagePx)
{
    if (!m_host || !r || !cb) {
        return;
    }
    if (!hasActive()) {
        return;
    }

    const int F = m_activeLayer;
    const bool accumulateFeedback =
        F >= 0 && F < LayerCount && m_host->m_opacity[F] > 1e-4f;
    if (!accumulateFeedback) {
        return;
    }

    copySceneToHistory(r, cb, stagePx);
    advanceFeedbackRingSlot();
    m_sceneHistWriteIdx = quint8(1 - m_sceneHistWriteIdx);
}

void FeedbackPipeline::ensureMaskResources(QRhi* r, const QSize& pixelSize)
{
    if (!m_host || !r || pixelSize.isEmpty() || !m_stackTex || !m_belowSrb) {
        return;
    }
    if (m_maskPixelSize == pixelSize && m_feedbackCompositePipeline && m_feedbackAboveCoveragePipeline
        && m_feedbackCoverageRt && m_feedbackCompositeRt) {
        return;
    }

    m_maskPixelSize = pixelSize;
    m_feedbackCoverageTex.reset();
    m_feedbackCoverageRt.reset();
    m_feedbackCoverageRp.reset();
    m_feedbackAboveCoveragePipeline.reset();
    m_feedbackCompositeTex.reset();
    m_feedbackCompositeRt.reset();
    m_feedbackCompositeRp.reset();
    m_feedbackCompositeSrb.reset();
    m_feedbackCompositeUbuf.reset();
    m_feedbackCompositePipeline.reset();

    m_feedbackCoverageTex.reset(
        r->newTexture(QRhiTexture::RGBA8, pixelSize, 1, QRhiTexture::RenderTarget));
    if (!m_feedbackCoverageTex->create()) {
        return;
    }
    m_feedbackCoverageRt.reset(r->newTextureRenderTarget(
        QRhiTextureRenderTargetDescription(QRhiColorAttachment(m_feedbackCoverageTex.get()))));
    m_feedbackCoverageRp.reset(m_feedbackCoverageRt->newCompatibleRenderPassDescriptor());
    m_feedbackCoverageRt->setRenderPassDescriptor(m_feedbackCoverageRp.get());
    if (!m_feedbackCoverageRt->create()) {
        return;
    }

    m_feedbackCompositeTex.reset(
        r->newTexture(QRhiTexture::RGBA8, pixelSize, 1, QRhiTexture::RenderTarget));
    if (!m_feedbackCompositeTex->create()) {
        return;
    }
    m_feedbackCompositeRt.reset(r->newTextureRenderTarget(
        QRhiTextureRenderTargetDescription(QRhiColorAttachment(m_feedbackCompositeTex.get()))));
    m_feedbackCompositeRp.reset(m_feedbackCompositeRt->newCompatibleRenderPassDescriptor());
    m_feedbackCompositeRt->setRenderPassDescriptor(m_feedbackCompositeRp.get());
    if (!m_feedbackCompositeRt->create()) {
        return;
    }

    m_feedbackCompositeUbuf.reset(r->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 16));
    if (!m_feedbackCompositeUbuf->create()) {
        return;
    }
    m_feedbackCompositeSrb.reset(r->newShaderResourceBindings());
    m_feedbackCompositeSrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::FragmentStage,
                                                 m_feedbackCompositeUbuf.get()),
        QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage,
                                                  m_stackTex.get(), m_host->m_sampler.get()),
        QRhiShaderResourceBinding::sampledTexture(2, QRhiShaderResourceBinding::FragmentStage,
                                                  m_feedbackCoverageTex.get(),
                                                  m_host->m_sampler.get()),
    });
    m_feedbackCompositeSrb->create();

    if (!createMixerGraphicsPipeline(r, m_belowSrb.get(), m_feedbackCoverageRp.get(),
                                     m_feedbackAboveCoveragePipeline,
                                     QStringLiteral(":/shaders/feedback_above_coverage.frag.qsb"))) {
        return;
    }
    if (!createEffectPipeline(r, QStringLiteral(":/shaders/feedback_layer_composite.frag.qsb"),
                              QStringLiteral(":/shaders/layer_feedback.vert.qsb"),
                              m_feedbackCompositeSrb.get(), m_feedbackCompositeRp.get(),
                              m_feedbackCompositePipeline)) {
        return;
    }
}

void FeedbackPipeline::clearMaskTextures(QRhi* /*r*/, QRhiCommandBuffer* cb, const QSize& stagePx)
{
    if (!cb || !stagePx.isValid() || stagePx.isEmpty()) {
        return;
    }
    if (!m_feedbackCoverageRt || !m_feedbackCompositeRt) {
        return;
    }
    const QColor black(0, 0, 0);
    cb->beginPass(m_feedbackCoverageRt.get(), black, { 1.0f, 0 }, nullptr);
    cb->endPass();
    cb->beginPass(m_feedbackCompositeRt.get(), black, { 1.0f, 0 }, nullptr);
    cb->endPass();
}

void FeedbackPipeline::runFeedbackAboveCoveragePass(QRhi* r, QRhiCommandBuffer* cb,
                                                    int feedbackLayer, const QSize& stagePx,
                                                    const QColor& clear)
{
    if (!r || !cb || !m_feedbackAboveCoveragePipeline || !m_feedbackCoverageRt || !m_belowSrb
        || !m_host) {
        return;
    }
    if (feedbackLayer < 0 || feedbackLayer >= LayerCount - 1) {
        return;
    }
    const int minLayerInclusive = feedbackLayer + 1;
    const int maxLayerExclusive = LayerCount;
    if (minLayerInclusive >= maxLayerExclusive) {
        return;
    }

    rebuildBelowMixerShaderResourceBindings();

    QRhiResourceUpdateBatch* batch = r->nextResourceUpdateBatch();
    updateBelowMixerUniformBuffer(batch, minLayerInclusive, maxLayerExclusive);
    cb->resourceUpdate(batch);

    cb->beginPass(m_feedbackCoverageRt.get(), clear, { 1.0f, 0 }, nullptr);
    cb->setGraphicsPipeline(m_feedbackAboveCoveragePipeline.get());
    cb->setViewport(QRhiViewport(0, 0, stagePx.width(), stagePx.height()));
    cb->setShaderResources(m_belowSrb.get());
    QRhiCommandBuffer::VertexInput vin(m_host->m_vbuf.get(), 0);
    cb->setVertexInput(0, 1, &vin);
    cb->draw(4);
    cb->endPass();
}

void FeedbackPipeline::runFeedbackLayerCompositePass(QRhi* r, QRhiCommandBuffer* cb,
                                                     int /*feedbackLayer*/, const QSize& stagePx,
                                                     const QColor& clear, bool applyMask)
{
    if (!r || !cb || !m_feedbackCompositePipeline || !m_feedbackCompositeRt || !m_feedbackCompositeSrb
        || !m_feedbackCompositeUbuf || !m_host) {
        return;
    }

    const float ubo[4] = { applyMask ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f };
    QRhiResourceUpdateBatch* batch = r->nextResourceUpdateBatch();
    batch->updateDynamicBuffer(m_feedbackCompositeUbuf.get(), 0, sizeof(ubo), ubo);
    cb->resourceUpdate(batch);

    cb->beginPass(m_feedbackCompositeRt.get(), clear, { 1.0f, 0 }, nullptr);
    cb->setGraphicsPipeline(m_feedbackCompositePipeline.get());
    cb->setViewport(QRhiViewport(0, 0, stagePx.width(), stagePx.height()));
    cb->setShaderResources(m_feedbackCompositeSrb.get());
    QRhiCommandBuffer::VertexInput vin(m_host->m_vbuf.get(), 0);
    cb->setVertexInput(0, 1, &vin);
    cb->draw(4);
    cb->endPass();
}

} // namespace pvj::render
