#include "InputMappingLabels.h"

namespace pvj::core {

QString formatTriggerInputLabel(InputType input, int channel, int number)
{
    switch (input) {
    case InputType::MidiCC:
        return QStringLiteral("CH%1 CC%2").arg(channel + 1).arg(number);
    case InputType::MidiNote:
        return QStringLiteral("CH%1 N%2").arg(channel + 1).arg(number);
    case InputType::MidiAftertouch:
        return QStringLiteral("CH%1 AT%2").arg(channel + 1).arg(number);
    default:
        return {};
    }
}

QString formatPropertyMappingLabel(const PropertyMapping& mapping)
{
    return formatTriggerInputLabel(mapping.input, mapping.channel, mapping.number);
}

} // namespace pvj::core
