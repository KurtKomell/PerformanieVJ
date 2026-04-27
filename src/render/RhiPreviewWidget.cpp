#include "RhiPreviewWidget.h"

#include <rhi/qshader.h>
#include <rhi/qrhi.h>

#include <QFile>
#include <QPainter>
#include <QPaintEvent>

namespace pvj::render {

namespace {

// Fullscreen quad: four vertices as a triangle strip, position (xy) + uv (xy).
// NDC coordinates: (-1,-1) bottom-left ... (+1,+1) top-right. UVs go (0,0)
// top-left to (1,1) bottom-right because image data starts at the top.
constexpr float kQuadVertices[] = {
    // x,    y,     u,   v
    -1.0f, -1.0f,  0.0f, 1.0f,
     1.0f, -1.0f,  1.0f, 1.0f,
    -1.0f,  1.0f,  0.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 0.0f,
};

QShader loadShader(const QString& resourcePath)
{
    QFile f(resourcePath);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning("RhiPreviewWidget: cannot open shader %s",
                 qUtf8Printable(resourcePath));
        return {};
    }
    return QShader::fromSerialized(f.readAll());
}

} // namespace

RhiPreviewWidget::RhiPreviewWidget(QWidget* parent)
    : QRhiWidget(parent)
{
    setMinimumSize(320, 180);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAttribute(Qt::WA_OpaquePaintEvent, false);

    // We want the widget to repaint itself through QRhiWidget::render, but
    // still allow QWidget::paintEvent to overlay the label text on top.
    setAutoRenderTarget(true);
}

RhiPreviewWidget::~RhiPreviewWidget() = default;

void RhiPreviewWidget::setLabel(const QString& text)
{
    if (m_label != text) {
        m_label = text;
        update();
    }
}

void RhiPreviewWidget::setRotationZ(float radians)
{
    if (m_rotationZ != radians) {
        m_rotationZ = radians;
        update();
    }
}

void RhiPreviewWidget::setFrame(QImage frame, qint64 /*pts*/)
{
    if (frame.isNull()) {
        clearFrame();
        return;
    }
    // Ensure a known memory layout (RGBA8, non-premultiplied) for the upload.
    if (frame.format() != QImage::Format_RGBA8888) {
        frame = frame.convertToFormat(QImage::Format_RGBA8888);
    }
    m_pendingFrame = std::move(frame);
    m_frameDirty   = true;
    update();
}

void RhiPreviewWidget::clearFrame()
{
    m_pendingFrame = QImage();
    m_frameDirty   = true;
    m_lastFrameSize = QSize();
    update();
}

void RhiPreviewWidget::releaseGpuResources()
{
    m_pipeline.reset();
    m_srb.reset();
    m_texture.reset();
    m_sampler.reset();
    m_ubuf.reset();
    m_vbuf.reset();
    m_pipelineReady = false;
}

void RhiPreviewWidget::releaseResources()
{
    releaseGpuResources();
}

void RhiPreviewWidget::initialize(QRhiCommandBuffer* cb)
{
    QRhi* r = rhi();
    if (!r) return;

    if (m_pipelineReady) {
        // Resources still valid - nothing to do. `initialize` gets called
        // again after the window is reparented or the graphics device is
        // rebuilt; in that case QRhiWidget first invokes releaseResources.
        return;
    }

    m_vbuf.reset(r->newBuffer(QRhiBuffer::Immutable,
                              QRhiBuffer::VertexBuffer,
                              sizeof(kQuadVertices)));
    m_vbuf->create();

    m_ubuf.reset(r->newBuffer(QRhiBuffer::Dynamic,
                              QRhiBuffer::UniformBuffer,
                              48 /* 3 * vec4 std140, matches textured_quad.vert */));
    m_ubuf->create();

    m_sampler.reset(r->newSampler(QRhiSampler::Linear, QRhiSampler::Linear,
                                  QRhiSampler::None,
                                  QRhiSampler::ClampToEdge,
                                  QRhiSampler::ClampToEdge));
    m_sampler->create();

    // 1x1 placeholder texture; replaced on the first setFrame() upload.
    m_texture.reset(r->newTexture(QRhiTexture::RGBA8, QSize(1, 1), 1, {}));
    m_texture->create();

    m_srb.reset(r->newShaderResourceBindings());
    m_srb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::VertexStage,
            m_ubuf.get()),
        QRhiShaderResourceBinding::sampledTexture(
            1, QRhiShaderResourceBinding::FragmentStage,
            m_texture.get(), m_sampler.get()),
    });
    m_srb->create();

    QShader vs = loadShader(QStringLiteral(":/shaders/textured_quad.vert.qsb"));
    QShader fs = loadShader(QStringLiteral(":/shaders/textured_quad.frag.qsb"));
    if (!vs.isValid() || !fs.isValid()) {
        qWarning("RhiPreviewWidget: shader modules missing from resources");
    }

    QRhiVertexInputLayout layout;
    layout.setBindings({ { 4 * sizeof(float) } });
    layout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float2, 0 },
        { 0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float) },
    });

    m_pipeline.reset(r->newGraphicsPipeline());
    m_pipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);
    m_pipeline->setShaderStages({
        { QRhiShaderStage::Vertex,   vs },
        { QRhiShaderStage::Fragment, fs },
    });
    m_pipeline->setVertexInputLayout(layout);
    m_pipeline->setShaderResourceBindings(m_srb.get());
    m_pipeline->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
    m_pipeline->create();

    // Static vertex buffer + initial UBO upload. `initialize` runs inside a
    // valid frame before beginPass(), so we can submit the batch straight
    // through the command buffer.
    QRhiResourceUpdateBatch* batch = r->nextResourceUpdateBatch();
    batch->uploadStaticBuffer(m_vbuf.get(), kQuadVertices);
    const float initialUbo[12] = {
        1.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f,
    };
    batch->updateDynamicBuffer(m_ubuf.get(), 0, sizeof(initialUbo), initialUbo);
    cb->resourceUpdate(batch);

    m_pipelineReady = true;
}

void RhiPreviewWidget::uploadFrameIfNeeded(QRhiResourceUpdateBatch* batch)
{
    if (!m_frameDirty) return;
    m_frameDirty = false;

    QRhi* r = rhi();
    if (!r) return;

    if (m_pendingFrame.isNull()) {
        // Clear the texture to solid dark gray by uploading a 1x1 pixel.
        static const quint8 dark[4] = { 12, 12, 12, 255 };
        if (m_texture->pixelSize() != QSize(1, 1)) {
            m_texture->setPixelSize(QSize(1, 1));
            m_texture->create();
            m_srb->setBindings({
                QRhiShaderResourceBinding::uniformBuffer(
                    0, QRhiShaderResourceBinding::VertexStage,
                    m_ubuf.get()),
                QRhiShaderResourceBinding::sampledTexture(
                    1, QRhiShaderResourceBinding::FragmentStage,
                    m_texture.get(), m_sampler.get()),
            });
            m_srb->create();
        }
        QRhiTextureSubresourceUploadDescription sub(dark, sizeof(dark));
        QRhiTextureUploadDescription desc(
            QRhiTextureUploadEntry(0, 0, sub));
        batch->uploadTexture(m_texture.get(), desc);
        m_lastFrameSize = QSize();
        return;
    }

    const QSize sz = m_pendingFrame.size();
    if (m_texture->pixelSize() != sz) {
        m_texture->setPixelSize(sz);
        m_texture->create();
        m_srb->setBindings({
            QRhiShaderResourceBinding::uniformBuffer(
                0, QRhiShaderResourceBinding::VertexStage,
                m_ubuf.get()),
            QRhiShaderResourceBinding::sampledTexture(
                1, QRhiShaderResourceBinding::FragmentStage,
                m_texture.get(), m_sampler.get()),
        });
        m_srb->create();
    }

    QRhiTextureSubresourceUploadDescription sub(m_pendingFrame);
    QRhiTextureUploadDescription desc(
        QRhiTextureUploadEntry(0, 0, sub));
    batch->uploadTexture(m_texture.get(), desc);
    m_lastFrameSize = sz;
}

void RhiPreviewWidget::updateUniformBuffer(QRhiResourceUpdateBatch* batch)
{
    // Compute aspect-correct letterboxing: scale NDC so the texture shows
    // with its native aspect ratio inside the widget's viewport.
    QSize widget = renderTarget() ? renderTarget()->pixelSize() : size();
    if (widget.isEmpty()) widget = QSize(1, 1);

    float sx = 1.0f;
    float sy = 1.0f;
    if (!m_lastFrameSize.isEmpty()) {
        const double srcA = double(m_lastFrameSize.width())  / double(m_lastFrameSize.height());
        const double dstA = double(widget.width())           / double(widget.height());
        if (srcA > dstA) {
            sy = float(dstA / srcA);
        } else {
            sx = float(srcA / dstA);
        }
    }
    float ubo[12] = {
        sx, sy, 0.0f, 0.0f,
        m_rotationZ, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f,
    };
    batch->updateDynamicBuffer(m_ubuf.get(), 0, sizeof(ubo), ubo);
}

void RhiPreviewWidget::render(QRhiCommandBuffer* cb)
{
    QRhi* r = rhi();
    if (!r || !m_pipelineReady) return;

    QRhiResourceUpdateBatch* batch = r->nextResourceUpdateBatch();
    uploadFrameIfNeeded(batch);
    updateUniformBuffer(batch);

    const QColor clear(12, 12, 12);
    cb->beginPass(renderTarget(), clear, { 1.0f, 0 }, batch);

    cb->setGraphicsPipeline(m_pipeline.get());
    const QSize vp = renderTarget()->pixelSize();
    cb->setViewport(QRhiViewport(0, 0, vp.width(), vp.height()));
    cb->setShaderResources();
    QRhiCommandBuffer::VertexInput vin(m_vbuf.get(), 0);
    cb->setVertexInput(0, 1, &vin);
    cb->draw(4);

    cb->endPass();
}

void RhiPreviewWidget::paintEvent(QPaintEvent* event)
{
    QRhiWidget::paintEvent(event);

    if (m_label.isEmpty()) return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QColor(230, 230, 230, 220));
    QFont f = p.font();
    f.setBold(true);
    if (m_lastFrameSize.isEmpty()) {
        f.setPointSizeF(f.pointSizeF() * 1.4);
        p.setFont(f);
        p.drawText(rect(), Qt::AlignCenter, m_label);
    } else {
        p.setFont(f);
        p.drawText(rect().adjusted(8, 6, -8, -6), Qt::AlignTop | Qt::AlignLeft, m_label);
    }
}

} // namespace pvj::render
