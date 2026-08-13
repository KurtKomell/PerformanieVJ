#pragma once

#include "IInputSource.h"

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>
#include <vector>

namespace pvj::input {

struct MidiPortInfo {
    QString name;
    int     index = -1;
};

/// RtMidi-based MIDI input; supports multiple open ports; messages on the GUI thread.
class MidiInput final : public QObject, public IInputSource
{
    Q_OBJECT
public:
    static constexpr char kSettingsPortNamesKey[] = "midi/portNames";
    static constexpr char kSettingsPortNameKey[]  = "midi/portName";  // legacy single port
    static constexpr char kSettingsPortIndexKey[] = "midi/portIndex";

    explicit MidiInput(QObject* parent = nullptr);
    ~MidiInput() override;

    QStringList portNames() const;
    QList<MidiPortInfo> enumeratePorts() const;
    QStringList openPortNames() const;
    bool isOpen() const;

    /// Opens all named ports (closes previous). Returns true if at least one opened.
    bool openPortsByNames(const QStringList& names);
    void closeAllPorts();

    void start() override;
    void stop() override;
    void reopenPreferredPort();

    static QStringList savedPortNames();
    static void        savePreferredPorts(const QStringList& names);
    static void        clearPreferredPorts();

    /// Legacy single-port helpers (first selected / first open).
    static QString savedPortName();
    static int     savedPortIndex();
    static void    savePreferredPort(const QString& name, int index);
    static void    clearPreferredPort();
    QString openPortName() const;
    bool openPort(int index);
    bool openPortByName(const QString& name);
    void closePort();

signals:
    void messageReceived(const QByteArray& bytes);

private:
    static void rtMidiCallback(double stamp, std::vector<unsigned char>* message, void* userData);

    bool openPortAtIndex(int index, const QString& expectedName);
    void enqueueMidiBytes(const QByteArray& bytes);
    void flushPendingMidi();

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace pvj::input
