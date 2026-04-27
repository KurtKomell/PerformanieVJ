#pragma once

namespace pvj::input {

/// Pluggable input backend (MIDI, keyboard filter, OSC, …).
class IInputSource
{
public:
    virtual ~IInputSource() = default;

    virtual void start() {}
    virtual void stop() {}
};

} // namespace pvj::input
