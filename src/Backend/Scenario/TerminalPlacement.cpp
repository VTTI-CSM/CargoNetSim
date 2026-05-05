#include "TerminalPlacement.h"

#include "Backend/Commons/LogCategories.h"

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

QString positionModeToString(TerminalPlacement::PositionMode m)
{
    qCDebug(lcScenario)
        << "positionModeToString() mode=" << static_cast<int>(m);
    switch (m)
    {
    case TerminalPlacement::PositionMode::NetworkNode: return QStringLiteral("network_node");
    case TerminalPlacement::PositionMode::LatLon:      return QStringLiteral("latlon");
    case TerminalPlacement::PositionMode::Scene:       return QStringLiteral("scene");
    }
    return QStringLiteral("network_node");
}

TerminalPlacement::PositionMode positionModeFromString(const QString &s, bool *ok)
{
    qCDebug(lcScenario)
        << "positionModeFromString() input=" << s;
    const QString lower = s.trimmed().toLower();
    if (ok) *ok = true;
    if (lower == QLatin1String("network_node")) return TerminalPlacement::PositionMode::NetworkNode;
    if (lower == QLatin1String("latlon"))       return TerminalPlacement::PositionMode::LatLon;
    if (lower == QLatin1String("scene"))        return TerminalPlacement::PositionMode::Scene;
    if (ok) *ok = false;
    return TerminalPlacement::PositionMode::NetworkNode;
}

QString roleToString(TerminalPlacement::TerminalRole r)
{
    switch (r)
    {
    case TerminalPlacement::TerminalRole::Transit:
        return QStringLiteral("transit");
    case TerminalPlacement::TerminalRole::Origin:
        return QStringLiteral("origin");
    case TerminalPlacement::TerminalRole::Destination:
        return QStringLiteral("destination");
    }
    return QStringLiteral("transit");
}

TerminalPlacement::TerminalRole roleFromString(const QString &s, bool *ok)
{
    const QString lower = s.trimmed().toLower();
    if (ok) *ok = true;
    if (lower == QLatin1String("origin"))
        return TerminalPlacement::TerminalRole::Origin;
    if (lower == QLatin1String("destination"))
        return TerminalPlacement::TerminalRole::Destination;
    if (lower == QLatin1String("transit"))
        return TerminalPlacement::TerminalRole::Transit;
    if (ok) *ok = false;
    return TerminalPlacement::TerminalRole::Transit;
}

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
