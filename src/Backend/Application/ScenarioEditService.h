#pragma once

#include "Backend/Commons/TransportationMode.h"
#include "Backend/Scenario/Connection.h"
#include "Backend/Scenario/FleetSpec.h"
#include "Backend/Scenario/GlobalLink.h"
#include "Backend/Scenario/LinkageSource.h"
#include "Backend/Scenario/NetworkSpec.h"
#include "Backend/Scenario/NodeLinkage.h"
#include "Backend/Scenario/RegionSpec.h"
#include "Backend/Scenario/SimulationSettings.h"
#include "Backend/Scenario/TerminalPlacement.h"

#include <QPointF>
#include <QString>
#include <QVariant>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{
class ScenarioDocument;
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

    static bool setTerminalProperty(
        Scenario::ScenarioDocument *doc,
        const QString              &terminalId,
        const QString              &key,
        const QVariant             &value);

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

    static bool createConnection(
        Scenario::ScenarioDocument              *doc,
        const QString                           &fromTerminalId,
        const QString                           &toTerminalId,
        TransportationTypes::TransportationMode mode);

    static bool createConnection(
        Scenario::ScenarioDocument              *doc,
        const QString                           &fromTerminalId,
        const QString                           &toTerminalId,
        TransportationTypes::TransportationMode mode,
        const QVariantMap                       &properties,
        Scenario::LinkageSource                  source);

    static bool removeConnection(
        Scenario::ScenarioDocument              *doc,
        const QString                           &fromTerminalId,
        const QString                           &toTerminalId,
        TransportationTypes::TransportationMode mode);

    static bool createGlobalLink(
        Scenario::ScenarioDocument              *doc,
        const QString                           &fromTerminalId,
        const QString                           &toTerminalId,
        TransportationTypes::TransportationMode mode);

    static bool createGlobalLink(
        Scenario::ScenarioDocument              *doc,
        const QString                           &fromTerminalId,
        const QString                           &toTerminalId,
        TransportationTypes::TransportationMode mode,
        const QVariantMap                       &properties,
        Scenario::LinkageSource                  source);

    static bool removeGlobalLink(
        Scenario::ScenarioDocument              *doc,
        const QString                           &fromTerminalId,
        const QString                           &toTerminalId,
        TransportationTypes::TransportationMode mode);

    static bool setConnectionProperty(
        Scenario::ScenarioDocument              *doc,
        const QString                           &fromId,
        const QString                           &toId,
        TransportationTypes::TransportationMode mode,
        const QString                           &key,
        const QVariant                          &value);

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

    static bool restoreConnection(
        Scenario::ScenarioDocument *doc,
        const Scenario::Connection &snapshot);

    static bool restoreGlobalLink(
        Scenario::ScenarioDocument *doc,
        const Scenario::GlobalLink &snapshot);

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
