#pragma once

#include "core/Model.h"

#include <QString>

namespace pvj::input {

/// Normalized hardware input for routing (MIDI channel is 0–15).
struct InputEvent {
    core::InputType type = core::InputType::None;
    int             channel = 0;
    int             number  = 0; ///< MIDI note or CC number (0–127)
    int             value   = 0; ///< MIDI velocity or CC value (0–127)
    int             qtKey   = 0; ///< Qt::Key for keyboard events
    int             keyboardModifiers = 0; ///< Qt::KeyboardModifiers as int
    QString         keyText; ///< QKeySequence string for persistence / matching
};

enum class LearnKind {
    None,
    CellTrigger,   ///< Next note or key → TriggerMapping (cell)
    PropertyCc,    ///< Next CC → PropertyMapping on selected cell
    PropertyNote,  ///< Next MIDI note → PropertyMapping (toggle / set-on-press)
    BankNav,       ///< Next MIDI note → TriggerMapping (bank next/prev/select/switch)
};

} // namespace pvj::input
