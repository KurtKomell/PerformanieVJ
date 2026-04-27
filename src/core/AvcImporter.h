#pragma once

#include "Project.h"

#include <QString>
#include <QStringList>

namespace pvj::core {

// Imports Resolume Avenue/Arena .avc composition files (read-only).
// Scope is intentionally minimal: we extract clip references and the
// column/layer layout. Resolume-specific effects, keyframes, blend modes
// and other parameters are ignored and reported via `warnings`.
//
// Layout mapping:
//   Resolume "column" -> our Bank (one Bank per column)
//   Resolume "layer"  -> row index inside the Bank (layer N -> cell index N)
// Clip files referenced by the composition become MediaLibrary entries; DXV
// clips will be playable once the DXV decoder (milestone M6) is implemented.
class AvcImporter
{
public:
    struct Result {
        bool        ok = false;
        QString     errorMessage;
        QStringList warnings;
    };

    static Result importFile(Project& project, const QString& filePath);
};

} // namespace pvj::core
