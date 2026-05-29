#pragma once

#include "core/FilterCatalog.h"

#include <QDialog>
#include <QString>

#include <functional>
#include <optional>

class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;

namespace pvj::app {

struct FilterPickerOptions {
    QString title;
    /// When null, cell-graph defaults apply (exclude NVIDIA Maxine catalog category).
    std::function<bool(const pvj::core::FilterCatalogEntry&)> includeEntry;
};

/// Modal filter catalog picker with search. Returns chosen typeId or nullopt on cancel.
class FilterPickerDialog : public QDialog
{
    Q_OBJECT
public:
    explicit FilterPickerDialog(QWidget* parent = nullptr);

    void setOptions(const FilterPickerOptions& opts);
    void rebuildTree();

    static std::optional<QString> pick(QWidget* parent, const FilterPickerOptions& opts = {});

private slots:
    void onSearchTextChanged(const QString& text);
    void onItemActivated(QTreeWidgetItem* item, int column);
    void onAcceptSelection();

private:
    void applySearchFilter(const QString& query);
    QString selectedTypeId() const;
    QTreeWidgetItem* addFilterItem(QTreeWidgetItem* categoryItem, const pvj::core::FilterCatalogEntry& e);

    FilterPickerOptions m_opts;
    QLineEdit*          m_searchEdit = nullptr;
    QTreeWidget*        m_tree       = nullptr;
    QHash<QString, QTreeWidgetItem*> m_categoryItems;
};

} // namespace pvj::app
