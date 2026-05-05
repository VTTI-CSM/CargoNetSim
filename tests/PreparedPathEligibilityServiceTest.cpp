#include <QTest>

#include "Backend/Commons/TransportationMode.h"
#include "Backend/Models/Path.h"
#include "Backend/Models/PathSegment.h"
#include "Backend/Scenario/PathPreparationService.h"
#include "Backend/Scenario/PreparedPathEligibilityService.h"
#include "Backend/Scenario/ScenarioDocument.h"

class PreparedPathEligibilityServiceTest : public QObject
{
    Q_OBJECT

    static CargoNetSim::Backend::Path *makePath(
        int pathId,
        CargoNetSim::Backend::TransportationTypes::TransportationMode mode)
    {
        using CargoNetSim::Backend::Path;
        using CargoNetSim::Backend::PathSegment;

        QList<PathSegment *> segments{
            new PathSegment(QStringLiteral("seg"),
                            QStringLiteral("A"),
                            QStringLiteral("B"), mode)};
        return new Path(pathId, 0.0, 0.0, 0.0, {}, segments);
    }

private slots:
    void test_prepared_paths_capture_backend_requirements()
    {
        using namespace CargoNetSim::Backend::Scenario;

        ScenarioDocument doc;
        const PreparedPathSet prepared =
            PathPreparationService::prepareDiscoveredPaths(
                {makePath(1, CargoNetSim::Backend::
                             TransportationTypes::
                                 TransportationMode::Ship)},
                doc, nullptr, nullptr, nullptr, nullptr);

        QCOMPARE(prepared.records().size(), size_t(1));
        QVERIFY(prepared.records().front().requirements.terminalRequired);
        QVERIFY(prepared.records().front().requirements.shipRequired);
        QVERIFY(!prepared.records().front().requirements.trainRequired);
        QVERIFY(!prepared.records().front().requirements.truckRequired);
    }

    void test_evaluate_blocks_missing_terminal_consumer()
    {
        using namespace CargoNetSim::Backend::Scenario;

        PreparedPathRequirements requirements;
        requirements.shipRequired = true;

        SimulatorAvailability availability;
        availability.terminalAvailable = false;
        availability.shipAvailable     = true;

        const auto eligibility =
            PreparedPathEligibilityService::evaluate(requirements,
                                                    availability);
        QVERIFY(!eligibility.selectable);
        QVERIFY(!eligibility.simulatable);
        QVERIFY(
            eligibility.blockingReason.contains(QStringLiteral("TerminalSim")));
    }

    void test_evaluate_blocks_missing_train_consumer()
    {
        using namespace CargoNetSim::Backend::Scenario;

        PreparedPathRequirements requirements;
        requirements.trainRequired = true;

        SimulatorAvailability availability;
        availability.terminalAvailable = true;
        availability.trainAvailable    = false;

        const auto eligibility =
            PreparedPathEligibilityService::evaluate(requirements,
                                                    availability);
        QVERIFY(!eligibility.selectable);
        QVERIFY(!eligibility.simulatable);
        QVERIFY(
            eligibility.blockingReason.contains(QStringLiteral("NeTrainSim")));
    }

    void test_evaluate_blocks_truck_execution()
    {
        using namespace CargoNetSim::Backend::Scenario;

        PreparedPathRequirements requirements;
        requirements.truckRequired = true;

        SimulatorAvailability availability;
        availability.terminalAvailable = true;
        availability.truckAvailable    = true;

        const auto eligibility =
            PreparedPathEligibilityService::evaluate(requirements,
                                                    availability);
        QVERIFY(!eligibility.selectable);
        QVERIFY(!eligibility.simulatable);
        QVERIFY(eligibility.blockingReason.contains(
            QStringLiteral("truck-backed execution is not supported")));
    }

    void test_validateSelection_reports_path_specific_reason()
    {
        using namespace CargoNetSim::Backend::Scenario;

        ScenarioDocument doc;
        const PreparedPathSet prepared =
            PathPreparationService::prepareDiscoveredPaths(
                {makePath(7, CargoNetSim::Backend::
                             TransportationTypes::
                                 TransportationMode::Train)},
                doc, nullptr, nullptr, nullptr, nullptr);

        SimulatorAvailability availability;
        availability.terminalAvailable = false;
        availability.trainAvailable    = false;

        QString err;
        QVERIFY(!PreparedPathEligibilityService::validateSelection(
            prepared, prepared.pathIdentities(), availability, &err));
        QVERIFY(err.contains(QStringLiteral("Path 7")));
        QVERIFY(err.contains(QStringLiteral("TerminalSim")));
    }
};

QTEST_MAIN(PreparedPathEligibilityServiceTest)
#include "PreparedPathEligibilityServiceTest.moc"
