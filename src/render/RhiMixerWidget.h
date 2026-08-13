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

#include "FeedbackLoop.h"

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

class MaxineFilterBackend;

// Stacks up to 14 mix layers: layer 0 = fixed black background, layers 1–13 = user clips.
class RhiMixerWidget : public QRhiWidget
{
    Q_OBJECT
public:
    static constexpr int LayerCount = 14;
    static constexpr int kFeedbackRingSize = pvj::core::kFeedbackRingCapacity;
    static constexpr int kTemporalHistoryCapacity = 16;
    static constexpr int BackgroundLayerIndex = 0;
    static constexpr int UserLayerMin = 1;
    static constexpr int UserLayerMax = 13;

    explicit RhiMixerWidget(QWidget* parent = nullptr);
    ~RhiMixerWidget() override;

    void setLabel(const QString& text);
    QString label() const { return m_label; }

    void setLayerOpacity(int layer, float o, bool requestRepaint = true);
    void setLayerCopyMode(int layer, pvj::core::CopyMode mode);
    void setLayerActive(int layer, bool active);

    float layerOpacity(int layer) const;
    pvj::core::CopyMode layerCopyMode(int layer) const;
    bool layerActive(int layer) const;

    /// Per-layer picture parameters (reserved; not currently consumed by the
    /// mixer fragment pipeline but kept so the model can round-trip values).
    void setLayerPicture(int layer, const pvj::core::PictureParams& p);
    pvj::core::PictureParams layerPicture(int layer) const;
    void setLayerMatteRole(int layer, pvj::core::LayerMatteRole role);
    pvj::core::LayerMatteRole layerMatteRole(int layer) const;

    void setLayerFilterChain(int layer, const QList<pvj::core::CellFilterNode>& chain);
    /// Post-mixer NVIDIA output chain (Output tab); applied after layer composite, before present.
    void setOutputFilterChain(const QList<pvj::core::CellFilterNode>& chain);
    const QList<pvj::core::CellFilterNode>& outputFilterChain() const { return m_outputFilterChain; }
    /// Weights for chroma/luma key filters (cell inspector key R / G / B), 0–1 each.
    void setLayerKeyChannels(int layer, float r, float g, float b);

    /// Enable/disable feedback layer and set its parameters.
    void setLayerFeedback(int layer, bool enabled, const pvj::core::FeedbackParams& p);
    bool layerFeedbackEnabled(int layer) const;
    pvj::core::FeedbackParams layerFeedback(int layer) const;

    /// Fixed "stage" resolution (in pixels) at which the internal mixer renders,
    /// independent of this widget's size. When set,
    /// the widget presents the stage texture letterboxed/pillarboxed onto its
    /// own surface. Call with an invalid (empty) QSize to restore the legacy
    /// behaviour of rendering directly at the widget's pixel size.
    void setStagePixelSize(QSize px);
    QSize stagePixelSize() const { return m_stagePixelSize; }

    bool hasActiveFeedbackLayer() const;

public slots:
    void setFrame(int layer, QImage frame, qint64 pts = 0);
    void clearFrame(int layer);

signals:
    /// Emitted when this mixer schedules another feedback animation frame (preview → fullscreen).
    void feedbackRepaintTick();

protected:
    void initialize(QRhiCommandBuffer* cb) override;
    void render(QRhiCommandBuffer* cb) override;
    void releaseResources() override;
    void paintEvent(QPaintEvent* event) override;

private:
    friend class FeedbackLoop;

    void releaseGpuResources();
    void releaseOffscreenGpuResources();

    bool ensureOffscreenSceneTargets(QRhi* r, const QSize& pixelSize);
    void ensurePresentPipelineForSwapchain(QRhi* r);

    void rebuildMixerShaderResourceBindings();
    void rebuildTextureCopyShaderResourceBindings(QRhiTexture* sourceTex);
    void rebuildPresentShaderResourceBindings(QRhiTexture* sourceTex);

    void uploadFramesIfNeeded(QRhiResourceUpdateBatch* batch);
    void updateMixerUniformBuffer(QRhiResourceUpdateBatch* batch, int maxLayerExclusive = -1,
                                  int minLayerInclusive = 0);
    void updatePresentUniformBuffer(QRhiResourceUpdateBatch* batch, const QSize& widgetPx);
    void updateFilterUniformBuffer(QRhiResourceUpdateBatch* batch, QRhiBuffer* ubuf,
                                   const pvj::core::CellFilterNode& node, const QSize& pixelSize,
                                   int layerIndex, int internalPass = 0);
    bool ensureLayerFilterTargets(QRhi* r, const QSize& pixelSize);
    bool ensureLayerFilterPassResources(QRhi* r, int layer);
    void rebuildLayerFilterShaderResourceBindings(int layer, QRhiTexture* sourceTex,
                                                  const QString& typeId,
                                                  QRhiTexture* origTex = nullptr,
                                                  QRhiTexture* historyTex = nullptr);
    QRhiGraphicsPipeline* ensureFilterPipeline(QRhi* r, const QString& typeId, int layerIndex,
                                               QRhiShaderResourceBindings* srb,
                                               QRhiRenderPassDescriptor* rp);
    void runPerLayerFilterChain(QRhi* r, QRhiCommandBuffer* cb, int layer, QRhiTexture* firstSource,
                                const QSize& stagePx, const QColor& clear,
                                const QList<pvj::core::CellFilterNode>* chainOverride = nullptr);
    bool ensureOutputFilterTargets(QRhi* r, const QSize& pixelSize);
    bool ensureOutputFilterPassResources(QRhi* r);
    void rebuildOutputFilterShaderResourceBindings(QRhiTexture* sourceTex, const QString& typeId,
                                                   QRhiTexture* origTex = nullptr,
                                                   QRhiTexture* historyTex = nullptr);
    bool ensureLayerTemporalHistory(QRhi* r, int layer, const QSize& pixelSize);
    bool ensureOutputTemporalHistory(QRhi* r, const QSize& pixelSize);
    void pushLayerTemporalHistory(QRhi* r, QRhiCommandBuffer* cb, int layer, QRhiTexture* sourceTex,
                                  const QSize& stagePx,
                                  const pvj::core::CellFilterNode& node);
    void pushOutputTemporalHistory(QRhi* r, QRhiCommandBuffer* cb, QRhiTexture* sourceTex,
                                   const QSize& stagePx, const pvj::core::CellFilterNode& node);
    QRhiTexture* layerTemporalHistorySample(int layer, int delayFrames) const;
    QRhiTexture* outputTemporalHistorySample(int delayFrames) const;
    void releaseTemporalGpuResources();
    void runOutputFilterChain(QRhi* r, QRhiCommandBuffer* cb, const QSize& stagePx, const QColor& clear);
    QRhiTexture* outputFilterResultTexture() const;

    QRhiGraphicsPipeline* ensureTextureCopyPipeline(QRhi* r, QRhiRenderPassDescriptor* rp);
    void runTextureCopyPass(QRhi* r, QRhiCommandBuffer* cb, QRhiTexture* sourceTex,
                            QRhiTextureRenderTarget* targetRt, const QSize& stagePx);

    void applyBackgroundLayerState();

    QRhiTexture* sourceTextureForLayer(int layer) const;
    QRhiTexture* filterOutputTextureForLayer(int layer) const;
    QRhiTexture* layerBaseTextureForInput(int layer) const;

    bool anyLayerHasVideo() const;
    bool shouldDrawCenterLabel() const;

    QString m_label;

    FeedbackLoop m_feedback;

    std::array<QImage, LayerCount>   m_pending{};
    std::array<bool, LayerCount>     m_dirty{};
    std::array<QSize, LayerCount>    m_sizes{};

    std::array<float, LayerCount>              m_opacity{};
    std::array<pvj::core::CopyMode, LayerCount> m_copyMode{};
    std::array<bool, LayerCount>               m_layerActive{};
    std::array<pvj::core::PictureParams, LayerCount>  m_layerPicture{};
    std::array<pvj::core::LayerMatteRole, LayerCount> m_layerMatteRole{};
    std::array<QList<pvj::core::CellFilterNode>, LayerCount> m_layerFilterChain{};
    QList<pvj::core::CellFilterNode> m_outputFilterChain;
    /// Per-layer RGB key weights for GPU key filters (defaults 1,1,1).
    std::array<std::array<float, 3>, LayerCount> m_layerKeyChannel{};

    std::unique_ptr<QRhiShaderResourceBindings> m_textureCopySrb;
    // Keyed by render-pass descriptor: copy targets with different attachment shapes
    // (e.g. layer filter ping targets vs. the feedback stack target) can both be used
    // within the same recorded frame, so pipelines must not be destroyed mid-frame when
    // switching between them (a still-recorded, not-yet-submitted draw may reference the
    // old pipeline object). Cache one pipeline per distinct rp instead of a single slot.
    QHash<QRhiRenderPassDescriptor*, QRhiGraphicsPipeline*> m_textureCopyPipelineByRp;
    std::vector<std::unique_ptr<QRhiGraphicsPipeline>> m_textureCopyPipelinesOwned;

    QElapsedTimer m_elapsed;
    quint32 m_presentFrame = 0;

    std::array<std::array<std::unique_ptr<QRhiTexture>, kTemporalHistoryCapacity>, LayerCount>
        m_layerTemporalTex{};
    std::array<std::array<std::unique_ptr<QRhiTextureRenderTarget>, kTemporalHistoryCapacity>,
               LayerCount>
        m_layerTemporalRt{};
    std::array<quint8, LayerCount> m_layerTemporalWriteIdx{};
    std::array<int, LayerCount> m_layerTemporalFilled{};
    QSize m_layerTemporalPixelSize;

    std::array<std::unique_ptr<QRhiTexture>, kTemporalHistoryCapacity> m_outputTemporalTex{};
    std::array<std::unique_ptr<QRhiTextureRenderTarget>, kTemporalHistoryCapacity>
        m_outputTemporalRt{};
    quint8 m_outputTemporalWriteIdx = 0;
    int m_outputTemporalFilled = 0;
    QSize m_outputTemporalPixelSize;

    std::unique_ptr<QRhiBuffer>   m_vbuf;
    std::unique_ptr<QRhiBuffer>   m_ubuf;
    std::unique_ptr<QRhiSampler>  m_sampler;
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
    std::array<std::unique_ptr<QRhiTexture>, LayerCount> m_layerFilterOrigTex{};
    std::array<std::unique_ptr<QRhiTextureRenderTarget>, LayerCount> m_layerFilterOrigRt{};
    std::unique_ptr<QRhiTexture> m_outputFilterOrigTex;
    std::unique_ptr<QRhiTextureRenderTarget> m_outputFilterOrigRt;
    std::array<int, LayerCount> m_layerFilterLastOut{};
    std::array<std::unique_ptr<QRhiBuffer>, LayerCount> m_layerFilterUbuf{};
    std::array<std::unique_ptr<QRhiShaderResourceBindings>, LayerCount> m_layerFilterSrb{};
    std::array<quint64, LayerCount> m_layerFilterSrbKey{};
    std::array<bool, LayerCount> m_layerFilterSrbReady{};

    std::array<std::unique_ptr<QRhiTexture>, 2> m_outputFilterPingTex{};
    std::array<std::unique_ptr<QRhiTextureRenderTarget>, 2> m_outputFilterPingRt{};
    int m_outputFilterLastOut = -1;
    std::unique_ptr<QRhiBuffer> m_outputFilterUbuf;
    std::unique_ptr<QRhiShaderResourceBindings> m_outputFilterSrb;
    quint64 m_outputFilterSrbKey = 0;
    bool m_outputFilterSrbReady = false;

    std::unique_ptr<QRhiTexture> m_blendBlackTex;

    MaxineFilterBackend* m_maxineBackend = nullptr;
    QSize m_maxinePixelSize;

    QSize m_filterPixelSize;
    QHash<QString, QRhiGraphicsPipeline*> m_filterPipelineByTypeId;
    std::vector<std::unique_ptr<QRhiGraphicsPipeline>> m_filterPipelineOwned;
};

} // namespace pvj::render
