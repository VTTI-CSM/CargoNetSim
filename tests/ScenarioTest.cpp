#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <containerLib/container.h>

#include "Backend/Scenario/Connection.h"
#include "Backend/Scenario/FleetSpec.h"
#include "Backend/Scenario/GlobalLink.h"
#include "Backend/Scenario/LatLon.h"
#include "Backend/Scenario/LinkageSource.h"
#include "Backend/Scenario/LinkageStrategy.h"
#include "Backend/Scenario/ModeDelayParams.h"
#include "Backend/Scenario/NetworkSpec.h"
#include "Backend/Scenario/NodeLinkage.h"
#include "Backend/Scenario/OutputSpec.h"
#include "Backend/Scenario/Point2D.h"
#include "Backend/Scenario/RegionSpec.h"
#include "Backend/Scenario/ScenarioDocument.h"
#include "Backend/Scenario/ScenarioSerializer.h"
#include "Backend/Scenario/ScenarioValidator.h"
#include "Backend/Scenario/SimulationSettings.h"
#include "Backend/Scenario/SystemDynamicsSpec.h"
#include "Backend/Scenario/TerminalPlacement.h"
#include "Backend/Scenario/TerminalTypeDefaults.h"
#include "Backend/Scenario/ValidationIssue.h"

class ScenarioTest : public QObject
{
    Q_OBJECT

private slots:
    void test_skeleton_runs()
    {
        QVERIFY(true);
    }

    void test_linkage_source_enum_values()
    {
        using CargoNetSim::Backend::Scenario::LinkageSource;
        QCOMPARE(static_cast<int>(LinkageSource::Manual), 0);
        QCOMPARE(static_cast<int>(LinkageSource::Auto),   1);
    }

    void test_linkage_strategy_to_string_round_trip()
    {
        using CargoNetSim::Backend::Scenario::LinkageStrategy;
        using CargoNetSim::Backend::Scenario::linkageStrategyToString;
        using CargoNetSim::Backend::Scenario::linkageStrategyFromString;

        QCOMPARE(linkageStrategyToString(LinkageStrategy::Manual), QStringLiteral("manual"));
        QCOMPARE(linkageStrategyToString(LinkageStrategy::Auto),   QStringLiteral("auto"));
        QCOMPARE(linkageStrategyToString(LinkageStrategy::Hybrid), QStringLiteral("hybrid"));

        bool ok = false;
        // Compare via int cast to avoid QTest::toString ADL picking up an
        // enum-in-namespace toString overload for failure-message formatting.
        QCOMPARE(static_cast<int>(linkageStrategyFromString("manual", &ok)),
                 static_cast<int>(LinkageStrategy::Manual));
        QVERIFY(ok);
        QCOMPARE(static_cast<int>(linkageStrategyFromString("hybrid", &ok)),
                 static_cast<int>(LinkageStrategy::Hybrid));
        QVERIFY(ok);
        linkageStrategyFromString("nonsense", &ok);
        QVERIFY(!ok);
    }

    void test_latlon_defaults_zero()
    {
        CargoNetSim::Backend::Scenario::LatLon p;
        QCOMPARE(p.latitude,  0.0);
        QCOMPARE(p.longitude, 0.0);
    }

    void test_point2d_defaults_zero()
    {
        CargoNetSim::Backend::Scenario::Point2D p;
        QCOMPARE(p.x, 0.0);
        QCOMPARE(p.y, 0.0);
    }

    void test_mode_delay_params_defaults()
    {
        CargoNetSim::Backend::Scenario::ModeDelayParams p;
        QCOMPARE(p.alpha, 0.5);
        QCOMPARE(p.beta,  2.0);
    }

    void test_system_dynamics_defaults_match_terminalsim()
    {
        // Mirrors TerminalSim terminal.h:43-61 exactly.
        CargoNetSim::Backend::Scenario::SystemDynamicsSpec sd;
        QCOMPARE(sd.enabled, false);
        QCOMPARE(sd.criticalUtilization,   0.7);
        QCOMPARE(sd.congestionExponent,    2.0);
        QCOMPARE(sd.congestionSensitivity, 1.0);
        QCOMPARE(sd.delaySensitivity,      0.5);
        QCOMPARE(sd.maxServiceRate,      100.0);
        QCOMPARE(sd.shipDelay.alpha,  0.5); QCOMPARE(sd.shipDelay.beta,  2.0);
        QCOMPARE(sd.truckDelay.alpha, 0.3); QCOMPARE(sd.truckDelay.beta, 2.5);
        QCOMPARE(sd.trainDelay.alpha, 0.8); QCOMPARE(sd.trainDelay.beta, 3.0);
        QCOMPARE(sd.shipArrivalPenalty,  14400.0);
        QCOMPARE(sd.truckArrivalPenalty,  1800.0);
        QCOMPARE(sd.trainArrivalPenalty,  7200.0);
    }

    void test_terminal_type_defaults_all_types_are_the_four_physical_kinds()
    {
        // Plan 8 removed Origin/Destination as terminal kinds — origin role
        // is derived via ScenarioDocument::isOrigin (initial_container_count
        // > 0 OR seeded pool present), independent of physical kind.
        const QStringList types =
            CargoNetSim::Backend::Scenario::TerminalTypeDefaults::allTypes();
        QCOMPARE(types.size(), 5);
        QVERIFY(types.contains(QStringLiteral("Sea Port Terminal")));
        QVERIFY(types.contains(QStringLiteral("Intermodal Land Terminal")));
        QVERIFY(types.contains(QStringLiteral("Train Stop/Depot")));
        QVERIFY(types.contains(QStringLiteral("Truck Parking")));
        QVERIFY(types.contains(QStringLiteral("Facility")));
    }

    void test_terminal_type_defaults_is_valid_type()
    {
        namespace TTD = CargoNetSim::Backend::Scenario::TerminalTypeDefaults;
        QVERIFY( TTD::isValidType("Sea Port Terminal"));
        QVERIFY( TTD::isValidType("Truck Parking"));
        QVERIFY(!TTD::isValidType("Origin"));            // removed in Plan 8
        QVERIFY(!TTD::isValidType("Destination"));       // removed in Plan 8
        QVERIFY(!TTD::isValidType("sea port terminal")); // case sensitive
        QVERIFY(!TTD::isValidType("Bogus Terminal"));
        QVERIFY(!TTD::isValidType(""));
    }

    // Plan 7 migration: TerminalTypeDefaults::interfacesFor now returns
    // QPair<QSet<TransportationMode>, QSet<TransportationMode>>. Tests
    // compare typed enum sets instead of QStringList.

    void test_terminal_type_defaults_interfaces_for_sea_port()
    {
        using Mode = CargoNetSim::Backend::TransportationTypes::TransportationMode;
        const auto pair = CargoNetSim::Backend::Scenario::
            TerminalTypeDefaults::interfacesFor("Sea Port Terminal");
        QCOMPARE(pair.first,  (QSet<Mode>{ Mode::Truck, Mode::Train }));
        QCOMPARE(pair.second, (QSet<Mode>{ Mode::Ship }));
    }

    void test_terminal_type_defaults_interfaces_for_intermodal()
    {
        using Mode = CargoNetSim::Backend::TransportationTypes::TransportationMode;
        const auto pair = CargoNetSim::Backend::Scenario::
            TerminalTypeDefaults::interfacesFor("Intermodal Land Terminal");
        QCOMPARE(pair.first,  (QSet<Mode>{ Mode::Truck, Mode::Train }));
        QVERIFY(pair.second.isEmpty());
    }

    void test_terminal_type_defaults_interfaces_for_train_stop()
    {
        using Mode = CargoNetSim::Backend::TransportationTypes::TransportationMode;
        const auto pair = CargoNetSim::Backend::Scenario::
            TerminalTypeDefaults::interfacesFor("Train Stop/Depot");
        QCOMPARE(pair.first,  (QSet<Mode>{ Mode::Train }));
        QVERIFY(pair.second.isEmpty());
    }

    void test_terminal_type_defaults_interfaces_for_truck_parking()
    {
        using Mode = CargoNetSim::Backend::TransportationTypes::TransportationMode;
        const auto pair = CargoNetSim::Backend::Scenario::
            TerminalTypeDefaults::interfacesFor("Truck Parking");
        QCOMPARE(pair.first,  (QSet<Mode>{ Mode::Truck }));
        QVERIFY(pair.second.isEmpty());
    }

    void test_terminal_type_defaults_interfaces_for_unknown_type_returns_empty()
    {
        const auto pair = CargoNetSim::Backend::Scenario::
            TerminalTypeDefaults::interfacesFor("Totally Fake Type");
        QVERIFY(pair.first.isEmpty());
        QVERIFY(pair.second.isEmpty());
    }

    void test_terminal_type_defaults_default_properties_sea_port_has_cost_dwell_customs()
    {
        const auto props = CargoNetSim::Backend::Scenario::
            TerminalTypeDefaults::defaultProperties("Sea Port Terminal");

        QVERIFY(props.contains("cost"));
        QVERIFY(props.contains("dwell_time"));
        QVERIFY(props.contains("customs"));
        QVERIFY(props.contains("capacity"));

        const auto cost = props.value("cost").toMap();
        QCOMPARE(cost.value("fixed_fees").toString(),   QStringLiteral("400"));
        QCOMPARE(cost.value("customs_fees").toString(), QStringLiteral("100"));
        QCOMPARE(cost.value("risk_factor").toString(),  QStringLiteral("0.015"));

        const auto dwell  = props.value("dwell_time").toMap();
        QCOMPARE(dwell.value("method").toString(), QStringLiteral("normal"));
        const auto params = dwell.value("parameters").toMap();
        QCOMPARE(params.value("mean").toString(),    QStringLiteral("2880"));
        QCOMPARE(params.value("std_dev").toString(), QStringLiteral("720"));
    }

    void test_terminal_type_defaults_default_properties_truck_parking_minimal()
    {
        const auto props = CargoNetSim::Backend::Scenario::
            TerminalTypeDefaults::defaultProperties("Truck Parking");

        QVERIFY(!props.contains("customs"));
        QVERIFY(!props.contains("capacity"));
        QVERIFY(!props.value("Show on Global Map").toBool());

        const auto ifaces = props.value("Available Interfaces").toMap();
        QCOMPARE(ifaces.value("land_side").toStringList(),
                 QStringList() << "Truck");
        QVERIFY(ifaces.value("sea_side").toStringList().isEmpty());
    }

    void test_terminal_type_defaults_default_properties_unknown_type_empty()
    {
        const auto props = CargoNetSim::Backend::Scenario::
            TerminalTypeDefaults::defaultProperties("Totally Fake Type");
        QVERIFY(props.isEmpty());
    }

    void test_node_linkage_defaults()
    {
        CargoNetSim::Backend::Scenario::NodeLinkage link;
        QCOMPARE(link.nodeId,   -1);
        // Cast to int to sidestep QTest::toString ADL for enum-in-namespace
        // (see LinkageStrategy.h).
        QCOMPARE(static_cast<int>(link.source),
                 static_cast<int>(CargoNetSim::Backend::Scenario::LinkageSource::Manual));
        QCOMPARE(link.excluded, false);
    }

    void test_connection_defaults()
    {
        CargoNetSim::Backend::Scenario::Connection c;
        QCOMPARE(static_cast<int>(c.source),
                 static_cast<int>(CargoNetSim::Backend::Scenario::LinkageSource::Manual));
    }

    void test_global_link_defaults()
    {
        CargoNetSim::Backend::Scenario::GlobalLink g;
        QCOMPARE(static_cast<int>(g.source),
                 static_cast<int>(CargoNetSim::Backend::Scenario::LinkageSource::Manual));
    }

    void test_fleet_spec_inline_entries_empty_by_default()
    {
        CargoNetSim::Backend::Scenario::FleetSpec f;
        QVERIFY(f.trainsInline.isEmpty());
        QVERIFY(f.shipsInline.isEmpty());
        QVERIFY(f.trucksInline.isEmpty());
    }

    void test_output_spec_defaults()
    {
        CargoNetSim::Backend::Scenario::OutputSpec out;
        QCOMPARE(out.directory, QStringLiteral("./results"));
        QCOMPARE(out.formats,   QStringList() << "json");
        QCOMPARE(out.containerTracking, true);
    }

    void test_simulation_settings_defaults()
    {
        CargoNetSim::Backend::Scenario::SimulationSettings s;
        QCOMPARE(s.timeStep,          60);
        QCOMPARE(s.shortestPathsN,     5);
        QCOMPARE(s.endTime,       86400.0);
        QCOMPARE(s.useSpecificTimeValues, false);
        QCOMPARE(s.shipMultiplier,  1.0);
        QCOMPARE(s.trainMultiplier, 1.0);
        QCOMPARE(s.truckMultiplier, 1.0);
        QCOMPARE(s.dwellMethod,  QStringLiteral("normal"));
        // dwellParams now has defaults matching the "normal" method
        QCOMPARE(s.dwellParams.size(), 2);
        QCOMPARE(s.dwellParams.value("mean").toDouble(), 2880.0);
        QCOMPARE(s.dwellParams.value("std_dev").toDouble(), 720.0);
        QVERIFY(s.fuelTypes.isEmpty());
    }

    void test_simulation_settings_mode_struct_useNetwork_defaults_true()
    {
        CargoNetSim::Backend::Scenario::SimulationSettings::Mode m;
        QCOMPARE(m.useNetwork, true);
    }

    void test_simulation_settings_fuel_struct_defaults_zero()
    {
        CargoNetSim::Backend::Scenario::SimulationSettings::Fuel f;
        QCOMPARE(f.energy, 0.0);
        QCOMPARE(f.carbon, 0.0);
        QCOMPARE(f.price,  0.0);
    }

    void test_network_spec_defaults()
    {
        CargoNetSim::Backend::Scenario::NetworkSpec n;
        QCOMPARE(n.referencePoint.x, 0.0);
        QCOMPARE(n.referencePoint.y, 0.0);
        QVERIFY(n.files.isEmpty());
    }

    void test_network_spec_type_to_string()
    {
        using T = CargoNetSim::Backend::Scenario::NetworkSpec::Type;
        using CargoNetSim::Backend::networkKindToString;
        QCOMPARE(networkKindToString(T::Rail),  QStringLiteral("rail"));
        QCOMPARE(networkKindToString(T::Truck), QStringLiteral("truck"));
    }

    void test_network_spec_type_from_string()
    {
        using T = CargoNetSim::Backend::Scenario::NetworkSpec::Type;
        using CargoNetSim::Backend::networkKindFromString;
        bool ok = false;
        // Cast to int to sidestep any QTest::toString ADL pitfalls with the
        // enum-in-namespace (see LinkageStrategy.h rationale).
        QCOMPARE(static_cast<int>(networkKindFromString("rail",  &ok)),
                 static_cast<int>(T::Rail));  QVERIFY(ok);
        QCOMPARE(static_cast<int>(networkKindFromString("truck", &ok)),
                 static_cast<int>(T::Truck)); QVERIFY(ok);
        QCOMPARE(static_cast<int>(networkKindFromString("RAIL",  &ok)),
                 static_cast<int>(T::Rail));  QVERIFY(ok);  // case insensitive
        networkKindFromString("ship", &ok);  QVERIFY(!ok);  // ship not a network
        networkKindFromString("",     &ok);  QVERIFY(!ok);
    }

    void test_terminal_placement_defaults()
    {
        CargoNetSim::Backend::Scenario::TerminalPlacement t;
        QCOMPARE(t.showOnGlobalMap, true);
        QCOMPARE(static_cast<int>(t.mode),
                 static_cast<int>(CargoNetSim::Backend::Scenario::
                                      TerminalPlacement::PositionMode::NetworkNode));
        QCOMPARE(t.nodeId, -1);
        QCOMPARE(t.interfaces.isSet, false);
        QVERIFY(t.interfaces.landSide.isEmpty());
        QVERIFY(t.interfaces.seaSide.isEmpty());
        // system_dynamics is omitted by default (enabled==false is the sentinel).
        QCOMPARE(t.systemDynamics.enabled, false);
    }

    void test_terminal_placement_position_mode_to_string()
    {
        using PM = CargoNetSim::Backend::Scenario::TerminalPlacement::PositionMode;
        using CargoNetSim::Backend::Scenario::positionModeToString;
        QCOMPARE(positionModeToString(PM::NetworkNode), QStringLiteral("network_node"));
        QCOMPARE(positionModeToString(PM::LatLon),      QStringLiteral("latlon"));
        QCOMPARE(positionModeToString(PM::Scene),       QStringLiteral("scene"));
    }

    void test_terminal_placement_position_mode_from_string()
    {
        using PM = CargoNetSim::Backend::Scenario::TerminalPlacement::PositionMode;
        using CargoNetSim::Backend::Scenario::positionModeFromString;
        bool ok = false;
        QCOMPARE(static_cast<int>(positionModeFromString("network_node", &ok)),
                 static_cast<int>(PM::NetworkNode)); QVERIFY(ok);
        QCOMPARE(static_cast<int>(positionModeFromString("latlon", &ok)),
                 static_cast<int>(PM::LatLon));      QVERIFY(ok);
        QCOMPARE(static_cast<int>(positionModeFromString("scene", &ok)),
                 static_cast<int>(PM::Scene));       QVERIFY(ok);
        positionModeFromString("pixel", &ok); QVERIFY(!ok);
    }

    void test_terminal_placement_interface_set_distinguishes_omitted_from_empty()
    {
        CargoNetSim::Backend::Scenario::TerminalPlacement::InterfaceSet iface;
        QCOMPARE(iface.isSet, false);                 // omitted
        iface.isSet = true;                           // explicit override
        QVERIFY(iface.landSide.isEmpty());            // explicitly empty
    }

    void test_region_spec_defaults()
    {
        CargoNetSim::Backend::Scenario::RegionSpec r;
        QCOMPARE(static_cast<int>(r.linkageStrategy),
                 static_cast<int>(CargoNetSim::Backend::Scenario::LinkageStrategy::Manual));
        QCOMPARE(static_cast<int>(r.connectionStrategy),
                 static_cast<int>(CargoNetSim::Backend::Scenario::LinkageStrategy::Manual));
        QVERIFY(r.networks.isEmpty());
        QVERIFY(r.terminalIds.isEmpty());
    }

    void test_region_spec_adds_networks_keyed_by_name()
    {
        CargoNetSim::Backend::Scenario::RegionSpec r;
        r.name = "USA";

        CargoNetSim::Backend::Scenario::NetworkSpec rail;
        rail.name = "USA_rail";
        rail.type = CargoNetSim::Backend::Scenario::NetworkSpec::Type::Rail;
        r.networks.insert(rail.name, rail);

        QCOMPARE(r.networks.size(), 1);
        QCOMPARE(static_cast<int>(r.networks["USA_rail"].type),
                 static_cast<int>(CargoNetSim::Backend::Scenario::NetworkSpec::Type::Rail));
    }

    // ---- ScenarioDocument ----

    void test_scenario_document_add_region_emits_signal_and_enforces_key()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;
        QSignalSpy spy(&doc, &ScenarioDocument::regionAdded);

        RegionSpec r; r.name = "USA"; r.color = "#ff8800";
        QVERIFY(doc.addRegion(r));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toString(), QStringLiteral("USA"));
        QCOMPARE(doc.regions.size(), 1);
        QCOMPARE(doc.regions["USA"].name, QStringLiteral("USA"));
    }

    void test_scenario_document_add_region_rejects_empty_name()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;
        RegionSpec r;  // empty name
        QVERIFY(!doc.addRegion(r));
        QVERIFY(doc.regions.isEmpty());
    }

    void test_scenario_document_add_region_rejects_duplicate()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;
        RegionSpec r; r.name = "USA";
        QVERIFY( doc.addRegion(r));
        QVERIFY(!doc.addRegion(r));
        QCOMPARE(doc.regions.size(), 1);
    }

    void test_scenario_document_remove_region_emits_signal()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;
        RegionSpec r; r.name = "USA"; doc.addRegion(r);

        QSignalSpy spy(&doc, &ScenarioDocument::regionRemoved);
        QVERIFY(doc.removeRegion("USA"));
        QCOMPARE(spy.count(), 1);
        QVERIFY(doc.regions.isEmpty());
    }

    void test_scenario_document_rename_region_updates_key_and_struct_name()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;
        RegionSpec r; r.name = "USA"; doc.addRegion(r);

        QSignalSpy renamedSpy(&doc, &ScenarioDocument::regionRenamed);
        QVERIFY(doc.renameRegion("USA", "United States"));
        QVERIFY(!doc.regions.contains("USA"));
        QVERIFY( doc.regions.contains("United States"));
        QCOMPARE(doc.regions["United States"].name, QStringLiteral("United States"));
        QCOMPARE(renamedSpy.count(), 1);
        const auto renamedArgs = renamedSpy.takeFirst();
        QCOMPARE(renamedArgs.at(0).toString(), QStringLiteral("USA"));
        QCOMPARE(renamedArgs.at(1).toString(), QStringLiteral("United States"));
    }

    void test_scenario_document_rename_region_reanchors_terminal_references()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;
        RegionSpec r; r.name = "USA"; doc.addRegion(r);

        TerminalPlacement t;
        t.id = "T1"; t.type = "Sea Port Terminal"; t.region = "USA";
        doc.addTerminal(t);

        QSignalSpy terminalChangedSpy(&doc, &ScenarioDocument::terminalChanged);
        QVERIFY(doc.renameRegion("USA", "US"));
        QCOMPARE(doc.terminals["T1"].region, QStringLiteral("US"));
        QCOMPARE(terminalChangedSpy.count(), 1);
        QCOMPARE(terminalChangedSpy.takeFirst().at(0).toString(),
                 QStringLiteral("T1"));
    }

    void test_scenario_document_rename_region_reanchors_connection_region()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;
        RegionSpec r; r.name = "USA"; doc.addRegion(r);

        TerminalPlacement a; a.id="A"; a.type="Sea Port Terminal"; a.region="USA";
        TerminalPlacement b; b.id="B"; b.type="Sea Port Terminal"; b.region="USA";
        doc.addTerminal(a); doc.addTerminal(b);

        Connection c;
        c.fromTerminalId = "A"; c.toTerminalId = "B";
        c.mode = CargoNetSim::Backend::TransportationTypes::TransportationMode::Truck; c.region = "USA";
        doc.addConnection(c);

        QVERIFY(doc.renameRegion("USA", "US"));
        QCOMPARE(doc.connections.at(0).region, QStringLiteral("US"));
    }

    void test_scenario_document_rename_region_reanchors_qualified_global_link_ids()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;
        RegionSpec usa; usa.name = "USA"; doc.addRegion(usa);
        RegionSpec can; can.name = "CAN"; doc.addRegion(can);

        TerminalPlacement t1; t1.id="T1"; t1.type="Sea Port Terminal"; t1.region="USA";
        TerminalPlacement c1; c1.id="C1"; c1.type="Sea Port Terminal"; c1.region="CAN";
        doc.addTerminal(t1); doc.addTerminal(c1);

        GlobalLink g;
        g.fromTerminalId = "USA/T1";
        g.toTerminalId   = "CAN/C1";
        g.mode           = CargoNetSim::Backend::TransportationTypes::TransportationMode::Ship;
        doc.addGlobalLink(g);

        QVERIFY(doc.renameRegion("USA", "US"));
        QCOMPARE(doc.globalLinks.at(0).fromTerminalId, QStringLiteral("US/T1"));
        QCOMPARE(doc.globalLinks.at(0).toTerminalId,   QStringLiteral("CAN/C1"));
    }

    void test_scenario_document_remove_region_cascades_into_terminals()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;
        RegionSpec r; r.name = "USA"; doc.addRegion(r);

        TerminalPlacement t;
        t.id = "T1"; t.type = "Sea Port Terminal"; t.region = "USA";
        doc.addTerminal(t);

        QSignalSpy terminalRemovedSpy(&doc, &ScenarioDocument::terminalRemoved);
        QVERIFY(doc.removeRegion("USA"));
        QVERIFY(doc.regions.isEmpty());
        QVERIFY(doc.terminals.isEmpty());          // cascaded
        QCOMPARE(terminalRemovedSpy.count(), 1);
    }

    void test_scenario_document_remove_terminal_cascades_into_linkages()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;
        RegionSpec r; r.name = "USA"; doc.addRegion(r);

        TerminalPlacement t;
        t.id = "T1"; t.type = "Intermodal Land Terminal"; t.region = "USA";
        doc.addTerminal(t);

        NodeLinkage l; l.terminalId = "T1"; l.networkName = "USA_rail"; l.nodeId = 7;
        doc.addLinkage(l);

        QSignalSpy linkageRemovedSpy(&doc, &ScenarioDocument::linkageRemoved);
        QVERIFY(doc.removeTerminal("T1"));
        QVERIFY(doc.linkages.isEmpty());
        QCOMPARE(linkageRemovedSpy.count(), 1);
    }

    void test_scenario_document_remove_terminal_cascades_into_connections()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;
        RegionSpec r; r.name = "USA"; doc.addRegion(r);

        TerminalPlacement a; a.id="A"; a.type="Sea Port Terminal"; a.region="USA";
        TerminalPlacement b; b.id="B"; b.type="Sea Port Terminal"; b.region="USA";
        doc.addTerminal(a); doc.addTerminal(b);

        Connection c;
        c.fromTerminalId = "A"; c.toTerminalId = "B";
        c.mode = CargoNetSim::Backend::TransportationTypes::TransportationMode::Truck; c.region = "USA";
        doc.addConnection(c);

        QSignalSpy connectionRemovedSpy(&doc, &ScenarioDocument::connectionRemoved);
        QVERIFY(doc.removeTerminal("A"));
        QVERIFY(doc.connections.isEmpty());
        QCOMPARE(connectionRemovedSpy.count(), 1);
    }

    void test_scenario_document_remove_terminal_cascades_into_qualified_global_links()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;
        RegionSpec usa; usa.name = "USA"; doc.addRegion(usa);
        RegionSpec can; can.name = "CAN"; doc.addRegion(can);

        TerminalPlacement t1; t1.id="T1"; t1.type="Sea Port Terminal"; t1.region="USA";
        TerminalPlacement c1; c1.id="C1"; c1.type="Sea Port Terminal"; c1.region="CAN";
        doc.addTerminal(t1); doc.addTerminal(c1);

        GlobalLink g;
        g.fromTerminalId = "USA/T1";
        g.toTerminalId   = "CAN/C1";
        g.mode           = CargoNetSim::Backend::TransportationTypes::TransportationMode::Ship;
        doc.addGlobalLink(g);

        QSignalSpy globalLinkRemovedSpy(&doc, &ScenarioDocument::globalLinkRemoved);
        QVERIFY(doc.removeTerminal("T1"));
        QVERIFY(doc.globalLinks.isEmpty());
        QCOMPARE(globalLinkRemovedSpy.count(), 1);
    }

    void test_scenario_document_add_terminal_requires_existing_region()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;
        TerminalPlacement t; t.id = "T1"; t.type = "Sea Port Terminal"; t.region = "Nowhere";
        QVERIFY(!doc.addTerminal(t));

        RegionSpec r; r.name = "USA"; doc.addRegion(r);
        t.region = "USA";
        QVERIFY( doc.addTerminal(t));
        QCOMPARE(doc.terminals.size(), 1);
    }

    void test_scenario_document_add_terminal_rejects_duplicate_id()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;
        RegionSpec r; r.name = "USA"; doc.addRegion(r);

        TerminalPlacement t; t.id = "T1"; t.type = "Sea Port Terminal"; t.region = "USA";
        QVERIFY( doc.addTerminal(t));
        QVERIFY(!doc.addTerminal(t));
    }

    void test_scenario_document_add_linkage_requires_terminal()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;
        NodeLinkage link; link.terminalId = "T_missing"; link.networkName = "USA_rail"; link.nodeId = 7;
        QVERIFY(!doc.addLinkage(link));
    }

    void test_scenario_document_reset_clears_everything_and_emits_documentReset()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;
        RegionSpec r; r.name = "USA"; doc.addRegion(r);

        QSignalSpy spy(&doc, &ScenarioDocument::documentReset);
        doc.reset();
        QCOMPARE(spy.count(), 1);
        QVERIFY(doc.regions.isEmpty());
        QVERIFY(doc.terminals.isEmpty());
    }

    void test_scenario_document_updateRegion_emits_regionChanged_and_persists_spec()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;

        RegionSpec r;
        r.name           = "USA";
        r.color          = "#00ff00";
        r.localOrigin    = { 38.0, -97.0 };
        r.globalPosition = { 38.0, -97.0 };
        QVERIFY(doc.addRegion(r));

        QSignalSpy spy(&doc, &ScenarioDocument::regionChanged);

        RegionSpec edited = r;
        edited.color          = "#ff0000";
        edited.localOrigin    = { 40.0, -80.0 };
        edited.globalPosition = { 41.0, -81.0 };
        QVERIFY(doc.updateRegion("USA", edited));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toString(), QStringLiteral("USA"));
        QCOMPARE(doc.regions["USA"].color,                  QStringLiteral("#ff0000"));
        QCOMPARE(doc.regions["USA"].localOrigin.latitude,   40.0);
        QCOMPARE(doc.regions["USA"].globalPosition.latitude, 41.0);

        RegionSpec other = edited; other.name = "Canada";
        QVERIFY(!doc.updateRegion("USA", other));
        QVERIFY(!doc.updateRegion("",    edited));
        QVERIFY(!doc.updateRegion("Ghost", edited));
    }

    void test_scenario_document_linkagesFor_filters_by_network_type()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;

        RegionSpec r; r.name = "USA"; doc.addRegion(r);

        NetworkSpec rail;  rail.name  = "USA_rail";  rail.type = NetworkSpec::Type::Rail;
        NetworkSpec truck; truck.name = "USA_truck"; truck.type = NetworkSpec::Type::Truck;
        QVERIFY(doc.addNetwork("USA", rail));
        QVERIFY(doc.addNetwork("USA", truck));

        TerminalPlacement t; t.id = "T1"; t.type = "Intermodal Land Terminal"; t.region = "USA";
        QVERIFY(doc.addTerminal(t));

        NodeLinkage lRail;  lRail.terminalId  = "T1"; lRail.networkName  = "USA_rail";  lRail.nodeId = 10;
        NodeLinkage lTruck; lTruck.terminalId = "T1"; lTruck.networkName = "USA_truck"; lTruck.nodeId = 20;
        QVERIFY(doc.addLinkage(lRail));
        QVERIFY(doc.addLinkage(lTruck));

        const auto railMatches  = doc.linkagesFor("T1", NetworkSpec::Type::Rail);
        const auto truckMatches = doc.linkagesFor("T1", NetworkSpec::Type::Truck);
        QCOMPARE(railMatches.size(),  1);
        QCOMPARE(railMatches.first().nodeId, 10);
        QCOMPARE(truckMatches.size(), 1);
        QCOMPARE(truckMatches.first().nodeId, 20);

        QVERIFY(doc.linkagesFor("unknown", NetworkSpec::Type::Rail).isEmpty());
    }

    void test_scenario_document_globalPositionOf_applies_spec_section_4_formula()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;

        RegionSpec r;
        r.name           = "USA";
        r.localOrigin    = { 38.0, -97.0 };
        r.globalPosition = { 40.0, -80.0 };
        doc.addRegion(r);

        TerminalPlacement t;
        t.id      = "T1";
        t.type    = "Sea Port Terminal";
        t.region  = "USA";
        t.mode    = TerminalPlacement::PositionMode::LatLon;
        t.latLon  = { 38.5, -96.5 };
        doc.addTerminal(t);

        const QPointF global = doc.globalPositionOf("T1");
        // Δlat = +0.5, Δlon = +0.5 → (40.5, -79.5). QPointF: x=lon, y=lat.
        QCOMPARE(global.y(), 40.5);
        QCOMPARE(global.x(), -79.5);

        QVERIFY(qIsNaN(doc.globalPositionOf("unknown").x()));
    }

    // ---- Origin containers + destinations (Task 0 retrofit) ----
    //
    // Old `test_scenario_document_originContainers_round_trip_and_clear_on_reset`
    // was removed when `setOriginContainers(QList)` (flat setter) was
    // replaced by `setOriginContainers(id, QList)` (per-terminal). The
    // round-trip + reset-clear coverage is preserved by the four tests
    // below — all exercise the same ownership contract through the new
    // keyed storage.

    void test_origin_terminal_ids_empty_by_default()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;
        QVERIFY(doc.originTerminalIds().isEmpty());
        QVERIFY(doc.containersAt(QStringLiteral("nope")).isEmpty());
        QVERIFY(doc.originContainers().isEmpty());
    }

    void test_set_origin_containers_populates_origin_ids_and_is_cleared_on_reset()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;
        RegionSpec r; r.name = QStringLiteral("USA");
        QVERIFY(doc.addRegion(r));
        TerminalPlacement t;
        t.id     = QStringLiteral("O");
        t.type   = QStringLiteral("Intermodal Land Terminal");
        t.region = QStringLiteral("USA");
        QVERIFY(doc.addTerminal(t));

        QList<ContainerCore::Container *> pool;
        pool << new ContainerCore::Container();
        pool << new ContainerCore::Container();
        doc.setOriginContainers(QStringLiteral("O"), std::move(pool));

        QCOMPARE(doc.originTerminalIds(), QStringList{QStringLiteral("O")});
        QCOMPARE(doc.containersAt(QStringLiteral("O")).size(), 2);
        QCOMPARE(doc.originContainers().size(), 2);  // flat view

        doc.reset();
        QVERIFY(doc.originTerminalIds().isEmpty());
        QVERIFY(doc.originContainers().isEmpty());
    }

    void test_destinations_for_returns_scalar_as_single_entry_list()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;
        RegionSpec r; r.name = QStringLiteral("USA");
        QVERIFY(doc.addRegion(r));
        TerminalPlacement t;
        t.id     = QStringLiteral("O");
        t.type   = QStringLiteral("Intermodal Land Terminal");
        t.region = QStringLiteral("USA");
        t.properties[QStringLiteral("destination_terminal")] =
            QString(QStringLiteral("D"));
        QVERIFY(doc.addTerminal(t));

        const auto routes = doc.destinationsFor(QStringLiteral("O"));
        QCOMPARE(routes.size(), 1);
        QCOMPARE(routes.first().terminal, QString(QStringLiteral("D")));
        QCOMPARE(routes.first().fraction, 1.0);
    }

    void test_destinations_for_parses_routing_list()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;
        RegionSpec r; r.name = QStringLiteral("USA");
        QVERIFY(doc.addRegion(r));
        TerminalPlacement t;
        t.id     = QStringLiteral("O");
        t.type   = QStringLiteral("Intermodal Land Terminal");
        t.region = QStringLiteral("USA");

        QVariantList list;
        list << QVariantMap{
            {QStringLiteral("terminal"), QStringLiteral("A")},
            {QStringLiteral("fraction"), 0.6}};
        list << QVariantMap{
            {QStringLiteral("terminal"), QStringLiteral("B")},
            {QStringLiteral("fraction"), 0.4}};
        t.properties[QStringLiteral("destinations")] = list;
        QVERIFY(doc.addTerminal(t));

        const auto routes = doc.destinationsFor(QStringLiteral("O"));
        QCOMPARE(routes.size(), 2);
        QCOMPARE(routes[0].terminal, QString(QStringLiteral("A")));
        QCOMPARE(routes[0].fraction, 0.6);
        QCOMPARE(routes[1].terminal, QString(QStringLiteral("B")));
        QCOMPARE(routes[1].fraction, 0.4);
    }

    // ---- isOrigin / isDestination role detection (Plan 8 single source
    //      of truth shared by CLI and GUI) ----

    void test_is_origin_via_initial_container_count_property()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;
        RegionSpec r; r.name = QStringLiteral("USA");
        QVERIFY(doc.addRegion(r));

        TerminalPlacement o;
        o.id     = QStringLiteral("O");
        o.type   = QStringLiteral("Intermodal Land Terminal");
        o.region = QStringLiteral("USA");
        o.properties[QStringLiteral("initial_container_count")] = 5;
        QVERIFY(doc.addTerminal(o));

        TerminalPlacement n;
        n.id     = QStringLiteral("N");
        n.type   = QStringLiteral("Intermodal Land Terminal");
        n.region = QStringLiteral("USA");
        QVERIFY(doc.addTerminal(n));

        QVERIFY( doc.isOrigin(QStringLiteral("O")));
        QVERIFY(!doc.isOrigin(QStringLiteral("N")));
        QVERIFY(!doc.isOrigin(QStringLiteral("missing")));
    }

    void test_is_origin_via_seeded_pool_overrides_count_zero()
    {
        // After applier runs, m_containersByTerminal is the authoritative
        // post-load signal — even if `initial_container_count` is absent /
        // zero the terminal is still an origin.
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;
        RegionSpec r; r.name = QStringLiteral("USA");
        QVERIFY(doc.addRegion(r));
        TerminalPlacement t;
        t.id     = QStringLiteral("O");
        t.type   = QStringLiteral("Intermodal Land Terminal");
        t.region = QStringLiteral("USA");
        QVERIFY(doc.addTerminal(t));

        QList<ContainerCore::Container *> pool;
        pool << new ContainerCore::Container();
        doc.setOriginContainers(QStringLiteral("O"), std::move(pool));

        QVERIFY(doc.isOrigin(QStringLiteral("O")));
    }

    void test_is_destination_via_scalar_destination_terminal()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;
        RegionSpec r; r.name = QStringLiteral("USA");
        QVERIFY(doc.addRegion(r));

        TerminalPlacement o;
        o.id     = QStringLiteral("O");
        o.type   = QStringLiteral("Intermodal Land Terminal");
        o.region = QStringLiteral("USA");
        o.properties[QStringLiteral("initial_container_count")] = 1;
        o.properties[QStringLiteral("destination_terminal")] =
            QString(QStringLiteral("D"));
        QVERIFY(doc.addTerminal(o));

        TerminalPlacement d;
        d.id     = QStringLiteral("D");
        d.type   = QStringLiteral("Intermodal Land Terminal");
        d.region = QStringLiteral("USA");
        QVERIFY(doc.addTerminal(d));

        QVERIFY( doc.isDestination(QStringLiteral("D")));
        QVERIFY(!doc.isDestination(QStringLiteral("O")));
        QVERIFY(!doc.isDestination(QStringLiteral("missing")));
    }

    void test_is_destination_via_destinations_list()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;
        RegionSpec r; r.name = QStringLiteral("USA");
        QVERIFY(doc.addRegion(r));

        TerminalPlacement o;
        o.id     = QStringLiteral("O");
        o.type   = QStringLiteral("Intermodal Land Terminal");
        o.region = QStringLiteral("USA");
        o.properties[QStringLiteral("initial_container_count")] = 10;
        QVariantList list;
        list << QVariantMap{
            {QStringLiteral("terminal"), QStringLiteral("A")},
            {QStringLiteral("fraction"), 0.5}};
        list << QVariantMap{
            {QStringLiteral("terminal"), QStringLiteral("B")},
            {QStringLiteral("fraction"), 0.5}};
        o.properties[QStringLiteral("destinations")] = list;
        QVERIFY(doc.addTerminal(o));

        TerminalPlacement a; a.id = QStringLiteral("A");
        a.type = QStringLiteral("Intermodal Land Terminal");
        a.region = QStringLiteral("USA");
        QVERIFY(doc.addTerminal(a));
        TerminalPlacement b; b.id = QStringLiteral("B");
        b.type = QStringLiteral("Intermodal Land Terminal");
        b.region = QStringLiteral("USA");
        QVERIFY(doc.addTerminal(b));

        QVERIFY(doc.isDestination(QStringLiteral("A")));
        QVERIFY(doc.isDestination(QStringLiteral("B")));
    }

    void test_is_destination_ignores_non_origin_terminals_with_dest_property()
    {
        // A terminal that happens to carry destination_terminal but is
        // NOT an origin (no count, no pool) does not flag its target as
        // a destination — only origins emit destination relationships.
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;
        RegionSpec r; r.name = QStringLiteral("USA");
        QVERIFY(doc.addRegion(r));

        TerminalPlacement t;
        t.id     = QStringLiteral("T");
        t.type   = QStringLiteral("Intermodal Land Terminal");
        t.region = QStringLiteral("USA");
        t.properties[QStringLiteral("destination_terminal")] =
            QString(QStringLiteral("X"));
        // intentionally NO initial_container_count
        QVERIFY(doc.addTerminal(t));

        QVERIFY(!doc.isOrigin(QStringLiteral("T")));
        QVERIFY(!doc.isDestination(QStringLiteral("X")));
    }

    void test_origin_terminal_ids_includes_pre_applier_count_origins()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;
        RegionSpec r; r.name = QStringLiteral("USA");
        QVERIFY(doc.addRegion(r));

        TerminalPlacement o;
        o.id     = QStringLiteral("O");
        o.type   = QStringLiteral("Intermodal Land Terminal");
        o.region = QStringLiteral("USA");
        o.properties[QStringLiteral("initial_container_count")] = 42;
        QVERIFY(doc.addTerminal(o));

        // No applier run → containersAt is empty, but O is an origin.
        QVERIFY(doc.containersAt(QStringLiteral("O")).isEmpty());
        QCOMPARE(doc.originTerminalIds(),
                 QStringList{ QStringLiteral("O") });
    }

    void test_has_any_origin_true_when_any_terminal_is_origin()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;
        RegionSpec r; r.name = QStringLiteral("USA");
        QVERIFY(doc.addRegion(r));

        TerminalPlacement t;
        t.id = QStringLiteral("N");
        t.type = QStringLiteral("Intermodal Land Terminal");
        t.region = QStringLiteral("USA");
        QVERIFY(doc.addTerminal(t));
        QVERIFY(!doc.hasAnyOrigin());

        TerminalPlacement o;
        o.id = QStringLiteral("O");
        o.type = QStringLiteral("Intermodal Land Terminal");
        o.region = QStringLiteral("USA");
        o.properties[QStringLiteral("initial_container_count")] = 1;
        QVERIFY(doc.addTerminal(o));
        QVERIFY(doc.hasAnyOrigin());
    }

    void test_has_any_origin_false_on_empty_document()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;
        QVERIFY(!doc.hasAnyOrigin());
    }

    // ---- Validator: origin-container rules (Task 0 retrofit) ----

    void test_validator_flags_missing_destination_on_origin()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;
        RegionSpec r; r.name = QStringLiteral("USA");
        QVERIFY(doc.addRegion(r));
        TerminalPlacement t;
        t.id     = QStringLiteral("O");
        t.type   = QStringLiteral("Intermodal Land Terminal");
        t.region = QStringLiteral("USA");
        t.properties[QStringLiteral("initial_container_count")] = 10;
        QVERIFY(doc.addTerminal(t));

        const auto issues = ScenarioValidator::validate(doc);
        bool found = false;
        for (const auto &i : issues)
            if (i.message.contains(
                    QStringLiteral("requires destination_terminal")))
                found = true;
        QVERIFY(found);
    }

    void test_validator_flags_both_destination_forms_set()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;
        RegionSpec r; r.name = QStringLiteral("USA");
        QVERIFY(doc.addRegion(r));
        TerminalPlacement o;
        o.id     = QStringLiteral("O");
        o.type   = QStringLiteral("Intermodal Land Terminal");
        o.region = QStringLiteral("USA");
        o.properties[QStringLiteral("initial_container_count")] = 10;
        o.properties[QStringLiteral("destination_terminal")] =
            QString(QStringLiteral("D"));
        o.properties[QStringLiteral("destinations")] = QVariantList{};
        QVERIFY(doc.addTerminal(o));
        TerminalPlacement d;
        d.id     = QStringLiteral("D");
        d.type   = QStringLiteral("Intermodal Land Terminal");
        d.region = QStringLiteral("USA");
        QVERIFY(doc.addTerminal(d));

        const auto issues = ScenarioValidator::validate(doc);
        bool found = false;
        for (const auto &i : issues)
            if (i.message.contains(QStringLiteral("mutually exclusive")))
                found = true;
        QVERIFY(found);
    }

    void test_validator_flags_fractions_not_summing_to_one()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;
        RegionSpec r; r.name = QStringLiteral("USA");
        QVERIFY(doc.addRegion(r));
        TerminalPlacement o;
        o.id     = QStringLiteral("O");
        o.type   = QStringLiteral("Intermodal Land Terminal");
        o.region = QStringLiteral("USA");
        o.properties[QStringLiteral("initial_container_count")] = 10;
        QVariantList list;
        list << QVariantMap{
            {QStringLiteral("terminal"), QStringLiteral("A")},
            {QStringLiteral("fraction"), 0.3}};
        list << QVariantMap{
            {QStringLiteral("terminal"), QStringLiteral("B")},
            {QStringLiteral("fraction"), 0.4}};  // 0.7 total
        o.properties[QStringLiteral("destinations")] = list;
        QVERIFY(doc.addTerminal(o));
        TerminalPlacement a;
        a.id = QStringLiteral("A"); a.type = QStringLiteral("Intermodal Land Terminal");
        a.region = QStringLiteral("USA");
        TerminalPlacement b;
        b.id = QStringLiteral("B"); b.type = QStringLiteral("Intermodal Land Terminal");
        b.region = QStringLiteral("USA");
        QVERIFY(doc.addTerminal(a));
        QVERIFY(doc.addTerminal(b));

        const auto issues = ScenarioValidator::validate(doc);
        bool found = false;
        for (const auto &i : issues)
            if (i.message.contains(QStringLiteral("sum to 1.0")))
                found = true;
        QVERIFY(found);
    }

    void test_validator_flags_unknown_destination_id()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;
        RegionSpec r; r.name = QStringLiteral("USA");
        QVERIFY(doc.addRegion(r));
        TerminalPlacement o;
        o.id     = QStringLiteral("O");
        o.type   = QStringLiteral("Intermodal Land Terminal");
        o.region = QStringLiteral("USA");
        o.properties[QStringLiteral("initial_container_count")] = 5;
        o.properties[QStringLiteral("destination_terminal")] =
            QString(QStringLiteral("GHOST"));
        QVERIFY(doc.addTerminal(o));

        const auto issues = ScenarioValidator::validate(doc);
        bool found = false;
        for (const auto &i : issues)
            if (i.message.contains(
                    QStringLiteral("unknown destination terminal 'GHOST'")))
                found = true;
        QVERIFY(found);
    }

    void test_validator_flags_self_referencing_destination()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;
        RegionSpec r; r.name = QStringLiteral("USA");
        QVERIFY(doc.addRegion(r));
        TerminalPlacement o;
        o.id     = QStringLiteral("O");
        o.type   = QStringLiteral("Intermodal Land Terminal");
        o.region = QStringLiteral("USA");
        o.properties[QStringLiteral("initial_container_count")] = 5;
        o.properties[QStringLiteral("destination_terminal")] =
            QString(QStringLiteral("O"));  // self-reference
        QVERIFY(doc.addTerminal(o));

        const auto issues = ScenarioValidator::validate(doc);
        bool found = false;
        for (const auto &i : issues)
            if (i.message.contains(
                    QStringLiteral("cannot self-reference")))
                found = true;
        QVERIFY(found);
    }

    // ---- ScenarioSerializer (JSON) ----

    void test_serializer_empty_document_round_trip()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument src;
        src.simulation.endTime = 3600.0;

        QJsonObject j = ScenarioSerializer::toJson(src);
        QCOMPARE(j.value("schema_version").toInt(), 1);

        std::unique_ptr<ScenarioDocument> back = ScenarioSerializer::fromJson(j);
        QVERIFY(back != nullptr);
        QCOMPARE(back->simulation.endTime, 3600.0);
    }

    void test_serializer_region_round_trip()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument src;

        RegionSpec r;
        r.name           = "USA";
        r.color          = "#ff8800";
        r.localOrigin    = { 38.0, -97.0 };
        r.globalPosition = { 38.0, -97.0 };
        src.addRegion(r);

        QJsonObject j = ScenarioSerializer::toJson(src);
        auto back = ScenarioSerializer::fromJson(j);
        QVERIFY(back);
        QCOMPARE(back->regions.size(), 1);
        const RegionSpec &b = back->regions["USA"];
        QCOMPARE(b.color, QStringLiteral("#ff8800"));
        QCOMPARE(b.localOrigin.latitude,  38.0);
        QCOMPARE(b.localOrigin.longitude, -97.0);
    }

    void test_serializer_terminal_with_interfaces_override_round_trip()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument src;
        RegionSpec r; r.name = "USA"; src.addRegion(r);

        TerminalPlacement t;
        t.id = "T1"; t.type = "Truck Parking"; t.region = "USA";
        t.mode = TerminalPlacement::PositionMode::Scene;
        t.scenePos = { 100.0, 200.0 };
        t.interfaces.isSet   = true;
        using Mode = CargoNetSim::Backend::TransportationTypes::TransportationMode;
        t.interfaces.landSide = { Mode::Truck };
        t.interfaces.seaSide.clear();
        src.addTerminal(t);

        QJsonObject j = ScenarioSerializer::toJson(src);
        auto back = ScenarioSerializer::fromJson(j);
        QVERIFY(back);
        QCOMPARE(back->terminals.size(), 1);
        const TerminalPlacement &b = back->terminals["T1"];
        QCOMPARE(static_cast<int>(b.mode),
                 static_cast<int>(TerminalPlacement::PositionMode::Scene));
        QCOMPARE(b.scenePos.x, 100.0);
        QCOMPARE(b.interfaces.isSet, true);
        QCOMPARE(b.interfaces.landSide,
                 (QSet<Mode>{ Mode::Truck }));
        QVERIFY(b.interfaces.seaSide.isEmpty());
    }

    void test_serializer_terminal_without_interfaces_override()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument src;
        RegionSpec r; r.name = "USA"; src.addRegion(r);

        TerminalPlacement t;
        t.id = "T2"; t.type = "Sea Port Terminal"; t.region = "USA";
        t.mode = TerminalPlacement::PositionMode::LatLon;
        t.latLon = { 40.7, -74.0 };
        // Do NOT set t.interfaces — omitted should stay omitted.
        src.addTerminal(t);

        QJsonObject j = ScenarioSerializer::toJson(src);
        auto back = ScenarioSerializer::fromJson(j);
        QVERIFY(back);
        QCOMPARE(back->terminals["T2"].interfaces.isSet, false);
        // system_dynamics defaults to enabled=false (== "omitted"); must
        // round-trip as still-disabled.
        QCOMPARE(back->terminals["T2"].systemDynamics.enabled, false);
    }

    void test_serializer_terminal_with_system_dynamics_round_trip()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument src;
        RegionSpec r; r.name = "USA"; src.addRegion(r);

        TerminalPlacement t;
        t.id = "T3"; t.type = "Sea Port Terminal"; t.region = "USA";
        t.mode = TerminalPlacement::PositionMode::Scene;
        t.scenePos = { 0.0, 0.0 };

        t.systemDynamics.enabled              = true;
        t.systemDynamics.criticalUtilization  = 0.65;
        t.systemDynamics.congestionExponent   = 2.2;
        t.systemDynamics.maxServiceRate       = 120.0;
        t.systemDynamics.shipDelay  = { 0.55, 2.1 };
        t.systemDynamics.truckDelay = { 0.35, 2.6 };
        t.systemDynamics.trainDelay = { 0.85, 3.1 };
        t.systemDynamics.shipArrivalPenalty  = 10000.0;
        t.systemDynamics.truckArrivalPenalty =  1500.0;
        t.systemDynamics.trainArrivalPenalty =  7000.0;
        src.addTerminal(t);

        QJsonObject j = ScenarioSerializer::toJson(src);
        auto back = ScenarioSerializer::fromJson(j);
        QVERIFY(back);
        const SystemDynamicsSpec &sd = back->terminals["T3"].systemDynamics;
        QCOMPARE(sd.enabled,              true);
        QCOMPARE(sd.criticalUtilization,  0.65);
        QCOMPARE(sd.congestionExponent,   2.2);
        QCOMPARE(sd.maxServiceRate,       120.0);
        QCOMPARE(sd.shipDelay.alpha,      0.55);
        QCOMPARE(sd.shipDelay.beta,       2.1);
        QCOMPARE(sd.truckDelay.alpha,     0.35);
        QCOMPARE(sd.trainDelay.beta,      3.1);
        QCOMPARE(sd.shipArrivalPenalty,   10000.0);
        QCOMPARE(sd.truckArrivalPenalty,  1500.0);
        QCOMPARE(sd.trainArrivalPenalty,  7000.0);
    }

    void test_serializer_network_round_trip()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument src;
        RegionSpec r; r.name = "USA"; src.addRegion(r);

        NetworkSpec n;
        n.name = "USA_rail";
        n.type = NetworkSpec::Type::Rail;
        n.referencePoint = { 0.0, 0.0 };
        n.files.insert("nodes", "data/rail/nodes.dat");
        n.files.insert("links", "data/rail/links.dat");
        src.addNetwork("USA", n);

        QJsonObject j = ScenarioSerializer::toJson(src);
        auto back = ScenarioSerializer::fromJson(j);
        QVERIFY(back);
        const NetworkSpec &b = back->regions["USA"].networks["USA_rail"];
        QCOMPARE(static_cast<int>(b.type),
                 static_cast<int>(NetworkSpec::Type::Rail));
        QCOMPARE(b.files.value("nodes"), QStringLiteral("data/rail/nodes.dat"));
    }

    void test_serializer_unknown_schema_version_returns_null()
    {
        using namespace CargoNetSim::Backend::Scenario;
        QJsonObject j;
        j["schema_version"] = 999;
        auto back = ScenarioSerializer::fromJson(j);
        QVERIFY(back == nullptr);
    }

    // ---- ScenarioSerializer (YAML) ----

    void test_serializer_yaml_minimal_fixture_parses()
    {
        using namespace CargoNetSim::Backend::Scenario;
        const QString path =
            QDir(QCoreApplication::applicationDirPath())
                .filePath("fixtures/scenario/minimal.yml");

        QString err;
        auto doc = ScenarioSerializer::fromYaml(path, &err);
        QVERIFY2(doc != nullptr, qPrintable(err));
        QCOMPARE(doc->simulation.endTime, 3600.0);
        QCOMPARE(doc->simulation.dwellMethod, QStringLiteral("normal"));
    }

    void test_serializer_yaml_full_fixture_parses()
    {
        using namespace CargoNetSim::Backend::Scenario;
        const QString path =
            QDir(QCoreApplication::applicationDirPath())
                .filePath("fixtures/scenario/full.yml");

        QString err;
        auto doc = ScenarioSerializer::fromYaml(path, &err);
        QVERIFY2(doc != nullptr, qPrintable(err));
        QCOMPARE(doc->regions.size(), 1);
        QCOMPARE(doc->regions["USA"].networks.size(), 2);
        QCOMPARE(doc->terminals.size(), 1);
        QCOMPARE(doc->linkages.size(), 1);
    }

    void test_serializer_yaml_roundtrip_to_tmp_file_and_back()
    {
        using namespace CargoNetSim::Backend::Scenario;
        const QString srcPath =
            QDir(QCoreApplication::applicationDirPath())
                .filePath("fixtures/scenario/full.yml");

        QString err;
        auto first = ScenarioSerializer::fromYaml(srcPath, &err);
        QVERIFY(first != nullptr);

        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString outPath = tmp.filePath("out.yml");
        QVERIFY2(ScenarioSerializer::toYaml(*first, outPath, &err), qPrintable(err));

        auto second = ScenarioSerializer::fromYaml(outPath, &err);
        QVERIFY2(second != nullptr, qPrintable(err));

        QCOMPARE(second->regions.size(),   first->regions.size());
        QCOMPARE(second->terminals.size(), first->terminals.size());
        QCOMPARE(second->simulation.endTime, first->simulation.endTime);
    }

    void test_serializer_yaml_relative_paths_resolved_against_yaml_dir()
    {
        using namespace CargoNetSim::Backend::Scenario;
        const QString path =
            QDir(QCoreApplication::applicationDirPath())
                .filePath("fixtures/scenario/full.yml");

        QString err;
        auto doc = ScenarioSerializer::fromYaml(path, &err);
        QVERIFY(doc != nullptr);

        // All `files.*` values should be absolute after parsing, anchored
        // on the YAML file's directory. This confirms path resolution ran.
        const QString expectedDir =
            QDir(QCoreApplication::applicationDirPath())
                .filePath("fixtures/scenario");
        const QString trainsNodes =
            doc->regions["USA"].networks["USA_rail"].files.value("nodes");
        QVERIFY(QDir::isAbsolutePath(trainsNodes));
        QVERIFY(trainsNodes.startsWith(expectedDir));
    }

    // ---- ScenarioValidator (structural) ----

    void test_validator_clean_full_fixture_has_no_errors()
    {
        using namespace CargoNetSim::Backend::Scenario;
        const QString path =
            QDir(QCoreApplication::applicationDirPath())
                .filePath("fixtures/scenario/full.yml");
        auto doc = ScenarioSerializer::fromYaml(path);
        QVERIFY(doc);

        QList<ValidationIssue> issues = ScenarioValidator::validate(*doc);
        int errorCount = 0;
        for (const auto &i : issues)
            if (i.severity == ValidationIssue::Error) ++errorCount;
        QCOMPARE(errorCount, 0);
    }

    void test_validator_bad_terminal_type_is_error()
    {
        using namespace CargoNetSim::Backend::Scenario;
        const QString path =
            QDir(QCoreApplication::applicationDirPath())
                .filePath("fixtures/scenario/invalid_bad_terminal_type.yml");
        auto doc = ScenarioSerializer::fromYaml(path);
        QVERIFY(doc);

        QList<ValidationIssue> issues = ScenarioValidator::validate(*doc);
        bool sawTypeError = false;
        for (const auto &i : issues)
        {
            if (i.severity == ValidationIssue::Error && i.message.contains("type"))
                sawTypeError = true;
        }
        QVERIFY(sawTypeError);
    }

    void test_validator_duplicate_region_rejected_at_parse_time()
    {
        using namespace CargoNetSim::Backend::Scenario;
        const QString path =
            QDir(QCoreApplication::applicationDirPath())
                .filePath("fixtures/scenario/invalid_duplicate_region.yml");
        QString err;
        auto doc = ScenarioSerializer::fromYaml(path, &err);
        // The invariant rejects duplicates at addRegion time.
        QVERIFY(doc == nullptr);
    }

    void test_validator_rail_truck_name_collision_defensive()
    {
        // ScenarioDocument::addNetwork stores networks in a QMap keyed by
        // name, which already prevents a name collision between rail and
        // truck at insertion time. The validator's rail/truck collision
        // rule is defense-in-depth for hand-built documents that bypass
        // addNetwork — e.g., by inserting directly into RegionSpec.networks
        // under different map keys. This exercises that unreachable-via-API
        // path to prove the rule fires.
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;
        RegionSpec region;
        region.name = "USA";
        NetworkSpec rail;
        rail.name = "SharedName"; rail.type = NetworkSpec::Type::Rail;
        NetworkSpec truck;
        truck.name = "SharedName"; truck.type = NetworkSpec::Type::Truck;
        // Distinct map keys bypass the name-uniqueness invariant.
        region.networks.insert("SharedName_rail",  rail);
        region.networks.insert("SharedName_truck", truck);
        doc.regions.insert("USA", region);

        const auto issues = ScenarioValidator::validate(doc);
        bool sawCollision = false;
        for (const auto &i : issues)
        {
            if (i.severity == ValidationIssue::Error
                && i.message.contains("both rail and truck"))
                sawCollision = true;
        }
        QVERIFY(sawCollision);
    }

    void test_validator_same_name_in_different_regions_is_ok()
    {
        // A rail network named "X" in region A and a truck network named
        // "X" in region B is fine — collision scope is per-region.
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;
        RegionSpec a; a.name = "A"; doc.addRegion(a);
        RegionSpec b; b.name = "B"; doc.addRegion(b);

        NetworkSpec nA; nA.name = "X"; nA.type = NetworkSpec::Type::Rail;
        NetworkSpec nB; nB.name = "X"; nB.type = NetworkSpec::Type::Truck;
        QVERIFY(doc.addNetwork("A", nA));
        QVERIFY(doc.addNetwork("B", nB));

        const auto issues = ScenarioValidator::validate(doc);
        for (const auto &i : issues)
        {
            QVERIFY2(!(i.severity == ValidationIssue::Error
                       && i.message.contains("both rail and truck")),
                     qPrintable("cross-region same-name must not be flagged"));
        }
    }

    // ---- ScenarioValidator (dwell-time + cross-reference) ----

    void test_validator_bad_dwell_method_is_error()
    {
        using namespace CargoNetSim::Backend::Scenario;
        const QString path =
            QDir(QCoreApplication::applicationDirPath())
                .filePath("fixtures/scenario/invalid_bad_dwell_method.yml");
        auto doc = ScenarioSerializer::fromYaml(path);
        QVERIFY(doc);

        auto issues = ScenarioValidator::validate(*doc);
        bool sawMethod = false;
        for (const auto &i : issues)
        {
            if (i.severity == ValidationIssue::Error && i.path.contains("dwell_time"))
                sawMethod = true;
        }
        QVERIFY(sawMethod);
    }

    void test_validator_normal_dwell_missing_std_dev_is_error()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;
        doc.simulation.dwellMethod = "normal";
        doc.simulation.dwellParams.clear();  // remove defaults
        doc.simulation.dwellParams.insert("mean", 1800.0);   // std_dev missing

        auto issues = ScenarioValidator::validate(doc);
        bool sawMissing = false;
        for (const auto &i : issues)
        {
            if (i.severity == ValidationIssue::Error &&
                i.message.contains("std_dev"))
                sawMissing = true;
        }
        QVERIFY(sawMissing);
    }

    void test_validator_normal_dwell_unknown_key_is_warning()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;
        doc.simulation.dwellMethod = "normal";
        doc.simulation.dwellParams.insert("mean", 1800.0);
        doc.simulation.dwellParams.insert("std_dev", 300.0);
        doc.simulation.dwellParams.insert("scale", 999.0);   // unknown for 'normal'

        auto issues = ScenarioValidator::validate(doc);
        bool sawUnknownWarning = false;
        for (const auto &i : issues)
        {
            if (i.severity == ValidationIssue::Warning &&
                i.message.contains("scale"))
                sawUnknownWarning = true;
        }
        QVERIFY(sawUnknownWarning);
    }

    void test_validator_gamma_params_round_trip()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;
        doc.simulation.dwellMethod = "gamma";
        doc.simulation.dwellParams.insert("shape", 2.0);
        doc.simulation.dwellParams.insert("scale", 1440.0);

        auto issues = ScenarioValidator::validate(doc);
        int errors = 0;
        for (const auto &i : issues)
            if (i.severity == ValidationIssue::Error) ++errors;
        QCOMPARE(errors, 0);
    }

    void test_validator_connection_region_mismatch_is_error()
    {
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;
        RegionSpec usa; usa.name = "USA"; doc.addRegion(usa);

        TerminalPlacement a; a.id="A"; a.type="Sea Port Terminal"; a.region="USA";
        TerminalPlacement b; b.id="B"; b.type="Sea Port Terminal"; b.region="USA";
        doc.addTerminal(a); doc.addTerminal(b);

        Connection c;
        c.fromTerminalId = "A"; c.toTerminalId = "B";
        c.mode = CargoNetSim::Backend::TransportationTypes::TransportationMode::Truck; c.region = "CAN";   // <-- mismatches both endpoints
        doc.addConnection(c);

        auto issues = ScenarioValidator::validate(doc);
        bool sawRegionError = false;
        for (const auto &i : issues)
        {
            if (i.severity == ValidationIssue::Error
                && i.path.endsWith(".region")
                && i.message.contains("does not match"))
                sawRegionError = true;
        }
        QVERIFY(sawRegionError);
    }

    void test_validator_connection_region_empty_is_not_error()
    {
        // Connection.region is user-optional; blank means "I trust the
        // endpoints." Only an explicit mismatch is flagged.
        using namespace CargoNetSim::Backend::Scenario;
        ScenarioDocument doc;
        RegionSpec usa; usa.name = "USA"; doc.addRegion(usa);

        TerminalPlacement a; a.id="A"; a.type="Sea Port Terminal"; a.region="USA";
        TerminalPlacement b; b.id="B"; b.type="Sea Port Terminal"; b.region="USA";
        doc.addTerminal(a); doc.addTerminal(b);

        Connection c;
        c.fromTerminalId = "A"; c.toTerminalId = "B";
        c.mode = CargoNetSim::Backend::TransportationTypes::TransportationMode::Truck;       // .region intentionally left blank
        doc.addConnection(c);

        auto issues = ScenarioValidator::validate(doc);
        for (const auto &i : issues)
            QVERIFY(!(i.severity == ValidationIssue::Error
                      && i.path.endsWith(".region")));
    }

    // ---- Golden round-trip integration ----

    void test_integration_full_fixture_validates_and_round_trips()
    {
        using namespace CargoNetSim::Backend::Scenario;
        const QString path =
            QDir(QCoreApplication::applicationDirPath())
                .filePath("fixtures/scenario/full.yml");

        // Parse YAML.
        QString err;
        auto first = ScenarioSerializer::fromYaml(path, &err);
        QVERIFY2(first != nullptr, qPrintable(err));

        // Validate → no errors.
        auto issues = ScenarioValidator::validate(*first);
        int errCount = 0;
        for (const auto &i : issues)
            if (i.severity == ValidationIssue::Error) ++errCount;
        QCOMPARE(errCount, 0);

        // Serialize to JSON.
        QJsonObject j1 = ScenarioSerializer::toJson(*first);

        // Re-parse from JSON.
        auto second = ScenarioSerializer::fromJson(j1);
        QVERIFY(second != nullptr);

        // Serialize second → JSON, compare structurally to j1.
        QJsonObject j2 = ScenarioSerializer::toJson(*second);
        QCOMPARE(QJsonDocument(j1).toJson(QJsonDocument::Compact),
                 QJsonDocument(j2).toJson(QJsonDocument::Compact));
    }

    void test_integration_yaml_to_json_to_yaml_preserves_structure()
    {
        using namespace CargoNetSim::Backend::Scenario;
        const QString path =
            QDir(QCoreApplication::applicationDirPath())
                .filePath("fixtures/scenario/full.yml");

        auto doc1 = ScenarioSerializer::fromYaml(path);
        QVERIFY(doc1);

        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString out = tmp.filePath("out.yml");
        QVERIFY(ScenarioSerializer::toYaml(*doc1, out));

        auto doc2 = ScenarioSerializer::fromYaml(out);
        QVERIFY(doc2);

        QCOMPARE(doc2->regions.size(),     doc1->regions.size());
        QCOMPARE(doc2->terminals.size(),   doc1->terminals.size());
        QCOMPARE(doc2->linkages.size(),    doc1->linkages.size());
        QCOMPARE(doc2->connections.size(), doc1->connections.size());
        QCOMPARE(doc2->globalLinks.size(), doc1->globalLinks.size());

        // Spot-check that scenario-relative paths stayed absolute after the
        // second pass (because out.yml now lives in tmp, its rel paths would
        // be broken — so writer must emit absolute paths for files).
        const QString nodes = doc2->regions["USA"].networks["USA_rail"].files.value("nodes");
        QVERIFY(QDir::isAbsolutePath(nodes));
    }
};

QTEST_MAIN(ScenarioTest)
#include "ScenarioTest.moc"
