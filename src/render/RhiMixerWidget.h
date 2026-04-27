#pragma once

#include "core/Model.h"

#include <QColor>
#include <QElapsedTimer>
#include <QHash>
#include <QImage>
#include <QRhiWidget>
#include <QString>

#include <array>
#include <memory>

QT_BEGIN_NAMESPACE
class QRhi;
class QRhiBuffer;
class QRhiCommandBuffer;
class QRhiGraphicsPipeline;
class QRhiRenderPassDescriptor;
class QRhiResourceUpdateBatch;
class QRhiSampler;
class QRhiShaderResourceBindings;
class QRhiTexture;
class QRhiTextureRenderTarget;
QT_END_NAMESPACE

namespace pvj::render {

// Stacks up to 12 video layers (bottom = layer 0, top = layer 11) with per-layer
// copy mode and opacity.
class RhiMixerWidget : public QRhiWidget
{
    Q_OBJECT
public:
    static constexpr int LayerCount = 12;

    explicit RhiMixerWidget(QWidget* parent = nullptr);
    ~RhiMixerWidget() override;

    void setLabel(const QString& text);
    QString label() const { return m_label; }

    void setLayerOpacity(int layer, float o);
    void setLayerCopyMode(int layer, pvj::core::CopyMode mode);
    void setLayerActive(int layer, bool active);

    float layerOpacity(int layer) const;
    pvj::core::CopyMode layerCopyMode(int layer) const;
    bool layerActive(int layer) const;

    /// Per-layer picture parameters (reserved; not currently consumed by the
    /// mixer fragment pipeline but kept so the model can round-trip values).
    void setLayerPicture(int layer, const pvj::core::PictureParams& p);
    pvj::core::PictureParams layerPicture(int layer) const;
    void setLayerFeedback(int layer, const pvj::core::FeedbackParams& p);
    pvj::core::FeedbackParams layerFeedback(int layer) const;

    void setLayerFilterChain(int layer, const QList<pvj::core::CellFilterNode>& chain);
    /// Weights for chroma/luma key filters (cell inspector key R / G / B), 0–1 each.
    void setLayerKeyChannels(int layer, float r, float g, float b);

    /// Fixed "stage" resolution (in pixels) at which the internal mixer renders,
    /// independent of this widget's size. When set,
    /// the widget presents the stage texture letterboxed/pillarboxed onto its
    /// own surface. Call with an invalid (empty) QSize to restore the legacy
    /// behaviour of rendering directly at the widget's pixel size.
    void setStagePixelSize(QSize px);
    QSize stagePixelSize() const { return m_stagePixelSize; }

public slots:
    void setFrame(int layer, QImage frame, qint64 pts = 0);
    void clearFrame(int layer);

protected:
    void initialize(QRhiCommandBuffer* cb) override;
    void render(QRhiCommandBuffer* cb) override;
    void releaseResources() override;
    void paintEvent(QPaintEvent* event) override;

private:
    void releaseGpuResources();
    void releaseOffscreenGpuResources();

    bool ensureOffscreenSceneTargets(QRhi* r, const QSize& pixelSize);
    void ensurePresentPipelineForSwapchain(QRhi* r);

    /// \a underComposite bound at slot 13; nullptr uses a placeholder (unused when mixerPhase is 0).
    void rebuildMixerShaderResourceBindings(QRhiTexture* underComposite = nullptr);
    void rebuildPresentShaderResourceBindings(QRhiTexture* sourceTex);

    void uploadFramesIfNeeded(QRhiResourceUpdateBatch* batch);
    void updateMixerUniformBuffer(QRhiResourceUpdateBatch* batch);
    void updatePresentUniformBuffer(QRhiResourceUpdateBatch* batch, const QSize& widgetPx);
    void updateFilterUniformBuffer(QRhiResourceUpdateBatch* batch, QRhiBuffer* ubuf,
                                   const pvj::core::CellFilterNode& node, const QSize& pixelSize,
                                   int layerIndex);
    bool ensureLayerFilterTargets(QRhi* r, const QSize& pixelSize);
    bool ensureLayerFeedbackTargets(QRhi* r, const QSize& pixelSize);
    QRhiGraphicsPipeline* ensureFilterPipeline(QRhi* r, const QString& typeId,
                                               QRhiShaderResourceBindings* srb,
                                               QRhiRenderPassDescriptor* rp);
    void updateLayerFeedbackUniformBuffer(QRhiResourceUpdateBatch* batch, int layer, const QSize& pixelSize);
    QRhiSampler* feedbackHistorySampler(pvj::core::WrapMode wrap) const;
    void runPerLayerFilterChain(QRhi* r, QRhiCommandBuffer* cb, int layer, QRhiTexture* firstSource,
                                const QSize& stagePx, const QColor& clear, bool copyHistoryToFilterScratch);

    QRhiTexture* sourceTextureForLayer(int layer) const;
    QRhiTexture* filterOutputTextureForLayer(int layer) const;

    bool anyLayerHasVideo() const;
    bool shouldDrawCenterLabel() const;

    QString m_label;

    std::array<QImage, LayerCount>   m_pending{};
    std::array<bool, LayerCount>     m_dirty{};
    std::array<QSize, LayerCount>    m_sizes{};

    std::array<float, LayerCount>              m_opacity{};
    std::array<pvj::core::CopyMode, LayerCount> m_copyMode{};
    std::array<bool, LayerCount>               m_layerActive{};
    std::array<pvj::core::PictureParams, LayerCount>  m_layerPicture{};
    std::array<pvj::core::FeedbackParams, LayerCount> m_layerFeedback{};
    std::array<QList<pvj::core::CellFilterNode>, LayerCount> m_layerFilterChain{};
    /// Per-layer RGB key weights for GPU key filters (defaults 1,1,1).
    std::array<std::array<float, 3>, LayerCount> m_layerKeyChannel{};

    QElapsedTimer m_elapsed;

    std::unique_ptr<QRhiBuffer>   m_vbuf;
    std::unique_ptr<QRhiBuffer>   m_ubuf;
    std::unique_ptr<QRhiSampler>  m_sampler;
    std::unique_ptr<QRhiSampler>  m_samplerFeedbackRepeat;
    std::unique_ptr<QRhiSampler>  m_samplerFeedbackMirror;
    std::array<std::unique_ptr<QRhiTexture>, LayerCount> m_tex;
    std::unique_ptr<QRhiShaderResourceBindings> m_srb;
    std::unique_ptr<QRhiGraphicsPipeline>       m_pipeline;

    bool m_pipelineReady = false;

    // Offscreen "stage" scene target + present resources.
    QSize m_stagePixelSize;
    QRhiRenderPassDescriptor* m_presentSwapchainRp = nullptr;

    std::unique_ptr<QRhiTexture> m_sceneTex;
    std::unique_ptr<QRhiTextureRenderTarget> m_sceneRt;
    std::unique_ptr<QRhiRenderPassDescriptor> m_sceneRp;
    std::unique_ptr<QRhiGraphicsPipeline> m_mixerOffscreenPipeline;

    std::unique_ptr<QRhiBuffer> m_presentUbuf;
    std::unique_ptr<QRhiShaderResourceBindings> m_presentSrb;
    std::unique_ptr<QRhiGraphicsPipeline> m_presentPipeline;
    std::unique_ptr<QRhiRenderPassDescriptor> m_filterRp;

    // Generic per-layer filter chain ping-pong targets.
    std::array<std::array<std::unique_ptr<QRhiTexture>, 2>, LayerCount> m_layerFilterPingTex{};
    std::array<std::array<std::unique_ptr<QRhiTextureRenderTarget>, 2>, LayerCount> m_layerFilterPingRt{};
    std::array<int, LayerCount> m_layerFilterLastOut{};

    // Per-layer feedback history ping-pong + composed output.
    std::array<std::array<std::unique_ptr<QRhiTexture>, 2>, LayerCount> m_layerHistTex{};
    std::array<std::array<std::unique_ptr<QRhiTextureRenderTarget>, 2>, LayerCount> m_layerHistRt{};
    std::array<int, LayerCount> m_layerHistReadIdx{};
    std::array<bool, LayerCount> m_layerFbHistoryValid{};
    std::array<std::unique_ptr<QRhiTexture>, LayerCount> m_layerFeedbackTex{};
    std::array<std::unique_ptr<QRhiTextureRenderTarget>, LayerCount> m_layerFeedbackRt{};
    std::array<std::unique_ptr<QRhiBuffer>, LayerCount> m_layerFeedbackUbuf{};
    std::array<std::unique_ptr<QRhiShaderResourceBindings>, LayerCount> m_layerFeedbackSrb{};
    std::unique_ptr<QRhiGraphicsPipeline> m_layerFeedbackPipeline;
    std::unique_ptr<QRhiRenderPassDescriptor> m_layerFeedbackRp;
    QSize m_feedbackPixelSize;

    QSize m_filterPixelSize;
    QHash<QString, QRhiGraphicsPipeline*> m_filterPipelineByTypeId;
    std::vector<std::unique_ptr<QRhiGraphicsPipeline>> m_filterPipelineOwned;
};

} // namespace pvj::render
