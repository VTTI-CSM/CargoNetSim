#include "ScenarioExecutor.h"

#include <exception>

#include "Backend/Commons/LogCategories.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Controllers/ConfigController.h"
#include "Backend/Controllers/RegionDataController.h"
#include "Backend/Controllers/VehicleController.h"
#include "ContainerAllocator.h"
#include "ResultsExtractor.h"
#include "ScenarioDocument.h"
#include "ScenarioRegistry.h"
#include "SimulationOrchestrator.h"
#include "SimulationRequestBuilder.h"

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

ScenarioExecutor::ScenarioExecutor(QObject *parent)
    : QObject(parent)
{
}

void ScenarioExecutor::setDocument(const ScenarioDocument *document)
{
    m_document = document;
}

void ScenarioExecutor::setRegistry(const ScenarioRegistry *registry)
{
    m_registry = registry;
}

void ScenarioExecutor::setPaths(
    const QList<CargoNetSim::Backend::Path *> &paths)
{
    m_paths = paths;
}

bool ScenarioExecutor::validateInputs(QString *err)
{
    qCDebug(lcScenario) << "ScenarioExecutor::validateInputs: entry";
    if (!m_document)
    {
        if (err)
            *err = QStringLiteral("Scenario document not set");
        qCWarning(lcScenario) << "ScenarioExecutor::validateInputs: document not set";
        return false;
    }
    if (!m_registry)
    {
        if (err)
            *err = QStringLiteral("Scenario registry not set");
        qCWarning(lcScenario) << "ScenarioExecutor::validateInputs: registry not set";
        return false;
    }
    if (m_document->originContainers().isEmpty())
    {
        if (err)
            *err = QStringLiteral(
                "No containers in the origin terminal");
        qCWarning(lcScenario) << "ScenarioExecutor::validateInputs: no containers in origin";
        return false;
    }
    if (m_paths.isEmpty())
    {
        if (err)
            *err =
                QStringLiteral("No paths selected for simulation");
        qCWarning(lcScenario) << "ScenarioExecutor::validateInputs: no paths selected";
        return false;
    }
    qCDebug(lcScenario) << "ScenarioExecutor::validateInputs: passed, paths:" << m_paths.size();
    return true;
}

bool ScenarioExecutor::run()
{
    qCInfo(lcScenario) << "ScenarioExecutor::run: started";
    m_results.clear();

    QString err;
    try
    {
        if (!validateInputs(&err))
        {
            qCWarning(lcScenario) << "ScenarioExecutor::run: validation failed:" << err;
            emit errorMessage(err);
            emit finished();
            return false;
        }

        auto &controller =
            CargoNetSim::CargoNetSimController::getInstance();
        auto *config     = controller.getConfigController();
        auto *vehicles   = controller.getVehicleController();
        auto *regionData = controller.getRegionDataController();

        // Build per-path simulation bundles
        qCDebug(lcScenario) << "ScenarioExecutor::run: building request bundles";
        SimulationRequestBuilder builder(
            *m_document, *m_registry, config, regionData, vehicles);

        SimulationRequestBundle bundle;
        const auto allocation =
            ContainerAllocator::allocate(*m_document, m_paths);
        if (!builder.build(m_paths, allocation, bundle, &err))
        {
            qCWarning(lcScenario) << "ScenarioExecutor::run: build failed:" << err;
            emit errorMessage(err);
            emit finished();
            return false;
        }
        qCDebug(lcScenario) << "ScenarioExecutor::run: bundles built successfully";

        // Submit to the three RabbitMQ clients. Relay status/error
        // signals through ourselves so subscribers see one consistent
        // signal stream from the executor.
        qCDebug(lcScenario) << "ScenarioExecutor::run: submitting to orchestrator";
        SimulationOrchestrator orch(controller.getShipClient(),
                                    controller.getTrainClient(),
                                    controller.getTruckManager(),
                                    this);
        connect(&orch, &SimulationOrchestrator::statusMessage,
                this, &ScenarioExecutor::statusMessage);
        connect(&orch, &SimulationOrchestrator::errorMessage,
                this, &ScenarioExecutor::errorMessage);
        if (!orch.submit(bundle, &err))
        {
            qCWarning(lcScenario) << "ScenarioExecutor::run: orchestrator submit failed:" << err;
            emit finished();
            return false;
        }
        qCDebug(lcScenario) << "ScenarioExecutor::run: orchestrator submit succeeded";

        // Extract per-path results from simulator state.
        qCDebug(lcScenario) << "ScenarioExecutor::run: extracting results";
        ResultsExtractor extractor(controller.getShipClient(),
                                   controller.getTrainClient(),
                                   controller.getTruckManager(),
                                   config, this);
        connect(&extractor, &ResultsExtractor::statusMessage,
                this, &ScenarioExecutor::statusMessage);
        connect(&extractor, &ResultsExtractor::errorMessage,
                this, &ScenarioExecutor::errorMessage);

        const int containerCount =
            m_document->originContainers().size();
        m_results = extractor.extract(m_paths, containerCount);
        qCDebug(lcScenario) << "ScenarioExecutor::run: extracted" << m_results.size() << "path results";
        for (const auto &r : m_results)
            emit pathResultReady(r);

        emit statusMessage(QStringLiteral(
            "Simulation validation completed successfully"));
    }
    catch (const std::exception &e)
    {
        qCCritical(lcScenario) << "ScenarioExecutor::run: exception:" << e.what();
        emit errorMessage(
            QString("Error during simulation: %1").arg(e.what()));
        emit finished();
        return false;
    }
    catch (...)
    {
        qCCritical(lcScenario) << "ScenarioExecutor::run: unknown exception";
        emit errorMessage(
            QStringLiteral("Unknown error during simulation"));
        emit finished();
        return false;
    }

    qCInfo(lcScenario) << "ScenarioExecutor::run: completed successfully";
    emit finished();
    return true;
}

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
