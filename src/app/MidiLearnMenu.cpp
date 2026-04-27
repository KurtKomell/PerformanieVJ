#include "MidiLearnMenu.h"

#include "core/PropertyRegistry.h"

#include <QMenu>
#include <QWidget>

namespace pvj::app {

void MidiLearnMenu::tagMidiWidget(QWidget* widget, const QString& propertyId, const QVariant& noteValue)
{
    if (!widget) {
        return;
    }
    widget->setProperty("pvjProperty", propertyId);
    if (noteValue.isValid()) {
        widget->setProperty("pvjMidiNoteValue", noteValue);
    } else {
        widget->setProperty("pvjMidiNoteValue", QVariant());
    }
}

void MidiLearnMenu::showForWidget(QWidget* parent, QWidget* widget, const QPoint& globalPos,
                                  const std::function<void(const QString&)>& onLearnCc,
                                  const std::function<void(const QString&, bool, double)>& onLearnNote,
                                  const std::function<void(const QString&)>& onClear)
{
    if (!widget) {
        return;
    }
    const QString prop = widget->property("pvjProperty").toString();
    if (prop.isEmpty()) {
        return;
    }
    namespace PReg = pvj::core::PropertyRegistry;
    const PReg::Kind k = PReg::kindOf(prop);
    const bool hasHit = widget->property("pvjMidiNoteValue").isValid();
    const double hitV = widget->property("pvjMidiNoteValue").toDouble();

    QMenu menu(parent);
    menu.addAction(QObject::tr("Learn MIDI CC…"), [onLearnCc, prop]() {
        if (onLearnCc) {
            onLearnCc(prop);
        }
    });
    if (k == PReg::Kind::Boolean) {
        menu.addAction(QObject::tr("Learn MIDI note (toggle)…"), [onLearnNote, prop]() {
            if (onLearnNote) {
                onLearnNote(prop, true, 0.0);
            }
        });
    } else if (k == PReg::Kind::Enum) {
        if (hasHit) {
            menu.addAction(QObject::tr("Learn MIDI note (select this value)…"), [onLearnNote, prop, hitV]() {
                if (onLearnNote) {
                    onLearnNote(prop, false, hitV);
                }
            });
        }
        menu.addAction(QObject::tr("Learn MIDI note (cycle / toggle)…"), [onLearnNote, prop]() {
            if (onLearnNote) {
                onLearnNote(prop, true, 0.0);
            }
        });
    } else {
        menu.addAction(QObject::tr("Learn MIDI note (invert / toggle)…"), [onLearnNote, prop]() {
            if (onLearnNote) {
                onLearnNote(prop, true, 0.0);
            }
        });
    }
    menu.addSeparator();
    menu.addAction(QObject::tr("Clear MIDI mapping for this control"), [onClear, prop]() {
        if (onClear) {
            onClear(prop);
        }
    });
    menu.exec(globalPos);
}

} // namespace pvj::app
