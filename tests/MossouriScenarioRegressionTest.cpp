#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QHash>
#include <QSet>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QTest>

#include "Backend/Application/AvailabilityService.h"
#include "Backend/Application/PreparedPathService.h"
#include "Backend/Bootstrap/BackendBootstrapService.h"
#include "Backend/Clients/TerminalClient/TerminalSimulationClient.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Models/Path.h"
#include "Backend/Models/Terminal.h"
#include "Backend/Scenario/PathDiscovery.h"
#include "Backend/Scenario/ScenarioRuntime.h"
#include "Backend/Scenario/ScenarioSerializer.h"

class MossouriScenarioRegressionTest : public QObject
{
    Q_OBJECT

private:
    struct CliInvocationResult
    {
        int        exitCode = -1;
        QByteArray standardOutput;
        QByteArray standardError;
    };

    static QString scenarioPath()
    {
        return QStringLiteral(
            "/home/ahmed/Documents/CargoNetSim/Mossouri_destination_v2.yml");
    }

    static QString cliBinaryPath()
    {
        return QStringLiteral(CARGONETSIM_CLI_BINARY);
    }

    static std::unique_ptr<CargoNetSim::Backend::Scenario::ScenarioDocument>
    loadScenarioDocument()
    {
        QString err;
        auto doc =
            CargoNetSim::Backend::Scenario::ScenarioSerializer::fromYaml(
                scenarioPath(), &err);
        if (!doc)
            QTest::qFail(qPrintable(err), __FILE__, __LINE__);
        return doc;
    }

    static void requireScenarioFile()
    {
        if (!QFileInfo::exists(scenarioPath()))
        {
            QSKIP("Mossouri scenario file is not present on this machine.");
        }
    }

    static void requireCliBinary()
    {
        if (!QFileInfo::exists(cliBinaryPath()))
        {
            QSKIP("cargonetsim-cli binary is not present in this build.");
        }
    }

    CliInvocationResult runCli(const QStringList &args,
                               const QString     &workingDirectory = QString())
    {
        requireCliBinary();

        CliInvocationResult result;
        QProcess process;
        if (!workingDirectory.isEmpty())
            process.setWorkingDirectory(workingDirectory);

        process.start(cliBinaryPath(), args);
        if (!process.waitForStarted())
        {
            QTest::qFail(qPrintable(process.errorString()),
                         __FILE__, __LINE__);
            return result;
        }
        if (!process.waitForFinished(5 * 60 * 1000))
        {
            QTest::qFail(qPrintable(process.errorString()),
                         __FILE__, __LINE__);
            return result;
        }

        result.exitCode = process.exitCode();
        result.standardOutput = process.readAllStandardOutput();
        result.standardError = process.readAllStandardError();
        return result;
    }

    static int jsonContainerSize(const QJsonValue &value)
    {
        if (value.isArray())
            return value.toArray().size();
        if (value.isObject())
            return value.toObject().size();
        return 0;
    }

    static QJsonObject parseJsonObject(const QByteArray &jsonBytes,
                                       const char       *context)
    {
        QJsonParseError parseError;
        const auto json =
            QJsonDocument::fromJson(jsonBytes, &parseError);
        if (parseError.error != QJsonParseError::NoError)
        {
            const QString message = QStringLiteral("%1: %2")
                                        .arg(QString::fromUtf8(context),
                                             parseError.errorString());
            QTest::qFail(qPrintable(message), __FILE__, __LINE__);
            return {};
        }
        if (!json.isObject())
        {
            const QString message = QStringLiteral(
                                        "%1: expected top-level JSON object")
                                        .arg(QString::fromUtf8(context));
            QTest::qFail(qPrintable(message), __FILE__, __LINE__);
            return {};
        }
        return json.object();
    }

    static QSet<QString> pathUidSetFromResults(
        const QJsonArray &pathsArray)
    {
        QSet<QString> pathUids;
        for (const auto &value : pathsArray)
        {
            if (!value.isObject())
                continue;
            const QString pathUid =
                value.toObject().value(QStringLiteral("path_uid")).toString();
            if (!pathUid.isEmpty())
                pathUids.insert(pathUid);
        }
        return pathUids;
    }

    static QString availabilitySummary(
        const QList<CargoNetSim::Backend::Application::BackendAvailabilityStatus>
            &statuses)
    {
        QStringList lines;
        lines.reserve(statuses.size());
        for (const auto &status : statuses)
        {
            lines.append(QStringLiteral(
                             "%1(%2): client=%3 connected=%4 consumers=%5 command=%6")
                             .arg(status.serverKey)
                             .arg(status.server)
                             .arg(status.clientExists)
                             .arg(status.connected)
                             .arg(status.hasConsumers)
                             .arg(status.commandAvailable));
        }
        return lines.join(QStringLiteral("; "));
    }

    static QString unavailableServersReason(
        const QStringList &requiredServers,
        const QList<CargoNetSim::Backend::Application::BackendAvailabilityStatus>
            &statuses,
        const QString &skipContext)
    {
        const QStringList missingServers =
            CargoNetSim::Backend::Application::AvailabilityService::
                missingCommandServers(requiredServers, statuses);

        if (!missingServers.isEmpty())
        {
            return QStringLiteral(
                       "%1 Missing command queues for: %2. "
                       "Availability snapshot: %3")
                .arg(skipContext)
                .arg(missingServers.join(QStringLiteral(", ")))
                .arg(availabilitySummary(statuses));
        }

        return {};
    }

    static QStringList requiredSimulationServers(
        const CargoNetSim::Backend::Scenario::ScenarioDocument &doc)
    {
        using Mode = CargoNetSim::Backend::TransportationTypes::
            TransportationMode;

        QSet<QString> servers = {QStringLiteral("terminal")};
        const auto addModeServer = [&servers](Mode mode) {
            if (mode == Mode::Train)
            {
                servers.insert(QStringLiteral("train"));
            }
            else if (mode == Mode::Ship)
            {
                servers.insert(QStringLiteral("ship"));
            }
            else if (mode == Mode::Truck)
            {
                servers.insert(QStringLiteral("truck"));
            }
        };

        for (const auto &connection : doc.connections)
            addModeServer(connection.mode);
        for (const auto &globalLink : doc.globalLinks)
            addModeServer(globalLink.mode);

        return QStringList(servers.begin(), servers.end());
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
        BackendBootstrapService bootstrapService;
        const auto bootstrapResult =
            bootstrapService.initializeAndStartController(
                /*integrationExePath=*/QString());
        QVERIFY2(bootstrapResult.succeeded(),
                 qPrintable(bootstrapResult.message));
        auto stopGuard = qScopeGuard([&controller]() {
            controller.stopAll();
        });

        QString err;
        auto doc = ScenarioSerializer::fromYaml(scenarioPath(), &err);
        QVERIFY2(doc != nullptr, qPrintable(err));

        ScenarioRuntime rt(std::move(doc));
        QVERIFY2(rt.load(), "ScenarioRuntime::load failed for Mossouri scenario");

        Application::AvailabilityService availabilityService;
        const auto availabilityResult =
            availabilityService.waitForCommandAvailability(
                {QStringLiteral("terminal")}, /*timeoutMs=*/5000);
        const auto unavailableReason = unavailableServersReason(
            {QStringLiteral("terminal")},
            availabilityResult.statuses,
            QStringLiteral("Live path discovery prerequisites are unavailable."));
        if (!unavailableReason.isEmpty())
            QSKIP(qPrintable(unavailableReason));

        PathDiscovery discovery;
        auto paths = discovery.findTopPaths(
            rt.document(), rt.registry(), /*n=*/10, &err);
        QVERIFY2(!paths.isEmpty(), qPrintable(err));
        qDeleteAll(paths);
    }

    void test_mossouri_cli_validate_succeeds()
    {
        requireScenarioFile();

        const auto result =
            runCli({QStringLiteral("validate"), scenarioPath()});

        QCOMPARE(result.exitCode, 0);
        QVERIFY2(result.standardOutput.isEmpty(),
                 "validate should not emit scenario data to stdout");
    }

    void test_mossouri_cli_preview_emits_complete_json()
    {
        requireScenarioFile();

        auto scenarioDoc = loadScenarioDocument();

        const auto result =
            runCli({QStringLiteral("preview"), scenarioPath()});

        QCOMPARE(result.exitCode, 0);
        QVERIFY2(!result.standardOutput.isEmpty(),
                 "preview must emit JSON to stdout");

        const QJsonObject root =
            parseJsonObject(result.standardOutput,
                            "preview output must be valid JSON");
        QVERIFY(root.contains(QStringLiteral("regions")));
        QVERIFY(root.contains(QStringLiteral("terminals")));
        QVERIFY(root.contains(QStringLiteral("linkages")));
        QVERIFY(root.contains(QStringLiteral("connections")));
        QVERIFY(root.contains(QStringLiteral("global_links")));
        QVERIFY(root.contains(QStringLiteral("output")));

        QCOMPARE(jsonContainerSize(root.value(QStringLiteral("regions"))),
                 scenarioDoc->regions.size());
        QCOMPARE(jsonContainerSize(root.value(QStringLiteral("terminals"))),
                 scenarioDoc->terminals.size());
        QCOMPARE(jsonContainerSize(root.value(QStringLiteral("linkages"))),
                 scenarioDoc->linkages.size());
        QCOMPARE(jsonContainerSize(root.value(QStringLiteral("connections"))),
                 scenarioDoc->connections.size());
        QCOMPARE(jsonContainerSize(root.value(QStringLiteral("global_links"))),
                 scenarioDoc->globalLinks.size());

        const QJsonObject outputObject =
            root.value(QStringLiteral("output")).toObject();
        QCOMPARE(outputObject.value(QStringLiteral("directory")).toString(),
                 scenarioDoc->output.directory);
        QVERIFY(outputObject.value(QStringLiteral("formats")).isArray());
        const QJsonArray formatsArray =
            outputObject.value(QStringLiteral("formats")).toArray();
        QCOMPARE(formatsArray.size(), scenarioDoc->output.formats.size());
        for (int i = 0; i < formatsArray.size(); ++i)
        {
            QCOMPARE(formatsArray.at(i).toString(),
                     scenarioDoc->output.formats.at(i));
        }

        const QJsonValue terminalsValue = root.value(QStringLiteral("terminals"));
        if (terminalsValue.isObject())
        {
            const auto terminalObject = terminalsValue.toObject();
            QCOMPARE(terminalObject.size(), scenarioDoc->terminals.size());
            for (auto it = scenarioDoc->terminals.constBegin();
                 it != scenarioDoc->terminals.constEnd(); ++it)
            {
                QVERIFY2(
                    terminalObject.contains(it.key()),
                    qPrintable(QStringLiteral(
                        "preview terminals are missing '%1'")
                                   .arg(it.key())));
            }
        }
    }

    void test_mossouri_cli_paths_emits_expected_discovery_json()
    {
        if (qEnvironmentVariable("CARGONETSIM_INTEGRATION").isEmpty())
            QSKIP("Requires live simulators (set CARGONETSIM_INTEGRATION=1)");

        requireScenarioFile();

        using namespace CargoNetSim::Backend;

        auto &controller =
            CargoNetSim::CargoNetSimController::getInstance();
        BackendBootstrapService bootstrapService;
        const auto bootstrapResult =
            bootstrapService.initializeAndStartController(
                /*integrationExePath=*/QString());
        QVERIFY2(bootstrapResult.succeeded(),
                 qPrintable(bootstrapResult.message));
        auto stopGuard = qScopeGuard([&controller]() {
            controller.stopAll();
        });

        Application::AvailabilityService availabilityService;
        const auto availabilityResult =
            availabilityService.waitForCommandAvailability(
                {QStringLiteral("terminal")}, /*timeoutMs=*/5000);
        const auto unavailableReason = unavailableServersReason(
            {QStringLiteral("terminal")},
            availabilityResult.statuses,
            QStringLiteral("CLI paths prerequisites are unavailable."));
        if (!unavailableReason.isEmpty())
            QSKIP(qPrintable(unavailableReason));

        Scenario::ScenarioRuntime runtimeForExpectation(
            loadScenarioDocument());
        QVERIFY2(runtimeForExpectation.load(),
                 "ScenarioRuntime::load failed while preparing expected CLI path set");

        const int expectedTopN = controller.getSimulationParams()
                                     .value("shortest_paths", 5)
                                     .toInt();
        Application::PreparedPathService preparedPathService(
            &controller);
        const auto preparedResult =
            preparedPathService.discoverAndPrepare(
                runtimeForExpectation, expectedTopN);
        QVERIFY2(preparedResult.succeeded(),
                 qPrintable(preparedResult.message));

        QSet<QString> expectedPathUids;
        for (const auto &record : preparedResult.preparedPaths.records())
        {
            QVERIFY(record.path);
            expectedPathUids.insert(record.path->getPathUid());
        }
        QVERIFY2(!expectedPathUids.isEmpty(),
                 "Expected prepared path set must not be empty");

        const auto result = runCli(
            {QStringLiteral("paths"),
             QStringLiteral("--json"),
             scenarioPath()});

        QCOMPARE(result.exitCode, 0);
        QVERIFY2(!result.standardOutput.isEmpty(),
                 "paths --json must emit JSON to stdout");

        const QJsonObject root =
            parseJsonObject(result.standardOutput,
                            "paths output must be valid JSON");
        QCOMPARE(root.value(QStringLiteral("top_n_requested")).toInt(),
                 expectedTopN);
        QCOMPARE(root.value(QStringLiteral("discovered_path_count")).toInt(),
                 preparedResult.preparedPathCount);
        QVERIFY(root.value(QStringLiteral("paths")).isArray());

        const auto pathsArray =
            root.value(QStringLiteral("paths")).toArray();
        QCOMPARE(pathsArray.size(), preparedResult.preparedPathCount);

        QSet<QString> actualPathUids;
        for (const auto &pathValue : pathsArray)
        {
            QVERIFY(pathValue.isObject());
            const auto pathObject = pathValue.toObject();
            const QString pathUid =
                pathObject.value(QStringLiteral("path_uid"))
                    .toString();
            QVERIFY2(!pathUid.isEmpty(),
                     "paths JSON entries must expose path_uid");
            actualPathUids.insert(pathUid);
            QVERIFY(pathObject.contains(
                QStringLiteral("predicted_metrics")));
            QVERIFY(pathObject.contains(QStringLiteral("segments")));
            QVERIFY(pathObject.contains(QStringLiteral("terminals")));
        }

        QCOMPARE(actualPathUids, expectedPathUids);
    }

    void test_mossouri_cli_run_emits_results_json_data()
    {
        QTest::addColumn<QStringList>("runArgs");

        QTest::newRow("explicit-all")
            << QStringList{QStringLiteral("run"),
                           QStringLiteral("--all"),
                           scenarioPath()};
        QTest::newRow("implicit-all-alias")
            << QStringList{QStringLiteral("run"),
                           scenarioPath()};
    }

    void test_mossouri_cli_run_emits_results_json()
    {
        if (qEnvironmentVariable("CARGONETSIM_INTEGRATION").isEmpty())
            QSKIP("Requires live simulators (set CARGONETSIM_INTEGRATION=1)");

        requireScenarioFile();
        QFETCH(QStringList, runArgs);

        using namespace CargoNetSim::Backend;

        auto &controller =
            CargoNetSim::CargoNetSimController::getInstance();
        BackendBootstrapService bootstrapService;
        const auto bootstrapResult =
            bootstrapService.initializeAndStartController(
                /*integrationExePath=*/QString());
        QVERIFY2(bootstrapResult.succeeded(),
                 qPrintable(bootstrapResult.message));
        auto stopGuard = qScopeGuard([&controller]() {
            controller.stopAll();
        });

        auto scenarioDoc = loadScenarioDocument();

        Application::AvailabilityService availabilityService;
        const QStringList requiredServers =
            requiredSimulationServers(*scenarioDoc);
        const auto availabilityResult =
            availabilityService.waitForCommandAvailability(
                requiredServers, /*timeoutMs=*/5000);
        const auto unavailableReason = unavailableServersReason(
            requiredServers,
            availabilityResult.statuses,
            QStringLiteral("CLI run prerequisites are unavailable."));
        if (!unavailableReason.isEmpty())
            QSKIP(qPrintable(unavailableReason));

        QTemporaryDir workDir;
        QVERIFY(workDir.isValid());

        Scenario::ScenarioRuntime runtimeForExpectation(
            loadScenarioDocument());
        QVERIFY2(runtimeForExpectation.load(),
                 "ScenarioRuntime::load failed while preparing expected CLI path set");

        const int expectedTopN = controller.getSimulationParams()
                                     .value("shortest_paths", 5)
                                     .toInt();
        Application::PreparedPathService preparedPathService(&controller);
        const auto preparedResult =
            preparedPathService.discoverAndPrepare(
                runtimeForExpectation, expectedTopN);
        QVERIFY2(preparedResult.succeeded(),
                 qPrintable(preparedResult.message));
        const int expectedDiscoveredPathCount =
            preparedResult.preparedPathCount;
        QVERIFY2(expectedDiscoveredPathCount > 0,
                 "Live backend preparation must discover at least one path");

        QSet<QString> expectedPathUids;
        QHash<QString, int> expectedRanksByUid;
        for (const auto &record : preparedResult.preparedPaths.records())
        {
            QVERIFY(record.path);
            const QString pathUid = record.path->getPathUid();
            QVERIFY2(!pathUid.isEmpty(),
                     "Prepared path records must expose stable path_uid values");
            expectedPathUids.insert(pathUid);
            expectedRanksByUid.insert(pathUid, record.path->getRank());
        }
        QCOMPARE(expectedPathUids.size(), expectedDiscoveredPathCount);

        const auto result = runCli(runArgs, workDir.path());

        QCOMPARE(result.exitCode, 0);
        QVERIFY2(result.standardOutput.isEmpty(),
                 "run should not emit results data to stdout");
        QVERIFY2(
            result.standardError.contains(
                QByteArray("run: discovered ")
                + QByteArray::number(expectedDiscoveredPathCount)
                + QByteArray(" path(s)")),
            "CLI run stderr must report the discovered path count");
        QVERIFY2(
            result.standardError.contains(
                QByteArray("run: selected ")
                + QByteArray::number(expectedDiscoveredPathCount)
                + QByteArray(" path(s) for simulation")),
            "CLI run stderr must report the selected simulation path count");

        const QString resultsPath =
            QDir(workDir.path()).filePath(
                QStringLiteral("results/results.json"));
        QVERIFY2(QFile::exists(resultsPath),
                 qPrintable(QStringLiteral(
                     "Expected CLI run output at %1")
                                .arg(resultsPath)));

        QFile resultsFile(resultsPath);
        QVERIFY(resultsFile.open(QIODevice::ReadOnly | QIODevice::Text));

        const QJsonObject root =
            parseJsonObject(resultsFile.readAll(),
                            "results.json must be valid JSON");
        QCOMPARE(root.value(QStringLiteral("schema_version")).toInt(), 2);
        QVERIFY(root.contains(QStringLiteral("generated_at")));
        const QString generatedAt =
            root.value(QStringLiteral("generated_at")).toString();
        const QRegularExpression isoUtc(
            QStringLiteral(R"(^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$)"));
        QVERIFY2(isoUtc.match(generatedAt).hasMatch(),
                 qPrintable(generatedAt));
        QVERIFY(root.value(QStringLiteral("paths")).isArray());
        const QJsonArray paths =
            root.value(QStringLiteral("paths")).toArray();
        QVERIFY2(!paths.isEmpty(),
                 "CLI run must emit at least one simulated path result");
        QCOMPARE(paths.size(), expectedDiscoveredPathCount);

        const QSet<QString> actualPathUids =
            pathUidSetFromResults(paths);
        QCOMPARE(actualPathUids, expectedPathUids);

        for (const auto &value : paths)
        {
            QVERIFY(value.isObject());
            const QJsonObject pathObject = value.toObject();
            QVERIFY(pathObject.contains(QStringLiteral("path_id")));
            QVERIFY(pathObject.contains(QStringLiteral("path_uid")));
            QVERIFY(pathObject.contains(QStringLiteral("origin")));
            QVERIFY(pathObject.contains(QStringLiteral("destination")));
            QVERIFY(pathObject.contains(QStringLiteral("rank")));
            QVERIFY(pathObject.contains(QStringLiteral("total_cost")));
            QVERIFY(pathObject.contains(QStringLiteral("edge_costs")));
            QVERIFY(pathObject.contains(QStringLiteral("terminal_costs")));
            QVERIFY(pathObject.contains(
                QStringLiteral("effective_container_count")));
            QVERIFY(pathObject.contains(QStringLiteral("metrics")));
            QVERIFY(pathObject.contains(QStringLiteral("segments")));

            const QString pathUid =
                pathObject.value(QStringLiteral("path_uid")).toString();
            QVERIFY2(expectedRanksByUid.contains(pathUid),
                     qPrintable(QStringLiteral(
                         "Unexpected path_uid in CLI results: %1")
                                    .arg(pathUid)));
            QCOMPARE(pathObject.value(QStringLiteral("rank")).toInt(),
                     expectedRanksByUid.value(pathUid));

            const auto metrics =
                pathObject.value(QStringLiteral("metrics")).toObject();
            QVERIFY(metrics.contains(
                QStringLiteral("preview_container_count")));
            QVERIFY(metrics.contains(
                QStringLiteral("preview_vehicles_needed")));
            QVERIFY(metrics.contains(QStringLiteral("distance_km")));
            QVERIFY(metrics.contains(QStringLiteral("travel_time_h")));
            QVERIFY(metrics.contains(QStringLiteral("per_vehicle")));
            QVERIFY(metrics.contains(QStringLiteral("per_container")));

            const auto segments =
                pathObject.value(QStringLiteral("segments")).toArray();
            QVERIFY2(!segments.isEmpty(),
                     "Every emitted CLI path must contain at least one segment");
        }
    }
};

QTEST_MAIN(MossouriScenarioRegressionTest)
#include "MossouriScenarioRegressionTest.moc"
