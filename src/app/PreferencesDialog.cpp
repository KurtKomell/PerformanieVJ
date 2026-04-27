#include "PreferencesDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace pvj::app {

namespace {
struct Preset { const char* label; int w; int h; };
constexpr Preset kPresets[] = {
    { "Custom",           0,    0    },
    { "HD 720p  (1280×720)",   1280,  720 },
    { "Full HD (1920×1080)",   1920, 1080 },
    { "QHD     (2560×1440)",   2560, 1440 },
    { "UHD 4K  (3840×2160)",   3840, 2160 },
    { "Square  (1080×1080)",   1080, 1080 },
    { "Vertical (1080×1920)",  1080, 1920 },
};
} // namespace

PreferencesDialog::PreferencesDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Preferences"));
    setModal(true);

    auto* root = new QVBoxLayout(this);

    auto* outGroup = new QGroupBox(tr("Output (Stage)"), this);
    auto* form = new QFormLayout(outGroup);

    m_preset = new QComboBox(outGroup);
    for (const auto& p : kPresets) {
        m_preset->addItem(QString::fromLatin1(p.label));
    }
    form->addRow(tr("Preset:"), m_preset);

    m_widthSpin = new QSpinBox(outGroup);
    m_widthSpin->setRange(16, 16384);
    m_widthSpin->setSingleStep(2);
    m_widthSpin->setSuffix(tr(" px"));
    form->addRow(tr("Width:"), m_widthSpin);

    m_heightSpin = new QSpinBox(outGroup);
    m_heightSpin->setRange(16, 16384);
    m_heightSpin->setSingleStep(2);
    m_heightSpin->setSuffix(tr(" px"));
    form->addRow(tr("Height:"), m_heightSpin);

    auto* help = new QLabel(
        tr("The stage size is the fixed pixel resolution used internally for the\n"
           "mixer. Preview windows and the fullscreen output\n"
           "letterbox/pillarbox the stage onto their surface."),
        outGroup);
    help->setWordWrap(true);
    form->addRow(help);

    root->addWidget(outGroup);

    m_buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply, this);
    root->addWidget(m_buttons);

    connect(m_buttons, &QDialogButtonBox::accepted, this, [this] {
        emit stagePixelSizeChanged(stagePixelSize());
        accept();
    });
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, [this] {
        emit stagePixelSizeChanged(stagePixelSize());
    });

    connect(m_preset, QOverload<int>::of(&QComboBox::activated),
            this, &PreferencesDialog::onPresetActivated);
}

void PreferencesDialog::setStagePixelSize(QSize size)
{
    if (size.width()  < m_widthSpin->minimum())  size.setWidth(m_widthSpin->minimum());
    if (size.height() < m_heightSpin->minimum()) size.setHeight(m_heightSpin->minimum());
    m_widthSpin->setValue(size.width());
    m_heightSpin->setValue(size.height());
    m_preset->setCurrentIndex(0);
    for (int i = 1; i < int(std::size(kPresets)); ++i) {
        if (kPresets[i].w == size.width() && kPresets[i].h == size.height()) {
            m_preset->setCurrentIndex(i);
            break;
        }
    }
}

QSize PreferencesDialog::stagePixelSize() const
{
    return QSize(m_widthSpin->value(), m_heightSpin->value());
}

void PreferencesDialog::onPresetActivated(int index)
{
    if (index <= 0 || index >= int(std::size(kPresets))) return;
    m_widthSpin->setValue(kPresets[index].w);
    m_heightSpin->setValue(kPresets[index].h);
}

} // namespace pvj::app
