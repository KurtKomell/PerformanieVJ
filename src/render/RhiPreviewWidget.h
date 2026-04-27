#pragma once

#include <QImage>
#include <QRhiWidget>
#include <QString>

#include <memory>

QT_BEGIN_NAMESPACE
class QRhiBuffer;
class QRhiGraphicsPipeline;
class QRhiResourceUpdateBatch;
class QRhiSampler;
class QRhiShaderResourceBindings;
class QRhiTexture;
QT_END_NAMESPACE

namespace pvj::render {

// GPU-accelerated preview surface. Same public API as pvj::app::PreviewWidget
// (setFrame / clearFrame / setLabel) so it can be used as a drop-in
// replacement for the A / Output previews. A CPU-side QImage frame is
// uploaded to a QRhiTexture on every update; aspect-correct letterboxing is
// handled in the vertex shader via a uniform buffer.
//
// This class is intentionally backend-agnostic: QRhi picks D3D11, Metal,
// Vulkan or OpenGL depending on the platform. In M6 a second code path
// will bypass the QImage round-trip and upload YUV/NV12 planes directly from
// FFmpeg/DXV into GPU textures.
class RhiPreviewWidget : public QRhiWidget
{
    Q_OBJECT
public:
    explicit RhiPreviewWidget(QWidget* parent = nullptr);
    ~RhiPreviewWidget() override;

    void setLabel(const QString& text);
    QString label() const { return m_label; }

    /// Rotation around Z (radians), applied in NDC before letterboxing scale.
    void setRotationZ(float radians);
    float rotationZ() const { return m_rotationZ; }

public slots:
    void setFrame(QImage frame, qint64 pts = 0);
    void clearFrame();

protected:
    // QRhiWidget
    void initialize(QRhiCommandBuffer* cb) override;
    void render(QRhiCommandBuffer* cb) override;
    void releaseResources() override;

    // QWidget - overlay the label on top of the GPU surface
    void paintEvent(QPaintEvent* event) override;

private:
    void releaseGpuResources();
    void uploadFrameIfNeeded(QRhiResourceUpdateBatch* batch);
    void updateUniformBuffer(QRhiResourceUpdateBatch* batch);

    QString m_label;
    QImage  m_pendingFrame;   // staged upload; consumed on next render
    bool    m_frameDirty = false;
    QSize   m_lastFrameSize;
    float   m_rotationZ = 0.0f;

    // GPU resources (all owned; freed in releaseResources())
    std::unique_ptr<QRhiBuffer>                 m_vbuf;
    std::unique_ptr<QRhiBuffer>                 m_ubuf;
    std::unique_ptr<QRhiSampler>                m_sampler;
    std::unique_ptr<QRhiTexture>                m_texture;
    std::unique_ptr<QRhiShaderResourceBindings> m_srb;
    std::unique_ptr<QRhiGraphicsPipeline>       m_pipeline;

    bool m_pipelineReady = false;
};

} // namespace pvj::render
