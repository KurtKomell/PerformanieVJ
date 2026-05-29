#pragma once

#include <QString>
#include <QVector>

namespace pvj::core {

/// Resolume-style video effect catalog for the filter node tree (stable `typeId` strings).
/// Rendering may only implement a subset; unknown IDs are preserved for future FFGL-style hooks.
struct FilterCatalogEntry {
    QString typeId;      ///< ASCII snake_case, stored in project files
    QString category;    ///< English category key (for grouping in UI)
    QString englishName; ///< Source string for QCoreApplication::translate("FilterCatalog", …)
};

const QVector<FilterCatalogEntry>& filterCatalogEntries();

/// O(1) average — builds hash on first use
QString filterCatalogEnglishName(const QString& typeId);

/// Catalog category string for a typeId (empty if unknown).
QString filterCatalogCategory(const QString& typeId);

/// True when `query` is empty or matches display name, English name, typeId, or category (case-insensitive).
bool filterCatalogMatchesSearch(const QString& query, const QString& displayName,
                                const QString& englishName, const QString& typeId,
                                const QString& category);

} // namespace pvj::core
