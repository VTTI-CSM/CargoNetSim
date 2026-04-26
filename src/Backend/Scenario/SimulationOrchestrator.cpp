#include "SimulationOrchestrator.h"
#include "SimulationRequestBuilder.h"

#include "Backend/Commons/LogCategories.h"
#include "Backend/Commons/Units.h"
#include "Backend/Clients/BaseClient/RabbitMQHandler.h"
#include "Backend/Clients/BaseClient/SimulationClientBase.h"
#include "Backend/Clients/ShipClient/ShipSimulationClient.h"
#include "Backend/Clients/TrainClient/TrainSimulationClient.h"
#include "Backend/Clients/TruckClient/TruckSimulationClient.h"
#include "Backend/Clients/TruckClient/TruckSimulationManager.h"
#include "Backend/Models/ShipSystem.h"
#include "Backend/Models/TrainSystem.h"
#include "Backend/Scenario/SimulatorCommandAvailability.h"

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

SimulationOrchestrator::SimulationOrchestrator(
    CargoNetSim::Backend::ShipClient::ShipSimulationClient    *shipClient,
    CargoNetSim::Backend::TrainClient::TrainSimulationClient  *trainClient,
    CargoNetSim::Backend::TruckClient::TruckSimulationManager *truckManager,
    QObject                                                   *parent)
    : QObject(parent)
    , m_shipClient(shipClient)
    , m_trainClient(trainClient)
    , m_truckManager(truckManager)
{
}

bool SimulationOrchestrator::submit(const SimulationRequestBundle &bundle,
                                    QString                       *err)
{
    qCInfo(lcScenario) << "SimulationOrchestrator::submit: entry"
                       << "trainNetworks:" << bundle.trainData.size()
                       << "shipNetworks:" << bundle.shipData.size()
                       << "truckNetworks:" << bundle.truckData.size();
    qCDebug(lcScenario) << "SimulationOrchestrator::submit: submitting trains";
    if (!submitTrain(bundle, err))
    {
        qCCritical(lcScenario) << "SimulationOrchestrator::submit: train submission failed";
        return false;
    }
    qCDebug(lcScenario) << "SimulationOrchestrator::submit: submitting ships";
    if (!submitShip(bundle, err))
    {
        qCCritical(lcScenario) << "SimulationOrchestrator::submit: ship submission failed";
        return false;
    }
    qCDebug(lcScenario) << "SimulationOrchestrator::submit: submitting trucks";
    if (!submitTruck(bundle, err))
    {
        qCCritical(lcScenario) << "SimulationOrchestrator::submit: truck submission failed";
        return false;
    }
    qCInfo(lcScenario) << "SimulationOrchestrator::submit: all modes submitted successfully";
    emit statusMessage(
        QStringLiteral("All simulations started successfully!"));
    return true;
}

bool SimulationOrchestrator::emitAndFail(const QString &msg, QString *err)
{
    if (err)
        *err = msg;
    emit errorMessage(msg);
    return false;
}

bool SimulationOrchestrator::ensureClientConnected(
    CargoNetSim::Backend::SimulationClientBase *client,
    const QString                              &kind,
    QString                                    *err)
{
    qCDebug(lcScenario) << "SimulationOrchestrator::ensureClientConnected:" << kind;
    if (!client)
    {
        qCWarning(lcScenario) << "SimulationOrchestrator::ensureClientConnected:"
                              << kind << "client is null";
        return emitAndFail(
            kind + QStringLiteral(" client is not available"), err);
    }
    if (!isCommandAvailable(client))
    {
        qCWarning(lcScenario) << "SimulationOrchestrator::ensureClientConnected:"
                              << kind << "command queue is unavailable";
        return emitAndFail(
            kind
                + QStringLiteral(
                    " client command queue is unavailable"),
            err);
    }
    return true;
}

// Mode-specific submission — ship + truck stubs remain until Tasks 12–13.
bool SimulationOrchestrator::submitTrain(
    const SimulationRequestBundle &bundle, QString *err)
{
    qCDebug(lcScenario) << "SimulationOrchestrator::submitTrain: entry, networks:"
                        << bundle.trainData.size();
    if (bundle.trainData.isEmpty())
        return true;

    if (!ensureClientConnected(m_trainClient,
                               QStringLiteral("Train"), err))
        return false;

    m_trainClient->resetServer();
    emit statusMessage(
        QStringLiteral("Setting up train simulations..."));

    for (auto it = bundle.trainData.constBegin();
         it != bundle.trainData.constEnd(); ++it)
    {
        const QString networkName = it.key();
        auto *trainNetwork =
            bundle.trainNetworks.value(networkName, nullptr);
        const auto &trainDataList = it.value();

        QList<CargoNetSim::Backend::Train *> trains;
        for (const auto &td : trainDataList)
            trains.append(td.train);

        m_trainClient->defineSimulator(trainNetwork, 1.0, trains);

        for (const auto &td : trainDataList)
        {
            if (!td.containers.isEmpty())
            {
                m_trainClient->addContainersToTrain(
                    networkName, td.train->getUserId(),
                    td.containers);
            }
        }
    }

    emit statusMessage(
        QStringLiteral("Running train simulations..."));
    for (auto it = bundle.trainData.constBegin();
         it != bundle.trainData.constEnd(); ++it)
    {
        m_trainClient->runSimulator({it.key()});
    }
    return true;
}

bool SimulationOrchestrator::submitShip(
    const SimulationRequestBundle &bundle, QString *err)
{
    qCDebug(lcScenario) << "SimulationOrchestrator::submitShip: entry, networks:"
                        << bundle.shipData.size();
    if (bundle.shipData.isEmpty())
        return true;

    if (!ensureClientConnected(m_shipClient,
                               QStringLiteral("Ship"), err))
        return false;

    m_shipClient->resetServer();
    emit statusMessage(
        QStringLiteral("Setting up ship simulations..."));

    for (auto it = bundle.shipData.constBegin();
         it != bundle.shipData.constEnd(); ++it)
    {
        const QString networkName  = it.key();
        const auto   &shipDataList = it.value();

        QList<CargoNetSim::Backend::Ship *> ships;
        QMap<QString, QStringList>          destinationTerminals;
        for (const auto &sd : shipDataList)
        {
            ships.append(sd.ship);
            destinationTerminals[sd.ship->getUserId()] =
                QStringList{sd.destinationTerminal};
        }

        // Empty networkPath → ShipNetSim uses its default world (spec §5.3).
        m_shipClient->defineSimulator(networkName, 1.0, ships,
                                      destinationTerminals,
                                      QString());

        for (const auto &sd : shipDataList)
        {
            if (!sd.containers.isEmpty())
            {
                m_shipClient->addContainersToShip(
                    networkName, sd.ship->getUserId(),
                    sd.containers);
            }
        }
    }

    emit statusMessage(
        QStringLiteral("Running ship simulations..."));
    for (auto it = bundle.shipData.constBegin();
         it != bundle.shipData.constEnd(); ++it)
    {
        m_shipClient->runSimulator({it.key()});
    }
    return true;
}

bool SimulationOrchestrator::submitTruck(
    const SimulationRequestBundle &bundle, QString *err)
{
    qCDebug(lcScenario) << "SimulationOrchestrator::submitTruck: entry, networks:"
                        << bundle.truckData.size();
    if (bundle.truckData.isEmpty())
        return true;

    if (!m_truckManager)
        return emitAndFail(
            QStringLiteral("Truck manager is not available"), err);

    m_truckManager->resetServer();
    emit statusMessage(
        QStringLiteral("Setting up truck simulations..."));

    for (auto it = bundle.truckData.constBegin();
         it != bundle.truckData.constEnd(); ++it)
    {
        const QString networkName  = it.key();
        const auto   &truckDataList = it.value();

        // masterFilePath left empty here — executor (Task 23) injects
        // the path from the scenario's truck fleet spec before calling
        // submit(). Same gap as SimulationValidationWorker preserves.
        CargoNetSim::Backend::TruckClient::ClientConfiguration config;
        config.simTime =
            Units::toSeconds(Units::hours(1.0)).value();
        m_truckManager->createClient(networkName, config);

        auto *client = m_truckManager->getClient(networkName);
        if (!client)
        {
            qCWarning(lcScenario) << "SimulationOrchestrator::submitTruck:"
                                  << "failed to create client for" << networkName;
            return emitAndFail(
                QString("Failed to create truck client for network %1")
                    .arg(networkName),
                err);
        }

        for (const auto &td : truckDataList)
        {
            client->addTrip(
                networkName,
                QString::number(td.originNode),
                QString::number(td.destinationNode),
                td.containers);
        }
    }

    emit statusMessage(
        QStringLiteral("Running truck simulations..."));
    m_truckManager->runSimulationAsync(bundle.truckData.keys());
    return true;
}

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
