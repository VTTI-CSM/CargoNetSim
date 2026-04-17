#include <QCoreApplication>
#include <QSignalSpy>
#include <QTest>
#include <memory>

#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Scenario/ScenarioDocument.h"
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
};

QTEST_MAIN(ScenarioRuntimeTest)
#include "ScenarioRuntimeTest.moc"
