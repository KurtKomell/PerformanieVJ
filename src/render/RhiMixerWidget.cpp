#include "RhiMixerWidget.h"
#include "ShaderLibrary.h"

#include "core/FilterParamSchema.h"

#include <rhi/qshader.h>
#include <rhi/qrhi.h>

#include <QFile>
#include <QPainter>
#include <QPaintEvent>
#include <QVector>

#include <algorithm>
#include <cstring>

namespace pvj::render {

namespace {

bool pictureParamsEqual(const pvj::core::PictureParams& a, const pvj::core::PictureParams& b)
{
    return a.zoom == b.zoom && a.rotationDeg == b.rotationDeg && a.brightness == b.brightness
        && a.contrast == b.contrast && a.saturation == b.saturation
        && a.circularMotion == b.circularMotion;
}

bool filterChainShallowEqual(const QList<pvj::core::CellFilterNode>& a,
                             const QList<pvj::core::CellFilterNode>& b)
{
    if (a.size() != b.size()) {
        return false;
    }
    for (int i = 0; i < a.size(); ++i) {
        if (a.at(i).typeId != b.at(i).typeId) {
            return false;
        }
        const auto& pa = a.at(i).params;
        const auto& pb = b.at(i).params;
        if (pa.size() != pb.size()) {
            return false;
        }
        for (int j = 0; j < pa.size(); ++j) {
            if (pa.at(j).name != pb.at(j).name) {
                return false;
            }
            if (pa.at(j).value != pb.at(j).value) {
                return false;
            }
        }
    }
    return true;
}

bool feedbackParamsEqual(const pvj::core::FeedbackParams& a, const pvj::core::FeedbackParams& b)
{
    return a.enabled == b.enabled
        && a.strength == b.strength
        && a.zoom == b.zoom
        && a.rotationDeg == b.rotationDeg
        && a.rotationAnimated == b.rotationAnimated
        && a.decay == b.decay
        && a.brightness == b.brightness
        && a.saturation == b.saturation
        && a.gamma == b.gamma
        && a.contrast == b.contrast
        && a.layerBrightness == b.layerBrightness
        && a.layerSaturation == b.layerSaturation
        && a.layerGamma == b.layerGamma
        && a.layerContrast == b.layerContrast
        && a.wrapMode == b.wrapMode;
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
                          QRhiShaderResourceBindings* srb,
                          QRhiRenderPassDescriptor* rp,
                          std::unique_ptr<QRhiGraphicsPipeline>& out)
{
    QShader vs = loadShader(QStringLiteral(":/shaders/textured_quad.vert.qsb"));
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

} // namespace

RhiMixerWidget::RhiMixerWidget(QWidget* parent)
    : QRhiWidget(parent)
{
    setMinimumSize(320, 180);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setAutoRenderTarget(true);

    for (int i = 0; i < LayerCount; ++i) {
        m_opacity[i]     = 1.0f;
        m_copyMode[i]    = pvj::core::CopyMode::Normal;
        m_layerActive[i] = false;
        m_layerFilterLastOut[i] = -1;
        m_layerHistReadIdx[i] = 0;
        m_layerFbHistoryValid[i] = false;
        m_layerKeyChannel[static_cast<size_t>(i)] = { 1.0f, 1.0f, 1.0f };
    }
    m_elapsed.start();
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
    if (layer < 0 || layer >= LayerCount) return;
    if (m_layerActive[layer] != active) {
        m_layerActive[layer] = active;
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

void RhiMixerWidget::setLayerFeedback(int layer, const pvj::core::FeedbackParams& p)
{
    if (layer < 0 || layer >= LayerCount) return;
    if (feedbackParamsEqual(m_layerFeedback[layer], p)) {
        return;
    }
    const bool reenable = !m_layerFeedback[layer].enabled && p.enabled;
    m_layerFeedback[layer] = p;
    if (reenable) {
        m_layerFbHistoryValid[layer] = false;
    }
    update();
}

pvj::core::FeedbackParams RhiMixerWidget::layerFeedback(int layer) const
{
    if (layer < 0 || layer >= LayerCount) return {};
    return m_layerFeedback[layer];
}

void RhiMixerWidget::setLayerFilterChain(int layer, const QList<pvj::core::CellFilterNode>& chain)
{
    if (layer < 0 || layer >= LayerCount) {
        return;
    }
    if (filterChainShallowEqual(m_layerFilterChain[layer], chain)) {
        return;
    }
    m_layerFilterChain[layer] = chain;
    m_layerFilterLastOut[layer] = -1;
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

void RhiMixerWidget::setFrame(int layer, QImage frame, qint64 /*pts*/)
{
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
    if (layer < 0 || layer >= LayerCount) return;
    m_pending[layer] = QImage();
    m_dirty[layer]   = true;
    m_sizes[layer]   = QSize();
    update();
}

void RhiMixerWidget::releaseOffscreenGpuResources()
{
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
    for (auto& pair : m_layerHistRt) {
        for (auto& rt : pair) {
            rt.reset();
        }
    }
    for (auto& pair : m_layerHistTex) {
        for (auto& tex : pair) {
            tex.reset();
        }
    }
    for (auto& idx : m_layerHistReadIdx) {
        idx = 0;
    }
    for (auto& valid : m_layerFbHistoryValid) {
        valid = false;
    }
    for (auto& rt : m_layerFeedbackRt) {
        rt.reset();
    }
    for (auto& tex : m_layerFeedbackTex) {
        tex.reset();
    }
    for (auto& srb : m_layerFeedbackSrb) {
        srb.reset();
    }
    for (auto& ubuf : m_layerFeedbackUbuf) {
        ubuf.reset();
    }
    m_layerFeedbackPipeline.reset();
    m_layerFeedbackRp.reset();
    m_feedbackPixelSize = {};
    m_filterPixelSize = {};
    m_filterRp.reset();
    m_filterPipelineByTypeId.clear();
    m_filterPipelineOwned.clear();
}

void RhiMixerWidget::releaseGpuResources()
{
    releaseOffscreenGpuResources();
    m_pipeline.reset();
    m_srb.reset();
    for (auto& t : m_tex) t.reset();
    m_samplerFeedbackRepeat.reset();
    m_samplerFeedbackMirror.reset();
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
    if (m_layerFeedback[layer].enabled && m_layerFbHistoryValid[layer]) {
        const int readIdx = qBound(0, m_layerHistReadIdx[layer], 1);
        if (m_layerHistTex[layer][readIdx]) {
            return m_layerHistTex[layer][readIdx].get();
        }
    }
    return m_tex[layer].get();
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

void RhiMixerWidget::rebuildMixerShaderResourceBindings(QRhiTexture* underComposite)
{
    if (!m_srb) return;

    QRhiTexture* under = underComposite ? underComposite : m_tex[0].get();
    QVector<QRhiShaderResourceBinding> binds;
    binds.reserve(2 + LayerCount);
    binds.append(QRhiShaderResourceBinding::uniformBuffer(
        0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
        m_ubuf.get()));
    for (int i = 0; i < LayerCount; ++i) {
        QRhiTexture* src = sourceTextureForLayer(i);
        if (!src) src = m_tex[i].get();
        binds.append(QRhiShaderResourceBinding::sampledTexture(
            1 + i, QRhiShaderResourceBinding::FragmentStage,
            src, m_sampler.get()));
    }
    binds.append(QRhiShaderResourceBinding::sampledTexture(
        13, QRhiShaderResourceBinding::FragmentStage,
        under, m_sampler.get()));
    m_srb->setBindings(binds.cbegin(), binds.cend());
    m_srb->create();
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

bool RhiMixerWidget::ensureLayerFeedbackTargets(QRhi* r, const QSize& pixelSize)
{
    if (!r || pixelSize.isEmpty()) {
        return false;
    }
    if (m_feedbackPixelSize != pixelSize) {
        for (auto& pair : m_layerHistRt) {
            for (auto& rt : pair) {
                rt.reset();
            }
        }
        for (auto& pair : m_layerHistTex) {
            for (auto& tex : pair) {
                tex.reset();
            }
        }
        for (auto& idx : m_layerHistReadIdx) {
            idx = 0;
        }
        for (auto& valid : m_layerFbHistoryValid) {
            valid = false;
        }
        m_layerFeedbackRp.reset();
        m_layerFeedbackPipeline.reset();
        m_feedbackPixelSize = pixelSize;
    }

    bool needed = false;
    for (int i = 0; i < LayerCount; ++i) {
        if (!m_layerFeedback[i].enabled) {
            continue;
        }
        needed = true;
        for (int k = 0; k < 2; ++k) {
            if (!m_layerHistTex[i][k]) {
                m_layerHistTex[i][k].reset(
                    r->newTexture(QRhiTexture::RGBA8, pixelSize, 1, QRhiTexture::RenderTarget));
                if (!m_layerHistTex[i][k]->create()) {
                    return false;
                }
            }
            if (!m_layerHistRt[i][k]) {
                m_layerHistRt[i][k].reset(r->newTextureRenderTarget(
                    QRhiTextureRenderTargetDescription(QRhiColorAttachment(m_layerHistTex[i][k].get()))));
                if (!m_layerFeedbackRp) {
                    m_layerFeedbackRp.reset(m_layerHistRt[i][k]->newCompatibleRenderPassDescriptor());
                }
                m_layerHistRt[i][k]->setRenderPassDescriptor(m_layerFeedbackRp.get());
                if (!m_layerHistRt[i][k]->create()) {
                    return false;
                }
            }
        }
        if (!m_layerFeedbackUbuf[i]) {
            m_layerFeedbackUbuf[i].reset(
                r->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(float) * 16));
            if (!m_layerFeedbackUbuf[i]->create()) {
                return false;
            }
        }
        if (!m_layerFeedbackSrb[i]) {
            m_layerFeedbackSrb[i].reset(r->newShaderResourceBindings());
        }
    }
    if (needed && !m_layerFeedbackPipeline) {
        int firstLayer = -1;
        for (int i = 0; i < LayerCount; ++i) {
            if (m_layerFeedback[i].enabled) {
                firstLayer = i;
                break;
            }
        }
        if (firstLayer >= 0) {
            std::unique_ptr<QRhiGraphicsPipeline> pipeline;
            if (!createEffectPipeline(r, QStringLiteral(":/shaders/layer_feedback.frag.qsb"),
                                      m_layerFeedbackSrb[firstLayer].get(), m_layerFeedbackRp.get(), pipeline)) {
                return false;
            }
            m_layerFeedbackPipeline = std::move(pipeline);
        }
    }
    return needed;
}

void RhiMixerWidget::updateLayerFeedbackUniformBuffer(QRhiResourceUpdateBatch* batch, int layer,
                                                      const QSize& pixelSize)
{
    if (!batch || layer < 0 || layer >= LayerCount || !m_layerFeedbackUbuf[layer]) {
        return;
    }
    const auto& fb = m_layerFeedback[layer];
    const float t = float(m_elapsed.isValid() ? m_elapsed.elapsed() * 1e-3 : 0.0);
    const float rot = float(fb.rotationAnimated ? fb.rotationDeg * t : fb.rotationDeg);
    const float ubo[16] = {
        float(qBound(0.0, fb.strength, 1.0)),
        float(qBound(-0.1, fb.decay, 0.1)),
        float(qBound(0.5, fb.zoom, 2.0)),
        rot * (3.14159265358979323846f / 180.0f),
        float(qBound(-1.0, fb.brightness, 1.0)),
        float(qBound(0.0, fb.saturation, 2.0)),
        float(qBound(0.1, fb.gamma, 4.0)),
        float(qBound(0.0, fb.contrast, 2.0)),
        float(qBound(-1.0, fb.layerBrightness, 1.0)),
        float(qBound(0.0, fb.layerSaturation, 2.0)),
        float(qBound(0.1, fb.layerGamma, 4.0)),
        float(qBound(0.0, fb.layerContrast, 2.0)),
        0.f,
        0.f,
        0.f,
        0.f,
    };
    Q_UNUSED(pixelSize);
    batch->updateDynamicBuffer(m_layerFeedbackUbuf[layer].get(), 0, sizeof(ubo), ubo);
}

QRhiSampler* RhiMixerWidget::feedbackHistorySampler(pvj::core::WrapMode wrap) const
{
    switch (wrap) {
    case pvj::core::WrapMode::Repeat:
    case pvj::core::WrapMode::Tile:
        return m_samplerFeedbackRepeat.get() ? m_samplerFeedbackRepeat.get() : m_sampler.get();
    case pvj::core::WrapMode::Mirror:
        return m_samplerFeedbackMirror.get() ? m_samplerFeedbackMirror.get() : m_sampler.get();
    case pvj::core::WrapMode::Clamp:
    default:
        return m_sampler.get();
    }
}

QRhiGraphicsPipeline* RhiMixerWidget::ensureFilterPipeline(QRhi* r, const QString& typeId,
                                                           QRhiShaderResourceBindings* srb,
                                                           QRhiRenderPassDescriptor* rp)
{
    const QString key = typeId.toLower();
    auto it = m_filterPipelineByTypeId.find(key);
    if (it != m_filterPipelineByTypeId.end() && it.value()) {
        return it.value();
    }
    std::unique_ptr<QRhiGraphicsPipeline> pipeline;
    if (!createEffectPipeline(r, effectFragmentShaderForType(key), srb, rp, pipeline)) {
        return nullptr;
    }
    QRhiGraphicsPipeline* raw = pipeline.get();
    m_filterPipelineOwned.push_back(std::move(pipeline));
    m_filterPipelineByTypeId.insert(key, raw);
    return raw;
}

void RhiMixerWidget::runPerLayerFilterChain(QRhi* r, QRhiCommandBuffer* cb, int i,
                                            QRhiTexture* firstSource, const QSize& stagePx,
                                            const QColor& clear, bool copyHistoryToFilterScratch)
{
    m_layerFilterLastOut[i] = -1;
    if (!r || !cb || !firstSource) {
        return;
    }
    if (!m_layerFilterPingRt[i][0] || !m_layerFilterPingRt[i][1]) {
        return;
    }
    const auto& chain = m_layerFilterChain[i];
    QRhiTexture* sourceTex = firstSource;
    int writeIdx = 0;
    if (copyHistoryToFilterScratch) {
        // The source may have been a render target in the pass above; sample it as a
        // texture in the same command buffer. Copy through an intermediate RT
        // (filter ping[0]) so the rest of the chain reads from a plain texture,
        // avoiding RT/texture hazards and visible flicker on some RHI backends.
        pvj::core::CellFilterNode passNode;
        passNode.typeId = QStringLiteral("__identity_hazard_copy__");
        {
            auto ubuf = std::unique_ptr<QRhiBuffer>(
                r->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(float) * 12));
            if (!ubuf->create()) {
                return;
            }
            auto srb = std::unique_ptr<QRhiShaderResourceBindings>(r->newShaderResourceBindings());
            srb->setBindings({
                QRhiShaderResourceBinding::uniformBuffer(
                    0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
                    ubuf.get()),
                QRhiShaderResourceBinding::sampledTexture(
                    1, QRhiShaderResourceBinding::FragmentStage, firstSource, m_sampler.get()),
                QRhiShaderResourceBinding::sampledTexture(
                    2, QRhiShaderResourceBinding::FragmentStage, firstSource, m_sampler.get()),
            });
            srb->create();
            QRhiGraphicsPipeline* copyPipeline = ensureFilterPipeline(
                r, passNode.typeId, srb.get(), m_layerFilterPingRt[i][0]->renderPassDescriptor());
            if (!copyPipeline) {
                return;
            }
            QRhiResourceUpdateBatch* ubatch = r->nextResourceUpdateBatch();
            updateFilterUniformBuffer(ubatch, ubuf.get(), passNode, stagePx, i);
            cb->resourceUpdate(ubatch);
            cb->beginPass(m_layerFilterPingRt[i][0].get(), clear, { 1.0f, 0 }, nullptr);
            cb->setGraphicsPipeline(copyPipeline);
            cb->setViewport(QRhiViewport(0, 0, stagePx.width(), stagePx.height()));
            cb->setShaderResources(srb.get());
            QRhiCommandBuffer::VertexInput vin(m_vbuf.get(), 0);
            cb->setVertexInput(0, 1, &vin);
            cb->draw(4);
            cb->endPass();
        }
        sourceTex = m_layerFilterPingTex[i][0].get();
        writeIdx = 1;
    }
    for (const auto& node : chain) {
        auto ubuf = std::unique_ptr<QRhiBuffer>(
            r->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(float) * 12));
        if (!ubuf->create()) {
            continue;
        }
        auto srb = std::unique_ptr<QRhiShaderResourceBindings>(r->newShaderResourceBindings());
        srb->setBindings({
            QRhiShaderResourceBinding::uniformBuffer(
                0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
                ubuf.get()),
            QRhiShaderResourceBinding::sampledTexture(
                1, QRhiShaderResourceBinding::FragmentStage, sourceTex, m_sampler.get()),
            QRhiShaderResourceBinding::sampledTexture(
                2, QRhiShaderResourceBinding::FragmentStage, sourceTex, m_sampler.get()),
        });
        srb->create();
        QRhiGraphicsPipeline* pipeline = ensureFilterPipeline(
            r, node.typeId, srb.get(), m_layerFilterPingRt[i][writeIdx]->renderPassDescriptor());
        if (!pipeline) {
            continue;
        }
        QRhiResourceUpdateBatch* ubatch = r->nextResourceUpdateBatch();
        updateFilterUniformBuffer(ubatch, ubuf.get(), node, stagePx, i);
        cb->resourceUpdate(ubatch);

        cb->beginPass(m_layerFilterPingRt[i][writeIdx].get(), clear, { 1.0f, 0 }, nullptr);
        cb->setGraphicsPipeline(pipeline);
        cb->setViewport(QRhiViewport(0, 0, stagePx.width(), stagePx.height()));
        cb->setShaderResources(srb.get());
        QRhiCommandBuffer::VertexInput vin(m_vbuf.get(), 0);
        cb->setVertexInput(0, 1, &vin);
        cb->draw(4);
        cb->endPass();

        sourceTex = m_layerFilterPingTex[i][writeIdx].get();
        m_layerFilterLastOut[i] = writeIdx;
        writeIdx = 1 - writeIdx;
    }
}

void RhiMixerWidget::updateFilterUniformBuffer(QRhiResourceUpdateBatch* batch, QRhiBuffer* ubuf,
                                               const pvj::core::CellFilterNode& node, const QSize& pixelSize,
                                               int layerIndex)
{
    if (!batch || !ubuf) {
        return;
    }
    const auto schemaIt = pvj::core::filterParamSchemas().find(node.typeId);
    float filterUbo[12] = { 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    filterUbo[2] = float(m_elapsed.isValid() ? m_elapsed.elapsed() * 1e-3 : 0.0);
    filterUbo[3] = float(pixelSize.width()) / float(qMax(pixelSize.height(), 1));
    if (schemaIt != pvj::core::filterParamSchemas().end()) {
        for (int i = 0; i < schemaIt.value().params.size() && i < 4; ++i) {
            const auto& spec = schemaIt.value().params[i];
            double v = spec.defaultV;
            for (const auto& p : node.params) {
                if (p.name == spec.name) {
                    v = p.value;
                    break;
                }
            }
            if (spec.kind == pvj::core::FilterParamKind::Angle) {
                v = v * 3.14159265358979323846 / 180.0;
            } else if (spec.kind == pvj::core::FilterParamKind::Bool) {
                v = v >= 0.5 ? 1.0 : 0.0;
            }
            filterUbo[8 + i] = float(v);
        }
    }
    const QString typeIdLower = node.typeId.toLower();
    const bool isBlur = (typeIdLower == QStringLiteral("blur")
                         || typeIdLower == QStringLiteral("fast_blur")
                         || typeIdLower == QStringLiteral("gaussian_blur")
                         || typeIdLower == QStringLiteral("box_blur")
                         || typeIdLower == QStringLiteral("temporal_blur")
                         || typeIdLower == QStringLiteral("radial_blur")
                         || typeIdLower == QStringLiteral("directional_blur")
                         || typeIdLower == QStringLiteral("motion_blur")
                         || typeIdLower == QStringLiteral("zoom_blur"));
    if (isBlur) {
        // Blur shader expects UV-space radius in ubuf.params.x (small values).
        // Node params are authored as "radius" in pixels and "amount" in 0..1.
        double amount = 1.0;
        double radiusPx = 0.0;
        bool hasRadius = false;
        for (const auto& p : node.params) {
            if (p.name == QStringLiteral("amount")) {
                amount = p.value;
            } else if (p.name == QStringLiteral("radius")) {
                radiusPx = p.value;
                hasRadius = true;
            }
        }
        const double minDim = double(qMax(1, qMin(pixelSize.width(), pixelSize.height())));
        if (!hasRadius) {
            // Legacy chains using only amount: map to practical UV blur range.
            radiusPx = qBound(0.0, amount, 1.0) * 16.0;
        }
        const double uvRadius = qBound(0.0, (radiusPx / minDim) * qBound(0.0, amount, 1.0), 0.08);
        filterUbo[8] = float(uvRadius);
        filterUbo[9] = float(radiusPx);
    }
    const bool isChromaKey = (typeIdLower == QStringLiteral("chroma_key"));
    const bool isLumaKey   = (typeIdLower == QStringLiteral("luma_key"));
    if (isChromaKey || isLumaKey) {
        float kr = 1.0f;
        float kg = 1.0f;
        float kb = 1.0f;
        if (layerIndex >= 0 && layerIndex < LayerCount) {
            const auto& w = m_layerKeyChannel[static_cast<size_t>(layerIndex)];
            kr = w[0];
            kg = w[1];
            kb = w[2];
        }
        filterUbo[4] = kr;
        filterUbo[5] = kg;
        filterUbo[6] = kb;
        filterUbo[7] = isLumaKey ? 1.0f : 0.0f;
    }
    batch->updateDynamicBuffer(ubuf, 0, sizeof(filterUbo), filterUbo);
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

    m_samplerFeedbackRepeat.reset(r->newSampler(QRhiSampler::Linear, QRhiSampler::Linear,
                                                QRhiSampler::None,
                                                QRhiSampler::Repeat,
                                                QRhiSampler::Repeat));
    m_samplerFeedbackRepeat->create();
    m_samplerFeedbackMirror.reset(r->newSampler(QRhiSampler::Linear, QRhiSampler::Linear,
                                                QRhiSampler::None,
                                                QRhiSampler::Mirror,
                                                QRhiSampler::Mirror));
    m_samplerFeedbackMirror->create();

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

void RhiMixerWidget::updateMixerUniformBuffer(QRhiResourceUpdateBatch* batch)
{
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
        ubo[base + 3] = 0.0f;

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
        ubo[pC + 3] = 0.0f;
    }

    const int kMixerCfgBase = kPicColBase + 4 * LayerCount;
    ubo[kMixerCfgBase + 0] = 0.0f;
    ubo[kMixerCfgBase + 1] = 0.0f;
    ubo[kMixerCfgBase + 2] = 0.0f;
    ubo[kMixerCfgBase + 3] = 0.0f;

    batch->updateDynamicBuffer(m_ubuf.get(), 0, sizeof(ubo), ubo);
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

    const QColor clear(12, 12, 12);
    const QSize vp = renderTarget()->pixelSize();
    const bool hasStage = m_stagePixelSize.isValid() && !m_stagePixelSize.isEmpty();
    const QSize stagePx = hasStage ? m_stagePixelSize : vp;
    const bool hasFilterChains = std::any_of(m_layerFilterChain.cbegin(), m_layerFilterChain.cend(),
                                             [](const auto& c) { return !c.isEmpty(); });
    const bool hasFeedbackLayers = std::any_of(m_layerFeedback.cbegin(), m_layerFeedback.cend(),
                                               [](const auto& fb) { return fb.enabled; });
    const bool useOffscreen = hasStage || hasFilterChains || hasFeedbackLayers;

    QRhiResourceUpdateBatch* batch = r->nextResourceUpdateBatch();
    uploadFramesIfNeeded(batch);
    updateMixerUniformBuffer(batch);
    if (hasFeedbackLayers) {
        ensureLayerFeedbackTargets(r, stagePx);
    }
    if (hasFilterChains) {
        ensureLayerFilterTargets(r, stagePx);
    }

    if (!useOffscreen) {
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

    // 0. Per-layer feedback history prepass.
    if (hasFeedbackLayers && m_layerFeedbackPipeline) {
        for (int i = 0; i < LayerCount; ++i) {
            if (!m_layerFeedback[i].enabled || !m_layerHistRt[i][0] || !m_layerHistRt[i][1]
                || !m_layerFeedbackSrb[i] || !m_layerFeedbackUbuf[i]) {
                continue;
            }
            const int readIdx = qBound(0, m_layerHistReadIdx[i], 1);
            const int writeIdx = 1 - readIdx;
            QRhiTexture* historyTex = m_layerHistTex[i][readIdx].get();
            QRhiTexture* curTex = m_tex[i].get();
            if (!historyTex || !curTex) {
                continue;
            }

            QRhiSampler* const histSampler = feedbackHistorySampler(m_layerFeedback[i].wrapMode);
            m_layerFeedbackSrb[i]->setBindings({
                QRhiShaderResourceBinding::uniformBuffer(
                    0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
                    m_layerFeedbackUbuf[i].get()),
                QRhiShaderResourceBinding::sampledTexture(
                    1, QRhiShaderResourceBinding::FragmentStage, curTex, m_sampler.get()),
                QRhiShaderResourceBinding::sampledTexture(
                    2, QRhiShaderResourceBinding::FragmentStage, historyTex, histSampler),
            });
            m_layerFeedbackSrb[i]->create();
            QRhiResourceUpdateBatch* fbBatch = r->nextResourceUpdateBatch();
            updateLayerFeedbackUniformBuffer(fbBatch, i, stagePx);
            cb->resourceUpdate(fbBatch);

            cb->beginPass(m_layerHistRt[i][writeIdx].get(), clear, { 1.0f, 0 }, nullptr);
            cb->setGraphicsPipeline(m_layerFeedbackPipeline.get());
            cb->setViewport(QRhiViewport(0, 0, stagePx.width(), stagePx.height()));
            cb->setShaderResources(m_layerFeedbackSrb[i].get());
            QRhiCommandBuffer::VertexInput vin(m_vbuf.get(), 0);
            cb->setVertexInput(0, 1, &vin);
            cb->draw(4);
            cb->endPass();

            m_layerHistReadIdx[i] = writeIdx;
            m_layerFbHistoryValid[i] = true;
        }
    }

    // 1. Per-layer filter prepass.
    if (hasFilterChains) {
        for (int i = 0; i < LayerCount; ++i) {
            const auto& chain = m_layerFilterChain[i];
            if (chain.isEmpty()) {
                m_layerFilterLastOut[i] = -1;
                continue;
            }
            QRhiTexture* first = m_tex[i].get();
            if (m_layerFeedback[i].enabled && m_layerFbHistoryValid[i]) {
                const int readIdx = qBound(0, m_layerHistReadIdx[i], 1);
                if (m_layerHistTex[i][readIdx]) {
                    first = m_layerHistTex[i][readIdx].get();
                }
            }
            runPerLayerFilterChain(r, cb, i, first, stagePx, clear, false);
        }
    } else {
        for (int i = 0; i < LayerCount; ++i) {
            m_layerFilterLastOut[i] = -1;
        }
    }
    rebuildMixerShaderResourceBindings(m_tex[0].get());
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

    // 3. Present pass: letterbox the scene onto the swapchain.
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
