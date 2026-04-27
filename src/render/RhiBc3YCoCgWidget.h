#pragma once

#include <QByteArray>
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

// Displays raw BC3 (DXT5) YCoCg texture data with the same YCoCg→RGB mapping as
// FFmpeg's dxt5ys_block (scaled), using a fragment shader after hardware BC3 sampling.
class RhiBc3YCoCgWidget : public QRhiWidget
{
    Q_OBJECT
public:
    explicit RhiBc3YCoCgWidget(QWidget* parent = nullptr);
    ~RhiBc3YCoCgWidget() override;

    void setLabel(const QString& text);
    QString label() const { return m_label; }

    /// Replaces texture content; `dxtBytes` is tightly packed BC3 blocks (16 bytes per 4×4 tile).
    void setBc3Texture(const QByteArray& dxtBytes, int width, int height);
    void clearTexture();

protected:
    void initialize(QRhiCommandBuffer* cb) override;
    void render(QRhiCommandBuffer* cb) override;
    void releaseResources() override;
    void paintEvent(QPaintEvent* event) override;

private:
    void releaseGpuResources();
    void uploadIfNeeded(QRhiResourceUpdateBatch* batch);
    void updateUniformBuffer(QRhiResourceUpdateBatch* batch);

    static int bytesPerRowBlocks(int width);

    QString   m_label;
    QByteArray m_pendingDxt;
    int       m_texW = 0;
    int       m_texH = 0;
    bool      m_dirty = false;

    std::unique_ptr<QRhiBuffer>                 m_vbuf;
    std::unique_ptr<QRhiBuffer>                 m_ubuf;
    std::unique_ptr<QRhiSampler>                m_sampler;
    std::unique_ptr<QRhiTexture>                m_texture;
    std::unique_ptr<QRhiShaderResourceBindings> m_srb;
    std::unique_ptr<QRhiGraphicsPipeline>       m_pipeline;

    bool m_pipelineReady = false;
};

} // namespace pvj::render
