#include <QSignalSpy>
#include <QTest>
#include <memory>

#include "Backend/Scenario/ScenarioDocument.h"
#include "Backend/Scenario/ScenarioRuntime.h"

class ScenarioRuntimeTest : public QObject
{
    Q_OBJECT
private slots:
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
