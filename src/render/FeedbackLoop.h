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

/// Classic infinite feedback (ping-pong):
///
///   Layer 0..F-1 ──mixer──► liveTex
///                              │
///                     passMode 0 (input grade)
///                              ▼
///                           midTex (+ optional mid filters)
///                              │
///   ping[read] ──transform──► grade * retention ──blend──► ping[write]
///     (scale/rotate/translate/wrap)              ▲
///                                                │ live
///                              ▼
///                     resultTexture → mixer from layer F
///
/// Formula: out = blend(live, retention, grade(transform(history))).
class FeedbackLoop {
public:
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

    quint8 m_writeIdx = 0;
    bool m_primed = false;
    QSize m_pixelSize;

    std::unique_ptr<QRhiTexture> m_liveTex;
    std::unique_ptr<QRhiTextureRenderTarget> m_liveRt;
    std::unique_ptr<QRhiRenderPassDescriptor> m_sharedRp;

    std::unique_ptr<QRhiTexture> m_midTex;
    std::unique_ptr<QRhiTextureRenderTarget> m_midRt;

    std::array<std::unique_ptr<QRhiTexture>, 2> m_pingTex{};
    std::array<std::unique_ptr<QRhiTextureRenderTarget>, 2> m_pingRt{};

    std::unique_ptr<QRhiBuffer> m_belowUbuf;
    std::unique_ptr<QRhiShaderResourceBindings> m_belowSrb;
    std::unique_ptr<QRhiGraphicsPipeline> m_mixerBelowPipeline;

    std::unique_ptr<QRhiBuffer> m_fbUbuf;
    std::unique_ptr<QRhiShaderResourceBindings> m_fbSrb;
    std::unique_ptr<QRhiGraphicsPipeline> m_fbPipeline;

    std::unique_ptr<QRhiTexture> m_blackTex;
    bool m_blackUploaded = false;

    bool ensureTargets(QRhi* r, const QSize& pixelSize);
    QRhiTexture* historyReadTexture() const;
    QRhiTexture* resultWriteTexture() const;
    QRhiTextureRenderTarget* resultWriteRt() const;

    void rebuildBelowMixerBindings(bool useBaseTextures);
    void updateBelowMixerUbo(QRhiResourceUpdateBatch* batch, int minLayerInclusive,
                             int maxLayerExclusive);
    void runPartialMixerPass(QRhi* r, QRhiCommandBuffer* cb, int minLayerInclusive,
                             int maxLayerExclusive, QRhiTextureRenderTarget* targetRt,
                             const QSize& stagePx, const QColor& clear, bool useBaseTextures);

    void rebuildFeedbackBindings(QRhiTexture* liveTex, QRhiTexture* historyTex);
    void updateFeedbackUbo(QRhiResourceUpdateBatch* batch, int feedbackLayer, const QSize& stagePx,
                           int passMode);
    void runFeedbackPass(QRhi* r, QRhiCommandBuffer* cb, int feedbackLayer, const QSize& stagePx,
                         const QColor& clear, QRhiTexture* liveTex, QRhiTexture* historyTex,
                         QRhiTextureRenderTarget* destRt, int passMode);
    void clearPingPong(QRhiCommandBuffer* cb);
};

} // namespace pvj::render
