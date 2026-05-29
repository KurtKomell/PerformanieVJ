#pragma once

#include "core/Model.h"

#include <QPoint>
#include <QVariant>

#include <functional>

class QWidget;

namespace pvj::app {

class MidiLearnMenu
{
public:
    static QString formatLabel(const pvj::core::PropertyMapping& mapping);

    static void tagMidiWidget(QWidget* widget, const QString& propertyId,
                              const QVariant& noteValue = QVariant());

    static void showForWidget(QWidget* parent, QWidget* widget, const QPoint& globalPos,
                              const std::function<void(const QString&)>& onLearnCc,
                              const std::function<void(const QString&, bool, double)>& onLearnNote,
                              const std::function<void(const QString&)>& onClear);
};

} // namespace pvj::app
