#include "BackendBootstrapService.h"

#include "Backend/Clients/BaseClient/RabbitMQHandler.h"
#include "Backend/Clients/BaseClient/SimulationClientBase.h"
#include "Backend/Clients/ShipClient/ShipSimulationClient.h"
#include "Backend/Clients/ShipClient/ShipState.h"
#include "Backend/Clients/ShipClient/SimulationResults.h"
#include "Backend/Clients/ShipClient/SimulationSummaryData.h"
#include "Backend/Clients/TerminalClient/TerminalSimulationClient.h"
#include "Backend/Clients/TrainClient/SimulationResults.h"
#include "Backend/Clients/TrainClient/SimulationSummaryData.h"
#include "Backend/Clients/TrainClient/TrainNetwork.h"
#include "Backend/Clients/TrainClient/TrainSimulationClient.h"
#include "Backend/Clients/TrainClient/TrainState.h"
#include "Backend/Clients/TruckClient/IntegrationLink.h"
#include "Backend/Clients/TruckClient/IntegrationNode.h"
#include "Backend/Clients/TruckClient/SimulationResults.h"
#include "Backend/Clients/TruckClient/SimulationSummaryData.h"
#include "Backend/Clients/TruckClient/TruckNetwork.h"
#include "Backend/Clients/TruckClient/TruckSimulationClient.h"
#include "Backend/Clients/TruckClient/TruckSimulationManager.h"
#include "Backend/Clients/TruckClient/TruckState.h"
#include "Backend/Commons/ClientType.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Models/SimulationTime.h"

#include <QtCore>

namespace CargoNetSim
{
namespace Backend
{

namespace
{

void registerAllMetatypes()
{
    qRegisterMetaType<ContainerCore::Package>(
        "ContainerCore::Package");
    qRegisterMetaType<ContainerCore::Package *>(
        "ContainerCore::Package *");
    qRegisterMetaType<ContainerCore::Container>(
        "ContainerCore::Container");
    qRegisterMetaType<ContainerCore::Container *>(
        "ContainerCore::Container*");
    qRegisterMetaType<QList<ContainerCore::Container>>(
        "QList<ContainerCore::Container>");
    qRegisterMetaType<QList<ContainerCore::Container *>>(
        "QList<ContainerCore::Container*>");

    qRegisterMetaType<RabbitMQHandler>(
        "CargoNetSim::Backend::RabbitMQHandler");
    qRegisterMetaType<RabbitMQHandler *>(
        "CargoNetSim::Backend::RabbitMQHandler*");
    qRegisterMetaType<SimulationClientBase>(
        "CargoNetSim::Backend::SimulationClientBase");
    qRegisterMetaType<SimulationClientBase *>(
        "CargoNetSim::Backend::SimulationClientBase*");

    qRegisterMetaType<ShipClient::ShipState>(
        "CargoNetSim::Backend::ShipClient::ShipState");
    qRegisterMetaType<ShipClient::ShipState *>(
        "CargoNetSim::Backend::ShipClient::ShipState*");
    qRegisterMetaType<ShipClient::SimulationSummaryData>(
        "CargoNetSim::Backend::ShipClient::SimulationSummaryData");
    qRegisterMetaType<ShipClient::SimulationSummaryData *>(
        "CargoNetSim::Backend::ShipClient::SimulationSummaryData*");
    qRegisterMetaType<ShipClient::SimulationResults>(
        "CargoNetSim::Backend::ShipClient::SimulationResults");
    qRegisterMetaType<ShipClient::SimulationResults *>(
        "CargoNetSim::Backend::ShipClient::SimulationResults*");
    qRegisterMetaType<ShipClient::ShipSimulationClient>(
        "CargoNetSim::Backend::ShipClient::ShipSimulationClient");
    qRegisterMetaType<ShipClient::ShipSimulationClient *>(
        "CargoNetSim::Backend::ShipClient::ShipSimulationClient*");

    qRegisterMetaType<CargoNetSim::Backend::Terminal>(
        "CargoNetSim::Backend::Terminal");
    qRegisterMetaType<CargoNetSim::Backend::Terminal *>(
        "CargoNetSim::Backend::Terminal*");
    qRegisterMetaType<CargoNetSim::Backend::PathSegment>(
        "CargoNetSim::Backend::PathSegment");
    qRegisterMetaType<CargoNetSim::Backend::PathSegment *>(
        "CargoNetSim::Backend::PathSegment*");
    qRegisterMetaType<CargoNetSim::Backend::Path>(
        "CargoNetSim::Backend::Path");
    qRegisterMetaType<CargoNetSim::Backend::Path *>(
        "CargoNetSim::Backend::Path*");
    qRegisterMetaType<CargoNetSim::Backend::TerminalSimulationClient>(
        "CargoNetSim::Backend::TerminalSimulationClient");
    qRegisterMetaType<CargoNetSim::Backend::TerminalSimulationClient *>(
        "CargoNetSim::Backend::TerminalSimulationClient *");

    qRegisterMetaType<TrainClient::TrainState>(
        "CargoNetSim::Backend::TrainClient::TrainState");
    qRegisterMetaType<TrainClient::TrainState *>(
        "CargoNetSim::Backend::TrainClient::TrainState*");
    qRegisterMetaType<TrainClient::SimulationSummaryData>(
        "CargoNetSim::Backend::TrainClient::SimulationSummaryData");
    qRegisterMetaType<TrainClient::SimulationSummaryData *>(
        "CargoNetSim::Backend::TrainClient::SimulationSummaryData*");
    qRegisterMetaType<TrainClient::SimulationResults>(
        "CargoNetSim::Backend::TrainClient::SimulationResults");
    qRegisterMetaType<TrainClient::SimulationResults *>(
        "CargoNetSim::Backend::TrainClient::SimulationResults*");
    qRegisterMetaType<TrainClient::TrainSimulationClient>(
        "CargoNetSim::Backend::TrainClient::TrainSimulationClient");
    qRegisterMetaType<TrainClient::TrainSimulationClient *>(
        "CargoNetSim::Backend::TrainClient::TrainSimulationClient*");
    qRegisterMetaType<TrainClient::NeTrainSimNetwork>(
        "CargoNetSim::Backend::TrainClient::NeTrainSimNetwork");
    qRegisterMetaType<TrainClient::NeTrainSimNetwork>(
        "CargoNetSim::Backend::TrainClient::NeTrainSimNetwork*");

    qRegisterMetaType<CargoNetSim::Backend::TruckClient::AsyncTripManager>(
        "CargoNetSim::Backend::TruckClient::AsyncTripManager");
    qRegisterMetaType<
        CargoNetSim::Backend::TruckClient::AsyncTripManager *>(
        "CargoNetSim::Backend::TruckClient::AsyncTripManager*");
    qRegisterMetaType<CargoNetSim::Backend::TruckClient::ContainerManager>(
        "CargoNetSim::Backend::TruckClient::ContainerManager");
    qRegisterMetaType<
        CargoNetSim::Backend::TruckClient::ContainerManager *>(
        "CargoNetSim::Backend::TruckClient::ContainerManager*");
    qRegisterMetaType<CargoNetSim::Backend::TruckClient::IntegrationNode>(
        "CargoNetSim::Backend::TruckClient::IntegrationNode");
    qRegisterMetaType<
        CargoNetSim::Backend::TruckClient::IntegrationNode *>(
        "CargoNetSim::Backend::TruckClient::IntegrationNode*");
    qRegisterMetaType<CargoNetSim::Backend::TruckClient::IntegrationLink>(
        "CargoNetSim::Backend::TruckClient::IntegrationLink");
    qRegisterMetaType<
        CargoNetSim::Backend::TruckClient::IntegrationLink *>(
        "CargoNetSim::Backend::TruckClient::IntegrationLink*");
    qRegisterMetaType<CargoNetSim::Backend::TruckClient::MessageFormatter>(
        "CargoNetSim::Backend::TruckClient::MessageFormatter");
    qRegisterMetaType<
        CargoNetSim::Backend::TruckClient::MessageFormatter *>(
        "CargoNetSim::Backend::TruckClient::MessageFormatter*");
    qRegisterMetaType<CargoNetSim::Backend::TruckClient::TripEndCallbackManager>(
        "CargoNetSim::Backend::TruckClient::TripEndCallbackManager");
    qRegisterMetaType<
        CargoNetSim::Backend::TruckClient::TripEndCallbackManager *>(
        "CargoNetSim::Backend::TruckClient::TripEndCallbackManager*");
    qRegisterMetaType<CargoNetSim::Backend::TruckClient::IntegrationSimulationConfig>(
        "CargoNetSim::Backend::TruckClient::IntegrationSimulationConfig");
    qRegisterMetaType<
        CargoNetSim::Backend::TruckClient::IntegrationSimulationConfig *>(
        "CargoNetSim::Backend::TruckClient::IntegrationSimulationConfig*");
    qRegisterMetaType<CargoNetSim::Backend::TruckClient::TransportationGraph<int>>(
        "CargoNetSim::Backend::TruckClient::TransportationGraph<int>");
    qRegisterMetaType<
        CargoNetSim::Backend::TruckClient::TransportationGraph<int> *>(
        "CargoNetSim::Backend::TruckClient::TransportationGraph<int>*");
    qRegisterMetaType<CargoNetSim::Backend::TruckClient::IntegrationNetwork>(
        "CargoNetSim::Backend::TruckClient::IntegrationNetwork");
    qRegisterMetaType<CargoNetSim::Backend::TruckClient::IntegrationNetwork *>(
        "CargoNetSim::Backend::TruckClient::IntegrationNetwork");
    qRegisterMetaType<CargoNetSim::Backend::TruckClient::IntegrationSimulationConfig>(
        "CargoNetSim::Backend::TruckClient::IntegrationSimulationConfig");
    qRegisterMetaType<
        CargoNetSim::Backend::TruckClient::IntegrationSimulationConfig *>(
        "CargoNetSim::Backend::TruckClient::IntegrationSimulationConfig");
    qRegisterMetaType<CargoNetSim::Backend::TruckClient::TripRequest>(
        "CargoNetSim::Backend::TruckClient::TripRequest");
    qRegisterMetaType<CargoNetSim::Backend::TruckClient::TripRequest *>(
        "CargoNetSim::Backend::TruckClient::TripRequest*");
    qRegisterMetaType<CargoNetSim::Backend::TruckClient::TripResult>(
        "CargoNetSim::Backend::TruckClient::TripResult");
    qRegisterMetaType<CargoNetSim::Backend::TruckClient::TripResult *>(
        "CargoNetSim::Backend::TruckClient::TripResult*");
    qRegisterMetaType<TruckClient::TruckState>(
        "CargoNetSim::Backend::TruckClient::TruckState");
    qRegisterMetaType<TruckClient::TruckState *>(
        "CargoNetSim::Backend::TruckClient::TruckState*");
    qRegisterMetaType<TruckClient::SimulationSummaryData>(
        "CargoNetSim::Backend::TruckClient::SimulationSummaryData");
    qRegisterMetaType<TruckClient::SimulationSummaryData *>(
        "CargoNetSim::Backend::TruckClient::SimulationSummaryData*");
    qRegisterMetaType<TruckClient::SimulationResults>(
        "CargoNetSim::Backend::TruckClient::SimulationResults");
    qRegisterMetaType<TruckClient::SimulationResults *>(
        "CargoNetSim::Backend::TruckClient::SimulationResults*");
    qRegisterMetaType<TruckClient::TruckSimulationClient>(
        "CargoNetSim::Backend::TruckClient::TruckSimulationClient");
    qRegisterMetaType<TruckClient::TruckSimulationClient *>(
        "CargoNetSim::Backend::TruckClient::TruckSimulationClient*");
    qRegisterMetaType<TruckClient::TruckSimulationManager>(
        "CargoNetSim::Backend::TruckClient::TruckSimulationManager");
    qRegisterMetaType<TruckClient::TruckSimulationManager *>(
        "CargoNetSim::Backend::TruckClient::TruckSimulationManager*");
    qRegisterMetaType<CargoNetSim::Backend::TruckClient::IntegrationNodeDataReader>(
        "CargoNetSim::Backend::TruckClient::IntegrationNodeDataReader");
    qRegisterMetaType<
        CargoNetSim::Backend::TruckClient::IntegrationNodeDataReader *>(
        "CargoNetSim::Backend::TruckClient::IntegrationNodeDataReader*");
    qRegisterMetaType<CargoNetSim::Backend::TruckClient::IntegrationLinkDataReader>(
        "CargoNetSim::Backend::TruckClient::IntegrationLinkDataReader");
    qRegisterMetaType<
        CargoNetSim::Backend::TruckClient::IntegrationLinkDataReader *>(
        "CargoNetSim::Backend::TruckClient::IntegrationLinkDataReader*");
    qRegisterMetaType<
        CargoNetSim::Backend::TruckClient::IntegrationSimulationConfigReader>(
        "CargoNetSim::Backend::TruckClient::IntegrationSimulationConfigReader");
    qRegisterMetaType<
        CargoNetSim::Backend::TruckClient::IntegrationSimulationConfigReader *>(
        "CargoNetSim::Backend::TruckClient::IntegrationSimulationConfigReader*");

    qRegisterMetaType<TerminalSimulationClient>(
        "CargoNetSim::Backend::TerminalSimulationClient");
    qRegisterMetaType<ClientType>(
        "CargoNetSim::Backend::ClientType");
    qRegisterMetaType<CargoNetSim::Backend::ShortestPathResult>(
        "CargoNetSim::Backend::ShortestPathResult");
    qRegisterMetaType<CargoNetSim::Backend::ShortestPathResult *>(
        "CargoNetSim::Backend::ShortestPathResult*");
    qRegisterMetaType<CargoNetSim::Backend::SimulationTime>(
        "CargoNetSim::Backend::SimulationTime");
    qRegisterMetaType<CargoNetSim::Backend::SimulationTime *>(
        "CargoNetSim::Backend::SimulationTime*");
}

BackendBootstrapResult requireController()
{
    BackendBootstrapResult result;
    if (CargoNetSim::CargoNetSimController::instance() == nullptr)
    {
        result.status = BackendBootstrapStatus::ControllerMissing;
        result.message = QStringLiteral(
            "CargoNetSimController must be constructed before backend bootstrap");
    }
    return result;
}

} // namespace

void BackendBootstrapService::registerMetatypesOnce()
{
    static bool metatypesRegistered = false;
    if (metatypesRegistered)
        return;

    registerAllMetatypes();
    qCInfo(lcInit) << "Backend metatypes registered successfully";
    metatypesRegistered = true;
}

BackendBootstrapResult BackendBootstrapService::registerOnly() const
{
    registerMetatypesOnce();
    return {};
}

BackendBootstrapResult
BackendBootstrapService::initializeController(
    const QString &integrationExePath) const
{
    registerMetatypesOnce();

    auto controllerCheck = requireController();
    if (!controllerCheck.succeeded())
        return controllerCheck;

    auto &controller = CargoNetSim::CargoNetSimController::getInstance();
    if (!controller.initialize(integrationExePath))
    {
        BackendBootstrapResult result;
        result.status = BackendBootstrapStatus::InitializeFailed;
        result.message = QStringLiteral(
            "CargoNetSimController::initialize failed");
        return result;
    }

    return {};
}

BackendBootstrapResult BackendBootstrapService::startController() const
{
    registerMetatypesOnce();

    auto controllerCheck = requireController();
    if (!controllerCheck.succeeded())
        return controllerCheck;

    auto &controller = CargoNetSim::CargoNetSimController::getInstance();
    if (!controller.startAll())
    {
        BackendBootstrapResult result;
        result.status = BackendBootstrapStatus::StartFailed;
        result.message = QStringLiteral(
            "CargoNetSimController::startAll failed");
        return result;
    }

    const auto logClientStatus = [](const char *label,
                                    CargoNetSim::Backend::SimulationClientBase *client) {
        if (client == nullptr)
        {
            qCDebug(lcInit)
                << "BackendBootstrapService::startController:"
                << label << "client missing immediately after startAll";
            return;
        }

        auto *handler = client->getRabbitMQHandler();
        qCDebug(lcInit)
            << "BackendBootstrapService::startController:"
            << label
            << "thread=" << client->thread()
            << "handler=" << handler
            << "clientConnected=" << client->isConnected()
            << "handlerConnected="
            << (handler ? handler->isConnected() : false);
    };
    logClientStatus("terminal", controller.getTerminalClient());
    logClientStatus("train", controller.getTrainClient());
    logClientStatus("ship", controller.getShipClient());
    qCDebug(lcInit)
        << "BackendBootstrapService::startController:"
        << "truckManager="
        << controller.getTruckManager()
        << "thread="
        << (controller.getTruckManager()
                ? controller.getTruckManager()->thread()
                : nullptr);

    return {};
}

BackendBootstrapResult
BackendBootstrapService::initializeAndStartController(
    const QString &integrationExePath) const
{
    auto initResult = initializeController(integrationExePath);
    if (!initResult.succeeded())
        return initResult;
    return startController();
}

} // namespace Backend
} // namespace CargoNetSim
