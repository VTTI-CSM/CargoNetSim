#include "SegmentCostMath.h"

#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

#include "Backend/Clients/ShipClient/ShipSimulationClient.h"
#include "Backend/Clients/ShipClient/ShipState.h"
#include "Backend/Clients/TrainClient/TrainSimulationClient.h"
#include "Backend/Clients/TrainClient/TrainState.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Commons/TransportationMode.h"
#include "Backend/Models/Path.h"
#include "Backend/Models/PathSegment.h"
#include "PropertyKeys.h"
#include "RuntimeArtifactIdentity.h"
#include "TerminalCostMath.h"

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

// Mode-agnostic aggregated vehicle metrics. Populated by per-mode state
// loops, then consumed by applyVehicleMetricsAndWriteback.
struct VehicleSegmentMetrics
{
    double travelTime        = 0.0;
    double distance          = 0.0;
    double carbonEmissions   = 0.0;
    double energyConsumption = 0.0;
    double risk              = 0.0;
    int    vehicleCount      = 0;
};

struct ComputedSegmentData
{
    PathSegment::SegmentMetricSnapshot actualMetrics;
    PathSegment::SegmentCostSnapshot   actualCosts;
    QVariantMap                        rawActualValues;
    QMap<QString, double>              rawActualCosts;
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
    if (m.vehicleCount == 0)
        return out;

    // Calculate containers per vehicle
    double containersPerVehicle =
        (double)containerCount / m.vehicleCount;

    // Calculate container-to-capacity ratio (how full is
    // each vehicle)
    double containerToCapacityRatio =
        containersPerVehicle / vehicleCapacity;

    // Adjust metrics by ratio
    m.carbonEmissions *= containerToCapacityRatio;
    m.energyConsumption *= containerToCapacityRatio;

    // Apply weights to metrics
    QMap<QString, double> simulatedValues;
    QMap<QString, double> simulatedCost;
    simulatedValues[PK::Segment::TravelTime]        = m.travelTime;
    simulatedValues[PK::Segment::Distance]          = m.distance;
    simulatedValues[PK::Segment::CarbonEmissions]   = m.carbonEmissions;
    simulatedValues[PK::Segment::EnergyConsumption] =
        m.energyConsumption;
    simulatedValues[PK::Segment::Risk] = m.risk;

    simulatedCost[PK::Segment::TravelTime] =
        m.travelTime * modeWeights[PK::Segment::TravelTime].toDouble();
    simulatedCost[PK::Segment::Distance] =
        m.distance * modeWeights[PK::Segment::Distance].toDouble();
    simulatedCost[PK::Segment::CarbonEmissions] =
        m.carbonEmissions
        * modeWeights[PK::Segment::CarbonEmissions].toDouble(); // kg
    simulatedCost[PK::Segment::EnergyConsumption] =
        m.energyConsumption
        * modeWeights[PK::Segment::EnergyConsumption]
              .toDouble(); // kWh
    simulatedCost[PK::Segment::Risk] =
        m.risk * modeWeights[PK::Segment::Risk].toDouble();
    simulatedCost[PK::Segment::Cost] = 0.0;

    out.actualMetrics.available = true;
    out.actualMetrics.travelTime =
        simulatedValues[PK::Segment::TravelTime] * 3600.0;
    out.actualMetrics.distance =
        simulatedValues[PK::Segment::Distance] * 1000.0;
    out.actualMetrics.carbonEmissions =
        simulatedValues[PK::Segment::CarbonEmissions];
    out.actualMetrics.energyConsumption =
        simulatedValues[PK::Segment::EnergyConsumption];
    out.actualMetrics.risk =
        simulatedValues[PK::Segment::Risk];

    out.actualCosts.available = true;
    out.actualCosts.travelTime =
        simulatedCost[PK::Segment::TravelTime];
    out.actualCosts.distance =
        simulatedCost[PK::Segment::Distance];
    out.actualCosts.carbonEmissions =
        simulatedCost[PK::Segment::CarbonEmissions];
    out.actualCosts.energyConsumption =
        simulatedCost[PK::Segment::EnergyConsumption];
    out.actualCosts.risk =
        simulatedCost[PK::Segment::Risk];
    out.actualCosts.directCost =
        simulatedCost[PK::Segment::Cost];

    for (auto it = simulatedValues.constBegin();
         it != simulatedValues.constEnd(); ++it)
    {
        out.rawActualValues.insert(it.key(), it.value());
    }
    out.rawActualCosts = simulatedCost;
    return out;
}

double totalCost(const ComputedSegmentData &data)
{
    return data.actualCosts.total();
}

void writeActualSegmentData(PathSegment               *segment,
                            const ComputedSegmentData &data)
{
    if (!segment || !data.actualMetrics.available)
        return;

    segment->clearActual();
    SegmentCostMath::deleteActualDetails(
        segment, PK::Segment::ActualCost);
    segment->setActualValues(data.rawActualValues);
    SegmentCostMath::setActualDetails(
        segment, data.rawActualCosts, PK::Segment::ActualCost);
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
                    shipState->getTripTime() / 3600.0;
                metrics.distance +=
                    shipState->getTravelledDistance() / 1000.0;
                metrics.carbonEmissions +=
                    shipState->getCarbonEmissions() / 1000.0;
                metrics.energyConsumption +=
                    shipState->getEnergyConsumption();
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
                    trainState->getTripTime() / 3600.0;
                metrics.distance +=
                    trainState->getTravelledDistance() / 1000.0;
                metrics.carbonEmissions +=
                    trainState->getTotalCarbonDioxideEmitted()
                    / 1000.0;
                metrics.energyConsumption +=
                    trainState->getTotalEnergyConsumed();
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

    metrics.vehicleCount =
        (containerCount + std::max(truckCapacity, 1) - 1)
        / std::max(truckCapacity, 1);
    metrics.risk =
        truckData.value(PK::Mode::RiskFactor, 0.012).toDouble();

    return computeVehicleSegmentData(metrics, modeWeights,
                                     containerCount, truckCapacity);
}

} // namespace

namespace SegmentCostMath
{

// Per-mode aggregation is mode-specific (ShipState / TrainState APIs
// differ); the post-aggregation math is shared via
// applyVehicleMetricsAndWriteback. Null-client/path/segment guards added
// to tolerate uninitialized extractors. Math VERBATIM from SVW.
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
    writeActualSegmentData(segment, data);
    const double cost = totalCost(data);

    qCDebug(lcScenario) << "SegmentCostMath::shipSegmentCost:"
                        << "computed cost =" << cost
                        << ", actual data available ="
                        << data.actualMetrics.available;

    return cost;
}

// Structurally identical to shipSegmentCost but targets TrainState's
// differently-named accessors and reads transportModes["rail"]. Math
// VERBATIM from SVW's original train branch.
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
    writeActualSegmentData(segment, data);
    const double cost = totalCost(data);

    qCDebug(lcScenario) << "SegmentCostMath::trainSegmentCost:"
                        << "computed cost =" << cost
                        << ", actual data available ="
                        << data.actualMetrics.available;

    return cost;
}

// Heuristic truck cost. VERBATIM from the original SimulationValidationWorker
// body. Truck does not loop over real per-trip state yet — the commented-out
// block inside shows how aggregation WOULD work once TruckSimulationManager's
// results API lands. For now, truckCount is a ceiling-divide of
// containerCount by capacity, and all metrics except risk remain zero.
//
// Note the unit-conversion divisors (`/3600.0`, `/1000.0`) on travelTime
// and distance weighting: they are quirks of the SVW placeholder formula
// preserved here for behavior parity. Since travelTime/distance remain
// zero in the heuristic path, the divisors have no observable effect.
// Truck cost — now delegates to the same shared helper as ship/train.
// Heuristic-only: estimates truckCount from ceiling-divide of
// containerCount by capacity. All metrics except risk remain zero until
// TruckSimulationManager's results API lands and the aggregation loop
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
    writeActualSegmentData(segment, data);
    const double cost = totalCost(data);

    qCDebug(lcScenario) << "SegmentCostMath::truckSegmentCost:"
                        << "computed cost =" << cost
                        << ", actual data available ="
                        << data.actualMetrics.available
                        << "(heuristic)";

    return cost;
}

// VERBATIM dispatch from the original SVW::calculateEdgeCosts. Loops
// segments, picks per-mode weights from costFunctionWeights (integer
// mode key, "default" fallback), and delegates to the per-mode cost
// function. Clients are forwarded through; null clients cause the
// per-mode function to early-return 0 (ship/train) or harmlessly ignore
// (truck heuristic).
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

// VERBATIM from SimulationValidationWorker — accumulates per-key values
// under attributes[underlyingKey]; existing keys are summed.
void setActualDetails(
    CargoNetSim::Backend::PathSegment *segment,
    const QMap<QString, double>       &details,
    const QString                     &underlyingKey)
{
    if (!segment)
    {
        return;
    }
    // Get current attributes
    QJsonObject currentAttributes =
        segment->getAttributes();
    // Create or get the underlying object
    QJsonObject underlyingObject;
    if (currentAttributes.contains(underlyingKey)
        && currentAttributes[underlyingKey].isObject())
    {
        underlyingObject =
            currentAttributes[underlyingKey].toObject();
    }
    // Set the segment details
    for (auto it = details.constBegin();
         it != details.constEnd(); ++it)
    {
        // Check if the key already exists in the underlying
        // object
        if (underlyingObject.contains(it.key()))
        {
            // Add the old value to the new value
            double oldValue =
                underlyingObject[it.key()].toDouble();
            underlyingObject[it.key()] =
                oldValue + it.value();
        }
        else
        {
            // Set the new value directly if key doesn't
            // exist
            underlyingObject[it.key()] = it.value();
        }
    }
    // Update the attributes with the modified object
    currentAttributes[underlyingKey] = underlyingObject;
    // Set the updated attributes back to the segment
    segment->setAttributes(currentAttributes);
}

// VERBATIM from SimulationValidationWorker — removes the underlyingKey
// (and all nested values) from the segment's attributes.
void deleteActualDetails(
    CargoNetSim::Backend::PathSegment *segment,
    const QString                     &underlyingKey)
{
    if (!segment)
    {
        return;
    }

    // Get current attributes
    QJsonObject currentAttributes =
        segment->getAttributes();

    // Check if the underlyingKey exists in the attributes
    if (currentAttributes.contains(underlyingKey))
    {
        // Remove the underlyingKey and all values below it
        currentAttributes.remove(underlyingKey);

        // Set the updated attributes back to the segment
        segment->setAttributes(currentAttributes);
    }
}

CargoNetSim::Backend::Scenario::PathExecutionResult
computePathExecutionResult(
    CargoNetSim::Backend::ShipClient::ShipSimulationClient    *shipClient,
    CargoNetSim::Backend::TrainClient::TrainSimulationClient  *trainClient,
    CargoNetSim::Backend::TruckClient::TruckSimulationManager *truckManager,
    CargoNetSim::Backend::Path                                *path,
    const QString                                             &pathIdentity,
    const QVariantMap                                         &costFunctionWeights,
    const QVariantMap                                         &transportModes,
    int                                                        containerCount)
{
    CargoNetSim::Backend::Scenario::PathExecutionResult result;
    if (!path)
        return result;

    qCDebug(lcScenario) << "SegmentCostMath::computePathExecutionResult:"
                        << "path ID =" << path->getPathId()
                        << ", containerCount =" << containerCount;

    const auto segments = path->getSegments();
    result.pathIdentity = pathIdentity.isEmpty()
        ? path->canonicalPathKey()
        : pathIdentity;
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

    result.terminalCosts = TerminalCostMath::totalTerminalCosts(
        segments, path->getTerminalsInPath(),
        costFunctionWeights, containerCount);
    result.totalCost = result.edgeCosts + result.terminalCosts;

    qCInfo(lcScenario)
        << "SegmentCostMath::computePathExecutionResult:"
        << "path" << result.pathId
        << "-> edgeCosts =" << result.edgeCosts
        << ", terminalCosts =" << result.terminalCosts
        << ", totalCost =" << result.totalCost;

    return result;
}

} // namespace SegmentCostMath
} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
