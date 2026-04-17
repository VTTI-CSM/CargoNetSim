#include "ScenarioRuntime.h"

#include <QMetaObject>
#include <QThread>

#include "Backend/Commons/LogCategories.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "ScenarioApplier.h"
#include "ScenarioDocument.h"
#include "ScenarioExecutor.h"
#include "ScenarioValidator.h"
#include "ValidationIssue.h"

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

ScenarioRuntime::ScenarioRuntime(
    std::unique_ptr<ScenarioDocument> doc, QObject *parent)
    : QObject(parent)
    , m_document(std::move(doc))
{
    // Relay CargoNetSimController progress/completion signals through
    // this runtime so consumers subscribe to one source of truth.
    auto &controller =
        CargoNetSim::CargoNetSimController::getInstance();
    connect(&controller,
            &CargoNetSim::CargoNetSimController::
                simulationStepCompleted,
            this, &ScenarioRuntime::onStepCompleted);
    connect(&controller,
            &CargoNetSim::CargoNetSimController::
                simulationCompleted,
            this, &ScenarioRuntime::completed);
}

ScenarioRuntime::~ScenarioRuntime()
{
    if (m_workerThread)
    {
        m_workerThread->quit();
        m_workerThread->wait();
    }
}

bool ScenarioRuntime::load()
{
    qCInfo(lcScenario) << "ScenarioRuntime::load: entry";
    if (!m_document)
    {
        qCWarning(lcScenario) << "ScenarioRuntime::load: no document to load";
        emit failed(QStringLiteral("No document to load"));
        return false;
    }

    // Plan-deviation: ScenarioValidator::validate and
    // ScenarioApplier::apply are static — call directly, no instance.
    qCDebug(lcScenario) << "ScenarioRuntime::load: validating document";
    const auto issues = ScenarioValidator::validate(*m_document);
    qCDebug(lcScenario) << "ScenarioRuntime::load: validation returned" << issues.size() << "issues";
    for (const auto &iss : issues)
    {
        if (iss.severity == ValidationIssue::Error)
        {
            qCWarning(lcScenario) << "ScenarioRuntime::load: validation error:" << iss.message;
            emit failed(QString("Validation error: %1")
                            .arg(iss.message));
            return false;
        }
    }

    qCDebug(lcScenario) << "ScenarioRuntime::load: applying scenario to controller";
    auto &controller =
        CargoNetSim::CargoNetSimController::getInstance();
    QString err;
    if (!ScenarioApplier::apply(*m_document, controller,
                                m_registry, &err))
    {
        qCWarning(lcScenario) << "ScenarioRuntime::load: apply failed:" << err;
        emit failed(QString("Apply failed: %1").arg(err));
        return false;
    }

    m_endTime = m_document->simulation.endTime;
    controller.setSimulationEndTime(m_endTime);
    m_loaded = true;
    qCInfo(lcScenario) << "ScenarioRuntime::load: succeeded, endTime:" << m_endTime;
    return true;
}

void ScenarioRuntime::setPaths(
    const QList<CargoNetSim::Backend::Path *> &paths)
{
    m_paths = paths;
    qCDebug(lcScenario) << "ScenarioRuntime::setPaths: set" << m_paths.size() << "paths";
}

bool ScenarioRuntime::startSimulation()
{
    qCInfo(lcScenario) << "ScenarioRuntime::startSimulation: entry";
    if (!m_loaded)
    {
        qCWarning(lcScenario) << "ScenarioRuntime::startSimulation: runtime not loaded";
        emit failed(QStringLiteral(
            "Runtime not loaded; call load() first"));
        return false;
    }
    if (m_workerThread)
    {
        qCWarning(lcScenario) << "ScenarioRuntime::startSimulation: already running";
        emit failed(
            QStringLiteral("Simulation already running"));
        return false;
    }

    m_workerThread = new QThread(this);
    m_executor     = new ScenarioExecutor();

    // Plan-deviation: Task 23 decoupled the executor from the runtime.
    // Inject inputs directly instead of setContext(this).
    m_executor->setDocument(m_document.get());
    m_executor->setRegistry(&m_registry);
    m_executor->setPaths(m_paths);

    m_executor->moveToThread(m_workerThread);

    connect(m_workerThread, &QThread::started, m_executor,
            [this] { m_executor->run(); });
    connect(m_executor, &ScenarioExecutor::statusMessage,
            this, &ScenarioRuntime::statusMessage);
    connect(m_executor, &ScenarioExecutor::errorMessage,
            this, &ScenarioRuntime::errorMessage);
    connect(m_executor, &ScenarioExecutor::finished, this,
            &ScenarioRuntime::onExecutorFinished);

    m_workerThread->start();
    return true;
}

void ScenarioRuntime::onStepCompleted(double currentTime,
                                      double progress)
{
    // Log only on significant progress jumps (>=5%) to avoid spam
    if (progress - m_lastProgress >= 5.0 || progress >= 100.0)
        qCDebug(lcScenario) << "ScenarioRuntime::onStepCompleted: time:" << currentTime
                            << "progress:" << progress << "%";
    m_lastTime     = currentTime;
    m_lastProgress = progress;
    emit progressChanged(currentTime, progress);
}

void ScenarioRuntime::onExecutorFinished()
{
    qCInfo(lcScenario) << "ScenarioRuntime::onExecutorFinished: executor done";
    if (m_executor)
        m_lastResults = m_executor->results();
    qCDebug(lcScenario) << "ScenarioRuntime::onExecutorFinished: collected"
                        << m_lastResults.size() << "results";
    if (m_workerThread)
    {
        m_workerThread->quit();
        m_workerThread->wait();
        m_workerThread->deleteLater();
        m_workerThread = nullptr;
    }
    if (m_executor)
    {
        m_executor->deleteLater();
        m_executor = nullptr;
    }
    emit completed();
}

void ScenarioRuntime::stop()
{
    qCInfo(lcScenario) << "ScenarioRuntime::stop: requested";
    CargoNetSim::CargoNetSimController::getInstance()
        .stopSimulation();
}

void ScenarioRuntime::pause()
{
    qCInfo(lcScenario) << "ScenarioRuntime::pause: requested";
    CargoNetSim::CargoNetSimController::getInstance()
        .pauseSimulation();
}

void ScenarioRuntime::resume()
{
    qCInfo(lcScenario) << "ScenarioRuntime::resume: requested";
    CargoNetSim::CargoNetSimController::getInstance()
        .resumeSimulation();
}

double ScenarioRuntime::currentTime() const
{
    return m_lastTime;
}

double ScenarioRuntime::progress() const
{
    return m_lastProgress;
}

bool ScenarioRuntime::isRunning() const
{
    return m_workerThread && m_workerThread->isRunning();
}

const ScenarioDocument &ScenarioRuntime::document() const
{
    return *m_document;
}

ScenarioDocument &ScenarioRuntime::document()
{
    return *m_document;
}

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
