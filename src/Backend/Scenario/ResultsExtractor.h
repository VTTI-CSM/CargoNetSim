#pragma once

#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QObject>
#include <QVector>
#include <QString>
#include <QVariantMap>

#include "Backend/Commons/TransportationMode.h"
#include "Backend/Models/Path.h" // for PathTerminal value type
#include "PathAllocation.h"
#include "ScenarioExecutionResult.h"
#include "PathSimulationResult.h"

// Forward-declared in global namespace so ResultsExtractor's friend decl
// can grant the unit-test class access to private math helpers.
class ResultsExtractorTest;

namespace CargoNetSim
{
namespace Backend
{
class Path;
class PathSegment;
class Terminal;
class ConfigController;
namespace ShipClient
{
class ShipSimulationClient;
}
namespace TrainClient
{
class TrainSimulationClient;
}
namespace TruckClient
{
class TruckSimulationManager;
}
class TerminalSimulationClient;
namespace Scenario
{

/**
 * @brief Reads simulator state, computes per-path costs.
 *
 * Replaces SimulationValidationWorker::extractResults and every
 * calculate* helper it drives. All math preserved VERBATIM from
 * SimulationValidationWorker.cpp:1901-2751 — see per-method comments
 * in the .cpp for exact source ranges.
 *
 * Stateless — no cached state between extract() calls. Clients are
 * owned by CargoNetSimController; this class takes raw non-owning
 * pointers and is invoked by the executor-owned live execution path.
 */
class ResultsExtractor : public QObject
{
    Q_OBJECT

    // Grants the test class access to private calculate*/setSegment*
    // methods so the pure math is exercisable without wiring live
    // RabbitMQ clients. Production code must not extend this list.
    friend class ::ResultsExtractorTest;

public:
    ResultsExtractor(
        CargoNetSim::Backend::ShipClient::ShipSimulationClient    *shipClient,
        CargoNetSim::Backend::TrainClient::TrainSimulationClient  *trainClient,
        CargoNetSim::Backend::TruckClient::TruckSimulationManager *truckManager,
        CargoNetSim::Backend::TerminalSimulationClient            *terminalClient,
        CargoNetSim::Backend::ConfigController                    *config,
        QObject                                                   *parent = nullptr);

    /**
     * @brief Compatibility summary view over the typed execution result set.
     */
    QList<PathSimulationResult>
    extract(const QList<CargoNetSim::Backend::Path *> &paths);

    /**
     * @brief Computes typed execution results without mutating the input
     *        paths' segment actual attributes.
     */
    ScenarioExecutionResultSet extractExecutionResults(
        const QList<CargoNetSim::Backend::Path *> &paths,
        const QVector<QString>                    &pathIdentities = {},
        const PathAllocation                      *allocation = nullptr,
        const QString                            &executionId = QString());

signals:
    void statusMessage(const QString &msg);
    void errorMessage(const QString &msg);

private:
    // Edge-cost math (Tasks 18-21).
    double calculateEdgeCosts(
        CargoNetSim::Backend::Path                       *path,
        const QList<CargoNetSim::Backend::PathSegment *> &segments,
        const QVariantMap                                &costFunctionWeights,
        const QVariantMap                                &transportModes,
        int                                               containerCount);

    double calculateShipSegmentCost(
        CargoNetSim::Backend::Path        *path,
        CargoNetSim::Backend::PathSegment *segment,
        int                                segmentCounter,
        const QVariantMap                 &modeWeights,
        const QVariantMap                 &transportModes,
        int                                containerCount);

    double calculateTrainSegmentCost(
        CargoNetSim::Backend::Path        *path,
        CargoNetSim::Backend::PathSegment *segment,
        int                                segmentCounter,
        const QVariantMap                 &modeWeights,
        const QVariantMap                 &transportModes,
        int                                containerCount);

    double calculateTruckSegmentCost(
        CargoNetSim::Backend::Path        *path,
        CargoNetSim::Backend::PathSegment *segment,
        int                                segmentCounter,
        const QVariantMap                 &modeWeights,
        const QVariantMap                 &transportModes,
        int                                containerCount);

    // Terminal-cost math (Tasks 15-17).
    double calculateTerminalCosts(
        const QList<CargoNetSim::Backend::PathSegment *> &segments,
        const QList<CargoNetSim::Backend::PathTerminal>  &terminals,
        const QVariantMap                                &costFunctionWeights,
        int                                               containerCount);

    double calculateSingleTerminalCost(
        CargoNetSim::Backend::Terminal *terminal,
        const QVariantMap              &costFunctionWeights,
        int                             containerCount,
        TransportationTypes::TransportationMode
            mode = TransportationTypes::TransportationMode::Any);

    double calculateTerminalDwellTime(const QJsonObject &config);

    bool calculateTerminalCustoms(const QJsonObject &config,
                                  double            &customsDelay,
                                  double            &customsCost);

    double calculateTerminalDirectCosts(const QJsonObject &config,
                                        bool               customsApplied);

    // Segment attribute writeback (Task 21).
    void setSegmentActualDetails(
        CargoNetSim::Backend::PathSegment *segment,
        const QMap<QString, double>       &details,
        const QString                     &underlyingKey);

    void deleteSegmentDetails(
        CargoNetSim::Backend::PathSegment *segment,
        const QString                     &underlyingKey);

    CargoNetSim::Backend::ShipClient::ShipSimulationClient    *m_shipClient;
    CargoNetSim::Backend::TrainClient::TrainSimulationClient  *m_trainClient;
    CargoNetSim::Backend::TruckClient::TruckSimulationManager *m_truckManager;
    CargoNetSim::Backend::TerminalSimulationClient            *m_terminalClient;
    CargoNetSim::Backend::ConfigController                    *m_config;
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
