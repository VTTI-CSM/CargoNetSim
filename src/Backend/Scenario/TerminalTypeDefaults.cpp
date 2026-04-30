#include "TerminalTypeDefaults.h"

#include "Backend/Commons/LogCategories.h"
#include "PropertyKeys.h"

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

namespace PK = PropertyKeys;

namespace TerminalTypeDefaults
{

QStringList allTypes()
{
    QStringList types = {
        QStringLiteral("Sea Port Terminal"),
        QStringLiteral("Intermodal Land Terminal"),
        QStringLiteral("Train Stop/Depot"),
        QStringLiteral("Truck Parking"),
        QStringLiteral("Facility"),
    };
    qCDebug(lcScenario) << "TerminalTypeDefaults::allTypes:" << types;
    return types;
}

bool isValidType(const QString &type)
{
    bool valid = allTypes().contains(type);
    qCDebug(lcScenario) << "TerminalTypeDefaults::isValidType:"
                        << type << "=" << valid;
    if (!valid)
    {
        qCWarning(lcScenario) << "TerminalTypeDefaults::isValidType:"
                              << "unknown terminal type:" << type;
    }
    return valid;
}

QPair<QSet<TransportationTypes::TransportationMode>,
      QSet<TransportationTypes::TransportationMode>>
interfacesFor(const QString &type)
{
    qCDebug(lcScenario) << "TerminalTypeDefaults::interfacesFor:" << type;

    using Mode = TransportationTypes::TransportationMode;

    // Sea Port Terminal — TerminalItem.cpp:125-129.
    if (type == QLatin1String("Sea Port Terminal"))
    {
        return { { Mode::Truck, Mode::Train }, { Mode::Ship } };
    }

    // Intermodal Land Terminal — TerminalItem.cpp:130-135.
    if (type == QLatin1String("Intermodal Land Terminal"))
    {
        return { { Mode::Truck, Mode::Train }, {} };
    }

    // Train Stop/Depot — TerminalItem.cpp:137-142.
    if (type == QLatin1String("Train Stop/Depot"))
    {
        return { { Mode::Train }, {} };
    }

    // Truck Parking — TerminalItem.cpp:143-148.
    if (type == QLatin1String("Truck Parking"))
    {
        return { { Mode::Truck }, {} };
    }

    if (type == QLatin1String("Facility"))
    {
        qCDebug(lcScenario)
            << "TerminalTypeDefaults::interfacesFor: Facility"
            << "(land={Truck}, sea={})";
        return { { Mode::Truck }, {} };
    }

    qCWarning(lcScenario) << "TerminalTypeDefaults::interfacesFor:"
                          << "unknown type, returning empty interfaces:" << type;
    return { {}, {} };
}

QMap<QString, QVariant> defaultProperties(const QString &type)
{
    qCDebug(lcScenario) << "TerminalTypeDefaults::defaultProperties:" << type;

    if (!isValidType(type))
    {
        qCWarning(lcScenario) << "TerminalTypeDefaults::defaultProperties:"
                              << "invalid type, returning empty:" << type;
        return {};
    }

    QMap<QString, QVariant> props;

    // "Name" is a placeholder. GUI and authoring services overwrite it with
    // a user-visible terminal name, while default creation keeps the property
    // key present for generic readers.
    props[QStringLiteral("Name")]              = type;
    props[QStringLiteral("Show on Global Map")] = true;

    QMap<QString, QVariant> cost;
    cost[QStringLiteral("fixed_fees")]   = QStringLiteral("400");
    cost[QStringLiteral("customs_fees")] = QStringLiteral("100");
    cost[PK::Mode::RiskFactor]  = QStringLiteral("0.015");
    props[QStringLiteral("cost")] = cost;

    // Facility: cost only, no dwell_time/customs/capacity.
    if (type == QLatin1String("Facility"))
    {
        props[QStringLiteral("Show on Global Map")] = false;
        qCInfo(lcScenario)
            << "TerminalTypeDefaults::defaultProperties:"
            << "generated" << props.size()
            << "properties for type:" << type;
        return props;
    }

    QMap<QString, QVariant> dwellParams;
    dwellParams[QStringLiteral("mean")]    = QStringLiteral("172800");
    dwellParams[QStringLiteral("std_dev")] = QStringLiteral("43200");
    QMap<QString, QVariant> dwell;
    dwell[QStringLiteral("method")]     = QStringLiteral("normal");
    dwell[QStringLiteral("parameters")] = dwellParams;
    props[QStringLiteral("dwell_time")] = dwell;

    // Sea-port / intermodal get customs + capacity (TerminalItem.cpp:106-119).
    if (type == QLatin1String("Sea Port Terminal") ||
        type == QLatin1String("Intermodal Land Terminal"))
    {
        QMap<QString, QVariant> customs;
        customs[QStringLiteral("probability")]    = QStringLiteral("0.08");
        customs[QStringLiteral("delay_mean")]     = QStringLiteral("172800");
        customs[QStringLiteral("delay_variance")] = QStringLiteral("7464960000");
        props[QStringLiteral("customs")] = customs;

        QMap<QString, QVariant> capacity;
        capacity[QStringLiteral("max_capacity")]       = 100000;
        capacity[QStringLiteral("critical_threshold")] = 0.8;
        props[QStringLiteral("capacity")] = capacity;
    }

    // "Show on Global Map" defaults to true; Intermodal / Train Stop /
    // Truck Parking all suppress the global mirror (TerminalItem.cpp:134-147).
    if (type == QLatin1String("Intermodal Land Terminal") ||
        type == QLatin1String("Train Stop/Depot") ||
        type == QLatin1String("Truck Parking"))
    {
        props[QStringLiteral("Show on Global Map")] = false;
    }

    qCInfo(lcScenario) << "TerminalTypeDefaults::defaultProperties:"
                       << "generated" << props.size() << "properties for type:" << type;
    return props;
}

} // namespace TerminalTypeDefaults
} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
