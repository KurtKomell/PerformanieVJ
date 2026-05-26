#pragma once

#include "Model.h"

#include <QList>
#include <QString>

namespace pvj::core {

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
};

} // namespace pvj::core
