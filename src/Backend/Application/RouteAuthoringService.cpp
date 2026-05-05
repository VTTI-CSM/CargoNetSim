#include "RouteAuthoringService.h"

#include "Backend/Commons/LogCategories.h"
#include "Backend/Commons/GeoDistance.h"
#include "Backend/Application/NetworkManagementService.h"
#include "Backend/Controllers/ConfigController.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Scenario/Connection.h"
#include "Backend/Scenario/GlobalLink.h"
#include "Backend/Scenario/NetworkSpec.h"
#include "Backend/Scenario/NodeLinkage.h"
#include "Backend/Scenario/PathMetricsCalculator.h"
#include "Backend/Scenario/PropertyKeys.h"
#include "Backend/Scenario/RouteMetricUnits.h"
#include "Backend/Scenario/ScenarioDocument.h"

#include <cmath>
#include <optional>

namespace PK = CargoNetSim::Backend::Scenario::PropertyKeys;

namespace
{
using Mode = CargoNetSim::Backend::TransportationTypes::TransportationMode;
using CargoNetSim::Backend::NetworkKind;
using CargoNetSim::Backend::ShortestPathResult;
using CargoNetSim::Backend::Scenario::NetworkSpec;
using CargoNetSim::Backend::Scenario::NodeLinkage;

std::optional<NetworkSpec::Type> networkTypeForMode(Mode mode)
{
    if (mode == Mode::Train)
        return NetworkKind::Rail;
    if (mode == Mode::Truck)
        return NetworkKind::Truck;
    return std::nullopt;
}

bool isUsableNetworkPath(const ShortestPathResult &path)
{
    return path.pathNodes.size() > 1
        && std::isfinite(path.totalLength)
        && path.totalLength > 0.0
        && std::isfinite(path.minTravelTime)
        && path.minTravelTime >= 0.0;
}

struct NetworkRouteCandidate
{
    QString            networkName;
    int                startNodeId = -1;
    int                endNodeId = -1;
    ShortestPathResult path;
    QVariantMap        canonicalProperties;
};

bool isBetterCandidate(const NetworkRouteCandidate &candidate,
                       const NetworkRouteCandidate &current)
{
    if (candidate.path.totalLength != current.path.totalLength)
        return candidate.path.totalLength < current.path.totalLength;
    if (candidate.path.minTravelTime != current.path.minTravelTime)
        return candidate.path.minTravelTime < current.path.minTravelTime;
    const int networkOrder =
        QString::compare(candidate.networkName,
                         current.networkName,
                         Qt::CaseSensitive);
    if (networkOrder != 0)
        return networkOrder < 0;
    if (candidate.startNodeId != current.startNodeId)
        return candidate.startNodeId < current.startNodeId;
    return candidate.endNodeId < current.endNodeId;
}
} // namespace

namespace CargoNetSim
{
namespace Backend
{
namespace Application
{

RouteAuthoringService::RouteAuthoringService(
    ::CargoNetSim::CargoNetSimController *controller)
    : m_controller(controller)
    , m_config(controller ? controller->getConfigController() : nullptr)
{
}

RouteAuthoringService::RouteAuthoringService(
    ConfigController *config)
    : m_config(config)
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

    const QVariantMap completedProperties =
        Scenario::RouteMetricUnits::completeCanonicalProperties(
            canonicalProperties);

    Scenario::Connection connection;
    connection.fromTerminalId = fromTerminalId;
    connection.toTerminalId = toTerminalId;
    connection.mode = mode;
    connection.region = fromRegion;
    connection.properties = completedProperties;
    connection.source = source;

    if (!document.addConnection(connection))
    {
        result.status = RouteAuthoringServiceStatus::MutationFailed;
        result.message =
            QStringLiteral("Failed to create connection");
        return result;
    }

    result.status = RouteAuthoringServiceStatus::Success;
    result.canonicalProperties = completedProperties;
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

    const QVariantMap completedProperties =
        Scenario::RouteMetricUnits::completeCanonicalProperties(
            canonicalProperties);

    Scenario::GlobalLink globalLink;
    globalLink.fromTerminalId = fromTerminalId;
    globalLink.toTerminalId = toTerminalId;
    globalLink.mode = mode;
    globalLink.properties = completedProperties;
    globalLink.source = source;

    if (!document.addGlobalLink(globalLink))
    {
        result.status = RouteAuthoringServiceStatus::MutationFailed;
        result.message =
            QStringLiteral("Failed to create global link");
        return result;
    }

    result.status = RouteAuthoringServiceStatus::Success;
    result.canonicalProperties = completedProperties;
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
        updated.properties =
            Scenario::RouteMetricUnits::completeCanonicalProperties(
                updated.properties);
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
        updated.properties =
            Scenario::RouteMetricUnits::completeCanonicalProperties(
                updated.properties);
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
        QVariantMap mergedProperties = updated.properties;
        for (auto it = canonicalProperties.constBegin();
             it != canonicalProperties.constEnd(); ++it)
        {
            mergedProperties[it.key()] = it.value();
        }
        updated.properties =
            Scenario::RouteMetricUnits::completeCanonicalProperties(
                mergedProperties);
        if (!document.updateConnection(fromTerminalId, toTerminalId, mode,
                                       updated))
        {
            result.status = RouteAuthoringServiceStatus::MutationFailed;
            result.message =
                QStringLiteral("Failed to update connection properties");
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
        QVariantMap mergedProperties = updated.properties;
        for (auto it = canonicalProperties.constBegin();
             it != canonicalProperties.constEnd(); ++it)
        {
            mergedProperties[it.key()] = it.value();
        }
        updated.properties =
            Scenario::RouteMetricUnits::completeCanonicalProperties(
                mergedProperties);
        if (!document.updateGlobalLink(fromTerminalId, toTerminalId, mode,
                                       updated))
        {
            result.status = RouteAuthoringServiceStatus::MutationFailed;
            result.message =
                QStringLiteral("Failed to update global-link properties");
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

    Scenario::Connection restored = snapshot;
    restored.properties =
        Scenario::RouteMetricUnits::completeCanonicalProperties(
            restored.properties);

    if (!document.addConnection(restored))
    {
        result.status = RouteAuthoringServiceStatus::MutationFailed;
        result.message =
            QStringLiteral("Failed to restore connection");
        return result;
    }

    result.status = RouteAuthoringServiceStatus::Success;
    result.canonicalProperties = restored.properties;
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

    Scenario::GlobalLink restored = snapshot;
    restored.properties =
        Scenario::RouteMetricUnits::completeCanonicalProperties(
            restored.properties);

    if (!document.addGlobalLink(restored))
    {
        result.status = RouteAuthoringServiceStatus::MutationFailed;
        result.message =
            QStringLiteral("Failed to restore global link");
        return result;
    }

    result.status = RouteAuthoringServiceStatus::Success;
    result.canonicalProperties = restored.properties;
    return result;
}

RouteAuthoringServiceResult
RouteAuthoringService::upsertNetworkBackedConnection(
    Scenario::ScenarioDocument              &document,
    const QString                          &fromTerminalId,
    const QString                          &toTerminalId,
    TransportationTypes::TransportationMode mode,
    Scenario::LinkageSource                 source) const
{
    RouteAuthoringServiceResult result;

    const auto networkType = networkTypeForMode(mode);
    if (!networkType.has_value())
    {
        result.status = RouteAuthoringServiceStatus::InvalidArguments;
        result.message = QStringLiteral(
            "Network-backed regional route authoring supports Train and Truck only");
        return result;
    }

    if (fromTerminalId.isEmpty() || toTerminalId.isEmpty()
        || fromTerminalId == toTerminalId
        || !document.terminals.contains(fromTerminalId)
        || !document.terminals.contains(toTerminalId))
    {
        result.status = RouteAuthoringServiceStatus::InvalidArguments;
        result.message = QStringLiteral(
            "Network-backed connection endpoints must be distinct existing terminals");
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
            "Network-backed regional routes require both terminals to be in the same region");
        return result;
    }

    if (!m_config)
    {
        result.status = RouteAuthoringServiceStatus::MetricComputationFailed;
        result.message =
            QStringLiteral("ConfigController unavailable for route metrics");
        return result;
    }

    const QList<NodeLinkage> fromLinkages =
        document.linkagesFor(fromTerminalId, networkType.value());
    const QList<NodeLinkage> toLinkages =
        document.linkagesFor(toTerminalId, networkType.value());
    QList<NodeLinkage> activeFromLinkages;
    for (const NodeLinkage &linkage : fromLinkages)
    {
        if (!linkage.excluded)
            activeFromLinkages.append(linkage);
    }
    QList<NodeLinkage> activeToLinkages;
    for (const NodeLinkage &linkage : toLinkages)
    {
        if (!linkage.excluded)
            activeToLinkages.append(linkage);
    }
    if (activeFromLinkages.isEmpty() || activeToLinkages.isEmpty())
    {
        result.status = RouteAuthoringServiceStatus::NotFound;
        result.message = QStringLiteral(
            "No persisted terminal-network linkages found for the requested mode");
        return result;
    }

    NetworkManagementService networkService(m_controller);
    std::optional<NetworkRouteCandidate> bestCandidate;
    bool foundSharedNetwork = false;
    RouteAuthoringServiceResult lastMetricFailure;

    for (const NodeLinkage &fromLink : activeFromLinkages)
    {
        for (const NodeLinkage &toLink : activeToLinkages)
        {
            if (fromLink.networkName != toLink.networkName)
                continue;

            foundSharedNetwork = true;
            if (fromLink.nodeId == toLink.nodeId)
                continue;

            const ShortestPathResult path =
                networkService.findShortestPath(
                    fromRegion, fromLink.networkName,
                    networkType.value(), fromLink.nodeId,
                    toLink.nodeId);
            if (!isUsableNetworkPath(path))
                continue;

            const auto propertyResult =
                computeCanonicalRouteProperties(
                    document, path, mode, true);
            if (!propertyResult.succeeded())
            {
                lastMetricFailure = propertyResult;
                continue;
            }

            NetworkRouteCandidate candidate;
            candidate.networkName = fromLink.networkName;
            candidate.startNodeId = fromLink.nodeId;
            candidate.endNodeId = toLink.nodeId;
            candidate.path = path;
            candidate.canonicalProperties =
                propertyResult.canonicalProperties;

            if (!bestCandidate.has_value()
                || isBetterCandidate(candidate,
                                     bestCandidate.value()))
            {
                bestCandidate = candidate;
            }
        }
    }

    if (!bestCandidate.has_value())
    {
        if (lastMetricFailure.status
            == RouteAuthoringServiceStatus::MetricComputationFailed)
            return lastMetricFailure;

        result.status = RouteAuthoringServiceStatus::NotFound;
        result.message = foundSharedNetwork
            ? QStringLiteral(
                  "No usable shortest path found between linked terminal nodes")
            : QStringLiteral(
                  "No common network links both terminals for the requested mode");
        return result;
    }

    const bool existingRoute =
        document.findConnection(fromTerminalId, toTerminalId, mode) != nullptr;
    RouteAuthoringServiceResult mutationResult =
        existingRoute
            ? setCanonicalRouteProperties(
                  document, fromTerminalId, toTerminalId, mode,
                  bestCandidate->canonicalProperties)
            : createConnection(
                  document, fromTerminalId, toTerminalId, mode,
                  bestCandidate->canonicalProperties, source);
    if (!mutationResult.succeeded())
        return mutationResult;

    mutationResult.selectedNetworkName = bestCandidate->networkName;
    mutationResult.selectedStartNodeId = bestCandidate->startNodeId;
    mutationResult.selectedEndNodeId = bestCandidate->endNodeId;
    mutationResult.selectedPath = bestCandidate->path;
    mutationResult.routeCreated = !existingRoute;
    mutationResult.routeUpdated = existingRoute;
    return mutationResult;
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
        mode, *m_config);
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

    result.status = RouteAuthoringServiceStatus::Success;
    result.canonicalProperties =
        Scenario::RouteMetricUnits::canonicalPropertiesFromMetrics(metrics);
    return result;
}

RouteAuthoringServiceResult
RouteAuthoringService::computeCanonicalRouteProperties(
    const Scenario::ScenarioDocument       &document,
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
        mode, *m_config, document.simulation);
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

    result.status = RouteAuthoringServiceStatus::Success;
    result.canonicalProperties =
        Scenario::RouteMetricUnits::canonicalPropertiesFromMetrics(metrics);
    return result;
}

RouteAuthoringServiceResult
RouteAuthoringService::computeEndpointCanonicalRouteProperties(
    const Scenario::ScenarioDocument       &document,
    const QString                          &fromTerminalId,
    const QString                          &toTerminalId,
    TransportationTypes::TransportationMode mode,
    std::optional<bool>                    overrideUseNetworkValue) const
{
    RouteAuthoringServiceResult result;
    if (!document.terminals.contains(fromTerminalId)
        || !document.terminals.contains(toTerminalId))
    {
        result.status = RouteAuthoringServiceStatus::InvalidArguments;
        result.message =
            QStringLiteral("Route metric endpoints must exist in the document");
        return result;
    }

    const QPointF from = document.globalPositionOf(fromTerminalId);
    const QPointF to = document.globalPositionOf(toTerminalId);
    if (!std::isfinite(from.x()) || !std::isfinite(from.y())
        || !std::isfinite(to.x()) || !std::isfinite(to.y()))
    {
        result.status = RouteAuthoringServiceStatus::MetricComputationFailed;
        result.message = QStringLiteral(
            "Route metric endpoints require valid global latitude/longitude");
        return result;
    }

    ShortestPathResult pathResult;
    pathResult.totalLength = Commons::GeoDistance::haversineMeters(
        from.y(), from.x(), to.y(), to.x());
    pathResult.minTravelTime = 0.0;
    pathResult.optimizationCriterion = QStringLiteral("endpoint_distance");

    const std::optional<bool> useNetworkOverride =
        overrideUseNetworkValue.has_value()
            ? overrideUseNetworkValue
            : std::optional<bool>(false);
    return computeCanonicalRouteProperties(document, pathResult, mode,
                                           useNetworkOverride);
}

} // namespace Application
} // namespace Backend
} // namespace CargoNetSim
