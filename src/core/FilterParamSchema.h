#pragma once

#include "Model.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVector>

namespace pvj::core {

enum class FilterParamKind {
    Float,
    Angle,
    Percent,
    Color,
    Bool,
    EnumIndex,
};

struct FilterParamSpec {
    QString name;
    QString label;
    QString section;
    FilterParamKind kind = FilterParamKind::Float;
    double minV = 0.0;
    double maxV = 1.0;
    double defaultV = 0.5;
    QStringList enumLabels;
};

/// Blur filters may exceed the legacy 4-parameter UBO slots (extras via namedParam in packer).
bool filterAllowsExtendedParams(const QString& typeId);

struct FilterNodeSpec {
    QString typeId;
    QVector<FilterParamSpec> params;
};

const QHash<QString, FilterNodeSpec>& filterParamSchemas();
QList<EffectParam> defaultParamsFor(const QString& typeId);

} // namespace pvj::core
