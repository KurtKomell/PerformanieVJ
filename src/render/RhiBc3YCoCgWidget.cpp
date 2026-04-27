#include "RhiBc3YCoCgWidget.h"

#include <rhi/qshader.h>
#include <rhi/qrhi.h>

#include <QFile>
#include <QPainter>
#include <QPaintEvent>

namespace pvj::render {

namespace {

constexpr float kQuadVertices[] = {
    -1.0f, -1.0f,  0.0f, 1.0f,
     1.0f, -1.0f,  1.0f, 1.0f,
    -1.0f,  1.0f,  0.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 0.0f,
};

QShader loadShader(const QString& resourcePath)
{
    QFile f(resourcePath);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning("RhiBc3YCoCgWidget: cannot open shader %s",
                 qUtf8Printable(resourcePath));
        return {};
    }
    return QShader::fromSerialized(f.readAll());
}

} // namespace

RhiBc3YCoCgWidget::RhiBc3YCoCgWidget(QWidget* parent)
    : QRhiWidget(parent)
{
    setMinimumSize(320, 180);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setAutoRenderTarget(true);
}

RhiBc3YCoCgWidget::~RhiBc3YCoCgWidget() = default;

void RhiBc3YCoCgWidget::setLabel(const QString& text)
{
    if (m_label != text) {
        m_label = text;
        update();
    }
}

int RhiBc3YCoCgWidget::bytesPerRowBlocks(int width)
{
    const int blocksWide = (width + 3) / 4;
    return blocksWide * 16;
}

void RhiBc3YCoCgWidget::setBc3Texture(const QByteArray& dxtBytes, int width, int height)
{
    if (width <= 0 || height <= 0 || dxtBytes.isEmpty()) {
        clearTexture();
        return;
    }
    m_pendingDxt = dxtBytes;
    m_texW       = width;
    m_texH       = height;
    m_dirty      = true;
    update();
}

void RhiBc3YCoCgWidget::clearTexture()
{
    m_pendingDxt.clear();
    m_texW  = 0;
    m_texH  = 0;
    m_dirty = true;
    update();
}

void RhiBc3YCoCgWidget::releaseGpuResources()
{
    m_pipeline.reset();
    m_srb.reset();
    m_texture.reset();
    m_sampler.reset();
    m_ubuf.reset();
    m_vbuf.reset();
    m_pipelineReady = false;
}

void RhiBc3YCoCgWidget::releaseResources()
{
    releaseGpuResources();
}

void RhiBc3YCoCgWidget::initialize(QRhiCommandBuffer* cb)
{
    QRhi* r = rhi();
    if (!r) return;
    if (m_pipelineReady) return;

    m_vbuf.reset(r->newBuffer(QRhiBuffer::Immutable,
                              QRhiBuffer::VertexBuffer,
                              sizeof(kQuadVertices)));
    m_vbuf->create();

    m_ubuf.reset(r->newBuffer(QRhiBuffer::Dynamic,
                              QRhiBuffer::UniformBuffer,
                              32));
    m_ubuf->create();

    m_sampler.reset(r->newSampler(QRhiSampler::Linear, QRhiSampler::Linear,
                                  QRhiSampler::None,
                                  QRhiSampler::ClampToEdge,
                                  QRhiSampler::ClampToEdge));
    m_sampler->create();

    m_texture.reset(r->newTexture(QRhiTexture::BC3, QSize(4, 4), 1, {}));
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

    QShader vs = loadShader(QStringLiteral(":/shaders/dxv_ycocg.vert.qsb"));
    QShader fs = loadShader(QStringLiteral(":/shaders/dxv_ycocg.frag.qsb"));
    if (!vs.isValid() || !fs.isValid()) {
        qWarning("RhiBc3YCoCgWidget: shader modules missing from resources");
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

    QRhiResourceUpdateBatch* batch = r->nextResourceUpdateBatch();
    batch->uploadStaticBuffer(m_vbuf.get(), kQuadVertices);
    const float initialUbo[8] = { 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    batch->updateDynamicBuffer(m_ubuf.get(), 0, sizeof(initialUbo), initialUbo);
    cb->resourceUpdate(batch);

    m_pipelineReady = true;
}

void RhiBc3YCoCgWidget::uploadIfNeeded(QRhiResourceUpdateBatch* batch)
{
    if (!m_dirty) return;
    m_dirty = false;

    auto rebuildSrb = [this] {
        m_srb->setBindings({
            QRhiShaderResourceBinding::uniformBuffer(
                0, QRhiShaderResourceBinding::VertexStage,
                m_ubuf.get()),
            QRhiShaderResourceBinding::sampledTexture(
                1, QRhiShaderResourceBinding::FragmentStage,
                m_texture.get(), m_sampler.get()),
        });
        m_srb->create();
    };

    if (m_pendingDxt.isEmpty() || m_texW <= 0 || m_texH <= 0) {
        static const quint8 dark[16] = {}; // one black BC3 block
        if (m_texture->pixelSize() != QSize(4, 4)) {
            m_texture->setPixelSize(QSize(4, 4));
            m_texture->create();
            rebuildSrb();
        }
        QRhiTextureSubresourceUploadDescription sub(dark, sizeof(dark));
        batch->uploadTexture(m_texture.get(),
                             QRhiTextureUploadDescription(QRhiTextureUploadEntry(0, 0, sub)));
        return;
    }

    const QSize sz(m_texW, m_texH);
    if (m_texture->pixelSize() != sz) {
        m_texture->setPixelSize(sz);
        m_texture->create();
        rebuildSrb();
    }

    const int stride = bytesPerRowBlocks(m_texW);
    QRhiTextureSubresourceUploadDescription sub(
        m_pendingDxt.constData(),
        quint32(m_pendingDxt.size()));
    sub.setDataStride(quint32(stride));

    batch->uploadTexture(m_texture.get(),
                         QRhiTextureUploadDescription(QRhiTextureUploadEntry(0, 0, sub)));
}

void RhiBc3YCoCgWidget::updateUniformBuffer(QRhiResourceUpdateBatch* batch)
{
    QSize widget = renderTarget() ? renderTarget()->pixelSize() : size();
    if (widget.isEmpty()) widget = QSize(1, 1);

    float sx = 1.0f;
    float sy = 1.0f;
    if (m_texW > 0 && m_texH > 0) {
        const double srcA = double(m_texW) / double(m_texH);
        const double dstA = double(widget.width()) / double(widget.height());
        if (srcA > dstA) {
            sy = float(dstA / srcA);
        } else {
            sx = float(srcA / dstA);
        }
    }

    const float ubo[8] = { sx, sy, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    batch->updateDynamicBuffer(m_ubuf.get(), 0, sizeof(ubo), ubo);
}

void RhiBc3YCoCgWidget::render(QRhiCommandBuffer* cb)
{
    QRhi* r = rhi();
    if (!r || !m_pipelineReady) return;

    QRhiResourceUpdateBatch* batch = r->nextResourceUpdateBatch();
    uploadIfNeeded(batch);
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

void RhiBc3YCoCgWidget::paintEvent(QPaintEvent* event)
{
    QRhiWidget::paintEvent(event);

    if (m_label.isEmpty()) return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QColor(230, 230, 230, 220));
    QFont f = p.font();
    f.setBold(true);
    if (m_texW <= 0) {
        f.setPointSizeF(f.pointSizeF() * 1.4);
        p.setFont(f);
        p.drawText(rect(), Qt::AlignCenter, m_label);
    } else {
        p.setFont(f);
        p.drawText(rect().adjusted(8, 6, -8, -6), Qt::AlignTop | Qt::AlignLeft, m_label);
    }
}

} // namespace pvj::render
