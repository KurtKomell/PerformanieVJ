#pragma once

#include <QString>

namespace pvj::app {

/// Platform-specific user data directory for settings (INI):
/// - Windows: `%LOCALAPPDATA%/PerformanieVJ`
/// - macOS: `~/Library/Application Support/PerformanieVJ`
/// - Linux: `~/.config/PerformanieVJ` (respects `$XDG_CONFIG_HOME`)
QString userDataRoot();

/// Routes all default `QSettings` to an INI file under `userDataRoot()`.
void configureUserStorage();

} // namespace pvj::app
