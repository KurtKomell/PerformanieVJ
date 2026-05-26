#include "RhiMixerWidget.h"
#include "FilterUniformPacker.h"
#include "ShaderLibrary.h"
#include "maxine/MaxineFilterBackend.h"
#include "maxine/MaxineTextureBridge.h"

#include "core/FilterEffectIds.h"
#include "core/FilterParamSchema.h"

#include <rhi/qshader.h>
#include <rhi/qrhi.h>

#include <QFile>
#include <QImage>
#include <QPainter>
#include <QPaintEvent>
#include <QVector>

#include <algorithm>
#include <cstring>
#include <optional>

namespace pvj::render {

namespace {

bool pictureParamsEqual(const pvj::core::PictureParams& a, const pvj::core::PictureParams& b)
{
    return a.zoom == b.zoom && a.rotationDeg == b.rotationDeg && a.brightness == b.brightness
        && a.contrast == b.contrast && a.saturation == b.saturation
        && a.circularMotion == b.circularMotion && a.wrapMode == b.wrapMode;
}

bool filterChainShallowEqual(const QList<pvj::core::CellFilterNode>& a,
                             const QList<pvj::core::CellFilterNode>& b)
{
    if (a.size() != b.size()) {
        return false;
    }
    auto paramValue = [](const pvj::core::CellFilterNode& node, const QString& name) -> std::optional<double> {
        for (const auto& p : node.params) {
            if (p.name == name) {
                return p.value;
            }
        }
        return std::nullopt;
    };
    for (int i = 0; i < a.size(); ++i) {
        if (a.at(i).typeId != b.at(i).typeId) {
            return false;
        }
        const auto& pa = a.at(i).params;
        const auto& pb = b.at(i).params;
        if (pa.size() != pb.size()) {
            return false;
        }
        for (const auto& p : pa) {
            const auto bv = paramValue(b.at(i), p.name);
            if (!bv.has_value()) {
                return false;
            }
            if (qAbs(p.value - *bv) > 1e-9) {
                return false;
            }
        }
    }
    return true;
}

bool feedbackParamsEqual(const pvj::core::FeedbackParams& a, const pvj::core::FeedbackParams& b)
{
    return a.loopRetention == b.loopRetention && a.liveInject == b.liveInject
        && a.saturation == b.saturation
        && a.brightness == b.brightness && a.contrast == b.contrast
        && a.hueShift == b.hueShift && a.gamma == b.gamma
        && a.rotationDeg == b.rotationDeg && a.zoom == b.zoom
        && a.frameDelay == b.frameDelay
        && a.inputMode == b.inputMode && a.wrapMode == b.wrapMode;
}

constexpr float kQuadVertices[] = {
    -1.0f, -1.0f,  0.0f, 1.0f,
     1.0f, -1.0f,  1.0f, 1.0f,
    -1.0f,  1.0f,  0.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 0.0f,
};

// std140 layout (see mixer.vert / mixer.frag Block):
//   vec4 scaleOffset
//   vec4 layers[12]
//   vec4 picUvA[12]
//   vec4 picColor[12]
//   vec4 mixerCfg
// = 38 vec4. Keep in sync with the shader.
static constexpr int kUboFloats = 4 + 3 * 4 * RhiMixerWidget::LayerCount + 4;
static constexpr int kUboSize   = kUboFloats * int(sizeof(float));

QShader loadShader(const QString& resourcePath)
{
    QFile f(resourcePath);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning("RhiMixerWidget: cannot open shader %s",
                 qUtf8Printable(resourcePath));
        return {};
    }
    return QShader::fromSerialized(f.readAll());
}

bool createMixerGraphicsPipeline(QRhi* r,
                                 QRhiShaderResourceBindings* srb,
                                 QRhiRenderPassDescriptor* rp,
                                 std::unique_ptr<QRhiGraphicsPipeline>& out,
                                 const QString& fragmentResource = QStringLiteral(":/shaders/mixer.frag.qsb"))
{
    QShader vs = loadShader(QStringLiteral(":/shaders/mixer.vert.qsb"));
    QShader fs = loadShader(fragmentResource);
    if (!vs.isValid() || !fs.isValid()) {
        qWarning("RhiMixerWidget: mixer shader modules missing from resources");
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
        { QRhiShaderStage::Vertex,   vs },
        { QRhiShaderStage::Fragment, fs },
    });
    out->setVertexInputLayout(layout);
    out->setShaderResourceBindings(srb);
    out->setRenderPassDescriptor(rp);
    if (!out->create()) {
        // A failed pipeline object must not be left non-null: callers cache the
        // pointer and may skip recreate (e.g. ensureOffscreenSceneTargets early exit).
        out.reset();
        return false;
    }
    return true;
}

bool createTexturedPresentPipeline(QRhi* r,
                                   QRhiShaderResourceBindings* srb,
                                   QRhiRenderPassDescriptor* rp,
                                   std::unique_ptr<QRhiGraphicsPipeline>& out)
{
    QShader vs = loadShader(QStringLiteral(":/shaders/textured_quad.vert.qsb"));
    QShader fs = loadShader(QStringLiteral(":/shaders/textured_quad.frag.qsb"));
    if (!vs.isValid() || !fs.isValid()) {
        qWarning("RhiMixerWidget: textured_quad shaders missing");
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
        { QRhiShaderStage::Vertex,   vs },
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

bool createEffectPipeline(QRhi* r,
                          const QString& fragmentResource,
                          const QString& vertexResource,
                          QRhiShaderResourceBindings* srb,
                          QRhiRenderPassDescriptor* rp,
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

bool createEffectPipeline(QRhi* r,
                          const QString& fragmentResource,
                          QRhiShaderResourceBindings* srb,
                          QRhiRenderPassDescriptor* rp,
                          std::unique_ptr<QRhiGraphicsPipeline>& out)
{
    return createEffectPipeline(r, fragmentResource, QStringLiteral(":/shaders/textured_quad.vert.qsb"),
                                srb, rp, out);
}

} // namespace

RhiMixerWidget::RhiMixerWidget(QWidget* parent)
    : QRhiWidget(parent)
{
#if defined(Q_OS_WIN)
    // Maxine readback uses D3D11 texture staging; default QRhi backend may be OpenGL.
    setApi(QRhiWidget::Api::Direct3D11);
#endif
    setMinimumSize(320, 180);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setAutoRenderTarget(true);

    for (int i = 0; i < LayerCount; ++i) {
        m_opacity[i]     = 1.0f;
        m_copyMode[i]    = pvj::core::CopyMode::Normal;
        m_layerActive[i] = false;
        m_layerFilterLastOut[i] = -1;
        m_layerMatteRole[i] = pvj::core::LayerMatteRole::None;
        m_layerKeyChannel[static_cast<size_t>(i)] = { 1.0f, 1.0f, 1.0f };
    }
    applyBackgroundLayerState();
    m_elapsed.start();
}

void RhiMixerWidget::applyBackgroundLayerState()
{
    m_layerActive[BackgroundLayerIndex] = true;
    m_opacity[BackgroundLayerIndex]     = 1.0f;
    m_copyMode[BackgroundLayerIndex]    = pvj::core::CopyMode::Normal;
    m_layerIsFeedback[BackgroundLayerIndex] = false;
    m_layerMatteRole[BackgroundLayerIndex]  = pvj::core::LayerMatteRole::None;
    m_layerFilterChain[BackgroundLayerIndex].clear();
    m_layerFilterLastOut[BackgroundLayerIndex] = -1;
}

RhiMixerWidget::~RhiMixerWidget() = default;

void RhiMixerWidget::setLabel(const QString& text)
{
    if (m_label != text) {
        m_label = text;
        update();
    }
}

void RhiMixerWidget::setLayerOpacity(int layer, float o)
{
    if (layer == BackgroundLayerIndex) {
        return;
    }
    if (layer < 0 || layer >= LayerCount) return;
    const float v = qBound(0.0f, o, 1.0f);
    if (m_opacity[layer] != v) {
        m_opacity[layer] = v;
        update();
    }
}

void RhiMixerWidget::setLayerCopyMode(int layer, pvj::core::CopyMode mode)
{
    if (layer < 0 || layer >= LayerCount) return;
    if (m_copyMode[layer] != mode) {
        m_copyMode[layer] = mode;
        update();
    }
}

void RhiMixerWidget::setLayerActive(int layer, bool active)
{
    if (layer == BackgroundLayerIndex) {
        applyBackgroundLayerState();
        return;
    }
    if (layer < 0 || layer >= LayerCount) return;
    if (m_layerActive[layer] != active) {
        m_layerActive[layer] = active;
        recomputeActiveFeedbackLayer();
        update();
    }
}

float RhiMixerWidget::layerOpacity(int layer) const
{
    if (layer < 0 || layer >= LayerCount) return 1.0f;
    return m_opacity[layer];
}

pvj::core::CopyMode RhiMixerWidget::layerCopyMode(int layer) const
{
    if (layer < 0 || layer >= LayerCount) return pvj::core::CopyMode::Normal;
    return m_copyMode[layer];
}

bool RhiMixerWidget::layerActive(int layer) const
{
    if (layer < 0 || layer >= LayerCount) return false;
    return m_layerActive[layer];
}

void RhiMixerWidget::setLayerPicture(int layer, const pvj::core::PictureParams& p)
{
    if (layer < 0 || layer >= LayerCount) return;
    if (pictureParamsEqual(m_layerPicture[layer], p)) {
        return;
    }
    m_layerPicture[layer] = p;
    update();
}

pvj::core::PictureParams RhiMixerWidget::layerPicture(int layer) const
{
    if (layer < 0 || layer >= LayerCount) return {};
    return m_layerPicture[layer];
}

void RhiMixerWidget::setLayerMatteRole(int layer, pvj::core::LayerMatteRole role)
{
    if (layer < 0 || layer >= LayerCount) return;
    if (m_layerMatteRole[layer] != role) {
        m_layerMatteRole[layer] = role;
        update();
    }
}

pvj::core::LayerMatteRole RhiMixerWidget::layerMatteRole(int layer) const
{
    if (layer < 0 || layer >= LayerCount) return pvj::core::LayerMatteRole::None;
    return m_layerMatteRole[layer];
}

void RhiMixerWidget::setLayerFilterChain(int layer, const QList<pvj::core::CellFilterNode>& chain)
{
    if (layer == BackgroundLayerIndex) {
        return;
    }
    if (layer < 0 || layer >= LayerCount) {
        return;
    }
    if (filterChainShallowEqual(m_layerFilterChain[layer], chain)) {
        return;
    }
    m_layerFilterChain[layer] = chain;
    if (layer == m_activeFeedbackLayer || m_layerIsFeedback[layer]) {
        releaseFeedbackGpuResources();
    }
    // Do not reset m_layerFilterLastOut here: updateMixerFromPlayingCells can run
    // between the feedback key filter pass and rebuildMixerShaderResourceBindings,
    // which would drop keyed output and cause visible flicker. runPerLayerFilterChain
    // resets lastOut at the start of each frame's filter pass when needed.
    update();
}

void RhiMixerWidget::setOutputFilterChain(const QList<pvj::core::CellFilterNode>& chain)
{
    if (filterChainShallowEqual(m_outputFilterChain, chain)) {
        return;
    }
    m_outputFilterChain = chain;
    m_outputFilterLastOut = -1;
    update();
}

void RhiMixerWidget::setLayerKeyChannels(int layer, float r, float g, float b)
{
    if (layer < 0 || layer >= LayerCount) {
        return;
    }
    const float cr = qBound(0.0f, r, 1.0f);
    const float cg = qBound(0.0f, g, 1.0f);
    const float cb = qBound(0.0f, b, 1.0f);
    auto& ch = m_layerKeyChannel[static_cast<size_t>(layer)];
    if (ch[0] != cr || ch[1] != cg || ch[2] != cb) {
        ch = { cr, cg, cb };
        update();
    }
}

void RhiMixerWidget::setLayerFeedback(int layer, bool enabled, const pvj::core::FeedbackParams& p)
{
    if (layer == BackgroundLayerIndex) {
        return;
    }
    if (layer < 0 || layer >= LayerCount) {
        return;
    }
    const bool changed = m_layerIsFeedback[layer] != enabled
        || !feedbackParamsEqual(m_layerFeedback[layer], p);
    if (!changed) {
        return;
    }
    m_layerIsFeedback[layer] = enabled;
    m_layerFeedback[layer] = p;
    if (enabled) {
        m_feedbackWriteIdx = 0;
    }
    recomputeActiveFeedbackLayer();
    update();
}

bool RhiMixerWidget::layerFeedbackEnabled(int layer) const
{
    if (layer < 0 || layer >= LayerCount) {
        return false;
    }
    return m_layerIsFeedback[layer];
}

pvj::core::FeedbackParams RhiMixerWidget::layerFeedback(int layer) const
{
    if (layer < 0 || layer >= LayerCount) {
        return {};
    }
    return m_layerFeedback[layer];
}

void RhiMixerWidget::recomputeActiveFeedbackLayer()
{
    const int previous = m_activeFeedbackLayer;
    m_activeFeedbackLayer = -1;
    for (int i = UserLayerMin; i < LayerCount; ++i) {
        if (m_layerIsFeedback[i] && m_layerActive[i]) {
            m_activeFeedbackLayer = i;
            break;
        }
    }
    if (previous >= 0 && m_activeFeedbackLayer < 0) {
        releaseFeedbackGpuResources();
    }
}

bool RhiMixerWidget::hasActiveFeedbackLayer() const
{
    const int f = m_activeFeedbackLayer;
    return f >= 0 && f < LayerCount && m_layerIsFeedback[f] && m_layerActive[f];
}

bool RhiMixerWidget::feedbackKeyFromAboveActive() const
{
    if (!hasActiveFeedbackLayer()) {
        return false;
    }
    const int f = m_activeFeedbackLayer;
    for (int i = f + 1; i < LayerCount; ++i) {
        if (m_layerActive[i]) {
            return true;
        }
    }
    return false;
}

pvj::core::FeedbackInputMode RhiMixerWidget::activeFeedbackInputMode() const
{
    if (!hasActiveFeedbackLayer()) {
        return pvj::core::FeedbackInputMode::StackComposite;
    }
    return m_layerFeedback[m_activeFeedbackLayer].inputMode;
}

void RhiMixerWidget::setFrame(int layer, QImage frame, qint64 /*pts*/)
{
    if (layer == BackgroundLayerIndex) {
        return;
    }
    if (layer < 0 || layer >= LayerCount) return;
    if (frame.isNull()) {
        clearFrame(layer);
        return;
    }
    if (frame.format() != QImage::Format_RGBA8888) {
        frame = frame.convertToFormat(QImage::Format_RGBA8888);
    }
    m_pending[layer] = std::move(frame);
    m_dirty[layer]   = true;
    update();
}

void RhiMixerWidget::clearFrame(int layer)
{
    if (layer == BackgroundLayerIndex) {
        return;
    }
    if (layer < 0 || layer >= LayerCount) return;
    m_pending[layer] = QImage();
    m_dirty[layer]   = true;
    m_sizes[layer]   = QSize();
    update();
}

void RhiMixerWidget::releaseFeedbackGpuResources()
{
    m_feedbackPipeline.reset();
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
    m_textureCopyPipeline.reset();
    m_textureCopySrb.reset();
    m_textureCopyPipelineRp = nullptr;
    m_feedbackPixelSize = {};
    m_feedbackWriteIdx = 0;
    m_sceneHistWriteIdx = 0;
}

void RhiMixerWidget::releaseOffscreenGpuResources()
{
    releaseFeedbackGpuResources();
    m_presentPipeline.reset();
    m_mixerOffscreenPipeline.reset();
    m_presentSrb.reset();
    m_presentUbuf.reset();
    m_sceneRt.reset();
    m_sceneTex.reset();
    m_sceneRp.reset();
    m_presentSwapchainRp = nullptr;

    for (auto& pair : m_layerFilterPingRt) {
        for (auto& rt : pair) {
            rt.reset();
        }
    }
    for (auto& pair : m_layerFilterPingTex) {
        for (auto& tex : pair) {
            tex.reset();
        }
    }
    for (auto& idx : m_layerFilterLastOut) {
        idx = -1;
    }
    for (auto& ubuf : m_layerFilterUbuf) {
        ubuf.reset();
    }
    for (auto& srb : m_layerFilterSrb) {
        srb.reset();
    }
    m_layerFilterSrbKey.fill(0);
    for (auto& ready : m_layerFilterSrbReady) {
        ready = false;
    }
    m_blendBlackTex.reset();
    m_filterPixelSize = {};
    m_filterRp.reset();
    m_filterPipelineByTypeId.clear();
    m_filterPipelineOwned.clear();
}

void RhiMixerWidget::releaseGpuResources()
{
    if (m_maxineBackend) {
        m_maxineBackend->shutdown();
        delete m_maxineBackend;
        m_maxineBackend = nullptr;
    }
    m_maxinePixelSize = {};
    releaseOffscreenGpuResources();
    m_pipeline.reset();
    m_srb.reset();
    for (auto& t : m_tex) t.reset();
    m_sampler.reset();
    m_ubuf.reset();
    m_vbuf.reset();
    m_pipelineReady = false;
}

void RhiMixerWidget::releaseResources()
{
    releaseGpuResources();
}

QRhiTexture* RhiMixerWidget::sourceTextureForLayer(int layer) const
{
    if (layer < 0 || layer >= LayerCount) return nullptr;
    if (QRhiTexture* filtered = filterOutputTextureForLayer(layer)) {
        return filtered;
    }
    if (hasActiveFeedbackLayer() && layer == m_activeFeedbackLayer) {
        if (QRhiTexture* fb = feedbackWriteTexture()) {
            return fb;
        }
    }
    return layerBaseTextureForInput(layer);
}

QRhiTexture* RhiMixerWidget::feedbackWriteTexture() const
{
    return m_feedbackTex[m_feedbackWriteIdx].get();
}

QRhiTexture* RhiMixerWidget::feedbackReadTexture() const
{
    if (!hasActiveFeedbackLayer()) {
        return nullptr;
    }
    const int delay = qBound(0, m_layerFeedback[m_activeFeedbackLayer].frameDelay,
                             kFeedbackRingSize - 2);
    const int readIdx = (m_feedbackWriteIdx + kFeedbackRingSize - 1 - delay) % kFeedbackRingSize;
    return m_feedbackTex[readIdx].get();
}

QRhiTextureRenderTarget* RhiMixerWidget::feedbackWriteRenderTarget() const
{
    return m_feedbackRt[m_feedbackWriteIdx].get();
}

QRhiTexture* RhiMixerWidget::sceneHistReadTexture() const
{
    return m_sceneHistTex[1 - m_sceneHistWriteIdx].get();
}

QRhiTextureRenderTarget* RhiMixerWidget::sceneHistWriteRenderTarget() const
{
    return m_sceneHistRt[m_sceneHistWriteIdx].get();
}

QRhiTexture* RhiMixerWidget::filterOutputTextureForLayer(int layer) const
{
    if (layer < 0 || layer >= LayerCount) {
        return nullptr;
    }
    const int outIdx = m_layerFilterLastOut[layer];
    if (outIdx < 0 || outIdx > 1) {
        return nullptr;
    }
    return m_layerFilterPingTex[layer][outIdx].get();
}

QRhiTexture* RhiMixerWidget::layerBaseTextureForInput(int layer) const
{
    if (layer < 0 || layer >= LayerCount) {
        return nullptr;
    }
    return m_tex[layer].get();
}

void RhiMixerWidget::rebuildMixerShaderResourceBindings()
{
    if (!m_srb) return;

    QVector<QRhiShaderResourceBinding> binds;
    binds.reserve(2 + (LayerCount - 1) + 2);
    binds.append(QRhiShaderResourceBinding::uniformBuffer(
        0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
        m_ubuf.get()));
    for (int gpuLayer = UserLayerMin; gpuLayer < LayerCount; ++gpuLayer) {
        QRhiTexture* src = sourceTextureForLayer(gpuLayer);
        if (!src) {
            src = m_tex[gpuLayer].get();
        }
        binds.append(QRhiShaderResourceBinding::sampledTexture(
            gpuLayer, QRhiShaderResourceBinding::FragmentStage,
            src, m_sampler.get()));
    }
    QRhiTexture* placeholder = m_tex[0].get();
    binds.append(QRhiShaderResourceBinding::sampledTexture(
        13, QRhiShaderResourceBinding::FragmentStage,
        placeholder, m_sampler.get()));
    binds.append(QRhiShaderResourceBinding::sampledTexture(
        14, QRhiShaderResourceBinding::FragmentStage,
        placeholder, m_sampler.get()));
    m_srb->setBindings(binds.cbegin(), binds.cend());
    m_srb->create();
}

void RhiMixerWidget::rebuildBelowMixerShaderResourceBindings()
{
    if (!m_belowSrb) return;

    QVector<QRhiShaderResourceBinding> binds;
    binds.reserve(2 + (LayerCount - 1) + 2);
    binds.append(QRhiShaderResourceBinding::uniformBuffer(
        0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
        m_belowUbuf.get()));
    for (int gpuLayer = UserLayerMin; gpuLayer < LayerCount; ++gpuLayer) {
        QRhiTexture* src = sourceTextureForLayer(gpuLayer);
        if (!src) {
            src = m_tex[gpuLayer].get();
        }
        binds.append(QRhiShaderResourceBinding::sampledTexture(
            gpuLayer, QRhiShaderResourceBinding::FragmentStage,
            src, m_sampler.get()));
    }
    QRhiTexture* placeholder = m_tex[0].get();
    binds.append(QRhiShaderResourceBinding::sampledTexture(
        13, QRhiShaderResourceBinding::FragmentStage,
        placeholder, m_sampler.get()));
    binds.append(QRhiShaderResourceBinding::sampledTexture(
        14, QRhiShaderResourceBinding::FragmentStage,
        placeholder, m_sampler.get()));
    m_belowSrb->setBindings(binds.cbegin(), binds.cend());
    m_belowSrb->create();
}

void RhiMixerWidget::rebuildFeedbackShaderResourceBindings(QRhiTexture* belowTex,
                                                           QRhiTexture* historyRead)
{
    if (!m_feedbackSrb || !belowTex || !historyRead) {
        return;
    }
    QRhiTexture* aboveTex = m_aboveTex ? m_aboveTex.get() : belowTex;
    m_feedbackSrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::FragmentStage,
            m_feedbackUbuf.get()),
        QRhiShaderResourceBinding::sampledTexture(
            1, QRhiShaderResourceBinding::FragmentStage,
            belowTex, m_sampler.get()),
        QRhiShaderResourceBinding::sampledTexture(
            2, QRhiShaderResourceBinding::FragmentStage,
            historyRead, m_sampler.get()),
        QRhiShaderResourceBinding::sampledTexture(
            3, QRhiShaderResourceBinding::FragmentStage,
            aboveTex, m_sampler.get()),
    });
    m_feedbackSrb->create();
}

void RhiMixerWidget::rebuildStackCombineShaderResourceBindings()
{
    if (!m_stackCombineSrb || !m_belowTex || !m_aboveTex || !m_stackTex) {
        return;
    }
    m_stackCombineSrb->setBindings({
        QRhiShaderResourceBinding::sampledTexture(
            1, QRhiShaderResourceBinding::FragmentStage,
            m_belowTex.get(), m_sampler.get()),
        QRhiShaderResourceBinding::sampledTexture(
            2, QRhiShaderResourceBinding::FragmentStage,
            m_aboveTex.get(), m_sampler.get()),
    });
    m_stackCombineSrb->create();
}

void RhiMixerWidget::rebuildTextureCopyShaderResourceBindings(QRhiTexture* sourceTex)
{
    if (!m_textureCopySrb || !sourceTex) {
        return;
    }
    m_textureCopySrb->setBindings({
        QRhiShaderResourceBinding::sampledTexture(
            1, QRhiShaderResourceBinding::FragmentStage,
            sourceTex, m_sampler.get()),
    });
    m_textureCopySrb->create();
}

void RhiMixerWidget::rebuildPresentShaderResourceBindings(QRhiTexture* sourceTex)
{
    if (!m_presentSrb || !sourceTex) return;
    m_presentSrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::VertexStage,
            m_presentUbuf.get()),
        QRhiShaderResourceBinding::sampledTexture(
            1, QRhiShaderResourceBinding::FragmentStage,
            sourceTex, m_sampler.get()),
    });
    m_presentSrb->create();
}

void RhiMixerWidget::updatePresentUniformBuffer(QRhiResourceUpdateBatch* batch, const QSize& widgetPx)
{
    float sx = 1.0f;
    float sy = 1.0f;
    const QSize stagePx = m_stagePixelSize.isValid() && !m_stagePixelSize.isEmpty()
        ? m_stagePixelSize : widgetPx;
    if (stagePx.isValid() && !stagePx.isEmpty() && !widgetPx.isEmpty()) {
        const double stageA  = double(stagePx.width())  / double(stagePx.height());
        const double widgetA = double(widgetPx.width()) / double(widgetPx.height());
        if (stageA > widgetA) {
            sy = float(widgetA / stageA);
        } else {
            sx = float(stageA / widgetA);
        }
    }
    const float ubo[12] = {
        sx,   sy,   0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f,
    };
    batch->updateDynamicBuffer(m_presentUbuf.get(), 0, sizeof(ubo), ubo);
}

void RhiMixerWidget::setStagePixelSize(QSize px)
{
    if (m_stagePixelSize == px) return;
    m_stagePixelSize = px;
    releaseOffscreenGpuResources();
    update();
}

void RhiMixerWidget::ensurePresentPipelineForSwapchain(QRhi* r)
{
    QRhiRenderPassDescriptor* swapRp = renderTarget()->renderPassDescriptor();
    if (m_presentPipeline && m_presentSwapchainRp == swapRp) {
        return;
    }
    if (!createTexturedPresentPipeline(r, m_presentSrb.get(), swapRp, m_presentPipeline)) {
        qWarning("RhiMixerWidget: failed to create present pipeline");
        return;
    }
    m_presentSwapchainRp = swapRp;
}

bool RhiMixerWidget::ensureOffscreenSceneTargets(QRhi* r, const QSize& pixelSize)
{
    if (!r || pixelSize.isEmpty()) return false;
    if (m_sceneTex && m_sceneTex->pixelSize() == pixelSize
        && m_sceneRt && m_sceneRp && m_mixerOffscreenPipeline
        ) {
        return true;
    }

    m_sceneTex.reset(r->newTexture(QRhiTexture::RGBA8, pixelSize, 1, QRhiTexture::RenderTarget));
    if (!m_sceneTex->create()) return false;

    m_sceneRt.reset(r->newTextureRenderTarget(QRhiTextureRenderTargetDescription(
        QRhiColorAttachment(m_sceneTex.get()))));
    m_sceneRp.reset(m_sceneRt->newCompatibleRenderPassDescriptor());
    m_sceneRt->setRenderPassDescriptor(m_sceneRp.get());
    if (!m_sceneRt->create()) return false;

    if (!m_presentUbuf) {
        m_presentUbuf.reset(r->newBuffer(QRhiBuffer::Dynamic,
                                         QRhiBuffer::UniformBuffer,
                                         48));
        m_presentUbuf->create();
    }
    if (!m_presentSrb) {
        m_presentSrb.reset(r->newShaderResourceBindings());
    }
    rebuildPresentShaderResourceBindings(m_sceneTex.get());

    if (!createMixerGraphicsPipeline(r, m_srb.get(), m_sceneRp.get(), m_mixerOffscreenPipeline)) {
        return false;
    }
    m_presentSwapchainRp = nullptr;
    return true;
}

bool RhiMixerWidget::ensureLayerFilterTargets(QRhi* r, const QSize& pixelSize)
{
    if (!r || pixelSize.isEmpty()) {
        return false;
    }
    if (m_filterPixelSize != pixelSize) {
        if (m_maxineBackend) {
            m_maxineBackend->invalidateSize();
        }
        m_maxinePixelSize = {};
        for (auto& pair : m_layerFilterPingRt) {
            for (auto& rt : pair) {
                rt.reset();
            }
        }
        for (auto& pair : m_layerFilterPingTex) {
            for (auto& tex : pair) {
                tex.reset();
            }
        }
        for (auto& idx : m_layerFilterLastOut) {
            idx = -1;
        }
        m_filterPixelSize = pixelSize;
        m_filterPipelineByTypeId.clear();
        m_filterPipelineOwned.clear();
        m_filterRp.reset();
        m_textureCopyPipeline.reset();
        m_textureCopyPipelineRp = nullptr;
        m_layerFilterSrbKey.fill(0);
        for (auto& ready : m_layerFilterSrbReady) {
            ready = false;
        }
        m_outputFilterPingRt[0].reset();
        m_outputFilterPingRt[1].reset();
        m_outputFilterPingTex[0].reset();
        m_outputFilterPingTex[1].reset();
        m_outputFilterLastOut = -1;
        m_outputFilterSrbKey = 0;
        m_outputFilterSrbReady = false;
    }
    bool needed = false;
    for (int i = 0; i < LayerCount; ++i) {
        if (m_layerFilterChain[i].isEmpty()) {
            continue;
        }
        needed = true;
        for (int k = 0; k < 2; ++k) {
            if (!m_layerFilterPingTex[i][k]) {
                m_layerFilterPingTex[i][k].reset(
                    r->newTexture(QRhiTexture::RGBA8, pixelSize, 1, QRhiTexture::RenderTarget));
                if (!m_layerFilterPingTex[i][k]->create()) {
                    return false;
                }
            }
            if (!m_layerFilterPingRt[i][k]) {
                m_layerFilterPingRt[i][k].reset(r->newTextureRenderTarget(
                    QRhiTextureRenderTargetDescription(QRhiColorAttachment(m_layerFilterPingTex[i][k].get()))));
                if (!m_filterRp) {
                    m_filterRp.reset(m_layerFilterPingRt[i][k]->newCompatibleRenderPassDescriptor());
                }
                m_layerFilterPingRt[i][k]->setRenderPassDescriptor(m_filterRp.get());
                if (!m_layerFilterPingRt[i][k]->create()) {
                    return false;
                }
            }
        }
    }
    return needed;
}

bool RhiMixerWidget::ensureLayerFilterPassResources(QRhi* r, int layer)
{
    if (!r || layer < 0 || layer >= LayerCount) {
        return false;
    }
    if (!m_layerFilterUbuf[layer]) {
        m_layerFilterUbuf[layer].reset(r->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
                                                    sizeof(EffectQuadUbo2)));
        if (!m_layerFilterUbuf[layer]->create()) {
            m_layerFilterUbuf[layer].reset();
            return false;
        }
    }
    if (!m_layerFilterSrb[layer]) {
        m_layerFilterSrb[layer].reset(r->newShaderResourceBindings());
    }
    return m_layerFilterUbuf[layer] && m_layerFilterSrb[layer];
}

bool RhiMixerWidget::ensureOutputFilterTargets(QRhi* r, const QSize& pixelSize)
{
    if (!r || pixelSize.isEmpty() || m_outputFilterChain.isEmpty()) {
        return false;
    }
    ensureLayerFilterTargets(r, pixelSize);
    for (int k = 0; k < 2; ++k) {
        if (!m_outputFilterPingTex[k]) {
            m_outputFilterPingTex[k].reset(
                r->newTexture(QRhiTexture::RGBA8, pixelSize, 1, QRhiTexture::RenderTarget));
            if (!m_outputFilterPingTex[k]->create()) {
                return false;
            }
        }
        if (!m_outputFilterPingRt[k]) {
            m_outputFilterPingRt[k].reset(r->newTextureRenderTarget(
                QRhiTextureRenderTargetDescription(QRhiColorAttachment(m_outputFilterPingTex[k].get()))));
            if (!m_filterRp) {
                m_filterRp.reset(m_outputFilterPingRt[k]->newCompatibleRenderPassDescriptor());
            }
            m_outputFilterPingRt[k]->setRenderPassDescriptor(m_filterRp.get());
            if (!m_outputFilterPingRt[k]->create()) {
                return false;
            }
        }
    }
    return true;
}

bool RhiMixerWidget::ensureOutputFilterPassResources(QRhi* r)
{
    if (!r) {
        return false;
    }
    if (!m_outputFilterUbuf) {
        m_outputFilterUbuf.reset(r->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
                                               sizeof(EffectQuadUbo2)));
        if (!m_outputFilterUbuf->create()) {
            m_outputFilterUbuf.reset();
            return false;
        }
    }
    if (!m_outputFilterSrb) {
        m_outputFilterSrb.reset(r->newShaderResourceBindings());
    }
    return m_outputFilterUbuf && m_outputFilterSrb;
}

void RhiMixerWidget::rebuildOutputFilterShaderResourceBindings(QRhiTexture* sourceTex,
                                                               const QString& typeId)
{
    if (!m_outputFilterSrb || !m_outputFilterUbuf || !sourceTex) {
        return;
    }
    QRhiTexture* secondTex = sourceTex;
    if (effectUsesTwoTextures(typeId) && m_blendBlackTex) {
        secondTex = m_blendBlackTex.get();
    }
    const quint64 bindingKey = (quint64(quintptr(sourceTex)) << 1)
        ^ quint64(quintptr(secondTex));
    if (m_outputFilterSrbKey == bindingKey && m_outputFilterSrbReady) {
        return;
    }
    m_outputFilterSrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            m_outputFilterUbuf.get()),
        QRhiShaderResourceBinding::sampledTexture(
            1, QRhiShaderResourceBinding::FragmentStage, sourceTex, m_sampler.get()),
        QRhiShaderResourceBinding::sampledTexture(
            2, QRhiShaderResourceBinding::FragmentStage, secondTex, m_sampler.get()),
    });
    m_outputFilterSrb->create();
    m_outputFilterSrbKey = bindingKey;
    m_outputFilterSrbReady = true;
}

QRhiTexture* RhiMixerWidget::outputFilterResultTexture() const
{
    if (m_outputFilterLastOut < 0 || !m_outputFilterPingTex[m_outputFilterLastOut]) {
        return m_sceneTex.get();
    }
    return m_outputFilterPingTex[m_outputFilterLastOut].get();
}

void RhiMixerWidget::runOutputFilterChain(QRhi* r, QRhiCommandBuffer* cb, const QSize& stagePx,
                                          const QColor& clear)
{
    m_outputFilterLastOut = -1;
    if (!r || !cb || !m_sceneTex || m_outputFilterChain.isEmpty()) {
        return;
    }
    if (!ensureOutputFilterPassResources(r)) {
        return;
    }
    QRhiBuffer* ubuf = m_outputFilterUbuf.get();
    QRhiTexture* sourceTex = m_sceneTex.get();
    int writeIdx = 0;

    if (!m_blendBlackTex) {
        m_blendBlackTex.reset(r->newTexture(QRhiTexture::RGBA8, QSize(1, 1), 1));
        if (m_blendBlackTex->create()) {
            QRhiResourceUpdateBatch* blackBatch = r->nextResourceUpdateBatch();
            QImage blackImg(1, 1, QImage::Format_RGBA8888);
            blackImg.fill(Qt::black);
            blackBatch->uploadTexture(m_blendBlackTex.get(), blackImg);
            cb->resourceUpdate(blackBatch);
        } else {
            m_blendBlackTex.reset();
        }
    }

    constexpr int kOutputPipelineLayer = 100;

    for (const auto& node : m_outputFilterChain) {
        if (pvj::core::filterUsesMaxineBackend(node.typeId)) {
            if (!m_maxineBackend) {
                m_maxineBackend = new MaxineFilterBackend();
            }
            if (m_maxinePixelSize != stagePx) {
                m_maxineBackend->invalidateSize();
                m_maxinePixelSize = stagePx;
            }
            m_maxineBackend->ensureInitialized(r);
            QRhiTextureRenderTarget* destRt = m_outputFilterPingRt[writeIdx].get();
            QRhiTexture* destTex = m_outputFilterPingTex[writeIdx].get();
            bool ok = false;
            bool readOk = false;
            if (maxineFiltersAvailable()) {
                r->finish();
                QImage input;
                readOk = readRgba8Texture(r, cb, sourceTex, stagePx, input) && !input.isNull();
                if (readOk) {
                    ok = m_maxineBackend->applyFromImage(r, cb, node, input, destRt, stagePx);
                }
            }
            if (!ok) {
                runTextureCopyPass(r, cb, sourceTex, destRt, stagePx);
            }
            sourceTex = destTex;
            m_outputFilterLastOut = writeIdx;
            writeIdx = 1 - writeIdx;
            continue;
        }

        const int internalPasses = qMax(1, pvj::core::filterEffectInternalPasses(node.typeId));
        for (int pass = 0; pass < internalPasses; ++pass) {
            rebuildOutputFilterShaderResourceBindings(sourceTex, node.typeId);
            QRhiGraphicsPipeline* pipeline = ensureFilterPipeline(
                r, node.typeId, kOutputPipelineLayer, m_outputFilterSrb.get(),
                m_outputFilterPingRt[writeIdx]->renderPassDescriptor());
            if (!pipeline) {
                continue;
            }
            QRhiResourceUpdateBatch* ubatch = r->nextResourceUpdateBatch();
            updateFilterUniformBuffer(ubatch, ubuf, node, stagePx, -1, pass);
            cb->resourceUpdate(ubatch);

            cb->beginPass(m_outputFilterPingRt[writeIdx].get(), clear, { 1.0f, 0 }, nullptr);
            cb->setGraphicsPipeline(pipeline);
            cb->setViewport(QRhiViewport(0, 0, stagePx.width(), stagePx.height()));
            cb->setShaderResources(m_outputFilterSrb.get());
            QRhiCommandBuffer::VertexInput vin(m_vbuf.get(), 0);
            cb->setVertexInput(0, 1, &vin);
            cb->draw(4);
            cb->endPass();

            sourceTex = m_outputFilterPingTex[writeIdx].get();
            m_outputFilterLastOut = writeIdx;
            writeIdx = 1 - writeIdx;
        }
    }
}

void RhiMixerWidget::rebuildLayerFilterShaderResourceBindings(int layer, QRhiTexture* sourceTex,
                                                              const QString& typeId)
{
    if (layer < 0 || layer >= LayerCount || !m_layerFilterSrb[layer] || !m_layerFilterUbuf[layer]
        || !sourceTex) {
        return;
    }
    QRhiTexture* secondTex = sourceTex;
    if (effectUsesTwoTextures(typeId) && m_blendBlackTex) {
        secondTex = m_blendBlackTex.get();
    }
    const quint64 bindingKey = (quint64(quintptr(sourceTex)) << 1)
        ^ quint64(quintptr(secondTex));
    if (m_layerFilterSrbKey[layer] == bindingKey && m_layerFilterSrbReady[layer]) {
        return;
    }
    m_layerFilterSrb[layer]->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            m_layerFilterUbuf[layer].get()),
        QRhiShaderResourceBinding::sampledTexture(
            1, QRhiShaderResourceBinding::FragmentStage, sourceTex, m_sampler.get()),
        QRhiShaderResourceBinding::sampledTexture(
            2, QRhiShaderResourceBinding::FragmentStage, secondTex, m_sampler.get()),
    });
    m_layerFilterSrb[layer]->create();
    m_layerFilterSrbKey[layer] = bindingKey;
    m_layerFilterSrbReady[layer] = true;
}

QRhiGraphicsPipeline* RhiMixerWidget::ensureFilterPipeline(QRhi* r, const QString& typeId,
                                                           int layerIndex,
                                                           QRhiShaderResourceBindings* srb,
                                                           QRhiRenderPassDescriptor* rp)
{
    const QString fragPath = effectFragmentShaderForType(typeId);
    const QString key = QStringLiteral("%1:L%2").arg(fragPath).arg(layerIndex);
    auto it = m_filterPipelineByTypeId.find(key);
    if (it != m_filterPipelineByTypeId.end() && it.value()) {
        return it.value();
    }
    std::unique_ptr<QRhiGraphicsPipeline> pipeline;
    if (!createEffectPipeline(r, fragPath, srb, rp, pipeline)) {
        return nullptr;
    }
    QRhiGraphicsPipeline* raw = pipeline.get();
    m_filterPipelineOwned.push_back(std::move(pipeline));
    m_filterPipelineByTypeId.insert(key, raw);
    return raw;
}

bool RhiMixerWidget::ensureFeedbackTargets(QRhi* r, const QSize& pixelSize)
{
    if (!r || pixelSize.isEmpty() || !hasActiveFeedbackLayer()) {
        return false;
    }
    if (m_feedbackPixelSize != pixelSize) {
        releaseFeedbackGpuResources();
        m_feedbackPixelSize = pixelSize;
    }

    for (int k = 0; k < kFeedbackRingSize; ++k) {
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
    if (!m_feedbackUbuf) {
        m_feedbackUbuf.reset(r->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 48));
        if (!m_feedbackUbuf->create()) {
            return false;
        }
    }
    if (!m_feedbackSrb) {
        m_feedbackSrb.reset(r->newShaderResourceBindings());
    }
    if (!m_stackCombineSrb) {
        m_stackCombineSrb.reset(r->newShaderResourceBindings());
    }
    rebuildBelowMixerShaderResourceBindings();
    rebuildStackCombineShaderResourceBindings();

    if (!m_mixerBelowPipeline) {
        if (!createMixerGraphicsPipeline(r, m_belowSrb.get(), m_belowRp.get(), m_mixerBelowPipeline)) {
            return false;
        }
    }
    if (!m_stackCombinePipeline) {
        // layer_feedback.vert has no vertex rotation (textured_quad.vert would need a bound UBO).
        if (!createEffectPipeline(r, QStringLiteral(":/shaders/stack_combine.frag.qsb"),
                                  QStringLiteral(":/shaders/layer_feedback.vert.qsb"),
                                  m_stackCombineSrb.get(), m_belowRp.get(), m_stackCombinePipeline)) {
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

    return true;
}

void RhiMixerWidget::runPartialMixerPass(QRhi* r, QRhiCommandBuffer* cb,
                                         int minLayerInclusive, int maxLayerExclusive,
                                         QRhiTextureRenderTarget* targetRt, const QSize& stagePx,
                                         const QColor& clear)
{
    if (!r || !cb || !targetRt || !m_mixerBelowPipeline || !m_belowSrb) {
        return;
    }
    // Partial passes run after the filter prepass; refresh bindings so keyed ping-pong
    // outputs are sampled instead of stale raw-layer textures from ensureFeedbackTargets.
    rebuildBelowMixerShaderResourceBindings();
    m_mixerMinLayerInclusive = minLayerInclusive;
    m_mixerMaxLayerExclusive = maxLayerExclusive;
    QRhiResourceUpdateBatch* batch = r->nextResourceUpdateBatch();
    updateBelowMixerUniformBuffer(batch, minLayerInclusive, maxLayerExclusive);
    cb->resourceUpdate(batch);

    cb->beginPass(targetRt, clear, { 1.0f, 0 }, nullptr);
    cb->setGraphicsPipeline(m_mixerBelowPipeline.get());
    cb->setViewport(QRhiViewport(0, 0, stagePx.width(), stagePx.height()));
    cb->setShaderResources(m_belowSrb.get());
    QRhiCommandBuffer::VertexInput vin(m_vbuf.get(), 0);
    cb->setVertexInput(0, 1, &vin);
    cb->draw(4);
    cb->endPass();
}

void RhiMixerWidget::runStackCombinePass(QRhi* r, QRhiCommandBuffer* cb, const QSize& stagePx,
                                         const QColor& clear)
{
    if (!r || !cb || !m_stackCombinePipeline || !m_stackRt) {
        return;
    }
    rebuildStackCombineShaderResourceBindings();
    cb->beginPass(m_stackRt.get(), clear, { 1.0f, 0 }, nullptr);
    cb->setGraphicsPipeline(m_stackCombinePipeline.get());
    cb->setViewport(QRhiViewport(0, 0, stagePx.width(), stagePx.height()));
    cb->setShaderResources(m_stackCombineSrb.get());
    QRhiCommandBuffer::VertexInput vin(m_vbuf.get(), 0);
    cb->setVertexInput(0, 1, &vin);
    cb->draw(4);
    cb->endPass();
}

bool RhiMixerWidget::ensureTextureCopyPipeline(QRhi* r, QRhiRenderPassDescriptor* rp)
{
    if (!r || !rp) {
        return false;
    }
    if (!m_textureCopySrb) {
        m_textureCopySrb.reset(r->newShaderResourceBindings());
    }
    if (m_textureCopyPipeline && m_textureCopyPipelineRp != rp) {
        m_textureCopyPipeline.reset();
    }
    if (!m_textureCopyPipeline) {
        if (!createEffectPipeline(r, QStringLiteral(":/shaders/textured_quad.frag.qsb"),
                                  QStringLiteral(":/shaders/layer_feedback.vert.qsb"),
                                  m_textureCopySrb.get(), rp, m_textureCopyPipeline)) {
            return false;
        }
        m_textureCopyPipelineRp = rp;
    }
    return m_textureCopyPipeline != nullptr;
}

void RhiMixerWidget::runTextureCopyPass(QRhi* r, QRhiCommandBuffer* cb, QRhiTexture* sourceTex,
                                        QRhiTextureRenderTarget* targetRt, const QSize& stagePx)
{
    if (!r || !cb || !sourceTex || !targetRt) {
        return;
    }
    if (!ensureTextureCopyPipeline(r, targetRt->renderPassDescriptor())) {
        return;
    }
    rebuildTextureCopyShaderResourceBindings(sourceTex);
    cb->beginPass(targetRt, QColor(0, 0, 0), { 1.0f, 0 }, nullptr);
    cb->setGraphicsPipeline(m_textureCopyPipeline.get());
    cb->setViewport(QRhiViewport(0, 0, stagePx.width(), stagePx.height()));
    cb->setShaderResources(m_textureCopySrb.get());
    QRhiCommandBuffer::VertexInput vin(m_vbuf.get(), 0);
    cb->setVertexInput(0, 1, &vin);
    cb->draw(4);
    cb->endPass();
}

void RhiMixerWidget::copySceneToHistory(QRhi* r, QRhiCommandBuffer* cb, const QSize& stagePx)
{
    if (!m_sceneTex || !sceneHistWriteRenderTarget()) {
        return;
    }
    runTextureCopyPass(r, cb, m_sceneTex.get(), sceneHistWriteRenderTarget(), stagePx);
}

void RhiMixerWidget::runFeedbackPass(QRhi* r, QRhiCommandBuffer* cb, int feedbackLayer,
                                     const QSize& stagePx, const QColor& clear,
                                     QRhiTexture* belowTex, QRhiTexture* historyRead,
                                     QRhiTextureRenderTarget* writeRt)
{
    if (!r || !cb || !m_feedbackPipeline || !belowTex || !historyRead || !writeRt) {
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
    QRhiCommandBuffer::VertexInput vin(m_vbuf.get(), 0);
    cb->setVertexInput(0, 1, &vin);
    cb->draw(4);
    cb->endPass();
}

void RhiMixerWidget::runPerLayerFilterChain(QRhi* r, QRhiCommandBuffer* cb, int i,
                                            QRhiTexture* firstSource, const QSize& stagePx,
                                            const QColor& clear,
                                            const QList<pvj::core::CellFilterNode>* chainOverride)
{
    m_layerFilterLastOut[i] = -1;
    if (!r || !cb || !firstSource) {
        return;
    }
    if (!m_layerFilterPingRt[i][0] || !m_layerFilterPingRt[i][1]) {
        return;
    }
    if (!ensureLayerFilterPassResources(r, i)) {
        return;
    }
    QRhiBuffer* ubuf = m_layerFilterUbuf[i].get();
    const QList<pvj::core::CellFilterNode>& chain = chainOverride ? *chainOverride : m_layerFilterChain[i];
    QRhiTexture* sourceTex = firstSource;
    int writeIdx = 0;

    if (!m_blendBlackTex) {
        m_blendBlackTex.reset(r->newTexture(QRhiTexture::RGBA8, QSize(1, 1), 1));
        if (m_blendBlackTex->create()) {
            QRhiResourceUpdateBatch* blackBatch = r->nextResourceUpdateBatch();
            QImage blackImg(1, 1, QImage::Format_RGBA8888);
            blackImg.fill(Qt::black);
            blackBatch->uploadTexture(m_blendBlackTex.get(), blackImg);
            cb->resourceUpdate(blackBatch);
        } else {
            m_blendBlackTex.reset();
        }
    }

    for (const auto& node : chain) {
        if (pvj::core::isFeedbackMarkerNode(node.typeId)) {
            continue;
        }
        if (pvj::core::filterUsesMaxineBackend(node.typeId)) {
            if (!m_maxineBackend) {
                m_maxineBackend = new MaxineFilterBackend();
            }
            if (m_maxinePixelSize != stagePx) {
                m_maxineBackend->invalidateSize();
                m_maxinePixelSize = stagePx;
            }
            m_maxineBackend->ensureInitialized(r);
            QRhiTextureRenderTarget* destRt = m_layerFilterPingRt[i][writeIdx].get();
            QRhiTexture* destTex = m_layerFilterPingTex[i][writeIdx].get();
            bool ok = false;
            bool readOk = false;
            if (maxineFiltersAvailable()) {
                r->finish();
                QImage input;
                readOk = readRgba8Texture(r, cb, sourceTex, stagePx, input) && !input.isNull();
                if (readOk) {
                    ok = m_maxineBackend->applyFromImage(r, cb, node, input, destRt, stagePx);
                }
            }
            if (!ok) {
                runTextureCopyPass(r, cb, sourceTex, destRt, stagePx);
            }
            sourceTex = destTex;
            m_layerFilterLastOut[i] = writeIdx;
            writeIdx = 1 - writeIdx;
            continue;
        }

        const int internalPasses = qMax(1, pvj::core::filterEffectInternalPasses(node.typeId));
        for (int pass = 0; pass < internalPasses; ++pass) {
            rebuildLayerFilterShaderResourceBindings(i, sourceTex, node.typeId);
            QRhiGraphicsPipeline* pipeline = ensureFilterPipeline(
                r, node.typeId, i, m_layerFilterSrb[i].get(),
                m_layerFilterPingRt[i][writeIdx]->renderPassDescriptor());
            if (!pipeline) {
                continue;
            }
            QRhiResourceUpdateBatch* ubatch = r->nextResourceUpdateBatch();
            updateFilterUniformBuffer(ubatch, ubuf, node, stagePx, i, pass);
            cb->resourceUpdate(ubatch);

            cb->beginPass(m_layerFilterPingRt[i][writeIdx].get(), clear, { 1.0f, 0 }, nullptr);
            cb->setGraphicsPipeline(pipeline);
            cb->setViewport(QRhiViewport(0, 0, stagePx.width(), stagePx.height()));
            cb->setShaderResources(m_layerFilterSrb[i].get());
            QRhiCommandBuffer::VertexInput vin(m_vbuf.get(), 0);
            cb->setVertexInput(0, 1, &vin);
            cb->draw(4);
            cb->endPass();

            sourceTex = m_layerFilterPingTex[i][writeIdx].get();
            m_layerFilterLastOut[i] = writeIdx;
            writeIdx = 1 - writeIdx;
        }
    }
}

void RhiMixerWidget::updateFilterUniformBuffer(QRhiResourceUpdateBatch* batch, QRhiBuffer* ubuf,
                                               const pvj::core::CellFilterNode& node, const QSize& pixelSize,
                                               int layerIndex, int internalPass)
{
    if (!batch || !ubuf) {
        return;
    }
    EffectQuadUbo2 packed;
    float keyRgb[3] = { 1.0f, 1.0f, 1.0f };
    if (layerIndex >= 0 && layerIndex < LayerCount) {
        const auto& w = m_layerKeyChannel[static_cast<size_t>(layerIndex)];
        keyRgb[0] = w[0];
        keyRgb[1] = w[1];
        keyRgb[2] = w[2];
    }
    const float elapsed = float(m_elapsed.isValid() ? m_elapsed.elapsed() * 1e-3 : 0.0);
    packFilterUniformBuffer(packed, node, pixelSize, elapsed, internalPass, keyRgb);
    batch->updateDynamicBuffer(ubuf, 0, sizeof(packed), &packed);
}

void RhiMixerWidget::initialize(QRhiCommandBuffer* cb)
{
    QRhi* r = rhi();
    if (!r) return;
    if (m_pipelineReady) return;

    if (!m_elapsed.isValid()) m_elapsed.start();

    m_vbuf.reset(r->newBuffer(QRhiBuffer::Immutable,
                              QRhiBuffer::VertexBuffer,
                              sizeof(kQuadVertices)));
    m_vbuf->create();

    m_ubuf.reset(r->newBuffer(QRhiBuffer::Dynamic,
                              QRhiBuffer::UniformBuffer,
                              kUboSize));
    m_ubuf->create();

    m_sampler.reset(r->newSampler(QRhiSampler::Linear, QRhiSampler::Linear,
                                  QRhiSampler::None,
                                  QRhiSampler::ClampToEdge,
                                  QRhiSampler::ClampToEdge));
    m_sampler->create();

    for (int i = 0; i < LayerCount; ++i) {
        m_tex[i].reset(r->newTexture(QRhiTexture::RGBA8, QSize(1, 1), 1, {}));
        m_tex[i]->create();
    }

    m_srb.reset(r->newShaderResourceBindings());
    rebuildMixerShaderResourceBindings();

    if (!createMixerGraphicsPipeline(r, m_srb.get(),
                                     renderTarget()->renderPassDescriptor(),
                                     m_pipeline)) {
        qWarning("RhiMixerWidget: failed to create mixer swapchain pipeline");
        return;
    }

    QRhiResourceUpdateBatch* batch = r->nextResourceUpdateBatch();
    batch->uploadStaticBuffer(m_vbuf.get(), kQuadVertices);
    float initial[kUboFloats];
    std::memset(initial, 0, sizeof(initial));
    initial[0] = 1.0f;
    initial[1] = 1.0f;
    batch->updateDynamicBuffer(m_ubuf.get(), 0, sizeof(initial), initial);
    cb->resourceUpdate(batch);

    m_pipelineReady = true;
}

void RhiMixerWidget::uploadFramesIfNeeded(QRhiResourceUpdateBatch* batch)
{
    static const quint8 dark[4] = { 12, 12, 12, 255 };
    for (int i = 0; i < LayerCount; ++i) {
        if (!m_dirty[i]) continue;
        m_dirty[i] = false;

        if (m_pending[i].isNull()) {
            if (m_tex[i]->pixelSize() != QSize(1, 1)) {
                m_tex[i]->setPixelSize(QSize(1, 1));
                m_tex[i]->create();
            }
            QRhiTextureSubresourceUploadDescription sub(dark, sizeof(dark));
            batch->uploadTexture(m_tex[i].get(),
                                 QRhiTextureUploadDescription(QRhiTextureUploadEntry(0, 0, sub)));
            m_sizes[i] = QSize();
        } else {
            const QSize sz = m_pending[i].size();
            if (m_tex[i]->pixelSize() != sz) {
                m_tex[i]->setPixelSize(sz);
                m_tex[i]->create();
            }
            QRhiTextureSubresourceUploadDescription sub(m_pending[i]);
            batch->uploadTexture(m_tex[i].get(),
                                 QRhiTextureUploadDescription(QRhiTextureUploadEntry(0, 0, sub)));
            m_sizes[i] = sz;
        }
    }
}

void RhiMixerWidget::updateMixerUniformBuffer(QRhiResourceUpdateBatch* batch, int maxLayerExclusive,
                                              int minLayerInclusive)
{
    updateBelowMixerUniformBuffer(batch, minLayerInclusive, maxLayerExclusive);
}

void RhiMixerWidget::updateBelowMixerUniformBuffer(QRhiResourceUpdateBatch* batch,
                                                   int minLayerInclusive, int maxLayerExclusive)
{
    if (!batch || !m_ubuf) {
        return;
    }
    QRhiBuffer* targetUbuf = m_ubuf.get();
    if (maxLayerExclusive >= 0 && maxLayerExclusive <= LayerCount && m_belowUbuf
        && (minLayerInclusive > 0 || maxLayerExclusive < LayerCount)) {
        targetUbuf = m_belowUbuf.get();
    }

    QSize dst = m_stagePixelSize.isValid() && !m_stagePixelSize.isEmpty()
        ? m_stagePixelSize
        : (renderTarget() ? renderTarget()->pixelSize() : size());
    if (dst.isEmpty()) dst = QSize(1, 1);

    QSize ref(16, 9);
    int bestArea = 0;
    for (int i = 0; i < LayerCount; ++i) {
        if (!m_layerActive[i]) continue;
        if (m_sizes[i].isEmpty()) continue;
        const int a = m_sizes[i].width() * m_sizes[i].height();
        if (a > bestArea) { bestArea = a; ref = m_sizes[i]; }
    }

    float sx = 1.0f;
    float sy = 1.0f;
    {
        const double srcA = double(ref.width()) / double(ref.height());
        const double dstA = double(dst.width()) / double(dst.height());
        if (srcA > dstA) sy = float(dstA / srcA);
        else             sx = float(srcA / dstA);
    }

    float ubo[kUboFloats];
    std::memset(ubo, 0, sizeof(ubo));

    const float t = float(m_elapsed.isValid() ? m_elapsed.elapsed() * 1e-3 : 0.0);
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
        ubo[base + 0] = m_opacity[i];
        ubo[base + 1] = float(static_cast<int>(m_copyMode[i]));
        const bool mixerActive = m_layerActive[i];
        ubo[base + 2] = mixerActive ? 1.0f : 0.0f;
        ubo[base + 3] = float(static_cast<int>(m_layerMatteRole[i]));

        const auto& pic = m_layerPicture[i];
        const int pA = kPicUvABase + i * 4;
        const float circAmp   = 0.1f * float(pic.circularMotion);
        const float circSpeed = 1.0f;
        ubo[pA + 0] = float(pic.zoom);
        ubo[pA + 1] = float(pic.rotationDeg) * (kPi / 180.0f);
        ubo[pA + 2] = circAmp;
        ubo[pA + 3] = circSpeed;

        const int pC = kPicColBase + i * 4;
        ubo[pC + 0] = float(pic.brightness);
        ubo[pC + 1] = float(pic.contrast);
        ubo[pC + 2] = float(pic.saturation);
        ubo[pC + 3] = float(static_cast<int>(pic.wrapMode));
    }

    const int kMixerCfgBase = kPicColBase + 4 * LayerCount;
    const bool isMainMixerUbuf = targetUbuf == m_ubuf.get();
    const bool feedbackActive = isMainMixerUbuf && hasActiveFeedbackLayer();
    if (feedbackActive) {
        ubo[kMixerCfgBase + 0] = float(LayerCount);
        ubo[kMixerCfgBase + 1] = float(m_activeFeedbackLayer);
        ubo[kMixerCfgBase + 2] = 1.0f;
        float mixerCfgW = 0.0f;
        const auto inputMode = activeFeedbackInputMode();
        if (inputMode == pvj::core::FeedbackInputMode::BelowOnly && feedbackKeyFromAboveActive()) {
            mixerCfgW = 1.0f;
        } else if (inputMode == pvj::core::FeedbackInputMode::StackComposite
                   || inputMode == pvj::core::FeedbackInputMode::SceneLoopback) {
            mixerCfgW = 2.0f;
        }
        ubo[kMixerCfgBase + 3] = mixerCfgW;
    } else {
        const int minLayer = (minLayerInclusive >= 0 && minLayerInclusive < LayerCount)
            ? minLayerInclusive : 0;
        const int maxLayer = (maxLayerExclusive >= 0 && maxLayerExclusive <= LayerCount)
            ? maxLayerExclusive : LayerCount;
        ubo[kMixerCfgBase + 0] = float(maxLayer);
        ubo[kMixerCfgBase + 1] = float(minLayer);
        ubo[kMixerCfgBase + 2] = 0.0f;
        ubo[kMixerCfgBase + 3] = 0.0f;
    }

    batch->updateDynamicBuffer(targetUbuf, 0, sizeof(ubo), ubo);
}

void RhiMixerWidget::updateFeedbackUniformBuffer(QRhiResourceUpdateBatch* batch, int feedbackLayer)
{
    if (!batch || !m_feedbackUbuf || feedbackLayer < 0 || feedbackLayer >= LayerCount) {
        return;
    }
    const auto& fb = m_layerFeedback[feedbackLayer];
    const float ubo[12] = {
        float(fb.loopRetention), float(fb.saturation), float(fb.brightness), float(fb.contrast),
        float(fb.hueShift), float(fb.gamma), float(fb.rotationDeg), float(fb.zoom),
        0.5f, 0.5f, float(fb.liveInject), float(static_cast<int>(fb.wrapMode)),
    };
    batch->updateDynamicBuffer(m_feedbackUbuf.get(), 0, sizeof(ubo), ubo);
}

bool RhiMixerWidget::anyLayerHasVideo() const
{
    for (int i = 0; i < LayerCount; ++i) {
        if (!m_layerActive[i]) continue;
        if (!m_sizes[i].isEmpty()) return true;
    }
    return false;
}

bool RhiMixerWidget::shouldDrawCenterLabel() const
{
    return !anyLayerHasVideo();
}

void RhiMixerWidget::render(QRhiCommandBuffer* cb)
{
    QRhi* r = rhi();
    if (!r || !m_pipelineReady || !m_pipeline) {
        return;
    }

    applyBackgroundLayerState();

    const QColor clear(0, 0, 0);
    const QSize vp = renderTarget()->pixelSize();
    const bool hasStage = m_stagePixelSize.isValid() && !m_stagePixelSize.isEmpty();
    const QSize stagePx = hasStage ? m_stagePixelSize : vp;
    const bool hasFilterChains = std::any_of(m_layerFilterChain.cbegin(), m_layerFilterChain.cend(),
                                             [](const auto& c) { return !c.isEmpty(); });
    const bool hasOutputFilters = !m_outputFilterChain.isEmpty();
    const int F = m_activeFeedbackLayer;
    const bool hasFeedback = hasActiveFeedbackLayer();
    const bool useOffscreen = hasStage || hasFilterChains || hasOutputFilters;
    const bool useOffscreenScene = useOffscreen || hasFeedback;
    const int mainMinLayer = 0;
    const int mainMaxLayer = -1;
    QRhiResourceUpdateBatch* batch = r->nextResourceUpdateBatch();
    uploadFramesIfNeeded(batch);
    if (hasFilterChains || hasOutputFilters) {
        ensureLayerFilterTargets(r, stagePx);
        if (hasOutputFilters) {
            ensureOutputFilterTargets(r, stagePx);
        }
    }

    if (!useOffscreenScene) {
        updateMixerUniformBuffer(batch, -1, 0);
        rebuildMixerShaderResourceBindings();
        cb->beginPass(renderTarget(), clear, { 1.0f, 0 }, batch);
        batch = nullptr;

        cb->setGraphicsPipeline(m_pipeline.get());
        cb->setViewport(QRhiViewport(0, 0, vp.width(), vp.height()));
        cb->setShaderResources();
        QRhiCommandBuffer::VertexInput vin(m_vbuf.get(), 0);
        cb->setVertexInput(0, 1, &vin);
        cb->draw(4);
        cb->endPass();
        return;
    }

    // Offscreen path: ensure scene target + present pipeline.
    if (!ensureOffscreenSceneTargets(r, stagePx)
        || !m_mixerOffscreenPipeline || !m_sceneRt) {
        rebuildMixerShaderResourceBindings();
        cb->beginPass(renderTarget(), clear, { 1.0f, 0 }, batch);
        batch = nullptr;
        if (!m_pipeline) {
            return;
        }
        cb->setGraphicsPipeline(m_pipeline.get());
        cb->setViewport(QRhiViewport(0, 0, vp.width(), vp.height()));
        cb->setShaderResources();
        QRhiCommandBuffer::VertexInput vin(m_vbuf.get(), 0);
        cb->setVertexInput(0, 1, &vin);
        cb->draw(4);
        cb->endPass();
        return;
    }

    ensurePresentPipelineForSwapchain(r);
    rebuildPresentShaderResourceBindings(m_sceneTex.get());
    updatePresentUniformBuffer(batch, vp);

    // Apply the batch before emitting per-pass commands.
    cb->resourceUpdate(batch);
    batch = nullptr;

    // 0. Per-layer filter prepass (from raw layer texture; feedback keyed after feedback pass).
    if (hasFilterChains) {
        for (int i = 0; i < LayerCount; ++i) {
            const auto& chain = m_layerFilterChain[i];
            if (chain.isEmpty()) {
                m_layerFilterLastOut[i] = -1;
                continue;
            }
            if (hasFeedback && i == F) {
                m_layerFilterLastOut[i] = -1;
                continue;
            }
            runPerLayerFilterChain(r, cb, i, layerBaseTextureForInput(i), stagePx, clear);
        }
    } else {
        for (int i = 0; i < LayerCount; ++i) {
            m_layerFilterLastOut[i] = -1;
        }
    }

    if (hasFeedback) {
        if (ensureFeedbackTargets(r, stagePx)) {
            const auto inputMode = activeFeedbackInputMode();
            QRhiTexture* feedbackBelow = m_belowTex.get();
            QRhiTexture* historyRead = feedbackReadTexture();
            QList<pvj::core::CellFilterNode> preChain;
            QList<pvj::core::CellFilterNode> postChain;
            pvj::core::splitFilterChainAtFeedbackMarker(m_layerFilterChain[F], &preChain, &postChain);

            if (inputMode == pvj::core::FeedbackInputMode::SceneLoopback) {
                feedbackBelow = sceneHistReadTexture();
                if (!feedbackBelow) {
                    feedbackBelow = m_belowTex.get();
                }
            } else if (inputMode == pvj::core::FeedbackInputMode::StackComposite) {
                runPartialMixerPass(r, cb, 0, F, m_belowRt.get(), stagePx, clear);
                const bool hasLayersAbove = std::any_of(
                    m_layerActive.cbegin() + F + 1, m_layerActive.cend(),
                    [](bool active) { return active; });
                if (hasLayersAbove && m_aboveRt) {
                    runPartialMixerPass(r, cb, F + 1, LayerCount, m_aboveRt.get(), stagePx, clear);
                    runStackCombinePass(r, cb, stagePx, clear);
                    feedbackBelow = m_stackTex.get();
                }
            } else {
                runPartialMixerPass(r, cb, 0, F, m_belowRt.get(), stagePx, clear);
            }

            QRhiTexture* filteredBelow = feedbackBelow;
            if (feedbackBelow && !preChain.isEmpty()) {
                runPerLayerFilterChain(r, cb, F, feedbackBelow, stagePx, clear, &preChain);
                if (QRhiTexture* preFiltered = filterOutputTextureForLayer(F)) {
                    filteredBelow = preFiltered;
                }
                m_layerFilterLastOut[F] = -1;
            }

            if (filteredBelow && historyRead) {
                runFeedbackPass(r, cb, F, stagePx, clear, filteredBelow, historyRead,
                                feedbackWriteRenderTarget());
            }

            if (!postChain.isEmpty()) {
                if (QRhiTexture* fbOut = feedbackWriteTexture()) {
                    runPerLayerFilterChain(r, cb, F, fbOut, stagePx, clear, &postChain);
                }
            } else {
                m_layerFilterLastOut[F] = -1;
            }
        }
    }

    // Build the mixer SRB exactly once per frame — feedback targets must already exist.
    rebuildMixerShaderResourceBindings();

    {
        QRhiResourceUpdateBatch* ubatch = r->nextResourceUpdateBatch();
        updateMixerUniformBuffer(ubatch, mainMaxLayer, mainMinLayer);
        cb->resourceUpdate(ubatch);
    }

    cb->beginPass(m_sceneRt.get(), clear, { 1.0f, 0 }, nullptr);
    cb->setGraphicsPipeline(m_mixerOffscreenPipeline.get());
    cb->setViewport(QRhiViewport(0, 0, stagePx.width(), stagePx.height()));
    cb->setShaderResources();
    {
        QRhiCommandBuffer::VertexInput vin(m_vbuf.get(), 0);
        cb->setVertexInput(0, 1, &vin);
    }
    cb->draw(4);
    cb->endPass();

    if (hasFeedback) {
        copySceneToHistory(r, cb, stagePx);
        m_feedbackWriteIdx = quint8((m_feedbackWriteIdx + 1) % kFeedbackRingSize);
        m_sceneHistWriteIdx = quint8(1 - m_sceneHistWriteIdx);
    }

    QRhiTexture* presentTex = m_sceneTex.get();
    if (hasOutputFilters) {
        runOutputFilterChain(r, cb, stagePx, clear);
        presentTex = outputFilterResultTexture();
        rebuildPresentShaderResourceBindings(presentTex);
    }

    // Present pass: letterbox the scene onto the swapchain.
    cb->beginPass(renderTarget(), clear, { 1.0f, 0 }, nullptr);
    cb->setGraphicsPipeline(m_presentPipeline.get());
    cb->setViewport(QRhiViewport(0, 0, vp.width(), vp.height()));
    cb->setShaderResources();
    {
        QRhiCommandBuffer::VertexInput vin(m_vbuf.get(), 0);
        cb->setVertexInput(0, 1, &vin);
    }
    cb->draw(4);
    cb->endPass();

}

void RhiMixerWidget::paintEvent(QPaintEvent* event)
{
    QRhiWidget::paintEvent(event);

    if (m_label.isEmpty()) return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QColor(230, 230, 230, 220));
    QFont f = p.font();
    f.setBold(true);
    if (shouldDrawCenterLabel()) {
        f.setPointSizeF(f.pointSizeF() * 1.4);
        p.setFont(f);
        p.drawText(rect(), Qt::AlignCenter, m_label);
    } else {
        p.setFont(f);
        p.drawText(rect().adjusted(8, 6, -8, -6), Qt::AlignTop | Qt::AlignLeft, m_label);
    }
}

} // namespace pvj::render
