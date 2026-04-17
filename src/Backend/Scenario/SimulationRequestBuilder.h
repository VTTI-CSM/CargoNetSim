#pragma once

#include <QList>
#include <QMap>
#include <QString>

#include "Backend/Clients/TrainClient/TrainNetwork.h"
#include "Backend/Clients/TruckClient/TruckNetwork.h"
#include "Backend/Models/ShipSystem.h"
#include "Backend/Models/TrainSystem.h"
#include "PathAllocation.h"
#include <containerLib/container.h>

namespace CargoNetSim
{
namespace Backend
{
class Path;
class ConfigController;
class RegionDataController;
class VehicleController;
namespace Scenario
{
class ScenarioDocument;
class ScenarioRegistry;
} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

/**
 * @brief Per-ship unit of work for ShipNetSim.
 *
 * Same layout as SimulationValidationWorker::ShipSimData
 * (SimulationValidationWorker.h:38-43).
 */
struct ShipSimData
{
    CargoNetSim::Backend::Ship       *ship = nullptr;
    QList<ContainerCore::Container *> containers;
    QString                           destinationTerminal;
};

/**
 * @brief Per-train unit of work for NeTrainSim.
 *
 * Same layout as SimulationValidationWorker::TrainSimData
 * (SimulationValidationWorker.h:45-49).
 */
struct TrainSimData
{
    CargoNetSim::Backend::Train      *train = nullptr;
    QList<ContainerCore::Container *> containers;
};

/**
 * @brief Per-truck-trip unit of work for INTEGRATION.
 *
 * Same layout as SimulationValidationWorker::TruckSimData
 * (SimulationValidationWorker.h:51-57).
 */
struct TruckSimData
{
    QString                           tripId;
    int                               originNode      = -1;
    int                               destinationNode = -1;
    QList<ContainerCore::Container *> containers;
};

/**
 * @brief Output of SimulationRequestBuilder::build — everything the
 *        orchestrator needs to submit work to the three clients.
 */
struct SimulationRequestBundle
{
    QMap<QString, QList<ShipSimData>>  shipData;
    QMap<QString, QList<TrainSimData>> trainData;
    QMap<QString, QList<TruckSimData>> truckData;
    QMap<QString, CargoNetSim::Backend::TrainClient::NeTrainSimNetwork *>
        trainNetworks;
    QMap<QString, CargoNetSim::Backend::TruckClient::IntegrationNetwork *>
        truckNetworks;

    /** @brief Accumulates another bundle's data into this one. */
    void merge(const SimulationRequestBundle &other);
};

/**
 * @brief Transforms (path × scenario) → simulation request bundle.
 *
 * Replaces the MainWindow-coupled setupSimulationDataForPath from
 * SimulationValidationWorker.cpp:272-887. All lookups go through
 * ScenarioDocument (terminals, linkages) and RegionDataController
 * (network instances) instead of scene items.
 *
 * Stateless transform — one class, one method, no member state.
 */
class SimulationRequestBuilder
{
public:
    SimulationRequestBuilder(const ScenarioDocument &doc,
                             const ScenarioRegistry &registry,
                             ConfigController       *config,
                             RegionDataController   *regionData,
                             VehicleController      *vehicles);

    /**
     * @brief Build a bundle for a single path.
     *
     * @param path Caller-owned Backend::Path. Must not be nullptr.
     * @param originContainers The container population to distribute
     *        across segments. Typically read by the executor from
     *        ScenarioDocument::originContainers() (Plan 1 accessor).
     * @param err On failure, receives a human-readable message.
     * @return true on success; on false, `bundle` is in an indeterminate
     *         state and must not be used.
     */
    bool buildForPath(
        CargoNetSim::Backend::Path              *path,
        const QList<ContainerCore::Container *> &originContainers,
        SimulationRequestBundle                 &bundle,
        QString                                 *err);

    /**
     * @brief Build a merged bundle for multiple paths.
     *
     * Iterates paths, calls buildForPath per path, accumulates via
     * SimulationRequestBundle::merge. Mirrors per-path iteration in
     * SimulationValidationWorker.cpp:222-258.
     *
     * Post-Plan-10: the per-path partition comes from
     * ContainerAllocator::allocate — the builder no longer contains
     * an implicit allocation policy.
     */
    bool build(
        const QList<CargoNetSim::Backend::Path *> &paths,
        const PathAllocation                       &allocation,
        SimulationRequestBundle                    &bundle,
        QString                                    *err);

private:
    bool buildTrainSegment(
        CargoNetSim::Backend::Path              *path,
        int                                      segmentIdx,
        const QString                           &startId,
        const QString                           &endId,
        int                                      trainContainerCount,
        const QList<ContainerCore::Container *> &containersAvailable,
        int                                     &trainCounter,
        int                                     &containerCounter,
        SimulationRequestBundle                 &bundle,
        QString                                 *err);

    bool buildTruckSegment(
        CargoNetSim::Backend::Path              *path,
        int                                      segmentIdx,
        const QString                           &startId,
        const QString                           &endId,
        int                                      truckContainerCount,
        const QList<ContainerCore::Container *> &containersAvailable,
        int                                     &truckCounter,
        int                                     &containerCounter,
        SimulationRequestBundle                 &bundle,
        QString                                 *err);

    bool buildShipSegment(
        CargoNetSim::Backend::Path              *path,
        int                                      segmentIdx,
        const QString                           &startId,
        const QString                           &endId,
        int                                      shipContainerCount,
        const QList<ContainerCore::Container *> &containersAvailable,
        int                                     &shipCounter,
        int                                     &containerCounter,
        SimulationRequestBundle                 &bundle,
        QString                                 *err);

    const ScenarioDocument &m_doc;
    const ScenarioRegistry &m_registry;
    ConfigController       *m_config;
    RegionDataController   *m_regionData;
    VehicleController      *m_vehicles;
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
