#pragma once

#include <QVariant>
#include <QVariantMap>
#include <optional>

#include "Backend/Commons/ShortestPathResult.h"
#include "Backend/Commons/TransportationMode.h"
#include "Backend/Scenario/Connection.h"
#include "Backend/Scenario/GlobalLink.h"
#include "Backend/Scenario/LinkageSource.h"

namespace CargoNetSim
{
class CargoNetSimController;

namespace Backend
{
class ConfigController;

namespace Scenario
{
class ScenarioDocument;
}

namespace Application
{

enum class RouteAuthoringServiceStatus
{
    Success,
    InvalidArguments,
    NotFound,
    MutationFailed,
    MetricComputationFailed
};

struct RouteAuthoringServiceResult
{
    RouteAuthoringServiceStatus status =
        RouteAuthoringServiceStatus::MutationFailed;
    QString     message;
    QVariantMap canonicalProperties;
    QString     selectedNetworkName;
    int         selectedStartNodeId = -1;
    int         selectedEndNodeId = -1;
    ShortestPathResult selectedPath;
    bool        routeCreated = false;
    bool        routeUpdated = false;

    bool succeeded() const
    {
        return status == RouteAuthoringServiceStatus::Success;
    }
};

class RouteAuthoringService
{
public:
    explicit RouteAuthoringService(::CargoNetSim::CargoNetSimController *controller);
    explicit RouteAuthoringService(ConfigController *config);

    RouteAuthoringServiceResult createConnection(
        Scenario::ScenarioDocument                     &document,
        const QString                                 &fromTerminalId,
        const QString                                 &toTerminalId,
        TransportationTypes::TransportationMode        mode,
        const QVariantMap                             &canonicalProperties,
        Scenario::LinkageSource                        source) const;

    RouteAuthoringServiceResult createGlobalLink(
        Scenario::ScenarioDocument                     &document,
        const QString                                 &fromTerminalId,
        const QString                                 &toTerminalId,
        TransportationTypes::TransportationMode        mode,
        const QVariantMap                             &canonicalProperties,
        Scenario::LinkageSource                        source) const;

    RouteAuthoringServiceResult removeRoute(
        Scenario::ScenarioDocument                     &document,
        const QString                                 &fromTerminalId,
        const QString                                 &toTerminalId,
        TransportationTypes::TransportationMode        mode) const;

    RouteAuthoringServiceResult setConnectionProperty(
        Scenario::ScenarioDocument                     &document,
        const QString                                 &fromTerminalId,
        const QString                                 &toTerminalId,
        TransportationTypes::TransportationMode        mode,
        const QString                                 &key,
        const QVariant                                &value) const;

    RouteAuthoringServiceResult setCanonicalRouteProperties(
        Scenario::ScenarioDocument                     &document,
        const QString                                 &fromTerminalId,
        const QString                                 &toTerminalId,
        TransportationTypes::TransportationMode        mode,
        const QVariantMap                             &canonicalProperties) const;

    RouteAuthoringServiceResult restoreConnection(
        Scenario::ScenarioDocument                     &document,
        const Scenario::Connection                     &snapshot) const;

    RouteAuthoringServiceResult restoreGlobalLink(
        Scenario::ScenarioDocument                     &document,
        const Scenario::GlobalLink                     &snapshot) const;

    RouteAuthoringServiceResult upsertNetworkBackedConnection(
        Scenario::ScenarioDocument                     &document,
        const QString                                 &fromTerminalId,
        const QString                                 &toTerminalId,
        TransportationTypes::TransportationMode        mode,
        Scenario::LinkageSource                        source) const;

    RouteAuthoringServiceResult computeCanonicalRouteProperties(
        const ShortestPathResult                      &pathResult,
        TransportationTypes::TransportationMode        mode,
        std::optional<bool>                           overrideUseNetworkValue =
            std::nullopt) const;

    RouteAuthoringServiceResult computeCanonicalRouteProperties(
        const Scenario::ScenarioDocument              &document,
        const ShortestPathResult                      &pathResult,
        TransportationTypes::TransportationMode        mode,
        std::optional<bool>                           overrideUseNetworkValue =
            std::nullopt) const;

    RouteAuthoringServiceResult computeEndpointCanonicalRouteProperties(
        const Scenario::ScenarioDocument              &document,
        const QString                                 &fromTerminalId,
        const QString                                 &toTerminalId,
        TransportationTypes::TransportationMode        mode,
        std::optional<bool>                           overrideUseNetworkValue =
            std::nullopt) const;

private:
    ::CargoNetSim::CargoNetSimController *m_controller = nullptr;
    ConfigController *m_config = nullptr;
};

} // namespace Application
} // namespace Backend
} // namespace CargoNetSim
