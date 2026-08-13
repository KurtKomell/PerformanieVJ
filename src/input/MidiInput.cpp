#include "MidiInput.h"

#include <QDebug>
#include <QHash>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QSettings>
#include <QtGlobal>

#include <vector>

#if defined(__has_include)
#if __has_include(<RtMidi.h>)
#include <RtMidi.h>
#elif __has_include(<rtmidi/RtMidi.h>)
#include <rtmidi/RtMidi.h>
#else
#include <RtMidi.h>
#endif
#else
#include <RtMidi.h>
#endif

namespace pvj::input {

struct MidiInput::Impl {
    struct OpenPort {
        std::unique_ptr<RtMidiIn> in;
        QString                   name;
        int                       index = -1;
    };

    std::unique_ptr<RtMidiIn>  enumerator;
    std::vector<OpenPort>      open;

    // RT callback → UI: keep only the latest CC per channel/controller so a busy UI
    // thread does not process a long backlog of intermediate fader values.
    QMutex                     pendingMutex;
    QHash<quint32, QByteArray> pendingCc;
    QList<QByteArray>          pendingOther;
    bool                       flushScheduled = false;
};

namespace {

quint32 ccMergeKey(int status, int ccNumber)
{
    return (quint32(status & 0xFF) << 8) | quint32(ccNumber & 0x7F);
}

} // namespace

void MidiInput::rtMidiCallback(double /*stamp*/, std::vector<unsigned char>* message, void* userData)
{
    auto* self = static_cast<MidiInput*>(userData);
    if (!message || message->empty() || !self) {
        return;
    }
    QByteArray bytes;
    bytes.resize(int(message->size()));
    for (size_t i = 0; i < message->size(); ++i) {
        bytes[static_cast<int>(i)] = static_cast<char>((*message)[i]);
    }
    self->enqueueMidiBytes(bytes);
}

void MidiInput::enqueueMidiBytes(const QByteArray& bytes)
{
    if (bytes.isEmpty() || !m_impl) {
        return;
    }
    bool needSchedule = false;
    {
        QMutexLocker lock(&m_impl->pendingMutex);
        const unsigned char status = static_cast<unsigned char>(bytes[0]);
        const unsigned char high = status & 0xF0;
        if (high == 0xB0 && bytes.size() >= 3) {
            const quint32 key = ccMergeKey(status, int(uchar(bytes[1])));
            m_impl->pendingCc.insert(key, bytes);
        } else {
            m_impl->pendingOther.append(bytes);
        }
        if (!m_impl->flushScheduled) {
            m_impl->flushScheduled = true;
            needSchedule = true;
        }
    }
    if (needSchedule) {
        QMetaObject::invokeMethod(
            this,
            [this]() { flushPendingMidi(); },
            Qt::QueuedConnection);
    }
}

void MidiInput::flushPendingMidi()
{
    if (!m_impl) {
        return;
    }
    QHash<quint32, QByteArray> ccs;
    QList<QByteArray> other;
    {
        QMutexLocker lock(&m_impl->pendingMutex);
        ccs.swap(m_impl->pendingCc);
        other.swap(m_impl->pendingOther);
    }

    for (const QByteArray& bytes : other) {
        emit messageReceived(bytes);
    }
    for (auto it = ccs.constBegin(); it != ccs.constEnd(); ++it) {
        emit messageReceived(it.value());
    }

    bool needSchedule = false;
    {
        QMutexLocker lock(&m_impl->pendingMutex);
        if (m_impl->pendingCc.isEmpty() && m_impl->pendingOther.isEmpty()) {
            m_impl->flushScheduled = false;
        } else {
            // More MIDI arrived while we were emitting — flush again.
            m_impl->flushScheduled = true;
            needSchedule = true;
        }
    }
    if (needSchedule) {
        QMetaObject::invokeMethod(
            this,
            [this]() { flushPendingMidi(); },
            Qt::QueuedConnection);
    }
}

MidiInput::MidiInput(QObject* parent)
    : QObject(parent)
    , m_impl(std::make_unique<Impl>())
{
    m_impl->enumerator = std::make_unique<RtMidiIn>(RtMidi::UNSPECIFIED, "PerformanieVJ");
    m_impl->enumerator->ignoreTypes(false, false, false);
}

MidiInput::~MidiInput()
{
    closeAllPorts();
}

QStringList MidiInput::savedPortNames()
{
    QSettings settings;
    QStringList names = settings.value(QLatin1String(kSettingsPortNamesKey)).toStringList();
    if (!names.isEmpty()) {
        return names;
    }
    const QString legacy = settings.value(QLatin1String(kSettingsPortNameKey)).toString();
    if (!legacy.isEmpty()) {
        return {legacy};
    }
    return {};
}

QString MidiInput::savedPortName()
{
    const QStringList names = savedPortNames();
    return names.isEmpty() ? QString() : names.front();
}

int MidiInput::savedPortIndex()
{
    return QSettings().value(QLatin1String(kSettingsPortIndexKey), 0).toInt();
}

void MidiInput::savePreferredPorts(const QStringList& names)
{
    QSettings settings;
    settings.setValue(QLatin1String(kSettingsPortNamesKey), names);
    if (!names.isEmpty()) {
        settings.setValue(QLatin1String(kSettingsPortNameKey), names.front());
    } else {
        settings.remove(QLatin1String(kSettingsPortNameKey));
        settings.remove(QLatin1String(kSettingsPortIndexKey));
    }
}

void MidiInput::savePreferredPort(const QString& name, int index)
{
    savePreferredPorts(name.isEmpty() ? QStringList{} : QStringList{name});
    if (index >= 0) {
        QSettings().setValue(QLatin1String(kSettingsPortIndexKey), index);
    }
}

void MidiInput::clearPreferredPorts()
{
    QSettings settings;
    settings.remove(QLatin1String(kSettingsPortNamesKey));
    settings.remove(QLatin1String(kSettingsPortNameKey));
    settings.remove(QLatin1String(kSettingsPortIndexKey));
}

void MidiInput::clearPreferredPort()
{
    clearPreferredPorts();
}

QStringList MidiInput::portNames() const
{
    QStringList out;
    for (const MidiPortInfo& p : enumeratePorts()) {
        out.append(p.name);
    }
    return out;
}

QList<MidiPortInfo> MidiInput::enumeratePorts() const
{
    QList<MidiPortInfo> out;
    if (!m_impl || !m_impl->enumerator) {
        return out;
    }
    try {
        const unsigned int n = m_impl->enumerator->getPortCount();
        out.reserve(int(n));
        for (unsigned int i = 0; i < n; ++i) {
            MidiPortInfo info;
            info.name  = QString::fromStdString(m_impl->enumerator->getPortName(i));
            info.index = int(i);
            out.append(info);
        }
    } catch (...) {
        qWarning() << "RtMidi getPortCount/getPortName failed.";
    }
    return out;
}

QStringList MidiInput::openPortNames() const
{
    QStringList out;
    if (!m_impl) {
        return out;
    }
    for (const Impl::OpenPort& p : m_impl->open) {
        out.append(p.name);
    }
    return out;
}

QString MidiInput::openPortName() const
{
    const QStringList names = openPortNames();
    return names.isEmpty() ? QString() : names.front();
}

bool MidiInput::isOpen() const
{
    return m_impl && !m_impl->open.empty();
}

void MidiInput::closeAllPorts()
{
    if (!m_impl) {
        return;
    }
    for (Impl::OpenPort& p : m_impl->open) {
        if (p.in) {
            try {
                p.in->closePort();
            } catch (...) {
                qWarning() << "RtMidi closePort failed for" << p.name;
            }
        }
    }
    m_impl->open.clear();
}

bool MidiInput::openPortAtIndex(int index, const QString& expectedName)
{
    if (!m_impl || index < 0) {
        return false;
    }
    Impl::OpenPort entry;
    entry.in = std::make_unique<RtMidiIn>(RtMidi::UNSPECIFIED, "PerformanieVJ");
    entry.in->ignoreTypes(false, false, false);
    entry.in->setCallback(&MidiInput::rtMidiCallback, this);
    try {
        entry.in->openPort(static_cast<unsigned int>(index));
        entry.name  = QString::fromStdString(entry.in->getPortName());
        entry.index = index;
    } catch (...) {
        qWarning() << "RtMidi openPort failed for index" << index << expectedName;
        return false;
    }
    if (!expectedName.isEmpty()) {
        entry.name = expectedName;
    }
    m_impl->open.push_back(std::move(entry));
    return true;
}

bool MidiInput::openPortsByNames(const QStringList& names)
{
    closeAllPorts();
    if (names.isEmpty()) {
        savePreferredPorts({});
        return false;
    }

    const QList<MidiPortInfo> available = enumeratePorts();
    QStringList               opened;
    for (const QString& want : names) {
        if (want.isEmpty()) {
            continue;
        }
        bool found = false;
        for (const MidiPortInfo& p : available) {
            if (p.name != want) {
                continue;
            }
            if (openPortAtIndex(p.index, p.name)) {
                opened.append(p.name);
            }
            found = true;
            break;
        }
        if (!found) {
            qWarning() << "MIDI port not found:" << want;
        }
    }

    savePreferredPorts(opened);
    if (opened.isEmpty() && !names.isEmpty()) {
        qWarning() << "MIDI: could not open any of the selected ports.";
    }
    return !opened.empty();
}

bool MidiInput::openPort(int index)
{
    const QList<MidiPortInfo> ports = enumeratePorts();
    for (const MidiPortInfo& p : ports) {
        if (p.index == index) {
            return openPortsByNames({p.name});
        }
    }
    return false;
}

bool MidiInput::openPortByName(const QString& name)
{
    return openPortsByNames(name.isEmpty() ? QStringList{} : QStringList{name});
}

void MidiInput::closePort()
{
    closeAllPorts();
    clearPreferredPorts();
}

void MidiInput::start()
{
    if (isOpen()) {
        return;
    }
    const QStringList saved = savedPortNames();
    if (!saved.isEmpty()) {
        openPortsByNames(saved);
        return;
    }
    const QList<MidiPortInfo> ports = enumeratePorts();
    if (!ports.isEmpty()) {
        openPortsByNames({ports.front().name});
    }
}

void MidiInput::stop()
{
    closeAllPorts();
}

void MidiInput::reopenPreferredPort()
{
    const QStringList saved = savedPortNames();
    closeAllPorts();
    if (!saved.isEmpty()) {
        openPortsByNames(saved);
    } else {
        start();
    }
}

} // namespace pvj::input
