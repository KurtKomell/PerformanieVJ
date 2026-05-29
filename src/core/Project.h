#pragma once

#include "Model.h"

#include <QList>
#include <QString>

namespace pvj::core {

/// Cell triggers (keyboard / MIDI note) apply to the cell index on every bank.
constexpr int kBankIndexAllBanks = -1;

class Project
{
public:
    Project();

    // Metadata
    QString filePath;         // absolute path of the .pvj file (empty for unsaved)
    QString formatVersion = QStringLiteral("1");
    QString sourceFormat  = QStringLiteral("pvj"); // "pvj", "vj2", "avc"

    // Content
    QList<MediaItem> mediaLibrary;
    QList<BankSet>   bankSets;     // single deck (TypeA only); legacy .pvj may list two — trimmed on load
    QList<TriggerMapping> triggerMappings;
    Settings settings;

    // Ensures the default BankSet structure: one TypeA set with 64 banks, each with gridRows×gridCols empty cells.
    void initializeDefault();
    void resizeBanksForGrid(int rows, int cols);
    /// Drop extra bank sets (legacy Deck B / GrandVJ Type 1), force TypeA, and remove trigger
    /// mappings that reference deck B or bank-set switching.
    void ensureSingleBankSet();

    /// Remove NVIDIA/Maxine filters from all cell chains (they belong on the Output tab).
    void stripMaxineFiltersFromCells();

    /// Normalize output filter chain to allowed NVIDIA entries only.
    void sanitizeOutputFilters();

    // Lookup helpers
    const MediaItem* findMedia(const QUuid& id) const;
    MediaItem*       findMedia(const QUuid& id);

    int bankSetIndex(BankSetType type) const;  // -1 if not present

    /// Portable QKeySequence string for the cell keyboard trigger, or empty if none.
    QString keyboardTriggerForCell(int bankSetIndex, int bankIndex, int cellIndex) const;
    /// Assigns at most one keyboard trigger per cell slot (all banks); steals the key from other cells.
    void setKeyboardTriggerForCell(int bankSetIndex, int bankIndex, int cellIndex,
                                  const QString& keyText, int qtKey = 0);
    void clearKeyboardTriggerForCell(int bankSetIndex, int bankIndex, int cellIndex);

    /// MIDI note bound to a cell slot (all banks), or channel < 0 if none.
    void midiCellTriggerForCell(int bankSetIndex, int bankIndex, int cellIndex,
                                int* channel, int* number) const;
    void setMidiCellTriggerForCell(int bankSetIndex, int cellIndex, int channel, int number);
    void clearMidiCellTriggerForCell(int bankSetIndex, int cellIndex);

    /// Label for a property mapping on a cell, e.g. "CH1 CC7", or empty if unmapped.
    QString propertyMappingLabel(int bankSetIndex, int bankIndex, int cellIndex,
                                 const QString& propertyId) const;

    /// Enforce at most one Key and one MidiNote per cell slot; migrate legacy per-bank entries.
    void normalizeCellSlotTriggers();

    /// @deprecated Use normalizeCellSlotTriggers().
    void normalizeKeyboardCellTriggers() { normalizeCellSlotTriggers(); }
};

} // namespace pvj::core
