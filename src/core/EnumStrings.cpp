#include "EnumStrings.h"

namespace pvj::core::enums {

QString toString(CopyMode m)
{
    switch (m) {
    case CopyMode::Normal:              return QStringLiteral("normal");
    case CopyMode::Add:                 return QStringLiteral("add");
    case CopyMode::Multiply:            return QStringLiteral("multiply");
    case CopyMode::Screen:              return QStringLiteral("screen");
    case CopyMode::Lighten:             return QStringLiteral("lighten");
    case CopyMode::Darken:              return QStringLiteral("darken");
    case CopyMode::Difference:          return QStringLiteral("difference");
    case CopyMode::Overlay:             return QStringLiteral("overlay");
    case CopyMode::Atop:                return QStringLiteral("atop");
    case CopyMode::Average:             return QStringLiteral("average");
    case CopyMode::Brightest:           return QStringLiteral("brightest");
    case CopyMode::BurnColor:           return QStringLiteral("burnColor");
    case CopyMode::BurnLinear:          return QStringLiteral("burnLinear");
    case CopyMode::ChromaDifference:    return QStringLiteral("chromaDifference");
    case CopyMode::ColorBlend:          return QStringLiteral("colorBlend");
    case CopyMode::DarkerColor:         return QStringLiteral("darkerColor");
    case CopyMode::Dimmest:             return QStringLiteral("dimmest");
    case CopyMode::Divide:              return QStringLiteral("divide");
    case CopyMode::Dodge:               return QStringLiteral("dodge");
    case CopyMode::Exclude:             return QStringLiteral("exclude");
    case CopyMode::Freeze:              return QStringLiteral("freeze");
    case CopyMode::Glow:                return QStringLiteral("glow");
    case CopyMode::HardLight:           return QStringLiteral("hardLight");
    case CopyMode::HardMix:             return QStringLiteral("hardMix");
    case CopyMode::Heat:                return QStringLiteral("heat");
    case CopyMode::HueBlend:            return QStringLiteral("hueBlend");
    case CopyMode::Inside:              return QStringLiteral("inside");
    case CopyMode::InsideLuminance:     return QStringLiteral("insideLuminance");
    case CopyMode::Inverse:             return QStringLiteral("inverse");
    case CopyMode::LighterColor:        return QStringLiteral("lighterColor");
    case CopyMode::LuminanceDifference: return QStringLiteral("luminanceDifference");
    case CopyMode::Maximum:             return QStringLiteral("maximum");
    case CopyMode::Minimum:             return QStringLiteral("minimum");
    case CopyMode::Negate:              return QStringLiteral("negate");
    case CopyMode::Outside:             return QStringLiteral("outside");
    case CopyMode::OutsideLuminance:    return QStringLiteral("outsideLuminance");
    case CopyMode::Over:                return QStringLiteral("over");
    case CopyMode::Pinlight:            return QStringLiteral("pinlight");
    case CopyMode::Reflect:             return QStringLiteral("reflect");
    case CopyMode::SoftLight:           return QStringLiteral("softLight");
    case CopyMode::LinearLight:         return QStringLiteral("linearLight");
    case CopyMode::StencilLuminance:    return QStringLiteral("stencilLuminance");
    case CopyMode::Subtract:            return QStringLiteral("subtract");
    case CopyMode::Subtractive:         return QStringLiteral("subtractive");
    case CopyMode::Under:               return QStringLiteral("under");
    case CopyMode::VividLight:          return QStringLiteral("vividLight");
    case CopyMode::Xor:                 return QStringLiteral("xor");
    case CopyMode::YFilm:               return QStringLiteral("yFilm");
    case CopyMode::ZFilm:               return QStringLiteral("zFilm");
    case CopyMode::DifferenceVivid:     return QStringLiteral("differenceVivid");
    case CopyMode::DifferenceRgb:       return QStringLiteral("differenceRgb");
    }
    return QStringLiteral("normal");
}

CopyMode copyModeFromString(const QString& s, CopyMode fallback)
{
    bool ok = false;
    const int asInt = s.toInt(&ok);
    if (ok && asInt >= 0 && asInt <= 50) {
        return static_cast<CopyMode>(asInt);
    }

    const QString k = s.toLower();
    if (k == QLatin1String("normal"))              return CopyMode::Normal;
    if (k == QLatin1String("add"))                 return CopyMode::Add;
    if (k == QLatin1String("multiply"))            return CopyMode::Multiply;
    if (k == QLatin1String("screen"))              return CopyMode::Screen;
    if (k == QLatin1String("lighten"))             return CopyMode::Lighten;
    if (k == QLatin1String("darken"))              return CopyMode::Darken;
    if (k == QLatin1String("difference"))          return CopyMode::Difference;
    if (k == QLatin1String("overlay"))             return CopyMode::Overlay;
    if (k == QLatin1String("atop"))                return CopyMode::Atop;
    if (k == QLatin1String("average"))             return CopyMode::Average;
    if (k == QLatin1String("brightest"))           return CopyMode::Brightest;
    if (k == QLatin1String("burncolor"))           return CopyMode::BurnColor;
    if (k == QLatin1String("burnlinear"))          return CopyMode::BurnLinear;
    if (k == QLatin1String("chromadifference"))    return CopyMode::ChromaDifference;
    if (k == QLatin1String("colorblend"))          return CopyMode::ColorBlend;
    if (k == QLatin1String("darkercolor"))         return CopyMode::DarkerColor;
    if (k == QLatin1String("dimmest"))             return CopyMode::Dimmest;
    if (k == QLatin1String("divide"))              return CopyMode::Divide;
    if (k == QLatin1String("dodge"))               return CopyMode::Dodge;
    if (k == QLatin1String("exclude"))             return CopyMode::Exclude;
    if (k == QLatin1String("freeze"))              return CopyMode::Freeze;
    if (k == QLatin1String("glow"))                return CopyMode::Glow;
    if (k == QLatin1String("hardlight"))           return CopyMode::HardLight;
    if (k == QLatin1String("hardmix"))             return CopyMode::HardMix;
    if (k == QLatin1String("heat"))                return CopyMode::Heat;
    if (k == QLatin1String("hueblend"))             return CopyMode::HueBlend;
    if (k == QLatin1String("inside"))              return CopyMode::Inside;
    if (k == QLatin1String("insideluminance"))     return CopyMode::InsideLuminance;
    if (k == QLatin1String("inverse"))             return CopyMode::Inverse;
    if (k == QLatin1String("lightercolor"))        return CopyMode::LighterColor;
    if (k == QLatin1String("luminancedifference"))   return CopyMode::LuminanceDifference;
    if (k == QLatin1String("maximum"))             return CopyMode::Maximum;
    if (k == QLatin1String("minimum"))             return CopyMode::Minimum;
    if (k == QLatin1String("negate"))              return CopyMode::Negate;
    if (k == QLatin1String("outside"))             return CopyMode::Outside;
    if (k == QLatin1String("outsideluminance"))    return CopyMode::OutsideLuminance;
    if (k == QLatin1String("over"))                return CopyMode::Over;
    if (k == QLatin1String("pinlight"))            return CopyMode::Pinlight;
    if (k == QLatin1String("reflect"))             return CopyMode::Reflect;
    if (k == QLatin1String("softlight"))           return CopyMode::SoftLight;
    if (k == QLatin1String("linearlight"))          return CopyMode::LinearLight;
    if (k == QLatin1String("stencilluminance"))    return CopyMode::StencilLuminance;
    if (k == QLatin1String("subtract"))            return CopyMode::Subtract;
    if (k == QLatin1String("subtractive"))         return CopyMode::Subtractive;
    if (k == QLatin1String("under"))               return CopyMode::Under;
    if (k == QLatin1String("vividlight"))          return CopyMode::VividLight;
    if (k == QLatin1String("xor"))                 return CopyMode::Xor;
    if (k == QLatin1String("yfilm"))               return CopyMode::YFilm;
    if (k == QLatin1String("zfilm"))               return CopyMode::ZFilm;
    if (k == QLatin1String("differencevivid"))     return CopyMode::DifferenceVivid;
    if (k == QLatin1String("differencergb"))       return CopyMode::DifferenceRgb;
    return fallback;
}

QString toString(MaskType m)
{
    switch (m) {
    case MaskType::None:      return QStringLiteral("none");
    case MaskType::Rectangle: return QStringLiteral("rectangle");
    case MaskType::Circle:    return QStringLiteral("circle");
    case MaskType::SoftEdge:  return QStringLiteral("softEdge");
    case MaskType::Ellipse:   return QStringLiteral("ellipse");
    case MaskType::Custom:    return QStringLiteral("custom");
    }
    return QStringLiteral("none");
}

MaskType maskTypeFromString(const QString& s, MaskType fallback)
{
    const QString k = s.toLower();
    if (k == QLatin1String("none"))      return MaskType::None;
    if (k == QLatin1String("rectangle")) return MaskType::Rectangle;
    if (k == QLatin1String("circle"))    return MaskType::Circle;
    if (k == QLatin1String("softedge"))  return MaskType::SoftEdge;
    if (k == QLatin1String("ellipse"))   return MaskType::Ellipse;
    if (k == QLatin1String("custom"))    return MaskType::Custom;
    return fallback;
}

QString toString(LayerMatteRole r)
{
    switch (r) {
    case LayerMatteRole::None:      return QStringLiteral("none");
    case LayerMatteRole::LumaMatte: return QStringLiteral("lumaMatte");
    case LayerMatteRole::AlphaMatte:return QStringLiteral("alphaMatte");
    case LayerMatteRole::KnockOut:  return QStringLiteral("knockOut");
    }
    return QStringLiteral("none");
}

LayerMatteRole layerMatteRoleFromString(const QString& s, LayerMatteRole fallback)
{
    bool ok = false;
    const int asInt = s.toInt(&ok);
    if (ok && asInt >= 0 && asInt <= 3) {
        return static_cast<LayerMatteRole>(asInt);
    }
    const QString k = s.toLower();
    if (k == QLatin1String("none"))      return LayerMatteRole::None;
    if (k == QLatin1String("lumamatte")) return LayerMatteRole::LumaMatte;
    if (k == QLatin1String("alphamatte"))return LayerMatteRole::AlphaMatte;
    if (k == QLatin1String("knockout"))  return LayerMatteRole::KnockOut;
    return fallback;
}

QString toString(KeyingMode m)
{
    switch (m) {
    case KeyingMode::Luma:   return QStringLiteral("luma");
    case KeyingMode::Chroma: return QStringLiteral("chroma");
    }
    return QStringLiteral("luma");
}

KeyingMode keyingModeFromString(const QString& s, KeyingMode fallback)
{
    bool ok = false;
    const int asInt = s.toInt(&ok);
    if (ok && asInt >= 0 && asInt <= 1) {
        return static_cast<KeyingMode>(asInt);
    }

    const QString k = s.toLower();
    if (k == QLatin1String("luma") || k == QLatin1String("sw")) {
        return KeyingMode::Luma;
    }
    if (k == QLatin1String("chroma") || k == QLatin1String("color") || k == QLatin1String("farbe")) {
        return KeyingMode::Chroma;
    }
    return fallback;
}

QString toString(VisualType v)
{
    switch (v) {
    case VisualType::Empty:       return QStringLiteral("empty");
    case VisualType::Media:       return QStringLiteral("media");
    case VisualType::Generator:   return QStringLiteral("generator");
    case VisualType::MixerFilter: return QStringLiteral("mixerFilter");
    }
    return QStringLiteral("empty");
}

VisualType visualTypeFromString(const QString& s, VisualType fallback)
{
    const QString k = s.toLower();
    if (k == QLatin1String("empty"))       return VisualType::Empty;
    if (k == QLatin1String("media"))       return VisualType::Media;
    if (k == QLatin1String("generator"))   return VisualType::Generator;
    if (k == QLatin1String("mixerfilter")) return VisualType::MixerFilter;
    return fallback;
}

QString toString(GeneratorKind g)
{
    switch (g) {
    case GeneratorKind::None:           return QStringLiteral("none");
    case GeneratorKind::InputSpout:     return QStringLiteral("inputSpout");
    case GeneratorKind::InputSyphon:    return QStringLiteral("inputSyphon");
    case GeneratorKind::InputNdi:       return QStringLiteral("inputNdi");
    case GeneratorKind::SolidColor:     return QStringLiteral("solidColor");
    case GeneratorKind::TestPattern:    return QStringLiteral("testPattern");
    case GeneratorKind::InternalFeedback: return QStringLiteral("internalFeedback");
    }
    return QStringLiteral("none");
}

GeneratorKind generatorFromString(const QString& s, GeneratorKind fallback)
{
    const QString k = s.toLower();
    if (k == QLatin1String("none"))           return GeneratorKind::None;
    if (k == QLatin1String("inputspout"))     return GeneratorKind::InputSpout;
    if (k == QLatin1String("inputsyphon"))    return GeneratorKind::InputSyphon;
    if (k == QLatin1String("inputndi"))       return GeneratorKind::InputNdi;
    if (k == QLatin1String("solidcolor"))     return GeneratorKind::SolidColor;
    if (k == QLatin1String("testpattern"))    return GeneratorKind::TestPattern;
    if (k == QLatin1String("internalfeedback")) return GeneratorKind::InternalFeedback;
    return fallback;
}

QString toString(InputType i)
{
    switch (i) {
    case InputType::None:     return QStringLiteral("none");
    case InputType::MidiNote: return QStringLiteral("midiNote");
    case InputType::MidiCC:   return QStringLiteral("midiCC");
    case InputType::MidiAftertouch: return QStringLiteral("midiAftertouch");
    case InputType::Key:      return QStringLiteral("key");
    case InputType::Osc:      return QStringLiteral("osc");
    }
    return QStringLiteral("none");
}

InputType inputTypeFromString(const QString& s, InputType fallback)
{
    const QString k = s.toLower();
    if (k == QLatin1String("none"))     return InputType::None;
    if (k == QLatin1String("midinote")) return InputType::MidiNote;
    if (k == QLatin1String("midicc"))   return InputType::MidiCC;
    if (k == QLatin1String("midiaftertouch")) return InputType::MidiAftertouch;
    if (k == QLatin1String("key"))      return InputType::Key;
    if (k == QLatin1String("osc"))      return InputType::Osc;
    return fallback;
}

QString toString(TriggerTarget t)
{
    switch (t) {
    case TriggerTarget::Cell:          return QStringLiteral("cell");
    case TriggerTarget::BankNext:      return QStringLiteral("bankNext");
    case TriggerTarget::BankPrev:      return QStringLiteral("bankPrev");
    case TriggerTarget::BankSelect:    return QStringLiteral("bankSelect");
    case TriggerTarget::BankSetSwitch: return QStringLiteral("bankSetSwitch");
    case TriggerTarget::Property:      return QStringLiteral("property");
    }
    return QStringLiteral("cell");
}

TriggerTarget triggerTargetFromString(const QString& s, TriggerTarget fallback)
{
    const QString k = s.toLower();
    if (k == QLatin1String("cell"))          return TriggerTarget::Cell;
    if (k == QLatin1String("banknext"))      return TriggerTarget::BankNext;
    if (k == QLatin1String("bankprev"))      return TriggerTarget::BankPrev;
    if (k == QLatin1String("bankselect"))    return TriggerTarget::BankSelect;
    if (k == QLatin1String("banksetswitch")) return TriggerTarget::BankSetSwitch;
    if (k == QLatin1String("property"))      return TriggerTarget::Property;
    return fallback;
}

QString toString(BankSetType t)
{
    return (t == BankSetType::TypeA) ? QStringLiteral("A") : QStringLiteral("B");
}

BankSetType bankSetTypeFromString(const QString& s, BankSetType fallback)
{
    const QString k = s.toUpper();
    if (k == QLatin1String("A") || k == QLatin1String("0")) return BankSetType::TypeA;
    if (k == QLatin1String("B") || k == QLatin1String("1")) return BankSetType::TypeB;
    return fallback;
}

QString toString(PropertyButtonMode m)
{
    switch (m) {
    case PropertyButtonMode::Continuous:
        return QStringLiteral("continuous");
    case PropertyButtonMode::Toggle:
        return QStringLiteral("toggle");
    case PropertyButtonMode::SetOnPress:
        return QStringLiteral("setOnPress");
    }
    return QStringLiteral("continuous");
}

PropertyButtonMode propertyButtonModeFromString(const QString& s, PropertyButtonMode fallback)
{
    const QString k = s.toLower();
    if (k == QLatin1String("continuous")) return PropertyButtonMode::Continuous;
    if (k == QLatin1String("toggle")) return PropertyButtonMode::Toggle;
    if (k == QLatin1String("setonpress")) return PropertyButtonMode::SetOnPress;
    return fallback;
}

QString toString(FeedbackInputMode m)
{
    switch (m) {
    case FeedbackInputMode::BelowOnly:       return QStringLiteral("belowOnly");
    case FeedbackInputMode::StackComposite: return QStringLiteral("stackComposite");
    case FeedbackInputMode::SceneLoopback:  return QStringLiteral("sceneLoopback");
    }
    return QStringLiteral("stackComposite");
}

FeedbackInputMode feedbackInputModeFromString(const QString& s, FeedbackInputMode fallback)
{
    const QString t = s.trimmed().toLower();
    if (t == QLatin1String("belowonly") || t == QLatin1String("below")) {
        return FeedbackInputMode::BelowOnly;
    }
    if (t == QLatin1String("stackcomposite") || t == QLatin1String("stack")) {
        return FeedbackInputMode::StackComposite;
    }
    if (t == QLatin1String("sceneloopback") || t == QLatin1String("scene")) {
        return FeedbackInputMode::SceneLoopback;
    }
    return fallback;
}

QString toString(WrapMode m)
{
    switch (m) {
    case WrapMode::Clamp:      return QStringLiteral("clamp");
    case WrapMode::Repeat:     return QStringLiteral("repeat");
    case WrapMode::Mirror:     return QStringLiteral("mirror");
    case WrapMode::MirrorOnce: return QStringLiteral("mirrorOnce");
    case WrapMode::Black:      return QStringLiteral("black");
    }
    return QStringLiteral("clamp");
}

WrapMode wrapModeFromString(const QString& s, WrapMode fallback)
{
    bool ok = false;
    const int asInt = s.toInt(&ok);
    if (ok && asInt >= 0 && asInt <= 4) {
        return static_cast<WrapMode>(asInt);
    }
    const QString k = s.toLower();
    if (k == QLatin1String("clamp"))      return WrapMode::Clamp;
    if (k == QLatin1String("repeat"))     return WrapMode::Repeat;
    if (k == QLatin1String("mirror"))     return WrapMode::Mirror;
    if (k == QLatin1String("mirroronce")) return WrapMode::MirrorOnce;
    if (k == QLatin1String("black"))      return WrapMode::Black;
    return fallback;
}

} // namespace pvj::core::enums
