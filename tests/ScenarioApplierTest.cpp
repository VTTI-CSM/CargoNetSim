#include <QCoreApplication>
#include <QDir>
#include <QJsonObject>
#include <QTest>

#include "Backend/Commons/TerminalInterface.h"
#include "Backend/Commons/TransportationMode.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Controllers/ConfigController.h"
#include "Backend/Controllers/RegionDataController.h"
#include "Backend/Controllers/VehicleController.h"
#include "Backend/Models/Terminal.h"
#include "Backend/Scenario/InterfaceConversion.h"
#include "Backend/Scenario/ScenarioApplier.h"
#include "Backend/Scenario/TerminalTypeDefaults.h"
#include "Backend/Scenario/ScenarioDocument.h"
#include "Backend/Scenario/ScenarioLinker.h"
#include "Backend/Scenario/ScenarioRegistry.h"
#include "Backend/Scenario/ScenarioSerializer.h"
#include "Backend/Scenario/ScenarioValidator.h"
#include "Backend/Scenario/TruckFleetSpec.h"
#include "Backend/Scenario/ValidationIssue.h"

class ScenarioApplierTest : public QObject
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

    void test_truck_fleet_spec_defaults()
    {
        CargoNetSim::Backend::Scenario::TruckFleetSpec s;
        QVERIFY(s.files.isEmpty());
        QVERIFY(s.inline_.isEmpty());
    }

    // ---- InterfaceConversion ----

    void test_interface_conversion_sea_port_strings_to_enum_map()
    {
        using namespace CargoNetSim::Backend;
        const QStringList land = { "Truck", "Rail" };
        const QStringList sea  = { "Ship" };

        auto map = Scenario::InterfaceConversion::toBackendInterfaces(land, sea);
        QVERIFY( map.contains(TerminalTypes::TerminalInterface::LAND_SIDE));
        QVERIFY( map.contains(TerminalTypes::TerminalInterface::SEA_SIDE));

        auto landSet = map[TerminalTypes::TerminalInterface::LAND_SIDE];
        QVERIFY(landSet.contains(TransportationTypes::TransportationMode::Truck));
        QVERIFY(landSet.contains(TransportationTypes::TransportationMode::Train));

        auto seaSet = map[TerminalTypes::TerminalInterface::SEA_SIDE];
        QVERIFY(seaSet.contains(TransportationTypes::TransportationMode::Ship));
    }

    void test_interface_conversion_empty_inputs_produce_empty_map()
    {
        using namespace CargoNetSim::Backend;
        auto map = Scenario::InterfaceConversion::toBackendInterfaces({}, {});
        QVERIFY(map.isEmpty());
    }

    void test_interface_conversion_unknown_mode_is_skipped()
    {
        using namespace CargoNetSim::Backend;
        auto map = Scenario::InterfaceConversion::toBackendInterfaces(
            { "Truck", "Helicopter" }, {});
        QCOMPARE(map.value(TerminalTypes::TerminalInterface::LAND_SIDE).size(), 1);
    }

    void test_interface_conversion_round_trip_back_to_strings()
    {
        using namespace CargoNetSim::Backend;
        QMap<TerminalTypes::TerminalInterface,
             QSet<TransportationTypes::TransportationMode>> m;
        m[TerminalTypes::TerminalInterface::LAND_SIDE] = {
            TransportationTypes::TransportationMode::Truck,
            TransportationTypes::TransportationMode::Train };
        m[TerminalTypes::TerminalInterface::SEA_SIDE] = {
            TransportationTypes::TransportationMode::Ship };

        auto pair = Scenario::InterfaceConversion::fromBackendInterfaces(m);
        QCOMPARE(pair.first.size(),  2);
        QVERIFY(pair.first.contains("Truck"));
        QVERIFY(pair.first.contains("Rail"));
        QCOMPARE(pair.second, QStringList{"Ship"});
    }

    // ---- ScenarioRegistry ----

    void test_registry_starts_empty()
    {
        CargoNetSim::Backend::Scenario::ScenarioRegistry r;
        QCOMPARE(r.terminalCount(), 0);
        QVERIFY(r.terminal("anything") == nullptr);
    }

    void test_registry_adds_and_looks_up_terminal()
    {
        using namespace CargoNetSim::Backend;
        Scenario::ScenarioRegistry r;

        auto *t = new Terminal({ "T1" }, "T1 Display", {}, {}, "USA");
        r.addTerminal("T1", t);   // registry takes ownership

        QCOMPARE(r.terminalCount(), 1);
        QVERIFY(r.terminal("T1") != nullptr);
        QCOMPARE(r.terminal("T1")->getDisplayName(), QStringLiteral("T1 Display"));
    }

    void test_registry_rejects_duplicate_id_and_does_not_leak()
    {
        using namespace CargoNetSim::Backend;
        Scenario::ScenarioRegistry r;

        auto *first  = new Terminal({ "T1" }, "First",  {}, {}, "USA");
        auto *second = new Terminal({ "T1" }, "Second", {}, {}, "USA");

        QVERIFY( r.addTerminal("T1", first));
        QVERIFY(!r.addTerminal("T1", second));  // refused + registry deletes it

        QCOMPARE(r.terminal("T1")->getDisplayName(), QStringLiteral("First"));
    }

    void test_registry_clear_deletes_terminals()
    {
        using namespace CargoNetSim::Backend;
        Scenario::ScenarioRegistry r;
        r.addTerminal("T1", new Terminal({ "T1" }, "", {}, {}, "USA"));
        r.addTerminal("T2", new Terminal({ "T2" }, "", {}, {}, "USA"));
        QCOMPARE(r.terminalCount(), 2);
        r.clear();
        QCOMPARE(r.terminalCount(), 0);
    }

    void test_registry_stores_truck_fleet_spec()
    {
        using namespace CargoNetSim::Backend;
        Scenario::ScenarioRegistry r;
        Scenario::TruckFleetSpec spec;
        spec.files = { "/abs/trucks.json" };
        r.setTruckFleet(spec);
        QCOMPARE(r.truckFleet().files, QStringList{ "/abs/trucks.json" });
    }

    // ---- ScenarioApplier (skeleton) ----

    void test_applier_apply_empty_document_clears_and_succeeds()
    {
        using namespace CargoNetSim::Backend::Scenario;

        // Obtain the singleton — tests assume no other test is mutating it
        // concurrently (ScenarioApplierTest is single-process).
        auto &ctl = CargoNetSim::CargoNetSimController::getInstance();
        auto *regions = ctl.getRegionDataController();
        QVERIFY(regions != nullptr);

        // Seed one region so clear() is observable.
        regions->addRegion("PreExisting");
        QVERIFY(regions->getRegionData("PreExisting") != nullptr);

        ScenarioDocument doc;
        ScenarioRegistry registry;
        QString err;
        QVERIFY2(ScenarioApplier::apply(doc, ctl, registry, &err), qPrintable(err));

        // After apply, the pre-existing region must be gone.
        QVERIFY(regions->getRegionData("PreExisting") == nullptr);
        QCOMPARE(registry.terminalCount(), 0);
    }

    // ---- ScenarioApplier — regions ----

    void test_applier_adds_regions_with_color_and_center_variables()
    {
        using namespace CargoNetSim::Backend::Scenario;
        auto &ctl = CargoNetSim::CargoNetSimController::getInstance();

        ScenarioDocument doc;
        RegionSpec usa;
        usa.name  = "USA";
        usa.color = "#ff8800";
        usa.localOrigin    = { 38.0, -97.0 };
        usa.globalPosition = { 38.0, -97.0 };
        doc.addRegion(usa);

        ScenarioRegistry registry;
        QString err;
        QVERIFY2(ScenarioApplier::apply(doc, ctl, registry, &err), qPrintable(err));

        auto *rd = ctl.getRegionDataController()->getRegionData("USA");
        QVERIFY(rd != nullptr);
        QCOMPARE(rd->getVariable("color").toString(), QStringLiteral("#ff8800"));

        auto center = rd->getVariable("scenarioRegionCenter").toMap();
        QCOMPARE(center.value("latitude").toDouble(),          38.0);
        QCOMPARE(center.value("longitude").toDouble(),        -97.0);
        QCOMPARE(center.value("shared_latitude").toDouble(),   38.0);
        QCOMPARE(center.value("shared_longitude").toDouble(), -97.0);

        // Must NOT touch the GUI-owned "regionCenterPoint" key.
        QVERIFY(!rd->getVariable("regionCenterPoint").isValid());
    }

    void test_applier_fails_on_empty_region_name()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;
        // Bypass the document invariant by constructing after — verify that
        // the applier itself also guards against empty names if any slip
        // through (defence in depth).
        RegionSpec r;
        // Note: doc.addRegion rejects empty-name, so to test the applier
        // guard we construct the document manually:
        doc.regions.insert("", r);

        auto &ctl = CargoNetSim::CargoNetSimController::getInstance();
        ScenarioRegistry registry;
        QString err;
        QVERIFY(!ScenarioApplier::apply(doc, ctl, registry, &err));
        QVERIFY(err.contains("region"));
    }

    // ---- ScenarioApplier — train networks ----

    void test_applier_loads_train_network_from_relative_path()
    {
        using namespace CargoNetSim::Backend::Scenario;

        ScenarioDocument doc;
        RegionSpec r; r.name = "USA"; doc.addRegion(r);

        NetworkSpec rail;
        rail.name = "USA_rail";
        rail.type = NetworkSpec::Type::Rail;
        rail.files["nodes"] = QDir(QCoreApplication::applicationDirPath())
                                  .filePath("fixtures/scenario/rail_nodes.dat");
        rail.files["links"] = QDir(QCoreApplication::applicationDirPath())
                                  .filePath("fixtures/scenario/rail_links.dat");
        doc.addNetwork("USA", rail);

        auto &ctl = CargoNetSim::CargoNetSimController::getInstance();
        ScenarioRegistry registry;
        QString err;
        QVERIFY2(ScenarioApplier::apply(doc, ctl, registry, &err), qPrintable(err));

        auto *rd = ctl.getRegionDataController()->getRegionData("USA");
        QVERIFY(rd->trainNetworkExists("USA_rail"));
        QVERIFY(rd->getTrainNetwork("USA_rail") != nullptr);
    }

    void test_applier_reports_missing_rail_file()
    {
        using namespace CargoNetSim::Backend::Scenario;

        ScenarioDocument doc;
        RegionSpec r; r.name = "USA"; doc.addRegion(r);

        NetworkSpec rail;
        rail.name = "Broken";
        rail.type = NetworkSpec::Type::Rail;
        rail.files["nodes"] = "/absolutely/does/not/exist/nodes.dat";
        rail.files["links"] = "/absolutely/does/not/exist/links.dat";
        doc.addNetwork("USA", rail);

        auto &ctl = CargoNetSim::CargoNetSimController::getInstance();
        ScenarioRegistry registry;
        QString err;
        QVERIFY(!ScenarioApplier::apply(doc, ctl, registry, &err));
        QVERIFY(err.contains("Broken"));
    }

    // ---- ScenarioApplier — truck networks ----

    void test_applier_loads_truck_network()
    {
        using namespace CargoNetSim::Backend::Scenario;

        ScenarioDocument doc;
        RegionSpec r; r.name = "USA"; doc.addRegion(r);

        NetworkSpec truck;
        truck.name = "USA_truck";
        truck.type = NetworkSpec::Type::Truck;
        truck.files["config"] = QDir(QCoreApplication::applicationDirPath())
                                    .filePath("fixtures/scenario/truck_config.dat");
        doc.addNetwork("USA", truck);

        auto &ctl = CargoNetSim::CargoNetSimController::getInstance();
        ScenarioRegistry registry;
        QString err;
        QVERIFY2(ScenarioApplier::apply(doc, ctl, registry, &err), qPrintable(err));

        auto *rd = ctl.getRegionDataController()->getRegionData("USA");
        QVERIFY(rd->truckNetworkExists("USA_truck"));
    }

    void test_applier_reports_missing_truck_config()
    {
        using namespace CargoNetSim::Backend::Scenario;

        ScenarioDocument doc;
        RegionSpec r; r.name = "USA"; doc.addRegion(r);

        NetworkSpec truck;
        truck.name = "BrokenTruck";
        truck.type = NetworkSpec::Type::Truck;
        // files["config"] deliberately missing

        doc.addNetwork("USA", truck);

        auto &ctl = CargoNetSim::CargoNetSimController::getInstance();
        ScenarioRegistry registry;
        QString err;
        QVERIFY(!ScenarioApplier::apply(doc, ctl, registry, &err));
        QVERIFY(err.contains("BrokenTruck"));
    }

    // ---- ScenarioApplier — terminals ----

    void test_applier_creates_terminal_with_type_defaults_for_interfaces()
    {
        using namespace CargoNetSim::Backend::Scenario;

        ScenarioDocument doc;
        RegionSpec r; r.name = "USA"; doc.addRegion(r);

        TerminalPlacement t;
        t.id = "SP1";
        t.type = "Sea Port Terminal";
        t.region = "USA";
        t.mode = TerminalPlacement::PositionMode::LatLon;
        t.latLon = { 40.7, -74.0 };
        // Deliberately omit t.interfaces — should default from type.
        doc.addTerminal(t);

        auto &ctl = CargoNetSim::CargoNetSimController::getInstance();
        ScenarioRegistry registry;
        QString err;
        QVERIFY2(ScenarioApplier::apply(doc, ctl, registry, &err), qPrintable(err));

        auto *term = registry.terminal("SP1");
        QVERIFY(term != nullptr);
        QCOMPARE(term->getRegion(), QStringLiteral("USA"));

        auto iface = term->getInterfaces();
        using namespace CargoNetSim::Backend;
        QVERIFY(iface.contains(TerminalTypes::TerminalInterface::LAND_SIDE));
        QVERIFY(iface.contains(TerminalTypes::TerminalInterface::SEA_SIDE));
        QVERIFY(iface[TerminalTypes::TerminalInterface::LAND_SIDE]
                    .contains(TransportationTypes::TransportationMode::Truck));
        QVERIFY(iface[TerminalTypes::TerminalInterface::LAND_SIDE]
                    .contains(TransportationTypes::TransportationMode::Train));
        QVERIFY(iface[TerminalTypes::TerminalInterface::SEA_SIDE]
                    .contains(TransportationTypes::TransportationMode::Ship));
    }

    void test_applier_explicit_interface_override_wins_over_defaults()
    {
        using namespace CargoNetSim::Backend::Scenario;

        ScenarioDocument doc;
        RegionSpec r; r.name = "USA"; doc.addRegion(r);

        TerminalPlacement t;
        t.id = "OddPort";
        t.type = "Sea Port Terminal";                // default: land=[Truck,Rail], sea=[Ship]
        t.region = "USA";
        t.mode = TerminalPlacement::PositionMode::LatLon;
        t.latLon = { 0, 0 };
        t.interfaces.isSet    = true;
        using Mode = CargoNetSim::Backend::TransportationTypes::TransportationMode;
        t.interfaces.landSide = { Mode::Truck };     // explicit: no Train
        t.interfaces.seaSide  = { Mode::Ship };
        doc.addTerminal(t);

        auto &ctl = CargoNetSim::CargoNetSimController::getInstance();
        ScenarioRegistry registry;
        QString err;
        QVERIFY2(ScenarioApplier::apply(doc, ctl, registry, &err), qPrintable(err));

        using namespace CargoNetSim::Backend;
        auto iface = registry.terminal("OddPort")->getInterfaces();
        QVERIFY(!iface[TerminalTypes::TerminalInterface::LAND_SIDE]
                    .contains(TransportationTypes::TransportationMode::Train));
    }

    void test_applier_propagates_properties_into_terminal_config()
    {
        using namespace CargoNetSim::Backend::Scenario;

        ScenarioDocument doc;
        RegionSpec r; r.name = "USA"; doc.addRegion(r);

        TerminalPlacement t;
        t.id = "T1";
        t.type = "Intermodal Land Terminal";
        t.region = "USA";
        t.mode = TerminalPlacement::PositionMode::Scene;
        t.scenePos = { 0, 0 };

        QVariantMap capacity; capacity["max_capacity"] = 5000; capacity["critical_threshold"] = 0.8;
        QVariantMap dwell;    dwell["method"] = "normal";
        QVariantMap dwellParams; dwellParams["mean"] = 1800; dwellParams["std_dev"] = 300;
        dwell["parameters"] = dwellParams;
        t.properties.insert("capacity",   capacity);
        t.properties.insert("dwell_time", dwell);

        doc.addTerminal(t);

        auto &ctl = CargoNetSim::CargoNetSimController::getInstance();
        ScenarioRegistry registry;
        QString err;
        QVERIFY2(ScenarioApplier::apply(doc, ctl, registry, &err), qPrintable(err));

        auto config = registry.terminal("T1")->getConfig();
        QVERIFY(config.contains("capacity"));
        QVERIFY(config.contains("dwell_time"));
        QCOMPARE(config.value("capacity").toObject().value("max_capacity").toInt(), 5000);
    }

    // ---- ScenarioApplier — fleet ----

    void test_applier_loads_trains_and_ships_from_fleet_files()
    {
        using namespace CargoNetSim::Backend::Scenario;

        ScenarioDocument doc;
        doc.fleet.trainsFiles = { QDir(QCoreApplication::applicationDirPath())
                                      .filePath("fixtures/scenario/fleet_trains.json") };
        doc.fleet.shipsFiles  = { QDir(QCoreApplication::applicationDirPath())
                                      .filePath("fixtures/scenario/fleet_ships.json") };

        auto &ctl = CargoNetSim::CargoNetSimController::getInstance();
        ScenarioRegistry registry;
        QString err;
        QVERIFY2(ScenarioApplier::apply(doc, ctl, registry, &err), qPrintable(err));

        QVERIFY(ctl.getVehicleController()->trainCount() >= 1);
        QVERIFY(ctl.getVehicleController()->shipCount()  >= 1);
    }

    void test_applier_multi_file_fleet_does_not_error()
    {
        using namespace CargoNetSim::Backend::Scenario;

        ScenarioDocument doc;
        const QString path = QDir(QCoreApplication::applicationDirPath())
                                 .filePath("fixtures/scenario/fleet_trains.json");
        // Smoke test: a two-element trainsFiles list must not fail or crash.
        // Both entries point to the same file so train IDs overwrite; this
        // verifies the apply path accepts a list without asserting iteration count.
        doc.fleet.trainsFiles = { path, path };

        auto &ctl = CargoNetSim::CargoNetSimController::getInstance();
        ScenarioRegistry registry;
        QString err;
        QVERIFY2(ScenarioApplier::apply(doc, ctl, registry, &err), qPrintable(err));
        QVERIFY(ctl.getVehicleController()->trainCount() >= 1);
    }

    void test_applier_stores_truck_fleet_spec_in_registry()
    {
        using namespace CargoNetSim::Backend::Scenario;

        ScenarioDocument doc;
        doc.fleet.trucksFiles = { "/some/abs/path/trucks.json" };

        QJsonObject truckInline; truckInline["user_id"] = "TRK-1";
        doc.fleet.trucksInline.append(truckInline);

        auto &ctl = CargoNetSim::CargoNetSimController::getInstance();
        ScenarioRegistry registry;
        QString err;
        QVERIFY2(ScenarioApplier::apply(doc, ctl, registry, &err), qPrintable(err));

        QCOMPARE(registry.truckFleet().files, QStringList{ "/some/abs/path/trucks.json" });
        QCOMPARE(registry.truckFleet().inline_.size(), 1);
    }

    // ---- ScenarioApplier — simulation settings (canonical key translation) ----

    void test_applier_pushes_fuel_types_into_config_controller()
    {
        using namespace CargoNetSim::Backend::Scenario;

        ScenarioDocument doc;
        SimulationSettings::Fuel diesel; diesel.energy = 35.8; diesel.carbon = 2.64; diesel.price = 1.2;
        doc.simulation.fuelTypes.insert("Diesel", diesel);
        doc.simulation.carbonRate = 0.05;

        auto &ctl = CargoNetSim::CargoNetSimController::getInstance();
        ScenarioRegistry registry;
        QString err;
        QVERIFY2(ScenarioApplier::apply(doc, ctl, registry, &err), qPrintable(err));

        auto fuelEnergy = ctl.getConfigController()->getFuelEnergy();
        QCOMPARE(fuelEnergy.value("Diesel").toDouble(), 35.8);

        auto fuelPrices = ctl.getConfigController()->getFuelPrices();
        QCOMPARE(fuelPrices.value("Diesel").toDouble(), 1.2);
    }

    void test_applier_sets_simulation_time_step_and_end_time()
    {
        using namespace CargoNetSim::Backend::Scenario;

        ScenarioDocument doc;
        doc.simulation.timeStep = 30;
        doc.simulation.endTime  = 7200.0;

        auto &ctl = CargoNetSim::CargoNetSimController::getInstance();
        ScenarioRegistry registry;
        QString err;
        QVERIFY2(ScenarioApplier::apply(doc, ctl, registry, &err), qPrintable(err));

        auto sim = ctl.getConfigController()->getSimulationParams();
        QCOMPARE(sim.value("time_step").toInt(),      30);
        QCOMPARE(sim.value("end_time").toDouble(), 7200.0);
    }

    // Regression guard: applier must translate Plan 1's short-form scenario
    // fields into the canonical keys ConfigController consumers read. If any
    // of these assertions starts failing, consumers (SettingsWidget,
    // getCostFunctionWeights, getTimeValueOfMoney) silently revert to
    // hardcoded defaults — not an error.
    // ---- Task 20: end-to-end integration ----

    void test_integration_full_scenario_materializes_backend_state()
    {
        using namespace CargoNetSim::Backend::Scenario;

        const QString yamlPath = QDir(QCoreApplication::applicationDirPath())
                                     .filePath("fixtures/scenario/applier_full.yml");
        QString err;
        auto doc = ScenarioSerializer::fromYaml(yamlPath, &err);
        QVERIFY2(doc != nullptr, qPrintable(err));

        const auto issues = ScenarioValidator::validate(*doc);
        int errorCount = 0;
        for (const auto &i : issues)
            if (i.severity == ValidationIssue::Error) ++errorCount;
        QCOMPARE(errorCount, 0);

        auto &ctl = CargoNetSim::CargoNetSimController::getInstance();
        ScenarioRegistry registry;
        QVERIFY2(ScenarioApplier::apply(*doc, ctl, registry, &err), qPrintable(err));

        // Every region declared in the YAML must exist in the backend.
        for (const QString &regionName : doc->regions.keys())
            QVERIFY(ctl.getRegionDataController()->getRegionData(regionName) != nullptr);

        // Every terminal in the document must exist in the registry.
        for (const QString &terminalId : doc->terminals.keys())
            QVERIFY(registry.terminal(terminalId) != nullptr);

        // Simulation settings reached ConfigController.
        QVERIFY(doc->simulation.endTime.has_value());
        QCOMPARE(ctl.getConfigController()->getSimulationParams().value("end_time").toDouble(),
                 doc->simulation.endTime.value());

        // Auto-rules produced at least one linkage (the full fixture uses Hybrid
        // strategy with land_terminal_to_rail_node on at least one region).
        const auto linkages = ScenarioLinker::resolveLinkages(*doc, registry);
        QVERIFY(!linkages.isEmpty());
    }

    void test_applier_translates_scenario_fields_to_canonical_config_keys()
    {
        using namespace CargoNetSim::Backend::Scenario;

        ScenarioDocument doc;
        doc.simulation.shortestPathsN        = 7;
        doc.simulation.useSpecificTimeValues = true;
        doc.simulation.carbonRate            = 12.5;
        doc.simulation.shipMultiplier        = 1.25;
        doc.simulation.railMultiplier        = 1.15;
        doc.simulation.truckMultiplier       = 1.05;

        doc.simulation.ship.speed      = 22.0;
        doc.simulation.ship.fuelRate   = 55.0;
        doc.simulation.ship.risk       = 0.04;
        doc.simulation.ship.timeValue  = 14.0;
        doc.simulation.ship.fuelType   = QStringLiteral("HFO");
        doc.simulation.ship.containers = 6000;
        doc.simulation.ship.useNetwork = false;

        auto &ctl = CargoNetSim::CargoNetSimController::getInstance();
        ScenarioRegistry registry;
        QString err;
        QVERIFY2(ScenarioApplier::apply(doc, ctl, registry, &err), qPrintable(err));

        // Top-level: simulation section uses canonical names.
        auto sim = ctl.getConfigController()->getSimulationParams();
        QCOMPARE(sim.value("shortest_paths").toInt(),         7);
        QCOMPARE(sim.value("use_mode_specific").toBool(),  true);
        QVERIFY(!sim.contains("shortest_paths_n"));         // scenario short-form must NOT leak through
        QVERIFY(!sim.contains("use_specific_time_values"));

        // Carbon taxes: rate + per-mode multipliers live here, not under transport_modes.
        auto taxes = ctl.getConfigController()->getCarbonTaxes();
        QCOMPARE(taxes.value("rate").toDouble(),             12.5);
        QCOMPARE(taxes.value("ship_multiplier").toDouble(),  1.25);
        QCOMPARE(taxes.value("rail_multiplier").toDouble(),  1.15);
        QCOMPARE(taxes.value("truck_multiplier").toDouble(), 1.05);

        // Per-mode settings: canonical field names.
        auto modes = ctl.getConfigController()->getTransportModes();
        auto ship  = modes.value("ship").toMap();
        QCOMPARE(ship.value("average_speed").toDouble(),              22.0);
        QCOMPARE(ship.value("average_fuel_consumption").toDouble(),   55.0);
        QCOMPARE(ship.value("risk_factor").toDouble(),                0.04);
        QCOMPARE(ship.value("time_value_of_money").toDouble(),        14.0);
        QCOMPARE(ship.value("fuel_type").toString(), QStringLiteral("HFO"));
        QCOMPARE(ship.value("average_container_number").toInt(),      6000);
        QCOMPARE(ship.value("use_network").toBool(),                 false);
        QVERIFY(!ship.contains("speed"));       // scenario short-form must NOT leak through
        QVERIFY(!ship.contains("fuel_rate"));
        QVERIFY(!ship.contains("risk"));
        QVERIFY(!ship.contains("time_value"));
        QVERIFY(!ship.contains("multiplier"));  // multiplier belongs under carbon_taxes

        // Downstream API consistency: getTimeValueOfMoney reads use_mode_specific
        // + time_value_of_money — if the translation is wrong, these go to defaults.
        auto tvm = ctl.getConfigController()->getTimeValueOfMoney();
        QCOMPARE(tvm.value("use_mode_specific").toBool(), true);
        QCOMPARE(tvm.value("ship").toDouble(),            14.0);
    }

    void test_applier_partial_scenario_inherits_config_defaults()
    {
        using namespace CargoNetSim::Backend::Scenario;

        auto &ctl = CargoNetSim::CargoNetSimController::getInstance();
        // Reset to config.xml baseline so absent-field assertions are stable
        // regardless of which prior test last wrote to the config.
        ctl.getConfigController()->loadConfig();

        // Only time_value_of_money is set in the scenario — all other fields nullopt
        ScenarioDocument doc;
        doc.simulation.timeValueOfMoney = 30.0;
        ScenarioRegistry registry;
        QString err;
        QVERIFY2(ScenarioApplier::apply(doc, ctl, registry, &err), qPrintable(err));

        // The overridden field uses the scenario value
        auto params = ctl.getConfigController()->getSimulationParams();
        QCOMPARE(params.value("time_value_of_money").toDouble(), 30.0);

        // Absent fields inherit config.xml values, not zeros
        // config.xml has: time_step=15, shortest_paths=10
        QCOMPARE(params.value("time_step").toInt(),       15);
        QCOMPARE(params.value("shortest_paths").toInt(),  10);

        // Absent transport mode fields inherit config.xml values
        auto modes = ctl.getConfigController()->getTransportModes();
        auto ship  = modes.value("ship").toMap();
        QCOMPARE(ship.value("risk_factor").toDouble(),    0.025);    // config.xml value
        QCOMPARE(ship.value("average_speed").toDouble(), 42.6);      // config.xml value

        // Absent fuel prices inherit config.xml values
        auto fuelPrices = ctl.getConfigController()->getFuelPrices();
        QCOMPARE(fuelPrices.value("HFO").toDouble(), 0.56);          // config.xml value
    }

    // ---- Task 0: origin-containers retrofit (Plan 5 prerequisite) ----

    void test_applier_creates_origin_containers_from_yaml()
    {
        using namespace CargoNetSim::Backend::Scenario;

        const QString yamlPath = QDir(QCoreApplication::applicationDirPath())
                                     .filePath("fixtures/scenario/origin_containers.yml");
        QString err;
        auto doc = ScenarioSerializer::fromYaml(yamlPath, &err);
        QVERIFY2(doc != nullptr, qPrintable(err));

        const auto issues = ScenarioValidator::validate(*doc);
        int errorCount = 0;
        for (const auto &i : issues)
            if (i.severity == ValidationIssue::Error) ++errorCount;
        QCOMPARE(errorCount, 0);

        auto &ctl = CargoNetSim::CargoNetSimController::getInstance();
        ScenarioRegistry registry;
        QVERIFY2(ScenarioApplier::apply(*doc, ctl, registry, &err),
                 qPrintable(err));

        // The applier has populated the per-terminal container pool on the
        // document. Only terminal "O" is an origin (count > 0).
        QCOMPARE(doc->originTerminalIds(),
                 QStringList{QStringLiteral("O")});
        QCOMPARE(doc->containersAt(QStringLiteral("O")).size(), 100);
        QVERIFY(doc->containersAt(QStringLiteral("D")).isEmpty());

        // Destinations are the scalar shorthand form — normalized to
        // a single-entry list by the Document.
        const auto routes = doc->destinationsFor(QStringLiteral("O"));
        QCOMPARE(routes.size(), 1);
        QCOMPARE(routes.first().terminal, QString(QStringLiteral("D")));
        QCOMPARE(routes.first().fraction, 1.0);

        // Flat view is still the single source for Plan 3 consumers
        // (ScenarioExecutor / SimulationRequestBuilder).
        QCOMPARE(doc->originContainers().size(), 100);
    }

    // ---- TerminalTypeDefaults — time units ----

    void test_default_dwell_time_values_are_in_seconds()
    {
        using namespace CargoNetSim::Backend::Scenario;

        auto props  = TerminalTypeDefaults::defaultProperties("Sea Port Terminal");
        auto dwell  = props.value("dwell_time").toMap();
        auto params = dwell.value("parameters").toMap();

        QCOMPARE(params.value("mean").toString(),    QStringLiteral("172800")); // 2 days
        QCOMPARE(params.value("std_dev").toString(), QStringLiteral("43200"));  // 12 hours
    }

    void test_default_customs_values_are_in_seconds()
    {
        using namespace CargoNetSim::Backend::Scenario;

        auto props   = TerminalTypeDefaults::defaultProperties("Sea Port Terminal");
        auto customs = props.value("customs").toMap();

        QCOMPARE(customs.value("delay_mean").toString(),
                 QStringLiteral("172800"));       // 48 h × 3600 s
        QCOMPARE(customs.value("delay_variance").toString(),
                 QStringLiteral("7464960000"));   // (24 h × 3600)² s²
    }

    void test_applier_injects_system_dynamics_with_terminalsim_keys()
    {
        using namespace CargoNetSim::Backend::Scenario;
        using namespace CargoNetSim::Backend;

        ScenarioDocument doc;
        RegionSpec r; r.name = "EU"; doc.addRegion(r);

        TerminalPlacement t;
        t.id     = "SD1";
        t.type   = "Sea Port Terminal";
        t.region = "EU";
        t.mode   = TerminalPlacement::PositionMode::LatLon;
        t.latLon = { 51.5, 0.1 };
        t.systemDynamics.enabled            = true;
        t.systemDynamics.maxServiceRate     = 250.0;
        t.systemDynamics.shipDelay.alpha    = 0.7;
        t.systemDynamics.shipDelay.beta     = 3.1;
        t.systemDynamics.shipArrivalPenalty = 3600.0;
        doc.addTerminal(t);

        auto &ctl = CargoNetSim::CargoNetSimController::getInstance();
        ScenarioRegistry registry;
        QString err;
        QVERIFY2(ScenarioApplier::apply(doc, ctl, registry, &err),
                 qPrintable(err));

        QJsonObject config = registry.terminal("SD1")->getConfig();
        QVERIFY2(config.contains("system_dynamics"),
                 "system_dynamics block must be present in terminal config");

        QJsonObject sd = config["system_dynamics"].toObject();
        QCOMPARE(sd["enabled"].toBool(),            true);
        QCOMPARE(sd["max_service_rate"].toDouble(), 250.0);

        QVERIFY2(sd.contains("mode_delay_params"),
                 "mode_delay_params nested key required by TerminalSim");
        QJsonObject ship = sd["mode_delay_params"].toObject()["ship"].toObject();
        QCOMPARE(ship["alpha"].toDouble(), 0.7);
        QCOMPARE(ship["beta"].toDouble(),  3.1);

        QVERIFY2(sd.contains("arrival_penalties"),
                 "arrival_penalties nested key required by TerminalSim");
        QCOMPARE(sd["arrival_penalties"].toObject()["ship"].toDouble(), 3600.0);
    }
};

QTEST_MAIN(ScenarioApplierTest)
#include "ScenarioApplierTest.moc"
