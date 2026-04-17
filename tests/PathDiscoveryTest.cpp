#include <QTest>

#include "Backend/Scenario/PathDiscovery.h"
#include "Backend/Scenario/ScenarioDocument.h"
#include "Backend/Scenario/ScenarioRegistry.h"

// Unit coverage for Backend::Scenario::PathDiscovery (Plan 5 Task 4).
//
// The empty-document slot runs headlessly — it exercises the
// "no-origins → empty result, no error written" contract and does not
// touch TerminalSim, so it succeeds in any CI environment.
//
// The fixture-document slot requires a live TerminalSim + ConfigController
// round-trip and is gated on CARGONETSIM_INTEGRATION, matching the
// convention used by ScenarioExecutorTest and the eventual Plan 5
// Task 20 end-to-end test.
class PathDiscoveryTest : public QObject
{
    Q_OBJECT

private slots:
    void test_empty_document_returns_empty_list_and_writes_no_error()
    {
        using namespace CargoNetSim::Backend;
        Scenario::ScenarioDocument doc;
        Scenario::ScenarioRegistry reg;
        Scenario::PathDiscovery    pd;

        QString    err;
        const auto paths = pd.findTopPaths(doc, reg, /*n=*/5, &err);

        QVERIFY(paths.isEmpty());
        // An empty scenario is not an error — no origins declared, so
        // there is nothing to discover. @p err must stay untouched.
        QVERIFY(err.isEmpty());
    }

    void test_fixture_document_returns_paths_between_declared_pair()
    {
        if (qEnvironmentVariable("CARGONETSIM_INTEGRATION").isEmpty())
            QSKIP("Requires live TerminalSim");

        // Intentionally a placeholder. Plan 5 Task 20 adds the live
        // end-to-end integration fixture; at this task's boundary we
        // only assert that PathDiscovery compiles, links, and the
        // headless contract holds. The integration slot is gated so
        // CI without a broker stays green.
    }
};

QTEST_MAIN(PathDiscoveryTest)
#include "PathDiscoveryTest.moc"
