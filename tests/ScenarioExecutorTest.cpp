#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QSignalSpy>
#include <QTest>

#include "Backend/Commons/TransportationMode.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Models/Path.h"
#include "Backend/Models/PathSegment.h"
#include "Backend/Scenario/PathPreparationService.h"
#include "Backend/Scenario/PathSimulationResult.h"
#include "Backend/Scenario/ScenarioExecutor.h"
#include "Backend/Scenario/ScenarioRuntime.h"
#include "Backend/Scenario/ScenarioSerializer.h"

class ScenarioExecutorTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        // Tier 1: construct the controller explicitly for this test
        // binary. Parent to QCoreApplication::instance() so Qt cleans
        // it up at the end of the binary's life. Guarded so repeated
        // initTestCase calls in the same binary are safe.
        if (!CargoNetSim::CargoNetSimController::instance())
        {
            new CargoNetSim::CargoNetSimController(
                /*logger=*/nullptr, QCoreApplication::instance());
        }
    }

    void test_path_simulation_result_defaults()
    {
        CargoNetSim::Backend::Scenario::PathSimulationResult r;
        QCOMPARE(r.pathId, -1);
        QCOMPARE(r.totalCost, 0.0);
        QCOMPARE(r.edgeCosts, 0.0);
        QCOMPARE(r.terminalCosts, 0.0);
    }

    void test_run_without_inputs_emits_error_and_finishes()
    {
        // No document/registry/paths set → run() must fail fast,
        // emit an errorMessage describing the missing input, and
        // emit finished() exactly once. Result list stays empty.
        CargoNetSim::Backend::Scenario::ScenarioExecutor ex;
        QSignalSpy errSpy(
            &ex,
            &CargoNetSim::Backend::Scenario::ScenarioExecutor::errorMessage);
        QSignalSpy finSpy(
            &ex,
            &CargoNetSim::Backend::Scenario::ScenarioExecutor::finished);
        QVERIFY(!ex.run());
        QCOMPARE(errSpy.count(), 1);
        QCOMPARE(finSpy.count(), 1);
        QVERIFY(ex.results().isEmpty());
    }

    void test_e2e_executor_runs_full_pipeline_against_live_backend()
    {
        // Integration-level test. Guarded so developers without the
        // three simulator servers running can skip (same pattern as
        // TerminalClientTest). Set CARGONETSIM_INTEGRATION=1 plus
        // spin up TerminalSim, NeTrainSim, ShipNetSim before running.
        if (qEnvironmentVariable("CARGONETSIM_INTEGRATION").isEmpty())
            QSKIP("Set CARGONETSIM_INTEGRATION=1 and run the three "
                  "simulator servers to execute this test.");

        using namespace CargoNetSim::Backend;
        using namespace CargoNetSim::Backend::Scenario;

        // 1. Initialize the singleton controller (brings up clients).
        auto &controller =
            CargoNetSim::CargoNetSimController::getInstance();
        QVERIFY(controller.initialize(/*truckExePath=*/""));
        QVERIFY(controller.startAll());

        // 2. Load YAML → document (serializer is static; see plan
        //    deviation).
        const QString yamlPath =
            QDir(QCoreApplication::applicationDirPath())
                .filePath("fixtures/executor/executor_scenario.yml");
        QString loadErr;
        auto doc = ScenarioSerializer::fromYaml(yamlPath, &loadErr);
        QVERIFY2(doc != nullptr, qPrintable(loadErr));

        // 3. Build runtime; load validates + applies.
        ScenarioRuntime rt(std::move(doc));
        QVERIFY(rt.load());

        // 4. Hand-build a Path O→D with one Train segment (no GUI
        //    PathFindingWorker in backend-pure tests). Path ctor takes
        //    (id, totalCost, edgeCost, termCost, terminals, segments).
        auto *segment = new PathSegment(
            "seg1", "O", "D",
            TransportationTypes::TransportationMode::Train);
        QList<PathTerminal>  emptyTerminals;
        QList<PathSegment *> segments{segment};
        auto *path =
            new Path(1, 0.0, 0.0, 0.0, emptyTerminals, segments);
        const auto prepared =
            PathPreparationService::prepareDiscoveredPaths(
                {path}, rt.document(),
                controller.getConfigController(),
                /*networks=*/nullptr,
                /*regionData=*/nullptr,
                controller.getVehicleController());
        rt.setPreparedPaths(prepared);
        QString selectionErr;
        QVERIFY2(rt.setSelectedPathKeys(prepared.pathIdentities(),
                                        &selectionErr),
                 qPrintable(selectionErr));

        // 5. Drive the simulation to completion via QEventLoop — CLI
        //    will use this same pattern (spec §5.3 idiom).
        QEventLoop loop;
        QObject::connect(&rt, &ScenarioRuntime::completed,
                         &loop, &QEventLoop::quit);
        QObject::connect(&rt, &ScenarioRuntime::failed,
                         &loop, &QEventLoop::quit);
        QSignalSpy statusSpy(&rt, &ScenarioRuntime::statusMessage);
        QSignalSpy errorSpy(&rt, &ScenarioRuntime::errorMessage);

        QVERIFY(rt.startSimulation());
        loop.exec();

        QVERIFY(errorSpy.isEmpty());
        QVERIFY(!statusSpy.isEmpty());

        const auto results = rt.results();
        QCOMPARE(results.size(), 1);
        QCOMPARE(results.first().pathId, 1);
        QVERIFY(results.first().totalCost > 0.0);

        controller.stopAll();
    }
};

QTEST_MAIN(ScenarioExecutorTest)
#include "ScenarioExecutorTest.moc"
