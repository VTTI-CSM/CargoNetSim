#include "ResultsExtractor.h"

#include "Backend/Commons/LogCategories.h"
#include "Backend/Controllers/ConfigController.h"
#include "Backend/Models/Path.h"
#include "SegmentCostMath.h"
#include "TerminalCostMath.h"

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

ResultsExtractor::ResultsExtractor(
    CargoNetSim::Backend::ShipClient::ShipSimulationClient    *shipClient,
    CargoNetSim::Backend::TrainClient::TrainSimulationClient  *trainClient,
    CargoNetSim::Backend::TruckClient::TruckSimulationManager *truckManager,
    CargoNetSim::Backend::ConfigController                    *config,
    QObject                                                   *parent)
    : QObject(parent)
    , m_shipClient(shipClient)
    , m_trainClient(trainClient)
    , m_truckManager(truckManager)
    , m_config(config)
{
}

QList<PathSimulationResult> ResultsExtractor::extract(
    const QList<CargoNetSim::Backend::Path *> &paths,
    int                                        containerCount)
{
    qCInfo(lcScenario) << "ResultsExtractor::extract: paths:" << paths.size()
                       << "containerCount:" << containerCount;
    QList<PathSimulationResult> results;
    if (!m_config)
    {
        qCWarning(lcScenario) << "ResultsExtractor::extract: config is null, returning empty";
        return results;
    }

    const QVariantMap costWeights =
        m_config->getCostFunctionWeights();
    const QVariantMap transportModes =
        m_config->getTransportModes();

    // Clear prior segment attributes — single source of truth in
    // SegmentCostMath::clearPathAttributes (shared with SVW).
    for (auto *path : paths)
        SegmentCostMath::clearPathAttributes(path);

    emit statusMessage(
        QStringLiteral("Extracting simulation results..."));

    for (auto *path : paths)
    {
        if (!path)
            continue;
        if (containerCount == 0)
        {
            qCWarning(lcScenario) << "ResultsExtractor::extract: containerCount is 0, skipping path";
            emit errorMessage(QStringLiteral(
                "No containers at origin terminal!"));
            continue;
        }

        // Single source of truth: same composition SVW uses.
        const auto r = SegmentCostMath::computePathCosts(
            m_shipClient, m_trainClient, m_truckManager, path,
            costWeights, transportModes, containerCount);
        results.append(r);
        qCDebug(lcScenario) << "ResultsExtractor::extract: pathId:" << r.pathId
                            << "totalCost:" << r.totalCost
                            << "edgeCosts:" << r.edgeCosts
                            << "terminalCosts:" << r.terminalCosts;

        emit statusMessage(QString("Path %1 simulation cost: "
                                   "$%2 (edges: $%3, "
                                   "terminals: $%4)")
                               .arg(r.pathId)
                               .arg(r.totalCost,     0, 'f', 2)
                               .arg(r.edgeCosts,     0, 'f', 2)
                               .arg(r.terminalCosts, 0, 'f', 2));
    }

    qCInfo(lcScenario) << "ResultsExtractor::extract: completed, results:" << results.size();
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
    int                             containerCount)
{
    qCDebug(lcScenario) << "ResultsExtractor::calculateSingleTerminalCost: containers:"
                        << containerCount;
    return TerminalCostMath::singleTerminalCost(
        terminal, costFunctionWeights, containerCount);
}

double ResultsExtractor::calculateTerminalCosts(
    const QList<CargoNetSim::Backend::PathSegment *> &segments,
    const QList<CargoNetSim::Backend::Terminal *>    &terminals,
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
