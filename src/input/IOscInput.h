#pragma once

#include "IInputSource.h"

namespace pvj::input {

/// OSC input is reserved for a later milestone; this stub keeps the
/// architecture slot without pulling in liblo/oscpack yet.
class OscInputStub final : public IInputSource
{
public:
    void start() override {}
    void stop() override {}
};

} // namespace pvj::input
