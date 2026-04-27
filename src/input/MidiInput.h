#pragma once

#include "IInputSource.h"

#include <QByteArray>
#include <QObject>
#include <QStringList>

#include <memory>

namespace pvj::input {

/// RtMidi-based MIDI input; delivers messages on the GUI thread via `messageReceived`.
class MidiInput final : public QObject, public IInputSource
{
    Q_OBJECT
public:
    explicit MidiInput(QObject* parent = nullptr);
    ~MidiInput() override;

    QStringList portNames() const;
    bool openPort(int index);
    void closePort();
    void start() override;
    void stop() override;

    bool isOpen() const { return m_open; }

signals:
    void messageReceived(const QByteArray& bytes);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    bool m_open = false;
};

} // namespace pvj::input
