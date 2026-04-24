#include "ServerStatusProbe.h"

#include "Backend/Clients/BaseClient/RabbitMQHandler.h"
#include "Backend/Clients/ShipClient/ShipSimulationClient.h"
#include "Backend/Clients/TerminalClient/TerminalSimulationClient.h"
#include "Backend/Clients/TrainClient/TrainSimulationClient.h"
#include "Backend/Clients/TruckClient/TruckSimulationManager.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Scenario/SimulatorCommandAvailability.h"

#include <QDebug>
#include <exception>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

ServerStatusProbe::ServerStatusProbe(QObject *parent)
    : QObject(parent)
{
}

namespace
{

ServerStatusProbe::ServerStatus makeStatus(const QString &name)
{
    ServerStatusProbe::ServerStatus s;
    s.server = name;
    return s;
}

} // namespace

QList<ServerStatusProbe::ServerStatus> ServerStatusProbe::pollAll()
{
    qCDebug(lcScenario) << "ServerStatusProbe::pollAll: begin";
    QList<ServerStatus> results;
    results.reserve(4);

    ServerStatus terminal = makeStatus("TerminalSim");
    ServerStatus train    = makeStatus("NeTrainSim");
    ServerStatus ship     = makeStatus("ShipNetSim");
    ServerStatus truck    = makeStatus("INTEGRATION");

    // Use nullable instance() rather than getInstance(): pollAll may
    // be called during startup (before main() constructs the
    // controller) or shutdown (after its destructor has cleared
    // s_instance). Returning four "disconnected" entries is the
    // correct semantics in that window, matching the test contract
    // in test_poll_without_controller_returns_disconnected_entries.
    auto *controllerPtr =
        CargoNetSim::CargoNetSimController::instance();
    if (controllerPtr == nullptr)
    {
        qCDebug(lcScenario) << "ServerStatusProbe::pollAll: no controller"
                            << "(startup/shutdown window) - returning"
                            << "disconnected entries";
        results << terminal << train << ship << truck;
        return results;
    }

    try
    {
        auto &controller = *controllerPtr;

        if (auto *c = controller.getTerminalClient())
        {
            terminal.clientExists = true;
            terminal.connected    = c->isConnected();
            if (auto *h = c->getRabbitMQHandler())
                terminal.hasConsumers = h->hasCommandQueueConsumers();
            terminal.commandAvailable = isCommandAvailable(
                terminal.connected, terminal.hasConsumers);
        }
        qCDebug(lcScenario) << "ServerStatusProbe::pollAll: TerminalSim"
                            << "connected=" << terminal.connected
                            << "hasConsumers=" << terminal.hasConsumers;

        if (auto *c = controller.getTrainClient())
        {
            train.clientExists = true;
            train.connected    = c->isConnected();
            if (auto *h = c->getRabbitMQHandler())
                train.hasConsumers = h->hasCommandQueueConsumers();
            train.commandAvailable = isCommandAvailable(
                train.connected, train.hasConsumers);
        }
        qCDebug(lcScenario) << "ServerStatusProbe::pollAll: NeTrainSim"
                            << "connected=" << train.connected
                            << "hasConsumers=" << train.hasConsumers;

        if (auto *c = controller.getShipClient())
        {
            ship.clientExists = true;
            ship.connected    = c->isConnected();
            if (auto *h = c->getRabbitMQHandler())
                ship.hasConsumers = h->hasCommandQueueConsumers();
            ship.commandAvailable = isCommandAvailable(
                ship.connected, ship.hasConsumers);
        }
        qCDebug(lcScenario) << "ServerStatusProbe::pollAll: ShipNetSim"
                            << "connected=" << ship.connected
                            << "hasConsumers=" << ship.hasConsumers;

        if (auto *tm = controller.getTruckManager())
        {
            const bool hasConcreteClients =
                !tm->getAllClientNames().isEmpty();
            truck.clientExists = hasConcreteClients;
            if (hasConcreteClients)
            {
                truck.connected    = tm->isConnected();
                truck.hasConsumers = tm->hasCommandQueueConsumers();
                truck.commandAvailable = isCommandAvailable(
                    truck.connected, truck.hasConsumers);
            }
        }
        qCDebug(lcScenario) << "ServerStatusProbe::pollAll: INTEGRATION"
                            << "connected=" << truck.connected
                            << "hasConsumers=" << truck.hasConsumers;
    }
    catch (const std::exception &e)
    {
        qCWarning(lcScenario) << "ServerStatusProbe::pollAll exception:" << e.what();
    }

    results << terminal << train << ship << truck;

    int connectedCount = (terminal.connected ? 1 : 0)
                       + (train.connected    ? 1 : 0)
                       + (ship.connected     ? 1 : 0)
                       + (truck.connected    ? 1 : 0);
    qCDebug(lcScenario) << "ServerStatusProbe::pollAll: complete,"
                        << connectedCount << "of 4 servers connected";
    return results;
}

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
