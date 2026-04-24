#include <QCoreApplication>
#include <QSignalSpy>
#include <QTest>
#include <memory>

#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Models/Path.h"
#include "Backend/Models/PathSegment.h"
#include "Backend/Scenario/ScenarioDocument.h"
#include "Backend/Scenario/PathPreparationService.h"
#include "Backend/Scenario/ScenarioRuntime.h"

class ScenarioRuntimeTest : public QObject
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

    void test_runtime_owns_document_and_registry()
    {
        using namespace CargoNetSim::Backend::Scenario;

        auto  doc    = std::make_unique<ScenarioDocument>();
        auto *rawDoc = doc.get();

        ScenarioRuntime rt(std::move(doc));
        QCOMPARE(&rt.document(), rawDoc);
        QVERIFY(!rt.isRunning());
        QCOMPARE(rt.currentTime(), 0.0);
    }

    void test_runtime_startSimulation_without_load_fails()
    {
        using namespace CargoNetSim::Backend::Scenario;

        ScenarioRuntime rt(std::make_unique<ScenarioDocument>());
        QSignalSpy failSpy(&rt, &ScenarioRuntime::failed);
        QCOMPARE(rt.startSimulation(), false);
        QCOMPARE(failSpy.count(), 1);
        QVERIFY(!rt.isRunning());
    }

    void test_runtime_selects_prepared_paths_by_identity()
    {
        using namespace CargoNetSim::Backend;
        using namespace CargoNetSim::Backend::Scenario;

        ScenarioRuntime rt(std::make_unique<ScenarioDocument>());

        auto *segment = new PathSegment(
            QStringLiteral("seg1"), QStringLiteral("O"),
            QStringLiteral("D"),
            TransportationTypes::TransportationMode::Train);
        QList<PathTerminal>  emptyTerminals;
        QList<PathSegment *> segments{segment};
        auto *path =
            new Path(1, 0.0, 0.0, 0.0, emptyTerminals, segments);

        const PreparedPathSet prepared =
            PathPreparationService::prepareDiscoveredPaths(
                {path}, rt.document(),
                /*config=*/nullptr,
                /*networks=*/nullptr,
                /*regionData=*/nullptr,
                /*vehicles=*/nullptr);

        rt.setPreparedPaths(prepared);

        QString err;
        QVERIFY2(rt.setSelectedPathKeys(prepared.pathIdentities(), &err),
                 qPrintable(err));
        QCOMPARE(rt.paths().size(), 1);
        QCOMPARE(rt.paths().front()->canonicalPathKey(),
                 prepared.records().front().canonicalPathKey);

        QVERIFY(!rt.setSelectedPathKeys(
            {QStringLiteral("missing-path")}, &err));
        QVERIFY(err.contains(QStringLiteral("Unknown prepared path identity")));
    }

    void test_runtime_exposes_backend_eligibility_and_blocks_unavailable_selection()
    {
        using namespace CargoNetSim::Backend;
        using namespace CargoNetSim::Backend::Scenario;

        ScenarioRuntime rt(std::make_unique<ScenarioDocument>());

        auto *segment = new PathSegment(
            QStringLiteral("seg1"), QStringLiteral("O"),
            QStringLiteral("D"),
            TransportationTypes::TransportationMode::Train);
        auto *path =
            new Path(7, 0.0, 0.0, 0.0, {}, {segment});

        const PreparedPathSet prepared =
            PathPreparationService::prepareDiscoveredPaths(
                {path}, rt.document(),
                /*config=*/nullptr,
                /*networks=*/nullptr,
                /*regionData=*/nullptr,
                /*vehicles=*/nullptr);

        rt.setPreparedPaths(prepared);
        QCOMPARE(rt.preparedPathEligibility().size(), 1);

        QString err;
        QVERIFY2(rt.setSelectedPathKeys(prepared.pathIdentities(), &err),
                 qPrintable(err));
        QVERIFY(!rt.validateCurrentSelectionForSimulation(&err));
        QVERIFY(err.contains(QStringLiteral("Path 7")));
        QVERIFY(err.contains(QStringLiteral("TerminalSim")));
    }
};

QTEST_MAIN(ScenarioRuntimeTest)
#include "ScenarioRuntimeTest.moc"
