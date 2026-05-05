#include <QCoreApplication>
#include <QJsonObject>
#include <QJsonValue>
#include <QTest>
#include <QVariantMap>
#include <cmath>

#include "Backend/Commons/TerminalInterface.h"
#include "Backend/Commons/TransportationMode.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Controllers/ConfigController.h"
#include "Backend/Models/Path.h"
#include "Backend/Models/PathSegment.h"
#include "Backend/Models/Terminal.h"
#include "Backend/Scenario/ResultsExtractor.h"

class ResultsExtractorTest : public QObject
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

    void test_empty_paths_returns_empty_results()
    {
        CargoNetSim::Backend::Scenario::ResultsExtractor ex(
            /*shipClient=*/nullptr,
            /*trainClient=*/nullptr,
            /*truckManager=*/nullptr,
            /*terminalClient=*/nullptr,
            /*config=*/nullptr);
        QList<CargoNetSim::Backend::Path *> paths;
        auto results = ex.extract(paths);
        QVERIFY(results.isEmpty());
    }

    void test_extract_returns_per_path_terminal_only_costs()
    {
        // No clients → per-mode segment cost returns 0 → edgeCosts == 0.
        // Single Train segment with estimated_cost.previousTerminalCost
        // and nextTerminalCost set → terminalCosts pulled from segment
        // attributes (totalTerminalCosts: single segment counts FULL prev
        // + FULL next = 100 + 200 = 300).
        using namespace CargoNetSim::Backend;

        QJsonObject estCost;
        estCost["previousTerminalCost"] = 100.0;
        estCost["nextTerminalCost"]     = 200.0;
        QJsonObject attrs;
        attrs["estimated_cost"] = estCost;
        auto *seg = new PathSegment(
            "seg0", "A", "B",
            TransportationTypes::TransportationMode::Train, attrs);
        QList<PathTerminal>  emptyTerms;
        QList<PathSegment *> segs{seg};
        auto *path = new Path(7, 0.0, 0.0, 0.0, emptyTerms, segs);
        path->setEffectiveContainerCount(10);

        auto &ctl =
            CargoNetSim::CargoNetSimController::getInstance();
        CargoNetSim::Backend::Scenario::ResultsExtractor ex(
            /*ship=*/nullptr, /*train=*/nullptr,
            /*truck=*/nullptr, /*terminal=*/nullptr,
            ctl.getConfigController());

        const auto results = ex.extract({path});
        QCOMPARE(results.size(), 1);
        QCOMPARE(results[0].pathId,        7);
        QCOMPARE(results[0].effectiveContainerCount, 10);
        QCOMPARE(results[0].edgeCosts,     0.0);
        QCOMPARE(results[0].terminalCosts, 300.0);
        QCOMPARE(results[0].totalCost,     300.0);

        QVERIFY(!results[0].canonicalPathKey.isEmpty());

        delete path;  // owns segments
    }

    void test_dwell_time_missing_is_zero()
    {
        CargoNetSim::Backend::Scenario::ResultsExtractor ex(
            nullptr, nullptr, nullptr, nullptr, nullptr);
        QCOMPARE(ex.calculateTerminalDwellTime(QJsonObject{}), 0.0);
    }

    void test_dwell_time_gamma()
    {
        QJsonObject params;
        params["shape"] = 2.0;
        params["scale"] = 3600.0;
        QJsonObject dwell;
        dwell["method"]     = "gamma";
        dwell["parameters"] = params;
        QJsonObject config;
        config["dwell_time"] = dwell;

        CargoNetSim::Backend::Scenario::ResultsExtractor ex(
            nullptr, nullptr, nullptr, nullptr, nullptr);
        QCOMPARE(ex.calculateTerminalDwellTime(config), 7200.0);
    }

    void test_dwell_time_exponential()
    {
        QJsonObject params;
        params["scale"] = 7200.0;
        QJsonObject dwell;
        dwell["method"]     = "exponential";
        dwell["parameters"] = params;
        QJsonObject config;
        config["dwell_time"] = dwell;

        CargoNetSim::Backend::Scenario::ResultsExtractor ex(
            nullptr, nullptr, nullptr, nullptr, nullptr);
        QCOMPARE(ex.calculateTerminalDwellTime(config), 7200.0);
    }

    void test_dwell_time_normal()
    {
        QJsonObject params;
        params["mean"] = 3600.0;
        QJsonObject dwell;
        dwell["method"]     = "normal";
        dwell["parameters"] = params;
        QJsonObject config;
        config["dwell_time"] = dwell;

        CargoNetSim::Backend::Scenario::ResultsExtractor ex(
            nullptr, nullptr, nullptr, nullptr, nullptr);
        QCOMPARE(ex.calculateTerminalDwellTime(config), 3600.0);
    }

    void test_dwell_time_lognormal()
    {
        const double mean  = std::log(3600.0);
        const double sigma = 0.25;
        QJsonObject params;
        params["mean"]  = mean;
        params["sigma"] = sigma;
        QJsonObject dwell;
        dwell["method"]     = "lognormal";
        dwell["parameters"] = params;
        QJsonObject config;
        config["dwell_time"] = dwell;

        CargoNetSim::Backend::Scenario::ResultsExtractor ex(
            nullptr, nullptr, nullptr, nullptr, nullptr);
        const double expected =
            3600.0 * std::exp(sigma * sigma / 2.0);
        QCOMPARE(ex.calculateTerminalDwellTime(config), expected);
    }

    void test_customs_applied_when_probability_and_delay()
    {
        QJsonObject customs;
        customs["probability"] = 0.5;
        customs["delay_mean"]  = 100.0;
        QJsonObject config;
        config["customs"] = customs;

        CargoNetSim::Backend::Scenario::ResultsExtractor ex(
            nullptr, nullptr, nullptr, nullptr, nullptr);
        double delay = 0.0, cost = 0.0;
        QVERIFY(ex.calculateTerminalCustoms(config, delay, cost));
        QCOMPARE(delay, 50.0);
        QCOMPARE(cost,   0.0);  // customs method doesn't set cost
    }

    void test_customs_skipped_when_missing()
    {
        CargoNetSim::Backend::Scenario::ResultsExtractor ex(
            nullptr, nullptr, nullptr, nullptr, nullptr);
        double delay = 0.0, cost = 0.0;
        QVERIFY(!ex.calculateTerminalCustoms(QJsonObject{},
                                             delay, cost));
    }

    void test_direct_costs_with_and_without_customs()
    {
        QJsonObject cost;
        cost["fixed_fees"]   = 400.0;
        cost["customs_fees"] = 100.0;
        cost["risk_factor"]  = 0.015;
        QJsonObject config;
        config["cost"] = cost;

        CargoNetSim::Backend::Scenario::ResultsExtractor ex(
            nullptr, nullptr, nullptr, nullptr, nullptr);
        // Without customs applied: fixed + risk_factor * 1.0
        QCOMPARE(ex.calculateTerminalDirectCosts(config, false),
                 400.0 + 0.015);
        // With customs: add customs_fees
        QCOMPARE(ex.calculateTerminalDirectCosts(config, true),
                 400.0 + 100.0 + 0.015);
    }

    void test_single_terminal_cost_null_terminal_returns_zero()
    {
        CargoNetSim::Backend::Scenario::ResultsExtractor ex(
            nullptr, nullptr, nullptr, nullptr, nullptr);
        QVariantMap weights;
        QCOMPARE(ex.calculateSingleTerminalCost(nullptr, weights, 10),
                 0.0);
    }

    void test_single_terminal_cost_combines_dwell_customs_direct()
    {
        using namespace CargoNetSim::Backend;

        // Config: gamma dwell (shape=2, scale=3600 → 7200 s/container),
        //         customs (prob=0.5, delay_mean=100 s → 50 s/container),
        //         cost (fixed=400, customs_fees=100, risk=0.015 → 500.015/container).
        QJsonObject dwellParams;
        dwellParams["shape"] = 2.0;
        dwellParams["scale"] = 3600.0;
        QJsonObject dwell;
        dwell["method"]     = "gamma";
        dwell["parameters"] = dwellParams;

        QJsonObject customs;
        customs["probability"] = 0.5;
        customs["delay_mean"]  = 100.0;

        QJsonObject cost;
        cost["fixed_fees"]   = 400.0;
        cost["customs_fees"] = 100.0;
        cost["risk_factor"]  = 0.015;

        QJsonObject config;
        config["dwell_time"] = dwell;
        config["customs"]    = customs;
        config["cost"]       = cost;

        QMap<TerminalTypes::TerminalInterface,
             QSet<TransportationTypes::TransportationMode>>
            noInterfaces;
        Terminal terminal(QStringList{"T1"}, "Terminal 1", config,
                          noInterfaces, "USA");

        // Weights: terminal_delay=1.0, terminal_cost=1.0 (in "default" sub-map).
        QVariantMap defaultWeights;
        defaultWeights["terminal_delay"] = 1.0;
        defaultWeights["terminal_cost"]  = 1.0;
        QVariantMap weights;
        weights["default"] = defaultWeights;

        CargoNetSim::Backend::Scenario::ResultsExtractor ex(
            nullptr, nullptr, nullptr, nullptr, nullptr);

        // Per-container: dwell=7200 s, customs=50 s, direct=500.015.
        // 10 containers: delay=72500 s, cost=5000.15.
        // Final = 72500 * 1.0 + 5000.15 * 1.0.
        const double expected = 72500.0 + 5000.15;
        QCOMPARE(ex.calculateSingleTerminalCost(&terminal, weights, 10),
                 expected);
    }

    void test_total_terminal_costs_empty_segments_is_zero()
    {
        CargoNetSim::Backend::Scenario::ResultsExtractor ex(
            nullptr, nullptr, nullptr, nullptr, nullptr);
        QList<CargoNetSim::Backend::PathSegment *> segs;
        QList<CargoNetSim::Backend::PathTerminal> terms;
        QCOMPARE(ex.calculateTerminalCosts(segs, terms,
                                           QVariantMap{}, 10),
                 0.0);
    }

    void test_total_terminal_costs_splits_previous_and_next()
    {
        using namespace CargoNetSim::Backend;

        // Two segments, each with estimated_cost.previousTerminalCost=100,
        //                         estimated_cost.nextTerminalCost=200.
        // Seg 0 (first): full prev + half next = 100 + 100 = 200.
        // Seg 1 (last):  half prev + full next = 50  + 200 = 250.
        // Total = 450.
        auto makeSeg =
            [](const QString &id) -> PathSegment *
        {
            QJsonObject estCost;
            estCost["previousTerminalCost"] = 100.0;
            estCost["nextTerminalCost"]     = 200.0;
            QJsonObject attrs;
            attrs["estimated_cost"] = estCost;
            auto *s = new PathSegment(
                id, "A", "B",
                TransportationTypes::TransportationMode::Train,
                attrs);
            return s;
        };
        auto *s0 = makeSeg("seg0");
        auto *s1 = makeSeg("seg1");
        QList<PathSegment *> segs{s0, s1};
        QList<PathTerminal>  terms;  // unused by current body

        CargoNetSim::Backend::Scenario::ResultsExtractor ex(
            nullptr, nullptr, nullptr, nullptr, nullptr);
        QCOMPARE(ex.calculateTerminalCosts(segs, terms,
                                           QVariantMap{}, 10),
                 450.0);

        qDeleteAll(segs);
    }

    void test_ship_segment_cost_returns_zero_without_client()
    {
        // Null shipClient → early return 0.0 (preserves SVW behavior
        // that crashes on shipClient->getAllShipsStates()).
        CargoNetSim::Backend::Scenario::ResultsExtractor ex(
            /*ship=*/nullptr, nullptr, nullptr, nullptr, nullptr);
        CargoNetSim::Backend::Path        *p = nullptr;
        CargoNetSim::Backend::PathSegment *s = nullptr;
        QCOMPARE(ex.calculateShipSegmentCost(p, s, 0,
                                             QVariantMap{},
                                             QVariantMap{}, 10),
                 0.0);
    }

    void test_train_segment_cost_returns_zero_without_client()
    {
        // Null trainClient → early return 0.0.
        CargoNetSim::Backend::Scenario::ResultsExtractor ex(
            /*ship=*/nullptr, /*train=*/nullptr,
            /*truck=*/nullptr, /*terminal=*/nullptr,
            /*config=*/nullptr);
        CargoNetSim::Backend::Path        *p = nullptr;
        CargoNetSim::Backend::PathSegment *s = nullptr;
        QCOMPARE(ex.calculateTrainSegmentCost(p, s, 0,
                                              QVariantMap{},
                                              QVariantMap{}, 10),
                 0.0);
    }

    void test_edge_costs_empty_path_is_zero()
    {
        // Empty segment list → 0.0 regardless of clients / weights.
        CargoNetSim::Backend::Scenario::ResultsExtractor ex(
            nullptr, nullptr, nullptr, nullptr, nullptr);
        QList<CargoNetSim::Backend::PathSegment *> segments;
        QCOMPARE(ex.calculateEdgeCosts(nullptr, segments,
                                       QVariantMap{},
                                       QVariantMap{}, 10),
                 0.0);
    }

    void test_set_segment_actual_details_accumulates()
    {
        using namespace CargoNetSim::Backend;

        auto *s = new PathSegment(
            "seg0", "A", "B",
            TransportationTypes::TransportationMode::Train);

        CargoNetSim::Backend::Scenario::ResultsExtractor ex(
            nullptr, nullptr, nullptr, nullptr, nullptr);

        // First call: {travelTime: 10} under "actual_values".
        QMap<QString, double> first;
        first["travelTime"] = 10.0;
        ex.setSegmentActualDetails(s, first, "actual_values");

        // Second call: {travelTime: 5}. Existing key should accumulate
        // to 15, NOT be overwritten.
        QMap<QString, double> second;
        second["travelTime"] = 5.0;
        ex.setSegmentActualDetails(s, second, "actual_values");

        const QJsonObject attrs = s->getAttributes();
        QVERIFY(attrs.contains("actual_values"));
        const QJsonObject av = attrs["actual_values"].toObject();
        QCOMPARE(av["travelTime"].toDouble(), 15.0);

        // deleteSegmentDetails wipes the whole key.
        ex.deleteSegmentDetails(s, "actual_values");
        QVERIFY(!s->getAttributes().contains("actual_values"));

        delete s;
    }

    void test_truck_segment_cost_heuristic_with_known_inputs()
    {
        // Truck path is heuristic — no client required; truckCount
        // derived from containerCount/capacity.
        //
        // Inputs:
        //   containerCount = 10
        //   transportModes.truck.average_container_number = 1 (default)
        //   transportModes.truck.risk_factor               = 0.012 (default)
        //   containersPerTruck = 10/10 = 1.0 → ratio = 1/1 = 1.0
        //   travelTime, distance, carbon, energy all remain 0.
        //   risk = 0.012 * 1.0 = 0.012
        //
        //   Weights: risk=100.0, everything else 0.0 (defaults).
        //   segmentCost = 0 + 0 + 0 + 0 + 0.012 * 100 = 1.2
        using namespace CargoNetSim::Backend;

        QVariantMap modeWeights;
        modeWeights["risk"] = 100.0;
        QVariantMap transportModes;  // empty → truck defaults apply

        auto *segment = new PathSegment(
            "seg0", "A", "B",
            TransportationTypes::TransportationMode::Truck);

        CargoNetSim::Backend::Scenario::ResultsExtractor ex(
            nullptr, nullptr, nullptr, nullptr, nullptr);
        QCOMPARE(ex.calculateTruckSegmentCost(nullptr, segment, 0,
                                              modeWeights,
                                              transportModes, 10),
                 1.2);

        // Segment must have received actual_values + actual_cost
        // writeback (SVW contract).
        const QJsonObject attrs = segment->getAttributes();
        QVERIFY(attrs.contains("actual_values"));
        QVERIFY(attrs.contains("actual_cost"));

        delete segment;
    }

    void test_extract_uses_per_path_effective_container_count()
    {
        using namespace CargoNetSim::Backend;

        auto *segA = new PathSegment(
            "segA", "O1", "D1",
            TransportationTypes::TransportationMode::Train);
        auto *segB = new PathSegment(
            "segB", "O2", "D2",
            TransportationTypes::TransportationMode::Train);

        auto *pathA = new Path(1, 0.0, 0.0, 0.0,
                               QList<PathTerminal>{},
                               QList<PathSegment *>{segA});
        auto *pathB = new Path(1, 0.0, 0.0, 0.0,
                               QList<PathTerminal>{},
                               QList<PathSegment *>{segB});
        pathA->setEffectiveContainerCount(8);
        pathB->setEffectiveContainerCount(0);

        auto &ctl =
            CargoNetSim::CargoNetSimController::getInstance();
        CargoNetSim::Backend::Scenario::ResultsExtractor ex(
            nullptr, nullptr, nullptr, nullptr,
            ctl.getConfigController());

        const auto results = ex.extract({pathA, pathB});
        QCOMPARE(results.size(), 1);
        QCOMPARE(results[0].pathId, 1);
        QCOMPARE(results[0].effectiveContainerCount, 8);
        QCOMPARE(results[0].originId, QStringLiteral("O1"));
        QCOMPARE(results[0].destinationId, QStringLiteral("D1"));

        delete pathA;
        delete pathB;
    }

    void test_extract_execution_results_keeps_actuals_off_prepared_path_segments()
    {
        using namespace CargoNetSim::Backend;

        auto *segment = new PathSegment(
            "seg0", "A", "B",
            TransportationTypes::TransportationMode::Truck);
        auto *path = new Path(9, 0.0, 0.0, 0.0,
                              QList<PathTerminal>{},
                              QList<PathSegment *>{segment});
        path->setEffectiveContainerCount(10);

        auto &ctl =
            CargoNetSim::CargoNetSimController::getInstance();
        CargoNetSim::Backend::Scenario::ResultsExtractor ex(
            nullptr, nullptr, nullptr, nullptr,
            ctl.getConfigController());

        const auto results =
            ex.extractExecutionResults({path},
                                       {QStringLiteral("prepared-A-B-r0")});
        QCOMPARE(results.size(), 1);

        const auto *result = results.findByPathIdentity(
            QStringLiteral("prepared-A-B-r0"));
        QVERIFY(result != nullptr);
        QCOMPARE(result->pathId, 9);
        QCOMPARE(result->segmentResults.size(), 1);
        QVERIFY(result->segmentResults.first().actualMetrics.available);
        QVERIFY(result->segmentResults.first().actualCosts.available);

        const QJsonObject attrs = segment->getAttributes();
        QVERIFY(!attrs.contains("actual_values"));
        QVERIFY(!attrs.contains("actual_cost"));

        delete path;
    }
};

QTEST_MAIN(ResultsExtractorTest)
#include "ResultsExtractorTest.moc"
