#pragma once

#include "Model.h"

#include <QString>

namespace pvj::core::enums {

// CopyMode
QString toString(CopyMode m);
CopyMode copyModeFromString(const QString& s, CopyMode fallback = CopyMode::Normal);

// MaskType
QString toString(MaskType m);
MaskType maskTypeFromString(const QString& s, MaskType fallback = MaskType::None);

// WrapMode
QString toString(WrapMode w);
WrapMode wrapModeFromString(const QString& s, WrapMode fallback = WrapMode::Clamp);

// VisualType
QString toString(VisualType v);
VisualType visualTypeFromString(const QString& s, VisualType fallback = VisualType::Empty);

// GeneratorKind
QString toString(GeneratorKind g);
GeneratorKind generatorFromString(const QString& s, GeneratorKind fallback = GeneratorKind::None);

// InputType
QString toString(InputType i);
InputType inputTypeFromString(const QString& s, InputType fallback = InputType::None);

// TriggerTarget
QString toString(TriggerTarget t);
TriggerTarget triggerTargetFromString(const QString& s, TriggerTarget fallback = TriggerTarget::Cell);

// BankSetType
QString toString(BankSetType t);
BankSetType bankSetTypeFromString(const QString& s, BankSetType fallback = BankSetType::TypeA);

// PropertyButtonMode
QString toString(PropertyButtonMode m);
PropertyButtonMode propertyButtonModeFromString(const QString& s,
                                                PropertyButtonMode fallback = PropertyButtonMode::Continuous);

} // namespace pvj::core::enums
