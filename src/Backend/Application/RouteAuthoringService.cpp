#include "RouteAuthoringService.h"

#include "Backend/Commons/LogCategories.h"
#include "Backend/Controllers/ConfigController.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Controllers/VehicleController.h"
#include "Backend/Scenario/Connection.h"
#include "Backend/Scenario/GlobalLink.h"
#include "Backend/Scenario/PathMetricsCalculator.h"
#include "Backend/Scenario/PropertyKeys.h"
#include "Backend/Scenario/RouteMetricUnits.h"
#include "Backend/Scenario/ScenarioDocument.h"

namespace PK = CargoNetSim::Backend::Scenario::PropertyKeys;

namespace CargoNetSim
{
namespace Backend
{
namespace Application
{

RouteAuthoringService::RouteAuthoringService(
    ::CargoNetSim::CargoNetSimController *controller)
    : RouteAuthoringService(
          controller ? controller->getConfigController() : nullptr,
          controller ? controller->getVehicleController() : nullptr)
{
}

RouteAuthoringService::RouteAuthoringService(
    ConfigController  *config,
    VehicleController *vehicles)
    : m_config(config)
    , m_vehicles(vehicles)
{
}

RouteAuthoringServiceResult RouteAuthoringService::createConnection(
    Scenario::ScenarioDocument              &document,
    const QString                          &fromTerminalId,
    const QString                          &toTerminalId,
    TransportationTypes::TransportationMode mode,
    const QVariantMap                      &canonicalProperties,
    Scenario::LinkageSource                 source) const
{
    RouteAuthoringServiceResult result;

    if (!document.terminals.contains(fromTerminalId)
        || !document.terminals.contains(toTerminalId))
    {
        result.status = RouteAuthoringServiceStatus::InvalidArguments;
        result.message = QStringLiteral(
            "Connection endpoints must exist in the document");
        return result;
    }

    const QString fromRegion =
        document.terminals.value(fromTerminalId).region;
    const QString toRegion =
        document.terminals.value(toTerminalId).region;
    if (fromRegion != toRegion)
    {
        result.status = RouteAuthoringServiceStatus::InvalidArguments;
        result.message = QStringLiteral(
            "Regional connections require both terminals to be in the same region");
        return result;
    }

    Scenario::Connection connection;
    connection.fromTerminalId = fromTerminalId;
    connection.toTerminalId = toTerminalId;
    connection.mode = mode;
    connection.region = fromRegion;
    connection.properties = canonicalProperties;
    connection.source = source;

    if (!document.addConnection(connection))
    {
        result.status = RouteAuthoringServiceStatus::MutationFailed;
        result.message =
            QStringLiteral("Failed to create connection");
        return result;
    }

    result.status = RouteAuthoringServiceStatus::Success;
    result.canonicalProperties = canonicalProperties;
    return result;
}

RouteAuthoringServiceResult RouteAuthoringService::createGlobalLink(
    Scenario::ScenarioDocument              &document,
    const QString                          &fromTerminalId,
    const QString                          &toTerminalId,
    TransportationTypes::TransportationMode mode,
    const QVariantMap                      &canonicalProperties,
    Scenario::LinkageSource                 source) const
{
    RouteAuthoringServiceResult result;

    if (!document.terminals.contains(fromTerminalId)
        || !document.terminals.contains(toTerminalId))
    {
        result.status = RouteAuthoringServiceStatus::InvalidArguments;
        result.message = QStringLiteral(
            "Global-link endpoints must exist in the document");
        return result;
    }

    const QString fromRegion =
        document.terminals.value(fromTerminalId).region;
    const QString toRegion =
        document.terminals.value(toTerminalId).region;
    if (fromRegion == toRegion)
    {
        result.status = RouteAuthoringServiceStatus::InvalidArguments;
        result.message = QStringLiteral(
            "Global links require terminals from different regions");
        return result;
    }

    Scenario::GlobalLink globalLink;
    globalLink.fromTerminalId = fromTerminalId;
    globalLink.toTerminalId = toTerminalId;
    globalLink.mode = mode;
    globalLink.properties = canonicalProperties;
    globalLink.source = source;

    if (!document.addGlobalLink(globalLink))
    {
        result.status = RouteAuthoringServiceStatus::MutationFailed;
        result.message =
            QStringLiteral("Failed to create global link");
        return result;
    }

    result.status = RouteAuthoringServiceStatus::Success;
    result.canonicalProperties = canonicalProperties;
    return result;
}

RouteAuthoringServiceResult RouteAuthoringService::removeRoute(
    Scenario::ScenarioDocument              &document,
    const QString                          &fromTerminalId,
    const QString                          &toTerminalId,
    TransportationTypes::TransportationMode mode) const
{
    RouteAuthoringServiceResult result;

    if (document.removeConnection(fromTerminalId, toTerminalId, mode)
        || document.removeGlobalLink(fromTerminalId, toTerminalId, mode))
    {
        result.status = RouteAuthoringServiceStatus::Success;
        return result;
    }

    result.status = RouteAuthoringServiceStatus::NotFound;
    result.message = QStringLiteral("Route not found");
    return result;
}

RouteAuthoringServiceResult RouteAuthoringService::setConnectionProperty(
    Scenario::ScenarioDocument              &document,
    const QString                          &fromTerminalId,
    const QString                          &toTerminalId,
    TransportationTypes::TransportationMode mode,
    const QString                          &key,
    const QVariant                         &value) const
{
    RouteAuthoringServiceResult result;

    if (auto *connection =
            document.findConnection(fromTerminalId, toTerminalId, mode))
    {
        Scenario::Connection updated = *connection;
        updated.properties[key] = value;
        if (!document.updateConnection(fromTerminalId, toTerminalId, mode,
                                       updated))
        {
            result.status = RouteAuthoringServiceStatus::MutationFailed;
            result.message =
                QStringLiteral("Failed to update connection property");
            return result;
        }
        result.status = RouteAuthoringServiceStatus::Success;
        result.canonicalProperties = updated.properties;
        return result;
    }

    if (auto *globalLink =
            document.findGlobalLink(fromTerminalId, toTerminalId, mode))
    {
        Scenario::GlobalLink updated = *globalLink;
        updated.properties[key] = value;
        if (!document.updateGlobalLink(fromTerminalId, toTerminalId, mode,
                                       updated))
        {
            result.status = RouteAuthoringServiceStatus::MutationFailed;
            result.message =
                QStringLiteral("Failed to update global-link property");
            return result;
        }
        result.status = RouteAuthoringServiceStatus::Success;
        result.canonicalProperties = updated.properties;
        return result;
    }

    result.status = RouteAuthoringServiceStatus::NotFound;
    result.message = QStringLiteral("Route not found");
    return result;
}

RouteAuthoringServiceResult
RouteAuthoringService::setCanonicalRouteProperties(
    Scenario::ScenarioDocument              &document,
    const QString                          &fromTerminalId,
    const QString                          &toTerminalId,
    TransportationTypes::TransportationMode mode,
    const QVariantMap                      &canonicalProperties) const
{
    RouteAuthoringServiceResult result;

    if (auto *connection =
            document.findConnection(fromTerminalId, toTerminalId, mode))
    {
        Scenario::Connection updated = *connection;
        for (auto it = canonicalProperties.constBegin();
             it != canonicalProperties.constEnd(); ++it)
        {
            updated.properties[it.key()] = it.value();
        }
        if (!document.updateConnection(fromTerminalId, toTerminalId, mode,
                                       updated))
        {
            result.status = RouteAuthoringServiceStatus::MutationFailed;
            result.message =
                QStringLiteral("Failed to update connection properties");
            return result;
        }
        result.status = RouteAuthoringServiceStatus::Success;
        result.canonicalProperties = canonicalProperties;
        return result;
    }

    if (auto *globalLink =
            document.findGlobalLink(fromTerminalId, toTerminalId, mode))
    {
        Scenario::GlobalLink updated = *globalLink;
        for (auto it = canonicalProperties.constBegin();
             it != canonicalProperties.constEnd(); ++it)
        {
            updated.properties[it.key()] = it.value();
        }
        if (!document.updateGlobalLink(fromTerminalId, toTerminalId, mode,
                                       updated))
        {
            result.status = RouteAuthoringServiceStatus::MutationFailed;
            result.message =
                QStringLiteral("Failed to update global-link properties");
            return result;
        }
        result.status = RouteAuthoringServiceStatus::Success;
        result.canonicalProperties = canonicalProperties;
        return result;
    }

    result.status = RouteAuthoringServiceStatus::NotFound;
    result.message = QStringLiteral("Route not found");
    return result;
}

RouteAuthoringServiceResult RouteAuthoringService::restoreConnection(
    Scenario::ScenarioDocument &document,
    const Scenario::Connection &snapshot) const
{
    RouteAuthoringServiceResult result;

    if (document.findConnection(snapshot.fromTerminalId,
                                snapshot.toTerminalId,
                                snapshot.mode))
    {
        result.status = RouteAuthoringServiceStatus::MutationFailed;
        result.message =
            QStringLiteral("Connection already exists");
        return result;
    }

    if (!document.addConnection(snapshot))
    {
        result.status = RouteAuthoringServiceStatus::MutationFailed;
        result.message =
            QStringLiteral("Failed to restore connection");
        return result;
    }

    result.status = RouteAuthoringServiceStatus::Success;
    result.canonicalProperties = snapshot.properties;
    return result;
}

RouteAuthoringServiceResult RouteAuthoringService::restoreGlobalLink(
    Scenario::ScenarioDocument &document,
    const Scenario::GlobalLink &snapshot) const
{
    RouteAuthoringServiceResult result;

    if (document.findGlobalLink(snapshot.fromTerminalId,
                                snapshot.toTerminalId,
                                snapshot.mode))
    {
        result.status = RouteAuthoringServiceStatus::MutationFailed;
        result.message =
            QStringLiteral("Global link already exists");
        return result;
    }

    if (!document.addGlobalLink(snapshot))
    {
        result.status = RouteAuthoringServiceStatus::MutationFailed;
        result.message =
            QStringLiteral("Failed to restore global link");
        return result;
    }

    result.status = RouteAuthoringServiceStatus::Success;
    result.canonicalProperties = snapshot.properties;
    return result;
}

RouteAuthoringServiceResult
RouteAuthoringService::computeCanonicalRouteProperties(
    const ShortestPathResult               &pathResult,
    TransportationTypes::TransportationMode mode,
    std::optional<bool>                    overrideUseNetworkValue) const
{
    RouteAuthoringServiceResult result;

    if (!m_config)
    {
        result.status = RouteAuthoringServiceStatus::MetricComputationFailed;
        result.message =
            QStringLiteral("ConfigController unavailable for route metrics");
        return result;
    }

    auto inputs = Scenario::PathMetricsCalculator::gatherInputs(
        mode, *m_config, m_vehicles);
    if (overrideUseNetworkValue.has_value())
    {
        inputs.modeProperties[PK::Mode::UseNetwork] =
            overrideUseNetworkValue.value();
    }

    const auto metrics = Scenario::PathMetricsCalculator::compute(
        pathResult.totalLength, pathResult.minTravelTime, mode, inputs);
    if (!metrics.valid)
    {
        result.status = RouteAuthoringServiceStatus::MetricComputationFailed;
        result.message =
            QStringLiteral("Failed to compute route metrics");
        return result;
    }

    QVariantMap displayRouteProperties;
    displayRouteProperties.insert(QStringLiteral("distance"),
                                  metrics.distanceKm);
    displayRouteProperties.insert(QStringLiteral("travelTime"),
                                  metrics.travelTimeHours);
    displayRouteProperties.insert(QStringLiteral("risk"),
                                  metrics.riskPerVehicle);
    displayRouteProperties.insert(QStringLiteral("energyConsumption"),
                                  metrics.energyPerVehicle);
    displayRouteProperties.insert(QStringLiteral("carbonEmissions"),
                                  metrics.carbonPerVehicle);

    result.status = RouteAuthoringServiceStatus::Success;
    result.canonicalProperties =
        Scenario::RouteMetricUnits::canonicalPropertiesFromDisplay(
            displayRouteProperties);
    return result;
}

} // namespace Application
} // namespace Backend
} // namespace CargoNetSim
