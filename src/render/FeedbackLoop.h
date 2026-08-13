#pragma once

#include "core/Model.h"

#include <array>
#include <memory>

#include <QColor>
#include <QSize>

class QRhi;
class QRhiCommandBuffer;
class QRhiTexture;
class QRhiTextureRenderTarget;
class QRhiRenderPassDescriptor;
class QRhiShaderResourceBindings;
class QRhiGraphicsPipeline;
class QRhiBuffer;
class QRhiResourceUpdateBatch;

namespace pvj::render {

class RhiMixerWidget;

/// Feedback path: stack layers below (no keying) + above (with keying), input grade then
/// feedback grade on the GPU, result faded into the mixer by layer opacity (0 = empty).
class FeedbackLoop {
public:
    static constexpr int kRingSize = pvj::core::kFeedbackRingCapacity;
    static constexpr int LayerCount = 14;
    static constexpr int BackgroundLayerIndex = 0;
    static constexpr int UserLayerMin = 1;

    explicit FeedbackLoop(RhiMixerWidget* host);
    ~FeedbackLoop();

    void setLayer(int layer, bool enabled, const pvj::core::FeedbackParams& p);
    bool isLayerFeedback(int layer) const;
    pvj::core::FeedbackParams layerParams(int layer) const;
    void notifyLayerActiveChanged();
    void onPreFeedbackTopologyChanged(int layer);

    bool hasActive() const;
    int activeLayer() const;
    QRhiTexture* resultTexture() const;
    pvj::core::FeedbackInputMode inputMode() const;

    void softResetRing();
    void releaseGpuResources();

    void renderFrame(QRhi* r, QRhiCommandBuffer* cb, const QSize& stagePx, const QColor& clear);
    void advanceAfterSceneComposite(QRhi* r, QRhiCommandBuffer* cb, const QSize& stagePx);

private:
    RhiMixerWidget* m_host = nullptr;

    int m_activeLayer = -1;
    std::array<bool, LayerCount> m_isFeedback{};
    std::array<pvj::core::FeedbackParams, LayerCount> m_params{};

    void recomputeActiveLayer();

    quint8 m_feedbackWriteIdx = 0;
    int m_feedbackRingFilled = 0;
    QSize m_feedbackPixelSize;

    std::unique_ptr<QRhiTexture> m_belowTex;
    std::unique_ptr<QRhiTextureRenderTarget> m_belowRt;
    std::unique_ptr<QRhiRenderPassDescriptor> m_belowRp;
    std::unique_ptr<QRhiShaderResourceBindings> m_belowSrb;
    std::unique_ptr<QRhiGraphicsPipeline> m_mixerBelowPipeline;
    std::unique_ptr<QRhiBuffer> m_belowUbuf;

    std::unique_ptr<QRhiTexture> m_aboveTex;
    std::unique_ptr<QRhiTextureRenderTarget> m_aboveRt;

    std::unique_ptr<QRhiTexture> m_stackTex;
    std::unique_ptr<QRhiTextureRenderTarget> m_stackRt;
    std::unique_ptr<QRhiShaderResourceBindings> m_stackCombineSrb;
    std::unique_ptr<QRhiGraphicsPipeline> m_stackCombinePipeline;

    std::array<std::unique_ptr<QRhiTexture>, kRingSize> m_feedbackTex{};
    std::array<std::unique_ptr<QRhiTextureRenderTarget>, kRingSize> m_feedbackRt{};
    std::unique_ptr<QRhiRenderPassDescriptor> m_feedbackRp;
    std::unique_ptr<QRhiBuffer> m_feedbackUbuf;
    std::unique_ptr<QRhiShaderResourceBindings> m_feedbackSrb;
    std::unique_ptr<QRhiGraphicsPipeline> m_feedbackPipeline;

    std::array<std::unique_ptr<QRhiTexture>, 2> m_sceneHistTex{};
    std::array<std::unique_ptr<QRhiTextureRenderTarget>, 2> m_sceneHistRt{};
    quint8 m_sceneHistWriteIdx = 0;
    bool m_sceneHistPrimed = false;

    /// Last written ring slot (-2 = post-filter stack; composite is mixer source).
    int m_lastResultIdx = -1;

    QSize m_maskPixelSize;
    std::unique_ptr<QRhiTexture> m_feedbackCoverageTex;
    std::unique_ptr<QRhiTextureRenderTarget> m_feedbackCoverageRt;
    std::unique_ptr<QRhiRenderPassDescriptor> m_feedbackCoverageRp;
    std::unique_ptr<QRhiGraphicsPipeline> m_feedbackAboveCoveragePipeline;

    std::unique_ptr<QRhiTexture> m_feedbackCompositeTex;
    std::unique_ptr<QRhiTextureRenderTarget> m_feedbackCompositeRt;
    std::unique_ptr<QRhiRenderPassDescriptor> m_feedbackCompositeRp;
    std::unique_ptr<QRhiShaderResourceBindings> m_feedbackCompositeSrb;
    std::unique_ptr<QRhiBuffer> m_feedbackCompositeUbuf;
    std::unique_ptr<QRhiGraphicsPipeline> m_feedbackCompositePipeline;

    bool ensureTargets(QRhi* r, const QSize& pixelSize);
    void ensureMaskResources(QRhi* r, const QSize& pixelSize);
    void clearMaskTextures(QRhi* r, QRhiCommandBuffer* cb, const QSize& stagePx);
    bool keyFromAboveActive() const;

    QRhiTexture* feedbackWriteTexture() const;
    QRhiTexture* feedbackReadTexture() const;
    QRhiTextureRenderTarget* feedbackWriteRenderTarget() const;
    QRhiTexture* sceneHistReadTexture() const;
    QRhiTextureRenderTarget* sceneHistWriteRenderTarget() const;

    void rebuildBelowMixerShaderResourceBindings(bool useBaseTextures = false);
    void rebuildStackCombineShaderResourceBindings();
    void rebuildFeedbackShaderResourceBindings(QRhiTexture* freshTex, QRhiTexture* historyRead);
    void updateBelowMixerUniformBuffer(QRhiResourceUpdateBatch* batch, int minLayerInclusive,
                                       int maxLayerExclusive);
    void updateFeedbackUniformBuffer(QRhiResourceUpdateBatch* batch, int feedbackLayer,
                                     const QSize& stagePx);

    void runPartialMixerPass(QRhi* r, QRhiCommandBuffer* cb, int minLayerInclusive,
                             int maxLayerExclusive, QRhiTextureRenderTarget* targetRt,
                             const QSize& stagePx, const QColor& clear,
                             bool useBaseTextures = false);
    void runStackCombinePass(QRhi* r, QRhiCommandBuffer* cb, const QSize& stagePx,
                             const QColor& clear);
    QRhiTexture* feedbackInjectSourceTexture(QRhi* r, QRhiCommandBuffer* cb, int feedbackLayer,
                                             const QSize& stagePx, const QColor& clear);

    void runFeedbackPass(QRhi* r, QRhiCommandBuffer* cb, int feedbackLayer, const QSize& stagePx,
                         const QColor& clear, QRhiTexture* freshTex, QRhiTexture* historyRead,
                         QRhiTextureRenderTarget* writeRt);
    void runFeedbackAccumulationStep(QRhi* r, QRhiCommandBuffer* cb, int feedbackLayer,
                                     const QSize& stagePx, const QColor& clear,
                                     QRhiTexture* filteredFresh);
    void applyFeedbackPostFilters(QRhi* r, QRhiCommandBuffer* cb, int feedbackLayer,
                                  const QSize& stagePx, const QColor& clear,
                                  const QList<pvj::core::CellFilterNode>& postChain);
    void primeFeedbackRingForDelay(QRhi* r, QRhiCommandBuffer* cb, int feedbackLayer,
                                   const QSize& stagePx, const QColor& clear,
                                   QRhiTexture* filteredFresh);
    void advanceFeedbackRingSlot();
    void copySceneToHistory(QRhi* r, QRhiCommandBuffer* cb, const QSize& stagePx);
    void clearFeedbackHistoryRing(QRhi* r, QRhiCommandBuffer* cb, const QSize& stagePx);

    void runFeedbackAboveCoveragePass(QRhi* r, QRhiCommandBuffer* cb, int feedbackLayer,
                                      const QSize& stagePx, const QColor& clear);
    void runFeedbackLayerCompositePass(QRhi* r, QRhiCommandBuffer* cb, const QSize& stagePx,
                                       const QColor& clear, bool applyMask, QRhiTexture* histSrc);
};

} // namespace pvj::render
