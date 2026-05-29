#include "FilterPickerDialog.h"

#include "core/FilterEffectIds.h"

#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace pvj::app {

namespace {

bool defaultIncludeCellFilter(const pvj::core::FilterCatalogEntry& e)
{
    return e.category != pvj::core::maxineFilterCategoryKey();
}

QString translatedCategory(const QString& categoryKey)
{
    return QCoreApplication::translate("FilterCatalog", categoryKey.toUtf8().constData());
}

QString translatedName(const QString& englishName)
{
    return QCoreApplication::translate("FilterCatalog", englishName.toUtf8().constData());
}

} // namespace

FilterPickerDialog::FilterPickerDialog(QWidget* parent)
    : QDialog(parent)
{
    setModal(true);
    resize(480, 420);

    auto* root = new QVBoxLayout(this);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Search filters…"));
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &FilterPickerDialog::onSearchTextChanged);
    root->addWidget(m_searchEdit);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setAlternatingRowColors(true);
    m_tree->setUniformRowHeights(true);
    connect(m_tree, &QTreeWidget::itemActivated, this, &FilterPickerDialog::onItemActivated);
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item, int) {
        if (item && !item->data(0, Qt::UserRole).toString().isEmpty()) {
            accept();
        }
    });
    root->addWidget(m_tree, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &FilterPickerDialog::onAcceptSelection);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);
}

void FilterPickerDialog::setOptions(const FilterPickerOptions& opts)
{
    m_opts = opts;
    if (m_opts.title.isEmpty()) {
        m_opts.title = tr("Select filter");
    }
    setWindowTitle(m_opts.title);
}

void FilterPickerDialog::rebuildTree()
{
    m_tree->clear();
    m_categoryItems.clear();

    const auto include = m_opts.includeEntry ? m_opts.includeEntry : defaultIncludeCellFilter;

    for (const auto& e : pvj::core::filterCatalogEntries()) {
        if (!include(e)) {
            continue;
        }
        const QString catLabel = translatedCategory(e.category);
        QTreeWidgetItem* catItem = m_categoryItems.value(catLabel);
        if (!catItem) {
            catItem = new QTreeWidgetItem(m_tree, { catLabel });
            catItem->setFlags(catItem->flags() & ~Qt::ItemIsSelectable);
            catItem->setExpanded(true);
            m_categoryItems.insert(catLabel, catItem);
        }
        addFilterItem(catItem, e);
    }

    if (m_tree->topLevelItemCount() > 0) {
        m_tree->setCurrentItem(m_tree->topLevelItem(0)->child(0));
    }
    applySearchFilter(m_searchEdit ? m_searchEdit->text() : QString());
}

QTreeWidgetItem* FilterPickerDialog::addFilterItem(QTreeWidgetItem* categoryItem,
                                                   const pvj::core::FilterCatalogEntry& e)
{
    const QString display = translatedName(e.englishName);
    auto* item = new QTreeWidgetItem(categoryItem, { display });
    item->setData(0, Qt::UserRole, e.typeId);
    item->setToolTip(0, e.typeId);
    return item;
}

void FilterPickerDialog::applySearchFilter(const QString& query)
{
    const QString q = query.trimmed();
    for (int ci = 0; ci < m_tree->topLevelItemCount(); ++ci) {
        QTreeWidgetItem* catItem = m_tree->topLevelItem(ci);
        int visibleChildren = 0;
        for (int i = 0; i < catItem->childCount(); ++i) {
            QTreeWidgetItem* child = catItem->child(i);
            const QString typeId = child->data(0, Qt::UserRole).toString();
            const pvj::core::FilterCatalogEntry* entry = nullptr;
            for (const auto& e : pvj::core::filterCatalogEntries()) {
                if (e.typeId == typeId) {
                    entry = &e;
                    break;
                }
            }
            bool visible = true;
            if (entry) {
                visible = pvj::core::filterCatalogMatchesSearch(
                    q, child->text(0), entry->englishName, entry->typeId,
                    translatedCategory(entry->category));
            }
            child->setHidden(!visible);
            if (visible) {
                ++visibleChildren;
            }
        }
        catItem->setHidden(visibleChildren == 0);
        if (!q.isEmpty() && visibleChildren > 0) {
            catItem->setExpanded(true);
        }
    }
}

void FilterPickerDialog::onSearchTextChanged(const QString& text)
{
    applySearchFilter(text);
}

QString FilterPickerDialog::selectedTypeId() const
{
    QTreeWidgetItem* item = m_tree->currentItem();
    if (!item) {
        return {};
    }
    const QString typeId = item->data(0, Qt::UserRole).toString();
    if (typeId.isEmpty() && item->childCount() > 0) {
        return item->child(0)->data(0, Qt::UserRole).toString();
    }
    return typeId;
}

void FilterPickerDialog::onItemActivated(QTreeWidgetItem* item, int /*column*/)
{
    if (!item) {
        return;
    }
    if (!item->data(0, Qt::UserRole).toString().isEmpty()) {
        accept();
    }
}

void FilterPickerDialog::onAcceptSelection()
{
    if (selectedTypeId().isEmpty()) {
        return;
    }
    accept();
}

std::optional<QString> FilterPickerDialog::pick(QWidget* parent, const FilterPickerOptions& opts)
{
    FilterPickerDialog dlg(parent);
    dlg.setOptions(opts);
    dlg.rebuildTree();
    if (dlg.m_tree->topLevelItemCount() == 0) {
        return std::nullopt;
    }
    dlg.m_searchEdit->setFocus();
    if (dlg.exec() != QDialog::Accepted) {
        return std::nullopt;
    }
    const QString typeId = dlg.selectedTypeId();
    if (typeId.isEmpty()) {
        return std::nullopt;
    }
    return typeId;
}

} // namespace pvj::app
