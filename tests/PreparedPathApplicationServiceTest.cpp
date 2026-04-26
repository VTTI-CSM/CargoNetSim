#include <QCoreApplication>
#include <QTest>

#include <memory>

#include "Backend/Application/PreparedPathService.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Scenario/RegionSpec.h"
#include "Backend/Scenario/ScenarioDocument.h"
#include "Backend/Scenario/ScenarioRuntime.h"
#include "Backend/Scenario/TerminalPlacement.h"

using namespace CargoNetSim::Backend;
using namespace CargoNetSim::Backend::Application;
using namespace CargoNetSim::Backend::Scenario;

namespace
{

std::unique_ptr<ScenarioDocument> makeNoOriginDoc()
{
    auto doc = std::make_unique<ScenarioDocument>();

    RegionSpec region;
    region.name = QStringLiteral("R1");
    doc->addRegion(region);

    TerminalPlacement t1;
    t1.id = QStringLiteral("T1");
    t1.type = QStringLiteral("Sea Port Terminal");
    t1.region = QStringLiteral("R1");
    t1.mode = TerminalPlacement::PositionMode::Scene;
    t1.scenePos = { 0.0, 0.0 };
    doc->addTerminal(t1);

    TerminalPlacement t2;
    t2.id = QStringLiteral("T2");
    t2.type = QStringLiteral("Sea Port Terminal");
    t2.region = QStringLiteral("R1");
    t2.mode = TerminalPlacement::PositionMode::Scene;
    t2.scenePos = { 100.0, 100.0 };
    doc->addTerminal(t2);

    return doc;
}

} // namespace

class PreparedPathApplicationServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (!CargoNetSim::CargoNetSimController::instance())
        {
            new CargoNetSim::CargoNetSimController(
                nullptr, QCoreApplication::instance());
        }
    }

    void test_invalid_top_n_returns_invalid_request()
    {
        auto doc = makeNoOriginDoc();
        PreparedPathService service(/*config=*/nullptr,
                                    /*networks=*/nullptr,
                                    /*regionData=*/nullptr,
                                    /*vehicles=*/nullptr);

        const auto result =
            service.discoverAndPrepare(*doc, ScenarioRegistry(), 0);

        QCOMPARE(result.status,
                 PreparedPathServiceStatus::InvalidRequest);
        QCOMPARE(result.message,
                 QStringLiteral("Requested path count must be positive"));
        QVERIFY(!result.succeeded());
        QVERIFY(result.preparedPaths.isEmpty());
    }

    void test_no_origins_returns_no_paths_without_touching_integration()
    {
        auto doc = makeNoOriginDoc();
        PreparedPathService service(/*config=*/nullptr,
                                    /*networks=*/nullptr,
                                    /*regionData=*/nullptr,
                                    /*vehicles=*/nullptr);

        const auto result =
            service.discoverAndPrepare(*doc, ScenarioRegistry(), 5);

        QCOMPARE(result.status,
                 PreparedPathServiceStatus::NoPathsFound);
        QCOMPARE(result.message,
                 QStringLiteral("No origin/destination pairs in scenario"));
        QVERIFY(!result.succeeded());
        QVERIFY(result.preparedPaths.isEmpty());
    }

    void test_runtime_overload_returns_no_paths_for_loaded_runtime_without_origins()
    {
        ScenarioRuntime runtime(makeNoOriginDoc());
        QVERIFY(runtime.load());

        PreparedPathService service(
            CargoNetSim::CargoNetSimController::getInstance()
                .getConfigController(),
            CargoNetSim::CargoNetSimController::getInstance()
                .getNetworkController(),
            CargoNetSim::CargoNetSimController::getInstance()
                .getRegionDataController(),
            CargoNetSim::CargoNetSimController::getInstance()
                .getVehicleController());

        const auto result = service.discoverAndPrepare(runtime, 5);

        QCOMPARE(result.status,
                 PreparedPathServiceStatus::NoPathsFound);
        QCOMPARE(result.message,
                 QStringLiteral("No origin/destination pairs in scenario"));
        QVERIFY(!result.succeeded());
        QVERIFY(result.preparedPaths.isEmpty());
    }
};

QTEST_MAIN(PreparedPathApplicationServiceTest)
#include "PreparedPathApplicationServiceTest.moc"
