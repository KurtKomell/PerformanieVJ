#pragma once

#include "Model.h"

#include <QString>
#include <QStringList>
#include <QUuid>

namespace pvj::core {
namespace PropertyRegistry {

enum class Kind {
    Continuous,
    Boolean,
    Enum,
};

QStringList allPropertyNames();
QString       labelFor(const QString& name);
/// Map GrandVJ / legacy tokens (e.g. TRSP, FADE) to canonical property ids (transparency, fade).
QString       resolvePropertyId(const QString& raw);
Kind          kindOf(const QString& name);
int           enumCountOf(const QString& name);
void          learnMinMax(const QString& name, double* minV, double* maxV);
bool          isFilterParamProperty(const QString& raw);
bool          isFeedbackProperty(const QString& raw);
bool          parseFilterParamProperty(const QString& raw, QUuid* node, QString* paramName);

bool readValue(const Cell& c, const QString& raw, double* out);

/// Apply a continuous value (already scaled to the property's natural range).
bool applyValue(Cell& c, const QString& raw, double v);

/// Map normalized 0..1 to an enum index and apply (CC scrub).
bool applyEnumFromNormalized(Cell& c, const QString& raw, double n01);

bool applyEnumIndex(Cell& c, const QString& raw, int index);

/// Flip bool, cycle enum, or invert continuous around midpoint.
bool toggleValue(Cell& c, const QString& raw);

bool applySetValue(Cell& c, const QString& raw, double v);

} // namespace PropertyRegistry
} // namespace pvj::core
