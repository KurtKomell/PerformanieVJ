#include "MidiInput.h"

#include <QDebug>
#include <QTimer>
#include <QtGlobal>

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

namespace {

void midiCallback(double /*stamp*/, std::vector<unsigned char>* message, void* userData)
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
    QTimer::singleShot(0, self, [self, bytes]() {
        emit self->messageReceived(bytes);
    });
}

} // namespace

struct MidiInput::Impl {
    std::unique_ptr<RtMidiIn> in;
};

MidiInput::MidiInput(QObject* parent)
    : QObject(parent)
    , m_impl(std::make_unique<Impl>())
{
    m_impl->in = std::make_unique<RtMidiIn>(RtMidi::UNSPECIFIED, "PerformanieVJ");
    m_impl->in->ignoreTypes(false, false, false);
    m_impl->in->setCallback(&midiCallback, this);
}

MidiInput::~MidiInput()
{
    closePort();
}

QStringList MidiInput::portNames() const
{
    QStringList out;
    if (!m_impl || !m_impl->in) {
        return out;
    }
    const unsigned int n = m_impl->in->getPortCount();
    for (unsigned int i = 0; i < n; ++i) {
        out.append(QString::fromStdString(m_impl->in->getPortName(i)));
    }
    return out;
}

bool MidiInput::openPort(int index)
{
    if (!m_impl || !m_impl->in) {
        return false;
    }
    closePort();
    if (m_impl->in->getPortCount() == 0U) {
        return false;
    }
    const unsigned int i = static_cast<unsigned int>(qBound(0, index, int(m_impl->in->getPortCount()) - 1));
    try {
        m_impl->in->openPort(i);
    } catch (...) {
        qWarning() << "RtMidi openPort failed.";
        return false;
    }
    m_open = true;
    return true;
}

void MidiInput::closePort()
{
    if (!m_impl || !m_impl->in) {
        return;
    }
    if (m_open) {
        m_impl->in->closePort();
        m_open = false;
    }
}

void MidiInput::start()
{
    if (m_open) {
        return;
    }
    if (!openPort(0)) {
        qWarning() << "MIDI: no input ports available.";
    }
}

void MidiInput::stop()
{
    closePort();
}

} // namespace pvj::input
