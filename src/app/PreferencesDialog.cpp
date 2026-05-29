#include "PreferencesDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
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

QString formatMidiActivityLine(const QByteArray& bytes)
{
    if (bytes.isEmpty()) {
        return PreferencesDialog::tr("Empty message");
    }
    const auto u = [&bytes](int i) -> int {
        return (i >= 0 && i < bytes.size()) ? int(static_cast<unsigned char>(bytes[i])) : 0;
    };
    const int status = u(0);
    if (status >= 0xF8) {
        return PreferencesDialog::tr("System message 0x%1")
            .arg(status, 2, 16, QLatin1Char('0'));
    }
    const int channel = (status & 0x0F) + 1;
    const int high    = status & 0xF0;

    if (bytes.size() >= 3 && high == 0xB0) {
        return PreferencesDialog::tr("CC — channel %1, controller %2, value %3")
            .arg(channel)
            .arg(u(1))
            .arg(u(2));
    }
    if (bytes.size() >= 3 && high == 0x90) {
        if (u(2) == 0) {
            return PreferencesDialog::tr("Note off — channel %1, note %2").arg(channel).arg(u(1));
        }
        return PreferencesDialog::tr("Note on — channel %1, note %2, velocity %3")
            .arg(channel)
            .arg(u(1))
            .arg(u(2));
    }
    if (bytes.size() >= 3 && high == 0x80) {
        return PreferencesDialog::tr("Note off — channel %1, note %2, velocity %3")
            .arg(channel)
            .arg(u(1))
            .arg(u(2));
    }
    if (bytes.size() >= 2 && (high == 0xC0 || high == 0xD0)) {
        return PreferencesDialog::tr("Program / channel pressure — channel %1, value %2")
            .arg(channel)
            .arg(u(1));
    }
    return PreferencesDialog::tr("Raw: %1").arg(QString::fromLatin1(bytes.toHex(' ')));
}
} // namespace

PreferencesDialog::PreferencesDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Preferences"));
    setModal(true);
    resize(480, 520);

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

    auto* midiGroup = new QGroupBox(tr("MIDI Input"), this);
    auto* midiForm  = new QFormLayout(midiGroup);

    m_midiDeviceList = new QListWidget(midiGroup);
    m_midiDeviceList->setMinimumHeight(120);
    m_midiDeviceList->setSelectionMode(QAbstractItemView::NoSelection);

    m_midiRefreshBtn = new QPushButton(tr("Refresh"), midiGroup);
    connect(m_midiRefreshBtn, &QPushButton::clicked, this, [this]() {
        emit midiDevicesRefreshRequested();
    });

    auto* midiRow = new QHBoxLayout;
    midiRow->addWidget(m_midiDeviceList, 1);
    midiRow->addWidget(m_midiRefreshBtn);
    midiForm->addRow(tr("Devices:"), midiRow);

    m_midiStatusLabel = new QLabel(midiGroup);
    m_midiStatusLabel->setWordWrap(true);
    m_midiStatusLabel->setStyleSheet(QStringLiteral("color: palette(mid);"));
    midiForm->addRow(QString(), m_midiStatusLabel);

    m_midiSignalField = new QLineEdit(midiGroup);
    m_midiSignalField->setReadOnly(true);
    m_midiSignalField->setMinimumHeight(28);
    QFont mono = m_midiSignalField->font();
    mono.setStyleHint(QFont::Monospace);
    m_midiSignalField->setFont(mono);
    midiForm->addRow(tr("Signal:"), m_midiSignalField);

    auto* midiHelp = new QLabel(
        tr("Check one or more MIDI input ports (multiple controllers at once).\n"
           "Use Refresh after plugging in a device. Uncheck all to disable MIDI input.\n"
           "The Signal field shows live MIDI when a port is open — move a knob or press a pad to test."),
        midiGroup);
    midiHelp->setWordWrap(true);
    midiForm->addRow(midiHelp);

    root->addWidget(midiGroup);

    m_buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply, this);
    root->addWidget(m_buttons);

    connect(m_buttons, &QDialogButtonBox::accepted, this, [this] {
        emit stagePixelSizeChanged(stagePixelSize());
        emitMidiPortsIfNeeded();
        accept();
    });
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, [this] {
        emit stagePixelSizeChanged(stagePixelSize());
        emitMidiPortsIfNeeded();
    });

    connect(m_preset, QOverload<int>::of(&QComboBox::activated),
            this, &PreferencesDialog::onPresetActivated);
}

void PreferencesDialog::rebuildMidiList(const QStringList& ports, const QStringList& selectedNames)
{
    if (!m_midiDeviceList) {
        return;
    }
    QSignalBlocker block(m_midiDeviceList);
    m_midiDeviceList->clear();
    for (const QString& name : ports) {
        auto* item = new QListWidgetItem(name, m_midiDeviceList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(selectedNames.contains(name) ? Qt::Checked : Qt::Unchecked);
    }
    m_midiDeviceList->setEnabled(!ports.isEmpty());
    m_midiRefreshBtn->setEnabled(true);
}

void PreferencesDialog::resetMidiSignalMonitor(bool inputEnabled)
{
    m_midiMessageCount = 0;
    if (!m_midiSignalField) {
        return;
    }
    if (!inputEnabled) {
        m_midiSignalField->setText(tr("No MIDI input enabled — select devices and click Apply."));
        m_midiSignalField->setStyleSheet(QString());
        return;
    }
    m_midiSignalField->setText(tr("Waiting for MIDI signal… move a control on your controller."));
    m_midiSignalField->setStyleSheet(QStringLiteral(
        "QLineEdit { background-color: palette(base); color: palette(mid); }"));
}

void PreferencesDialog::reportMidiInputActivity(const QByteArray& bytes)
{
    if (!m_midiSignalField) {
        return;
    }
    ++m_midiMessageCount;
    const QString line = formatMidiActivityLine(bytes);
    m_midiSignalField->setText(
        tr("Signal received — %1  (messages: %2)").arg(line).arg(m_midiMessageCount));
    m_midiSignalField->setStyleSheet(QStringLiteral(
        "QLineEdit { background-color: #1a3d28; color: #b8f0c8; "
        "border: 1px solid #3dbf6a; }"));
}

void PreferencesDialog::setMidiInputPorts(const QStringList& ports,
                                          const QStringList& selectedNames,
                                          const QStringList& connectedNames)
{
    rebuildMidiList(ports, selectedNames);
    resetMidiSignalMonitor(!connectedNames.isEmpty());

    if (!m_midiStatusLabel) {
        return;
    }
    if (ports.isEmpty()) {
        m_midiStatusLabel->setText(
            tr("No MIDI input devices detected. Connect a controller or install a virtual MIDI cable (e.g. loopMIDI)."));
        return;
    }
    if (!connectedNames.isEmpty()) {
        m_midiStatusLabel->setText(
            tr("Currently connected (%1): %2")
                .arg(connectedNames.size())
                .arg(connectedNames.join(QStringLiteral(", "))));
    } else if (!selectedNames.isEmpty()) {
        m_midiStatusLabel->setText(
            tr("Saved selection not open (ports may be in use by another app): %1")
                .arg(selectedNames.join(QStringLiteral(", "))));
    } else {
        m_midiStatusLabel->setText(tr("No MIDI input enabled."));
    }
}

QStringList PreferencesDialog::selectedMidiPortNames() const
{
    QStringList out;
    if (!m_midiDeviceList) {
        return out;
    }
    for (int i = 0; i < m_midiDeviceList->count(); ++i) {
        const QListWidgetItem* item = m_midiDeviceList->item(i);
        if (item && item->checkState() == Qt::Checked) {
            out.append(item->text());
        }
    }
    return out;
}

void PreferencesDialog::emitMidiPortsIfNeeded()
{
    emit midiInputPortsChanged(selectedMidiPortNames());
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
