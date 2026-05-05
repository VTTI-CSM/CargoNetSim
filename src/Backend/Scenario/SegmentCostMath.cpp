#include "SegmentCostMath.h"

#include <algorithm>

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

#include "Backend/Clients/ShipClient/ShipSimulationClient.h"
#include "Backend/Clients/ShipClient/ShipState.h"
#include "Backend/Clients/TrainClient/TrainSimulationClient.h"
#include "Backend/Clients/TrainClient/TrainState.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Commons/TransportationMode.h"
#include "Backend/Commons/Units.h"
#include "Backend/Models/Path.h"
#include "Backend/Models/PathSegment.h"
#include "PropertyKeys.h"
#include "RuntimeArtifactIdentity.h"

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{
namespace
{

using Mode = TransportationTypes::TransportationMode;
namespace PK = PropertyKeys;

// Mode-agnostic aggregated vehicle metrics in canonical units. Populated by
// per-mode state loops, then consumed by computeVehicleSegmentData.
struct VehicleSegmentMetrics
{
    double travelTime        = 0.0; // seconds
    double distance          = 0.0; // metres
    double carbonEmissions   = 0.0; // tonnes CO2
    double energyConsumption = 0.0; // kWh
    double risk              = 0.0; // fraction
    int    vehicleCount      = 0;
};

struct ComputedSegmentData
{
    PathSegment::SegmentMetricSnapshot actualMetrics;
    PathSegment::SegmentCostSnapshot   actualCosts;
};

// Post-aggregation work shared by ship and train segment cost — compute
// container-to-capacity ratio, scale carbon/energy by ratio, and build the
// typed per-segment execution data. Returns an unavailable snapshot for
// empty populations.
ComputedSegmentData computeVehicleSegmentData(
    VehicleSegmentMetrics              m,
    const QVariantMap                 &modeWeights,
    int                                containerCount,
    int                                vehicleCapacity)
{
    ComputedSegmentData out;
    if (m.vehicleCount == 0 || containerCount <= 0)
        return out;

    const int effectiveVehicleCapacity =
        std::max(vehicleCapacity, 1);
    if (vehicleCapacity <= 0)
    {
        qCWarning(lcScenario)
            << "SegmentCostMath::computeVehicleSegmentData:"
            << "invalid vehicle capacity" << vehicleCapacity
            << "using" << effectiveVehicleCapacity;
    }

    // Calculate containers per vehicle
    double containersPerVehicle =
        (double)containerCount / m.vehicleCount;

    // Calculate container-to-capacity ratio (how full is
    // each vehicle)
    double containerToCapacityRatio =
        containersPerVehicle / effectiveVehicleCapacity;

    // Adjust metrics by ratio
    m.carbonEmissions *= containerToCapacityRatio;
    m.energyConsumption *= containerToCapacityRatio;

    out.actualMetrics.available = true;
    out.actualMetrics.travelTime = m.travelTime;
    out.actualMetrics.distance = m.distance;
    out.actualMetrics.carbonEmissions = m.carbonEmissions;
    out.actualMetrics.energyConsumption = m.energyConsumption;
    out.actualMetrics.risk = m.risk;

    out.actualCosts.available = true;
    out.actualCosts.travelTime =
        m.travelTime * modeWeights[PK::Segment::TravelTime].toDouble();
    out.actualCosts.distance =
        m.distance * modeWeights[PK::Segment::Distance].toDouble();
    out.actualCosts.carbonEmissions =
        m.carbonEmissions
        * modeWeights[PK::Segment::CarbonEmissions].toDouble();
    out.actualCosts.energyConsumption =
        m.energyConsumption
        * modeWeights[PK::Segment::EnergyConsumption].toDouble();
    out.actualCosts.risk =
        m.risk * modeWeights[PK::Segment::Risk].toDouble();
    out.actualCosts.directCost = 0.0;
    return out;
}

double totalCost(const ComputedSegmentData &data)
{
    return data.actualCosts.total();
}

ComputedSegmentData computeShipSegmentData(
    CargoNetSim::Backend::ShipClient::ShipSimulationClient *shipClient,
    CargoNetSim::Backend::Path                             *path,
    CargoNetSim::Backend::PathSegment                      *segment,
    int                                                     segmentCounter,
    const QVariantMap                                      &modeWeights,
    const QVariantMap                                      &transportModes,
    int                                                     containerCount)
{
    if (!shipClient || !path || !segment)
        return {};

    VehicleSegmentMetrics metrics;
    const QVariantMap shipData =
        transportModes.value(transportationModeToString(Mode::Ship)).toMap();
    const double perShipRisk =
        shipData.value(PK::Mode::RiskFactor, 0.025).toDouble();

    auto shipStates = shipClient->getAllShipsStates();
    for (auto networkName : shipStates.keys())
    {
        for (auto shipState : shipStates[networkName])
        {
            if (!shipState)
                continue;

            RuntimeArtifactIdentity artifact;
            if (RuntimeArtifacts::decode(
                    shipState->getShipId(), artifact)
                && artifact.artifactType
                       == QStringLiteral("ship")
                && artifact.pathKey == path->canonicalPathKey()
                && artifact.segmentIndex == segmentCounter)
            {
                metrics.vehicleCount++;
                metrics.travelTime +=
                    shipState->tripTimeUnits().value();
                metrics.distance +=
                    shipState->travelledDistanceUnits().value();
                metrics.carbonEmissions +=
                    Units::toMetricTons(
                        shipState->carbonEmissionsUnits())
                        .value();
                metrics.energyConsumption +=
                    shipState->energyConsumptionUnits().value();
                metrics.risk += perShipRisk;
            }
        }
    }

    const int shipCapacity =
        shipData.value(PK::Mode::AverageContainerNumber, 10000)
            .toInt();

    return computeVehicleSegmentData(metrics, modeWeights,
                                     containerCount, shipCapacity);
}

ComputedSegmentData computeTrainSegmentData(
    CargoNetSim::Backend::TrainClient::TrainSimulationClient *trainClient,
    CargoNetSim::Backend::Path                               *path,
    CargoNetSim::Backend::PathSegment                        *segment,
    int                                                       segmentCounter,
    const QVariantMap                                        &modeWeights,
    const QVariantMap                                        &transportModes,
    int                                                       containerCount)
{
    if (!trainClient || !path || !segment)
        return {};

    VehicleSegmentMetrics metrics;
    const QVariantMap trainData =
        transportModes.value(transportationModeToString(Mode::Train)).toMap();
    const double perTrainRisk =
        trainData.value(PK::Mode::RiskFactor, 0.006).toDouble();

    auto trainStates = trainClient->getAllTrainsStates();
    for (auto networkName : trainStates.keys())
    {
        for (auto trainState : trainStates[networkName])
        {
            if (!trainState)
                continue;

            RuntimeArtifactIdentity artifact;
            if (RuntimeArtifacts::decode(
                    trainState->getTrainUserId(), artifact)
                && artifact.artifactType
                       == QStringLiteral("train")
                && artifact.pathKey == path->canonicalPathKey()
                && artifact.segmentIndex == segmentCounter)
            {
                metrics.vehicleCount++;
                metrics.travelTime +=
                    trainState->tripTimeUnits().value();
                metrics.distance +=
                    trainState->travelledDistanceUnits().value();
                metrics.carbonEmissions +=
                    Units::toMetricTons(
                        trainState->totalCarbonDioxideEmittedUnits())
                        .value();
                metrics.energyConsumption +=
                    trainState->totalEnergyConsumedUnits().value();
                metrics.risk += perTrainRisk;
            }
        }
    }

    const int trainCapacity =
        trainData.value(PK::Mode::AverageContainerNumber, 400)
            .toInt();

    return computeVehicleSegmentData(metrics, modeWeights,
                                     containerCount, trainCapacity);
}

ComputedSegmentData computeTruckSegmentData(
    CargoNetSim::Backend::TruckClient::TruckSimulationManager *truckManager,
    CargoNetSim::Backend::Path                                *path,
    CargoNetSim::Backend::PathSegment                         *segment,
    int                                                        segmentCounter,
    const QVariantMap                                         &modeWeights,
    const QVariantMap                                         &transportModes,
    int                                                        containerCount)
{
    if (!segment)
        return {};

    Q_UNUSED(truckManager);
    Q_UNUSED(path);
    Q_UNUSED(segmentCounter);

    VehicleSegmentMetrics metrics;

    const QVariantMap truckData =
        transportModes.value(transportationModeToString(Mode::Truck)).toMap();
    const int truckCapacity =
        truckData.value(PK::Mode::AverageContainerNumber, 1).toInt();
    const int effectiveTruckCapacity = std::max(truckCapacity, 1);

    metrics.vehicleCount =
        (containerCount + effectiveTruckCapacity - 1)
        / effectiveTruckCapacity;
    metrics.risk =
        truckData.value(PK::Mode::RiskFactor, 0.012).toDouble();

    return computeVehicleSegmentData(metrics, modeWeights,
                                     containerCount,
                                     truckCapacity);
}

} // namespace

namespace SegmentCostMath
{

// Per-mode aggregation is mode-specific (ShipState / TrainState APIs differ);
// the post-aggregation math is shared and returns typed execution data.
// Null-client/path/segment guards tolerate uninitialized extractors.
double shipSegmentCost(
    CargoNetSim::Backend::ShipClient::ShipSimulationClient *shipClient,
    CargoNetSim::Backend::Path                             *path,
    CargoNetSim::Backend::PathSegment                      *segment,
    int                                                     segmentCounter,
    const QVariantMap                                      &modeWeights,
    const QVariantMap                                      &transportModes,
    int                                                     containerCount)
{
    qCDebug(lcScenario) << "SegmentCostMath::shipSegmentCost:"
                        << "segment" << (segment ? segment->getStart() : "?")
                        << "->" << (segment ? segment->getEnd() : "?")
                        << ", segIdx =" << segmentCounter;

    if (!shipClient || !path || !segment)
        return 0.0;
    const auto data = computeShipSegmentData(
        shipClient, path, segment, segmentCounter, modeWeights,
        transportModes, containerCount);
    const double cost = totalCost(data);

    qCDebug(lcScenario) << "SegmentCostMath::shipSegmentCost:"
                        << "computed cost =" << cost
                        << ", actual data available ="
                        << data.actualMetrics.available;

    return cost;
}

// Structurally identical to shipSegmentCost but targets TrainState's
// differently-named accessors and reads transportModes["rail"].
double trainSegmentCost(
    CargoNetSim::Backend::TrainClient::TrainSimulationClient *trainClient,
    CargoNetSim::Backend::Path                               *path,
    CargoNetSim::Backend::PathSegment                        *segment,
    int                                                       segmentCounter,
    const QVariantMap                                        &modeWeights,
    const QVariantMap                                        &transportModes,
    int                                                       containerCount)
{
    qCDebug(lcScenario) << "SegmentCostMath::trainSegmentCost:"
                        << "segment" << (segment ? segment->getStart() : "?")
                        << "->" << (segment ? segment->getEnd() : "?")
                        << ", segIdx =" << segmentCounter;

    if (!trainClient || !path || !segment)
        return 0.0;
    const auto data = computeTrainSegmentData(
        trainClient, path, segment, segmentCounter, modeWeights,
        transportModes, containerCount);
    const double cost = totalCost(data);

    qCDebug(lcScenario) << "SegmentCostMath::trainSegmentCost:"
                        << "computed cost =" << cost
                        << ", actual data available ="
                        << data.actualMetrics.available;

    return cost;
}

// Heuristic truck cost. It delegates to the same shared helper as ship/train
// using the canonical seconds/metres/tonnes contract. Truck count is estimated
// from ceiling-divide of containerCount by capacity; all metrics except risk
// remain zero until TruckSimulationManager exposes trip actuals and the
// aggregation loop
// below is uncommented.
double truckSegmentCost(
    CargoNetSim::Backend::TruckClient::TruckSimulationManager *truckManager,
    CargoNetSim::Backend::Path                                *path,
    CargoNetSim::Backend::PathSegment                         *segment,
    int                                                        segmentCounter,
    const QVariantMap                                         &modeWeights,
    const QVariantMap                                         &transportModes,
    int                                                        containerCount)
{
    qCDebug(lcScenario) << "SegmentCostMath::truckSegmentCost:"
                        << "segment" << (segment ? segment->getStart() : "?")
                        << "->" << (segment ? segment->getEnd() : "?")
                        << ", segIdx =" << segmentCounter;

    if (!segment)
        return 0.0;

    const auto data = computeTruckSegmentData(
        truckManager, path, segment, segmentCounter, modeWeights,
        transportModes, containerCount);
    const double cost = totalCost(data);

    qCDebug(lcScenario) << "SegmentCostMath::truckSegmentCost:"
                        << "computed cost =" << cost
                        << ", actual data available ="
                        << data.actualMetrics.available
                        << "(heuristic)";

    return cost;
}

// Loops segments, picks per-mode weights from costFunctionWeights (integer
// mode key, "default" fallback), and delegates to the per-mode cost function.
// Clients are forwarded through; null clients cause the per-mode function to
// early-return 0 (ship/train) or harmlessly ignore (truck heuristic).
double edgeCosts(
    CargoNetSim::Backend::ShipClient::ShipSimulationClient    *shipClient,
    CargoNetSim::Backend::TrainClient::TrainSimulationClient  *trainClient,
    CargoNetSim::Backend::TruckClient::TruckSimulationManager *truckManager,
    CargoNetSim::Backend::Path                                *path,
    const QList<CargoNetSim::Backend::PathSegment *>          &segments,
    const QVariantMap                                         &costFunctionWeights,
    const QVariantMap                                         &transportModes,
    int                                                        containerCount)
{
    using Mode = CargoNetSim::Backend::TransportationTypes::
        TransportationMode;

    qCDebug(lcScenario) << "SegmentCostMath::edgeCosts:"
                        << "segment count =" << segments.size()
                        << ", containerCount =" << containerCount;

    double totalEdgeCosts = 0.0;

    for (int segmentCounter = 0;
         segmentCounter < segments.size(); ++segmentCounter)
    {
        CargoNetSim::Backend::PathSegment *segment =
            segments[segmentCounter];
        if (!segment)
            continue;

        const Mode mode = segment->getMode();

        // Get the mode-specific weights (fallback to "default")
        QVariantMap modeWeights;
        const QString modeKey =
            QString::number(static_cast<int>(mode));
        if (costFunctionWeights.contains(modeKey))
        {
            modeWeights =
                costFunctionWeights[modeKey].toMap();
        }
        else
        {
            modeWeights =
                costFunctionWeights["default"].toMap();
        }

        double segmentCost = 0.0;
        if (mode == Mode::Ship)
        {
            segmentCost = shipSegmentCost(
                shipClient, path, segment, segmentCounter,
                modeWeights, transportModes, containerCount);
        }
        else if (mode == Mode::Train)
        {
            segmentCost = trainSegmentCost(
                trainClient, path, segment, segmentCounter,
                modeWeights, transportModes, containerCount);
        }
        else if (mode == Mode::Truck)
        {
            segmentCost = truckSegmentCost(
                truckManager, path, segment, segmentCounter,
                modeWeights, transportModes, containerCount);
        }

        totalEdgeCosts += segmentCost;
    }

    qCDebug(lcScenario) << "SegmentCostMath::edgeCosts:"
                        << "total edge cost =" << totalEdgeCosts;

    return totalEdgeCosts;
}

CargoNetSim::Backend::Scenario::PathExecutionResult
computePathExecutionResult(
    CargoNetSim::Backend::ShipClient::ShipSimulationClient    *shipClient,
    CargoNetSim::Backend::TrainClient::TrainSimulationClient  *trainClient,
    CargoNetSim::Backend::TruckClient::TruckSimulationManager *truckManager,
    CargoNetSim::Backend::Path                                *path,
    const QString                                             &executionPathKey,
    const QVariantMap                                         &costFunctionWeights,
    const QVariantMap                                         &transportModes,
    int                                                        containerCount,
    bool                                                       emitInfoLog)
{
    CargoNetSim::Backend::Scenario::PathExecutionResult result;
    if (!path)
        return result;

    qCDebug(lcScenario) << "SegmentCostMath::computePathExecutionResult:"
                        << "path ID =" << path->getPathId()
                        << ", containerCount =" << containerCount;

    const auto segments = path->getSegments();
    result.executionPathKey = executionPathKey.isEmpty()
        ? path->canonicalPathKey()
        : executionPathKey;
    result.pathId = path->getPathId();
    result.canonicalPathKey = path->canonicalPathKey();
    result.pathUid = path->getPathUid();
    result.originId = path->getOriginId().isEmpty()
        ? path->getStartTerminal()
        : path->getOriginId();
    result.destinationId = path->getDestinationId().isEmpty()
        ? path->getEndTerminal()
        : path->getDestinationId();
    result.rank = path->getRank();
    result.effectiveContainerCount = containerCount;

    using Mode =
        CargoNetSim::Backend::TransportationTypes::TransportationMode;

    for (int segmentCounter = 0;
         segmentCounter < segments.size(); ++segmentCounter)
    {
        auto *segment = segments[segmentCounter];
        if (!segment)
            continue;

        QVariantMap modeWeights;
        const QString modeKey =
            QString::number(
                static_cast<int>(segment->getMode()));
        if (costFunctionWeights.contains(modeKey))
            modeWeights = costFunctionWeights.value(modeKey).toMap();
        else
            modeWeights = costFunctionWeights.value(QStringLiteral("default")).toMap();

        ComputedSegmentData computed;
        if (segment->getMode() == Mode::Ship)
        {
            computed = computeShipSegmentData(
                shipClient, path, segment, segmentCounter,
                modeWeights, transportModes, containerCount);
        }
        else if (segment->getMode() == Mode::Train)
        {
            computed = computeTrainSegmentData(
                trainClient, path, segment, segmentCounter,
                modeWeights, transportModes, containerCount);
        }
        else if (segment->getMode() == Mode::Truck)
        {
            computed = computeTruckSegmentData(
                truckManager, path, segment, segmentCounter,
                modeWeights, transportModes, containerCount);
        }

        SegmentExecutionResult segmentResult;
        segmentResult.segmentIndex = segmentCounter;
        segmentResult.segmentId = segment->getPathSegmentId();
        segmentResult.startTerminalId = segment->getStart();
        segmentResult.endTerminalId = segment->getEnd();
        segmentResult.mode = segment->getMode();
        segmentResult.actualMetrics = computed.actualMetrics;
        segmentResult.actualCosts = computed.actualCosts;
        result.segmentResults.append(segmentResult);
        result.edgeCosts += totalCost(computed);
    }

    // Predicted terminal totals are returned by TerminalSim during path
    // discovery. ResultsExtractor replaces this with TerminalSim actual
    // execution totals when terminal execution records are available.
    result.terminalCosts = path->getTotalTerminalCosts();
    result.totalCost = result.edgeCosts + result.terminalCosts;

    if (emitInfoLog)
    {
        qCInfo(lcScenario)
            << "SegmentCostMath::computePathExecutionResult:"
            << "path" << result.pathId
            << "-> edgeCosts =" << result.edgeCosts
            << ", terminalCosts =" << result.terminalCosts
            << ", totalCost =" << result.totalCost;
    }

    return result;
}

} // namespace SegmentCostMath
} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
