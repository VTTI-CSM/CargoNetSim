#pragma once

#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>
#include <QVariantMap>

#include "Backend/Commons/TransportationMode.h"
#include "Backend/Models/Path.h" // for PathTerminal value type
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
 * pointers. Follows the same ownership contract as
 * SimulationOrchestrator.
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
        CargoNetSim::Backend::ConfigController                    *config,
        QObject                                                   *parent = nullptr);

    /**
     * @brief For each path, computes total / edge / terminal costs and
     *        writes per-segment actual_values + actual_cost attributes
     *        via PathSegment::setAttributes.
     *
     * Mirrors SimulationValidationWorker.cpp:1901-1979 at the driver
     * level; per-mode cost math lives in the private calculate* methods.
     */
    QList<PathSimulationResult>
    extract(const QList<CargoNetSim::Backend::Path *> &paths,
            int                                        containerCount);

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
    CargoNetSim::Backend::ConfigController                    *m_config;
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
