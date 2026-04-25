#include "ResultsExtractor.h"

#include <algorithm>

#include "Backend/Clients/TerminalClient/TerminalSimulationClient.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Controllers/ConfigController.h"
#include "Backend/Models/Path.h"
#include "PropertyKeys.h"
#include "SegmentCostMath.h"
#include "TerminalCostMath.h"

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

namespace
{

namespace PK = PropertyKeys;

QVariantMap terminalWeightsForMode(
    const QVariantMap &costWeights,
    TransportationTypes::TransportationMode mode)
{
    const QString modeKey =
        QString::number(static_cast<int>(mode));
    return costWeights.contains(modeKey)
        ? costWeights.value(modeKey).toMap()
        : costWeights.value(QStringLiteral("default")).toMap();
}

TransportationTypes::TransportationMode terminalArrivalModeFromJson(
    const QJsonArray &rawBatchRecords)
{
    if (rawBatchRecords.isEmpty() || !rawBatchRecords.first().isObject())
        return TransportationTypes::TransportationMode::Any;

    const QJsonObject firstRecord =
        rawBatchRecords.first().toObject();
    const QString mode =
        firstRecord.value(QStringLiteral("vehicle_mode")).toString();
    return mode.isEmpty()
        ? TransportationTypes::TransportationMode::Any
        : transportationModeFromString(mode);
}

QHash<QString, QList<TerminalExecutionResult>>
loadTerminalExecutionResults(
    CargoNetSim::Backend::TerminalSimulationClient *terminalClient,
    const QVariantMap                              &costWeights,
    const QString                                  &executionId,
    const QStringList                              &pathIdentities)
{
    QHash<QString, QList<TerminalExecutionResult>> byPathIdentity;
    if (!terminalClient || executionId.isEmpty()
        || pathIdentities.isEmpty())
    {
        return byPathIdentity;
    }

    const QJsonArray rawResults =
        terminalClient->getTerminalExecutionResults(
            executionId, {}, pathIdentities);
    for (const auto &value : rawResults)
    {
        if (!value.isObject())
            continue;

        auto terminalResult =
            TerminalExecutionResult::fromJson(value.toObject());
        terminalResult.arrivalMode =
            terminalArrivalModeFromJson(
                terminalResult.rawBatchRecords);
        const QVariantMap weights = terminalWeightsForMode(
            costWeights, terminalResult.arrivalMode);
        terminalResult.actualWeightedDelayContribution =
            terminalResult.actualTotalHandlingSeconds
            * weights.value(PK::Segment::TerminalDelay)
                  .toDouble();
        terminalResult.actualWeightedCostContribution =
            terminalResult.actualDirectCostUsd
            * weights.value(PK::Segment::TerminalCost)
                  .toDouble();
        terminalResult.actualWeightedTotalContribution =
            terminalResult.actualWeightedDelayContribution
            + terminalResult.actualWeightedCostContribution;
        byPathIdentity[terminalResult.pathIdentity].append(
            terminalResult);
    }

    for (auto it = byPathIdentity.begin();
         it != byPathIdentity.end(); ++it)
    {
        auto &terminalResults = it.value();
        std::sort(
            terminalResults.begin(), terminalResults.end(),
            [](const TerminalExecutionResult &lhs,
               const TerminalExecutionResult &rhs) {
                if (lhs.terminalSequenceIndex
                    != rhs.terminalSequenceIndex)
                {
                    return lhs.terminalSequenceIndex
                        < rhs.terminalSequenceIndex;
                }
                return lhs.scenarioTerminalId
                    < rhs.scenarioTerminalId;
            });
    }

    return byPathIdentity;
}

} // namespace

ResultsExtractor::ResultsExtractor(
    CargoNetSim::Backend::ShipClient::ShipSimulationClient    *shipClient,
    CargoNetSim::Backend::TrainClient::TrainSimulationClient  *trainClient,
    CargoNetSim::Backend::TruckClient::TruckSimulationManager *truckManager,
    CargoNetSim::Backend::TerminalSimulationClient            *terminalClient,
    CargoNetSim::Backend::ConfigController                    *config,
    QObject                                                   *parent)
    : QObject(parent)
    , m_shipClient(shipClient)
    , m_trainClient(trainClient)
    , m_truckManager(truckManager)
    , m_terminalClient(terminalClient)
    , m_config(config)
{
}

QList<PathSimulationResult> ResultsExtractor::extract(
    const QList<CargoNetSim::Backend::Path *> &paths)
{
    return extractExecutionResults(paths).summaryResults();
}

ScenarioExecutionResultSet ResultsExtractor::extractExecutionResults(
    const QList<CargoNetSim::Backend::Path *> &paths,
    const QVector<QString>                    &pathIdentities,
    const PathAllocation                      *allocation,
    const QString                            &executionId)
{
    qCInfo(lcScenario) << "ResultsExtractor::extract: paths:" << paths.size()
                       << "(per-path container counts carried on Path snapshots)";
    ScenarioExecutionResultSet results;
    if (!m_config)
    {
        qCWarning(lcScenario) << "ResultsExtractor::extract: config is null, returning empty";
        return results;
    }

    const QVariantMap costWeights =
        m_config->getCostFunctionWeights();
    const QVariantMap transportModes =
        m_config->getTransportModes();
    QStringList resolvedPathIdentities;
    resolvedPathIdentities.reserve(paths.size());
    for (int index = 0; index < paths.size(); ++index)
    {
        auto *path = paths[index];
        if (!path)
            continue;
        resolvedPathIdentities.append(
            (index < pathIdentities.size()
             && !pathIdentities[index].isEmpty())
            ? pathIdentities[index]
            : path->canonicalPathKey());
    }
    const auto terminalResultsByPathIdentity =
        loadTerminalExecutionResults(
            m_terminalClient, costWeights, executionId,
            resolvedPathIdentities);

    emit statusMessage(
        QStringLiteral("Extracting simulation results..."));

    for (int index = 0; index < paths.size(); ++index)
    {
        auto *path = paths[index];
        if (!path)
            continue;
        const int containerCount = allocation
            ? allocation->effectiveContainerCountForPath(path)
            : path->getEffectiveContainerCount();
        if (containerCount == 0)
        {
            qCWarning(lcScenario)
                << "ResultsExtractor::extract: effectiveContainerCount is 0, skipping path"
                << path->canonicalPathKey();
            continue;
        }

        const QString pathIdentity =
            (index < pathIdentities.size()
             && !pathIdentities[index].isEmpty())
            ? pathIdentities[index]
            : path->canonicalPathKey();

        auto result = SegmentCostMath::computePathExecutionResult(
            m_shipClient, m_trainClient, m_truckManager, path,
            pathIdentity, costWeights, transportModes, containerCount);
        result.executionId = executionId;
        result.terminalResults =
            terminalResultsByPathIdentity.value(pathIdentity);
        for (const auto &terminalResult : result.terminalResults)
        {
            result.modeledActualTerminalCosts +=
                terminalResult.actualWeightedTotalContribution;
        }
        const auto summary = result.toSimulationResult();
        results.addPathResult(result);
        qCDebug(lcScenario) << "ResultsExtractor::extractExecutionResults: pathId:" << summary.pathId
                            << "pathKey:" << summary.canonicalPathKey
                            << "totalCost:" << summary.totalCost
                            << "edgeCosts:" << summary.edgeCosts
                            << "terminalCosts:" << summary.terminalCosts;

        emit statusMessage(QString("Path %1 simulation cost: "
                                   "$%2 (edges: $%3, "
                                   "terminals: $%4)")
                               .arg(summary.pathId)
                               .arg(summary.totalCost,     0, 'f', 2)
                               .arg(summary.edgeCosts,     0, 'f', 2)
                               .arg(summary.terminalCosts, 0, 'f', 2));
    }

    qCInfo(lcScenario) << "ResultsExtractor::extractExecutionResults: completed, results:" << results.size();
    if (m_terminalClient && !executionId.isEmpty())
    {
        const int cleared =
            m_terminalClient->clearTerminalExecutionResults(
                executionId);
        qCDebug(lcScenario)
            << "ResultsExtractor::extractExecutionResults:"
            << "cleared terminal execution records:" << cleared
            << "for executionId =" << executionId;
    }
    emit statusMessage(QStringLiteral(
        "Results extraction completed successfully"));
    return results;
}

// --- Terminal-cost math (Tasks 15-17) ---

double ResultsExtractor::calculateTerminalDwellTime(
    const QJsonObject &config)
{
    qCDebug(lcScenario) << "ResultsExtractor::calculateTerminalDwellTime: entry";
    return TerminalCostMath::dwellTime(config);
}

bool ResultsExtractor::calculateTerminalCustoms(
    const QJsonObject &config,
    double            &customsDelay,
    double            &customsCost)
{
    qCDebug(lcScenario) << "ResultsExtractor::calculateTerminalCustoms: entry";
    return TerminalCostMath::customs(config, customsDelay,
                                     customsCost);
}

double ResultsExtractor::calculateTerminalDirectCosts(
    const QJsonObject &config, bool customsApplied)
{
    qCDebug(lcScenario) << "ResultsExtractor::calculateTerminalDirectCosts: customsApplied:"
                        << customsApplied;
    return TerminalCostMath::directCosts(config, customsApplied);
}

double ResultsExtractor::calculateSingleTerminalCost(
    CargoNetSim::Backend::Terminal *terminal,
    const QVariantMap              &costFunctionWeights,
    int                             containerCount,
    TransportationTypes::TransportationMode mode)
{
    qCDebug(lcScenario) << "ResultsExtractor::calculateSingleTerminalCost: containers:"
                        << containerCount;
    return TerminalCostMath::singleTerminalCost(
        terminal, costFunctionWeights, containerCount, mode);
}

double ResultsExtractor::calculateTerminalCosts(
    const QList<CargoNetSim::Backend::PathSegment *> &segments,
    const QList<CargoNetSim::Backend::PathTerminal>  &terminals,
    const QVariantMap                                &costFunctionWeights,
    int                                               containerCount)
{
    qCDebug(lcScenario) << "ResultsExtractor::calculateTerminalCosts: segments:"
                        << segments.size() << "terminals:" << terminals.size();
    return TerminalCostMath::totalTerminalCosts(
        segments, terminals, costFunctionWeights, containerCount);
}

// --- Edge-cost math ---

double ResultsExtractor::calculateEdgeCosts(
    CargoNetSim::Backend::Path                       *path,
    const QList<CargoNetSim::Backend::PathSegment *> &segments,
    const QVariantMap                                &costFunctionWeights,
    const QVariantMap                                &transportModes,
    int                                               containerCount)
{
    qCDebug(lcScenario) << "ResultsExtractor::calculateEdgeCosts: segments:"
                        << segments.size() << "containers:" << containerCount;
    return SegmentCostMath::edgeCosts(
        m_shipClient, m_trainClient, m_truckManager, path,
        segments, costFunctionWeights, transportModes,
        containerCount);
}

double ResultsExtractor::calculateShipSegmentCost(
    CargoNetSim::Backend::Path        *path,
    CargoNetSim::Backend::PathSegment *segment,
    int                                segmentCounter,
    const QVariantMap                 &modeWeights,
    const QVariantMap                 &transportModes,
    int                                containerCount)
{
    qCDebug(lcScenario) << "ResultsExtractor::calculateShipSegmentCost: segment:"
                        << segmentCounter;
    return SegmentCostMath::shipSegmentCost(
        m_shipClient, path, segment, segmentCounter,
        modeWeights, transportModes, containerCount);
}

double ResultsExtractor::calculateTrainSegmentCost(
    CargoNetSim::Backend::Path        *path,
    CargoNetSim::Backend::PathSegment *segment,
    int                                segmentCounter,
    const QVariantMap                 &modeWeights,
    const QVariantMap                 &transportModes,
    int                                containerCount)
{
    qCDebug(lcScenario) << "ResultsExtractor::calculateTrainSegmentCost: segment:"
                        << segmentCounter;
    return SegmentCostMath::trainSegmentCost(
        m_trainClient, path, segment, segmentCounter,
        modeWeights, transportModes, containerCount);
}

double ResultsExtractor::calculateTruckSegmentCost(
    CargoNetSim::Backend::Path        *path,
    CargoNetSim::Backend::PathSegment *segment,
    int                                segmentCounter,
    const QVariantMap                 &modeWeights,
    const QVariantMap                 &transportModes,
    int                                containerCount)
{
    qCDebug(lcScenario) << "ResultsExtractor::calculateTruckSegmentCost: segment:"
                        << segmentCounter;
    return SegmentCostMath::truckSegmentCost(
        m_truckManager, path, segment, segmentCounter,
        modeWeights, transportModes, containerCount);
}

// --- Segment attribute writeback (Task 21 helpers, used by ship above) ---

void ResultsExtractor::setSegmentActualDetails(
    CargoNetSim::Backend::PathSegment *segment,
    const QMap<QString, double>       &details,
    const QString                     &underlyingKey)
{
    SegmentCostMath::setActualDetails(segment, details,
                                      underlyingKey);
}

void ResultsExtractor::deleteSegmentDetails(
    CargoNetSim::Backend::PathSegment *segment,
    const QString                     &underlyingKey)
{
    SegmentCostMath::deleteActualDetails(segment, underlyingKey);
}

// Remaining calculate* bodies (train, truck, edge dispatch) added in Tasks 19-21.

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
