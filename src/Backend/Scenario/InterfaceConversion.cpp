#include "InterfaceConversion.h"

#include "Backend/Commons/LogCategories.h"

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{
namespace InterfaceConversion
{

InterfaceMap toBackendInterfaces(const QStringList &landStrings,
                                 const QStringList &seaStrings)
{
    qCDebug(lcScenario) << "InterfaceConversion::toBackendInterfaces:"
                        << "land=" << landStrings << "sea=" << seaStrings;

    InterfaceMap map;

    QSet<TransportationTypes::TransportationMode> landSet;
    for (const QString &s : landStrings)
    {
        if (s == QLatin1String("Truck"))
            landSet.insert(TransportationTypes::TransportationMode::Truck);
        else if (s == QLatin1String("Rail"))
            landSet.insert(TransportationTypes::TransportationMode::Train);
        else
            qCWarning(lcScenario) << "InterfaceConversion::toBackendInterfaces:"
                                  << "unknown land mode string skipped:" << s;
    }
    if (!landSet.isEmpty())
        map.insert(TerminalTypes::TerminalInterface::LAND_SIDE, landSet);

    QSet<TransportationTypes::TransportationMode> seaSet;
    for (const QString &s : seaStrings)
    {
        if (s == QLatin1String("Ship"))
            seaSet.insert(TransportationTypes::TransportationMode::Ship);
        else
            qCWarning(lcScenario) << "InterfaceConversion::toBackendInterfaces:"
                                  << "unknown sea mode string skipped:" << s;
    }
    if (!seaSet.isEmpty())
        map.insert(TerminalTypes::TerminalInterface::SEA_SIDE, seaSet);

    qCDebug(lcScenario) << "InterfaceConversion::toBackendInterfaces:"
                        << "result interfaces=" << map.size();
    return map;
}

QPair<QStringList, QStringList> fromBackendInterfaces(const InterfaceMap &map)
{
    qCDebug(lcScenario) << "InterfaceConversion::fromBackendInterfaces:"
                        << "map size=" << map.size();

    QStringList land, sea;
    if (map.contains(TerminalTypes::TerminalInterface::LAND_SIDE))
    {
        for (auto mode : map.value(TerminalTypes::TerminalInterface::LAND_SIDE))
        {
            switch (mode)
            {
            case TransportationTypes::TransportationMode::Truck: land << "Truck"; break;
            case TransportationTypes::TransportationMode::Train: land << "Rail";  break;
            default: break;
            }
        }
    }
    if (map.contains(TerminalTypes::TerminalInterface::SEA_SIDE))
    {
        for (auto mode : map.value(TerminalTypes::TerminalInterface::SEA_SIDE))
        {
            if (mode == TransportationTypes::TransportationMode::Ship)
                sea << "Ship";
        }
    }

    qCDebug(lcScenario) << "InterfaceConversion::fromBackendInterfaces:"
                        << "land=" << land << "sea=" << sea;
    return { land, sea };
}

} // namespace InterfaceConversion
} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
