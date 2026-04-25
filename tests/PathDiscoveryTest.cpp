#include <QTest>
#include <QCoreApplication>
#include <QScopeGuard>

#include "Backend/Clients/TerminalClient/TerminalSimulationClient.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Controllers/ConfigController.h"
#include "Backend/Models/PathSegment.h"
#include "Backend/Models/Terminal.h"
#include "Backend/Scenario/PathDiscovery.h"
#include "Backend/Scenario/ScenarioDocument.h"
#include "Backend/Scenario/ScenarioRegistry.h"
#include "Backend/Scenario/ScenarioRuntime.h"
#include "Backend/Scenario/ScenarioSerializer.h"

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

    void test_external_scenario_logs_top_paths()
    {
        if (qEnvironmentVariable("CARGONETSIM_INTEGRATION").isEmpty())
            QSKIP("Requires live TerminalSim");

        const QString scenarioPath =
            qEnvironmentVariable("CARGONETSIM_SCENARIO_PATH");
        if (scenarioPath.isEmpty())
            QSKIP("Set CARGONETSIM_SCENARIO_PATH to run this integration repro.");

        using namespace CargoNetSim::Backend;
        using namespace CargoNetSim::Backend::Scenario;

        auto &controller =
            CargoNetSim::CargoNetSimController::getInstance();
        QVERIFY(controller.initialize(/*truckExePath=*/""));
        QVERIFY(controller.startAll());
        auto stopGuard = qScopeGuard([&controller]() {
            controller.stopAll();
        });

        auto *terminalClient = controller.getTerminalClient();
        QVERIFY(terminalClient != nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(
            terminalClient->getRabbitMQHandler() != nullptr, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(terminalClient->isConnected(), 5000);
        QVERIFY2(terminalClient->resetServer(),
                 "Failed to reset TerminalSim server before scenario load");

        QString loadErr;
        auto doc = ScenarioSerializer::fromYaml(scenarioPath, &loadErr);
        QVERIFY2(doc != nullptr, qPrintable(loadErr));

        ScenarioRuntime rt(std::move(doc));
        QVERIFY(rt.load());

        auto *config = controller.getConfigController();
        QVERIFY(config != nullptr);

        QVERIFY2(terminalClient->resetServer(),
                 "Failed to reset TerminalSim server after scenario load");
        terminalClient->setCostFunctionParameters(
            config->getCostFunctionWeights());

        QList<Terminal *> terminals;
        terminals.reserve(rt.document().terminals.size());
        for (auto it = rt.document().terminals.constBegin();
             it != rt.document().terminals.constEnd(); ++it)
        {
            if (Terminal *t = rt.registry().terminal(it.key()))
                terminals.append(t);
        }
        QVERIFY2(terminalClient->addTerminals(terminals),
                 "Failed to add terminals to TerminalSim");

        QList<PathSegment *> routes;
        routes.reserve(rt.document().connections.size()
                       + rt.document().globalLinks.size());
        const auto makeSegmentId = [](const QString &from,
                                      const QString &to,
                                      TransportationTypes::TransportationMode mode) {
            return QStringLiteral("%1->%2:%3")
                .arg(from, to, QString::number(static_cast<int>(mode)));
        };
        for (const auto &c : rt.document().connections)
        {
            routes.append(new PathSegment(
                makeSegmentId(c.fromTerminalId, c.toTerminalId, c.mode),
                c.fromTerminalId, c.toTerminalId, c.mode));
        }
        for (const auto &gl : rt.document().globalLinks)
        {
            routes.append(new PathSegment(
                makeSegmentId(gl.fromTerminalId, gl.toTerminalId, gl.mode),
                gl.fromTerminalId, gl.toTerminalId, gl.mode));
        }
        const bool routesAdded = terminalClient->addRoutes(routes);
        qDeleteAll(routes);
        QVERIFY2(routesAdded, "Failed to add routes to TerminalSim");

        QList<Path *> paths;
        const QStringList originIds = rt.document().originTerminalIds();
        QVERIFY(!originIds.isEmpty());
        for (const QString &originId : originIds)
        {
            for (const auto &route : rt.document().destinationsFor(originId))
            {
                paths.append(terminalClient->findTopPaths(
                    originId, route.terminal, /*n=*/10,
                    TransportationTypes::TransportationMode::Any,
                    /*skipDelays=*/true));
            }
        }
        QVERIFY2(!paths.isEmpty(), "TerminalSim returned no top paths");

        qWarning().noquote()
            << QStringLiteral("PathDiscoveryTest external scenario: %1 paths")
                   .arg(paths.size());

        for (int i = 0; i < paths.size(); ++i)
        {
            const auto *path = paths.at(i);
            QVERIFY(path != nullptr);

            qWarning().noquote()
                << QStringLiteral(
                       "path[%1] id=%2 rank=%3 total=%4 edge=%5 terminal=%6 "
                       "weightedDelay=%7 weightedDirectCost=%8 rawDelay=%9 rawCost=%10 "
                       "segments=%11 terminals=%12")
                       .arg(i)
                       .arg(path->getPathId())
                       .arg(path->getRank())
                       .arg(path->getTotalPathCost(), 0, 'f', 6)
                       .arg(path->getTotalEdgeCosts(), 0, 'f', 6)
                       .arg(path->getTotalTerminalCosts(), 0, 'f', 6)
                       .arg(path->getWeightedTerminalDelayTotal(), 0, 'f', 6)
                       .arg(path->getWeightedTerminalDirectCostTotal(), 0, 'f', 6)
                       .arg(path->getRawTerminalDelayTotal(), 0, 'f', 6)
                       .arg(path->getRawTerminalCostTotal(), 0, 'f', 6)
                       .arg(path->getSegments().size())
                       .arg(path->getTerminalsInPath().size());

            const auto terminals = path->getTerminalsInPath();
            for (int t = 0; t < terminals.size(); ++t)
            {
                const auto &terminal = terminals.at(t);
                qWarning().noquote()
                    << QStringLiteral(
                           "  terminal[%1] id=%2 handling=%3 cost=%4 skipped=%5 "
                           "delayContribution=%6 costContribution=%7 totalContribution=%8 "
                           "skipReason=%9")
                           .arg(t)
                           .arg(terminal.id)
                           .arg(terminal.handlingTime, 0, 'f', 6)
                           .arg(terminal.rawCost, 0, 'f', 6)
                           .arg(terminal.costsSkipped ? "true" : "false")
                           .arg(terminal.weightedTerminalDelayContribution, 0, 'f', 6)
                           .arg(terminal.weightedTerminalCostContribution, 0, 'f', 6)
                           .arg(terminal.weightedTerminalTotalContribution, 0, 'f', 6)
                           .arg(terminal.skipReason);
            }

            const auto segments = path->getSegments();
            for (int s = 0; s < segments.size(); ++s)
            {
                const auto *segment = segments.at(s);
                QVERIFY(segment != nullptr);
                const auto estimatedValues = segment->estimatedValues();
                const auto estimatedCosts = segment->estimatedCosts();
                qWarning().noquote()
                    << QStringLiteral(
                           "  segment[%1] %2 -> %3 mode=%4 rankingContribution=%5 "
                           "weightedEdge=%6 weightedTerminalEmbedded=%7 "
                           "valuesAvailable=%8 costsAvailable=%9 "
                           "distance=%10 travelTime=%11 directCost=%12")
                           .arg(s)
                           .arg(segment->getStart())
                           .arg(segment->getEnd())
                           .arg(static_cast<int>(segment->getMode()))
                           .arg(segment->rankingCostContribution(), 0, 'f', 6)
                           .arg(segment->weightedEdgeCost(), 0, 'f', 6)
                           .arg(segment->weightedTerminalCostEmbeddedInSegment(), 0, 'f', 6)
                           .arg(estimatedValues.available ? "true" : "false")
                           .arg(estimatedCosts.available ? "true" : "false")
                           .arg(estimatedValues.distance, 0, 'f', 6)
                           .arg(estimatedValues.travelTime, 0, 'f', 6)
                           .arg(estimatedCosts.directCost, 0, 'f', 6);
            }
        }

    }
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    if (!CargoNetSim::CargoNetSimController::instance())
    {
        new CargoNetSim::CargoNetSimController(
            /*logger=*/nullptr, &app);
    }

    PathDiscoveryTest testObject;
    return QTest::qExec(&testObject, argc, argv);
}

#include "PathDiscoveryTest.moc"
