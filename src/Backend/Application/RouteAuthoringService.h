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
class VehicleController;

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

    bool succeeded() const
    {
        return status == RouteAuthoringServiceStatus::Success;
    }
};

class RouteAuthoringService
{
public:
    explicit RouteAuthoringService(::CargoNetSim::CargoNetSimController *controller);
    RouteAuthoringService(ConfigController  *config,
                          VehicleController *vehicles);

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

    RouteAuthoringServiceResult computeCanonicalRouteProperties(
        const ShortestPathResult                      &pathResult,
        TransportationTypes::TransportationMode        mode,
        std::optional<bool>                           overrideUseNetworkValue =
            std::nullopt) const;

private:
    ConfigController  *m_config = nullptr;
    VehicleController *m_vehicles = nullptr;
};

} // namespace Application
} // namespace Backend
} // namespace CargoNetSim
