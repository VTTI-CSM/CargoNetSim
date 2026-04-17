#include "ServerStatusProbe.h"

#include "Backend/Clients/BaseClient/RabbitMQHandler.h"
#include "Backend/Clients/ShipClient/ShipSimulationClient.h"
#include "Backend/Clients/TerminalClient/TerminalSimulationClient.h"
#include "Backend/Clients/TrainClient/TrainSimulationClient.h"
#include "Backend/Clients/TruckClient/TruckSimulationManager.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Controllers/CargoNetSimController.h"

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

    try
    {
        auto &controller =
            CargoNetSim::CargoNetSimController::getInstance();

        if (auto *c = controller.getTerminalClient())
        {
            terminal.clientExists = true;
            terminal.connected    = c->isConnected();
            if (auto *h = c->getRabbitMQHandler())
                terminal.hasConsumers = h->hasCommandQueueConsumers();
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
        }
        qCDebug(lcScenario) << "ServerStatusProbe::pollAll: ShipNetSim"
                            << "connected=" << ship.connected
                            << "hasConsumers=" << ship.hasConsumers;

        if (auto *tm = controller.getTruckManager())
        {
            truck.clientExists = true;
            // Truck manager aggregates per-network clients; "connected"
            // maps to "any command queue consumer active" which is also
            // the truck-manager-level check used today.
            truck.hasConsumers = tm->hasCommandQueueConsumers();
            truck.connected    = truck.hasConsumers;
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
