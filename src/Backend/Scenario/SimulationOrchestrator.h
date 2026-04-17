#pragma once

#include <QObject>
#include <QString>

namespace CargoNetSim
{
namespace Backend
{
class SimulationClientBase;
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
struct SimulationRequestBundle;

/**
 * @brief Submits a SimulationRequestBundle to the three RabbitMQ clients.
 *
 * Replaces SimulationValidationWorker::runSimulations. Stateless — each
 * submit() call is self-contained.
 *
 * Ownership: clients are owned by CargoNetSimController; this class
 * takes raw pointers (non-owning) and must never outlive them.
 *
 * Separation of concerns: this class only wires bundle data to clients;
 * no bundle-building logic (that's SimulationRequestBuilder) and no
 * result extraction (that's ResultsExtractor, Task 14+).
 */
class SimulationOrchestrator : public QObject
{
    Q_OBJECT
public:
    SimulationOrchestrator(
        CargoNetSim::Backend::ShipClient::ShipSimulationClient    *shipClient,
        CargoNetSim::Backend::TrainClient::TrainSimulationClient  *trainClient,
        CargoNetSim::Backend::TruckClient::TruckSimulationManager *truckManager,
        QObject                                                   *parent = nullptr);

    /**
     * @brief For each populated mode of the bundle, reset the client,
     *        call defineSimulator / addContainers, then runSimulator.
     *
     * Empty-mode branches are no-ops. If a mode is populated but its
     * client is null/disconnected, returns false with a descriptive
     * message via @p err and emits errorMessage.
     */
    bool submit(const SimulationRequestBundle &bundle, QString *err);

signals:
    void statusMessage(const QString &msg);
    void errorMessage(const QString &msg);

private:
    bool submitTrain(const SimulationRequestBundle &bundle, QString *err);
    bool submitShip (const SimulationRequestBundle &bundle, QString *err);
    bool submitTruck(const SimulationRequestBundle &bundle, QString *err);

    /**
     * @brief Emits @p msg via errorMessage, copies to @p err if non-null,
     *        and returns false. Single source for the "report and fail"
     *        pattern used by all three submit* methods.
     */
    bool emitAndFail(const QString &msg, QString *err);

    /**
     * @brief Null + connectivity guard for SimulationClientBase-derived
     *        clients (Train and Ship). On failure, routes through
     *        emitAndFail with a kind-prefixed message.
     *
     * Not used by Truck (TruckSimulationManager has a different API —
     * no RabbitMQ handler indirection); truck uses emitAndFail directly.
     */
    bool ensureClientConnected(
        CargoNetSim::Backend::SimulationClientBase *client,
        const QString                              &kind,
        QString                                    *err);

    CargoNetSim::Backend::ShipClient::ShipSimulationClient    *m_shipClient;
    CargoNetSim::Backend::TrainClient::TrainSimulationClient  *m_trainClient;
    CargoNetSim::Backend::TruckClient::TruckSimulationManager *m_truckManager;
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
