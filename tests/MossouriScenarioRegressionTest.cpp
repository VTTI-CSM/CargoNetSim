#include <QCoreApplication>
#include <QFileInfo>
#include <QScopeGuard>
#include <QTest>

#include "Backend/Clients/TerminalClient/TerminalSimulationClient.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Controllers/ConfigController.h"
#include "Backend/Models/Path.h"
#include "Backend/Models/Terminal.h"
#include "Backend/Scenario/PathDiscovery.h"
#include "Backend/Scenario/ScenarioRuntime.h"
#include "Backend/Scenario/ScenarioSerializer.h"

class MossouriScenarioRegressionTest : public QObject
{
    Q_OBJECT

private:
    static QString scenarioPath()
    {
        return QStringLiteral(
            "/home/ahmed/Documents/CargoNetSim/Mossouri_destination_v2.yml");
    }

    static void requireScenarioFile()
    {
        if (!QFileInfo::exists(scenarioPath()))
        {
            QSKIP("Mossouri scenario file is not present on this machine.");
        }
    }

private slots:
    void initTestCase()
    {
        if (!CargoNetSim::CargoNetSimController::instance())
        {
            new CargoNetSim::CargoNetSimController(
                /*logger=*/nullptr, QCoreApplication::instance());
        }
    }

    void test_mossouri_yaml_load_populates_registry_for_every_terminal()
    {
        using namespace CargoNetSim::Backend::Scenario;

        requireScenarioFile();

        QString err;
        auto doc = ScenarioSerializer::fromYaml(scenarioPath(), &err);
        QVERIFY2(doc != nullptr, qPrintable(err));
        QVERIFY2(!doc->terminals.isEmpty(),
                 "Mossouri scenario must declare terminals");

        const int documentTerminalCount = doc->terminals.size();
        ScenarioRuntime rt(std::move(doc));
        QVERIFY2(rt.load(), "ScenarioRuntime::load failed for Mossouri scenario");

        QCOMPARE(rt.registry().terminalCount(), documentTerminalCount);

        QStringList missingTerminalIds;
        for (auto it = rt.document().terminals.constBegin();
             it != rt.document().terminals.constEnd(); ++it)
        {
            if (rt.registry().terminal(it.key()) == nullptr)
                missingTerminalIds.append(it.key());
        }

        QVERIFY2(
            missingTerminalIds.isEmpty(),
            qPrintable(QStringLiteral(
                "Registry is missing %1 terminal(s): %2")
                           .arg(missingTerminalIds.size())
                           .arg(missingTerminalIds.join(QStringLiteral(", ")))));
    }

    void test_mossouri_yaml_builds_complete_terminal_upload_batch()
    {
        using namespace CargoNetSim::Backend;
        using namespace CargoNetSim::Backend::Scenario;

        requireScenarioFile();

        QString err;
        auto doc = ScenarioSerializer::fromYaml(scenarioPath(), &err);
        QVERIFY2(doc != nullptr, qPrintable(err));

        const int documentTerminalCount = doc->terminals.size();
        ScenarioRuntime rt(std::move(doc));
        QVERIFY2(rt.load(), "ScenarioRuntime::load failed for Mossouri scenario");

        QList<Terminal *> terminalBatch;
        terminalBatch.reserve(rt.document().terminals.size());
        QStringList missingTerminalIds;
        for (auto it = rt.document().terminals.constBegin();
             it != rt.document().terminals.constEnd(); ++it)
        {
            if (Terminal *terminal = rt.registry().terminal(it.key()))
                terminalBatch.append(terminal);
            else
                missingTerminalIds.append(it.key());
        }

        QVERIFY2(
            missingTerminalIds.isEmpty(),
            qPrintable(QStringLiteral(
                "PathDiscovery terminal upload batch is missing %1 terminal(s): %2")
                           .arg(missingTerminalIds.size())
                           .arg(missingTerminalIds.join(QStringLiteral(", ")))));
        QCOMPARE(terminalBatch.size(), documentTerminalCount);
    }

    void test_mossouri_live_path_discovery_returns_paths()
    {
        if (qEnvironmentVariable("CARGONETSIM_INTEGRATION").isEmpty())
            QSKIP("Requires live TerminalSim");

        requireScenarioFile();

        using namespace CargoNetSim::Backend;
        using namespace CargoNetSim::Backend::Scenario;

        auto &controller =
            CargoNetSim::CargoNetSimController::getInstance();
        QVERIFY(controller.initialize(/*truckExePath=*/QString()));
        QVERIFY(controller.startAll());
        auto stopGuard = qScopeGuard([&controller]() {
            controller.stopAll();
        });

        QString err;
        auto doc = ScenarioSerializer::fromYaml(scenarioPath(), &err);
        QVERIFY2(doc != nullptr, qPrintable(err));

        ScenarioRuntime rt(std::move(doc));
        QVERIFY2(rt.load(), "ScenarioRuntime::load failed for Mossouri scenario");

        PathDiscovery discovery;
        auto paths = discovery.findTopPaths(
            rt.document(), rt.registry(), /*n=*/10, &err);
        QVERIFY2(!paths.isEmpty(), qPrintable(err));
        qDeleteAll(paths);
    }
};

QTEST_MAIN(MossouriScenarioRegressionTest)
#include "MossouriScenarioRegressionTest.moc"
