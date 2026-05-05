#pragma once

#include "Backend/Commons/TransportationMode.h"
#include "Backend/Scenario/FleetSpec.h"
#include "Backend/Scenario/LinkageSource.h"
#include "Backend/Scenario/NetworkSpec.h"
#include "Backend/Scenario/NodeLinkage.h"
#include "Backend/Scenario/RegionSpec.h"
#include "Backend/Scenario/SimulationSettings.h"
#include "Backend/Scenario/TerminalPlacement.h"

#include <QJsonObject>
#include <QMap>
#include <QPointF>
#include <QList>
#include <QString>
#include <QVariant>
#include <optional>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{
class ScenarioDocument;
struct DestinationRoute;
}

namespace Application
{

class ScenarioEditService
{
public:
    static QString createTerminal(
        Scenario::ScenarioDocument *doc,
        const QString              &terminalType,
        const QString              &region,
        const QPointF              &localLatLon,
        Scenario::TerminalPlacement::TerminalRole role =
            Scenario::TerminalPlacement::TerminalRole::Transit);

    static bool setTerminalRole(
        Scenario::ScenarioDocument *doc,
        const QString              &terminalId,
        Scenario::TerminalPlacement::TerminalRole role);

    static bool setTerminalType(
        Scenario::ScenarioDocument *doc,
        const QString              &terminalId,
        const QString              &newType);

    static bool removeTerminal(
        Scenario::ScenarioDocument *doc,
        const QString              &terminalId);

    static bool updateTerminalPosition(
        Scenario::ScenarioDocument *doc,
        const QString              &terminalId,
        const QPointF              &localLatLon);

    static bool updateTerminalPlacement(
        Scenario::ScenarioDocument         *doc,
        const QString                      &terminalId,
        const Scenario::TerminalPlacement &placement);

    static bool setTerminalProperty(
        Scenario::ScenarioDocument *doc,
        const QString              &terminalId,
        const QString              &key,
        const QVariant             &value);

    static bool setOriginDestinationScalar(
        Scenario::ScenarioDocument *doc,
        const QString              &originTerminalId,
        const QString              &destinationTerminalId);

    static bool setOriginDestinationRoutes(
        Scenario::ScenarioDocument              *doc,
        const QString                           &originTerminalId,
        const QList<Scenario::DestinationRoute> &routes);

    static bool clearOriginDestinations(
        Scenario::ScenarioDocument *doc,
        const QString              &originTerminalId);

    static bool linkTerminalToNode(
        Scenario::ScenarioDocument *doc,
        const QString              &terminalId,
        const QString              &networkName,
        int                         nodeId,
        Scenario::LinkageSource     source);

    static bool unlinkTerminalFromNode(
        Scenario::ScenarioDocument *doc,
        const QString              &terminalId,
        const QString              &networkName,
        int                         nodeId);

    static bool updateRegionLocalOrigin(
        Scenario::ScenarioDocument *doc,
        const QString              &regionName,
        const QPointF              &latLon);

    static bool updateRegionGlobalPosition(
        Scenario::ScenarioDocument *doc,
        const QString              &regionName,
        const QPointF              &latLon);

    static bool addRegion(
        Scenario::ScenarioDocument *doc,
        const Scenario::RegionSpec &spec);

    static bool removeRegion(
        Scenario::ScenarioDocument *doc,
        const QString              &name);

    static bool renameRegion(
        Scenario::ScenarioDocument *doc,
        const QString              &oldName,
        const QString              &newName);

    static bool updateRegionColor(
        Scenario::ScenarioDocument *doc,
        const QString              &name,
        const QString              &colorHex);

    static bool addNetwork(
        Scenario::ScenarioDocument *doc,
        const QString              &region,
        const Scenario::NetworkSpec &spec);

    static std::optional<Scenario::NetworkSpec> buildNetworkSpec(
        const Scenario::ScenarioDocument *doc,
        const QString                    &region,
        const QString                    &networkName,
        NetworkKind                       kind,
        const QMap<QString, QString>     &files,
        QString                          *error = nullptr);

    static bool addNetworkFromFiles(
        Scenario::ScenarioDocument   *doc,
        const QString                &region,
        const QString                &networkName,
        NetworkKind                   kind,
        const QMap<QString, QString> &files,
        QString                      *error = nullptr);

    static bool removeNetwork(
        Scenario::ScenarioDocument *doc,
        const QString              &region,
        const QString              &networkName);

    static bool renameNetwork(
        Scenario::ScenarioDocument *doc,
        const QString              &region,
        const QString              &oldName,
        const QString              &newName);

    static bool updateSimulationSettings(
        Scenario::ScenarioDocument         *doc,
        const Scenario::SimulationSettings &settings);

    static bool updateFleet(
        Scenario::ScenarioDocument *doc,
        const Scenario::FleetSpec  &fleet);

    static bool updateComparisonSnapshots(
        Scenario::ScenarioDocument *doc,
        const QList<QJsonObject>   &snapshots);

    static bool restoreLinkage(
        Scenario::ScenarioDocument *doc,
        const Scenario::NodeLinkage &snapshot);

    static bool restoreTerminal(
        Scenario::ScenarioDocument         *doc,
        const Scenario::TerminalPlacement &snapshot);
};

} // namespace Application
} // namespace Backend
} // namespace CargoNetSim
