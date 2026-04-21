#include "ScenarioApplier.h"

#include "Backend/Commons/LogCategories.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Controllers/ConfigController.h"
#include "Backend/Controllers/RegionDataController.h"
#include "Backend/Controllers/VehicleController.h"
#include "Backend/Models/ShipSystem.h"
#include "Backend/Models/TrainSystem.h"
#include "Backend/Models/Terminal.h"
#include "InterfaceConversion.h"
#include "PropertyKeys.h"
#include "TerminalTypeDefaults.h"

#include <containerLib/container.h>

#include <QJsonObject>
#include <QJsonValue>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

namespace PK = PropertyKeys;

bool ScenarioApplier::apply(ScenarioDocument &doc,
                            CargoNetSim::CargoNetSimController &controller,
                            ScenarioRegistry &registry,
                            QString *error)
{
    qCInfo(lcScenario) << "ScenarioApplier::apply: begin";
    clearAll(controller, registry);

    qCDebug(lcScenario) << "ScenarioApplier::apply: applyRegions";
    if (!applyRegions         (doc, controller, error))          return false;
    qCDebug(lcScenario) << "ScenarioApplier::apply: applyNetworks";
    if (!applyNetworks        (doc, controller, error))          return false;
    qCDebug(lcScenario) << "ScenarioApplier::apply: applyTerminals";
    if (!applyTerminals       (doc, registry,   error))          return false;
    qCDebug(lcScenario) << "ScenarioApplier::apply: applyFleet";
    if (!applyFleet           (doc, controller, registry, error))return false;
    qCDebug(lcScenario) << "ScenarioApplier::apply: applySettings";
    if (!applySettings        (doc, controller, error))          return false;
    qCDebug(lcScenario) << "ScenarioApplier::apply: applyOriginContainers";
    if (!applyOriginContainers(doc,             error))          return false;
    qCInfo(lcScenario) << "ScenarioApplier::apply: complete";
    return true;
}

void ScenarioApplier::clearAll(CargoNetSim::CargoNetSimController &controller,
                               ScenarioRegistry &registry)
{
    qCInfo(lcScenario) << "ScenarioApplier::clearAll: clearing controllers and registry";
    auto *regions  = controller.getRegionDataController();
    auto *vehicles = controller.getVehicleController();
    if (regions)  regions->clear();
    if (vehicles) vehicles->clear();
    registry.clear();
}

bool ScenarioApplier::applyRegions(const ScenarioDocument &doc,
                                   CargoNetSim::CargoNetSimController &controller,
                                   QString *error)
{
    qCDebug(lcScenario) << "ScenarioApplier::applyRegions:"
                        << doc.regions.size() << "regions";
    auto *regions = controller.getRegionDataController();
    if (!regions)
    {
        qCWarning(lcScenario) << "ScenarioApplier::applyRegions: RegionDataController unavailable";
        if (error) *error = QStringLiteral("RegionDataController unavailable");
        return false;
    }

    for (auto it = doc.regions.constBegin(); it != doc.regions.constEnd(); ++it)
    {
        const RegionSpec &r = it.value();
        if (r.name.isEmpty())
        {
            qCWarning(lcScenario) << "ScenarioApplier::applyRegions: region with empty name";
            if (error) *error = QStringLiteral("Scenario contains a region with no name");
            return false;
        }
        if (!regions->addRegion(r.name))
        {
            qCWarning(lcScenario) << "ScenarioApplier::applyRegions: failed to add region"
                                  << r.name;
            if (error) *error = QStringLiteral("Failed to add region '%1'").arg(r.name);
            return false;
        }
        auto *rd = regions->getRegionData(r.name);
        if (!rd)
        {
            if (error) *error = QStringLiteral("Region '%1' disappeared after add").arg(r.name);
            return false;
        }
        if (!r.color.isEmpty()) {
            qCInfo(lcScenario)
                << "ScenarioApplier::applyRegions: setRegionVariable color for"
                << r.name << "value=" << r.color;
            // Route through the controller so regionVariableChanged fires —
            // observers (RegionManagerWidget) that refreshed on the earlier
            // regionAdded signal need this event to pick up the colour,
            // since it's only set AFTER the add.
            regions->setRegionVariable(r.name, "color", r.color);
        }

        QVariantMap center;
        center["latitude"]         = r.localOrigin.latitude;
        center["longitude"]        = r.localOrigin.longitude;
        center["shared_latitude"]  = r.globalPosition.latitude;
        center["shared_longitude"] = r.globalPosition.longitude;
        // Distinct from the GUI-owned "regionCenterPoint" pointer key
        // (ViewController.cpp:3287). Plan 4's RegionCenterPointFactory
        // reads this map and writes the resulting item pointer under
        // the legacy key.
        rd->setVariable("scenarioRegionCenter", center);
    }
    qCDebug(lcScenario) << "ScenarioApplier::applyRegions: success";
    return true;
}

bool ScenarioApplier::applyNetworks(const ScenarioDocument &doc,
                                    CargoNetSim::CargoNetSimController &controller,
                                    QString *error)
{
    int networkCount = 0;
    for (const auto &r : doc.regions)
        networkCount += r.networks.size();
    qCDebug(lcScenario) << "ScenarioApplier::applyNetworks:"
                        << networkCount << "networks across"
                        << doc.regions.size() << "regions";
    auto *regions = controller.getRegionDataController();

    for (auto it = doc.regions.constBegin(); it != doc.regions.constEnd(); ++it)
    {
        auto *rd = regions->getRegionData(it.key());
        if (!rd) continue;

        for (const NetworkSpec &n : it.value().networks.values())
        {
            try
            {
                switch (n.type)
                {
                case NetworkSpec::Type::Rail:
                {
                    const QString nodesPath = n.files.value("nodes");
                    const QString linksPath = n.files.value("links");
                    if (nodesPath.isEmpty() || linksPath.isEmpty())
                    {
                        if (error)
                            *error = QStringLiteral("Rail network '%1' requires both nodes and links file paths")
                                         .arg(n.name);
                        return false;
                    }
                    rd->addTrainNetwork(n.name, nodesPath, linksPath);
                    break;
                }
                case NetworkSpec::Type::Truck:
                {
                    const QString configPath = n.files.value("config");
                    if (configPath.isEmpty())
                    {
                        if (error)
                            *error = QStringLiteral("Truck network '%1' requires a config file path")
                                         .arg(n.name);
                        return false;
                    }
                    rd->addTruckNetwork(n.name, configPath);
                    break;
                }
                }
            }
            catch (const std::exception &e)
            {
                qCCritical(lcScenario) << "ScenarioApplier::applyNetworks: exception loading"
                                       << n.name << "-" << e.what();
                if (error)
                    *error = QStringLiteral("Failed to load network '%1': %2")
                                 .arg(n.name, QString::fromUtf8(e.what()));
                return false;
            }
        }
    }
    qCDebug(lcScenario) << "ScenarioApplier::applyNetworks: success";
    return true;
}

namespace
{

// Copy the property subset that Backend::Terminal expects into its config
// JSON. Mirrors PathFindingWorker.cpp:454-550.
QJsonObject buildTerminalConfigJson(const QMap<QString, QVariant> &properties)
{
    QJsonObject config;
    const QStringList keysToCopy = {
        "cost", "customs", "dwell_time", "capacity",
        "mode_network_aliases", "system_dynamics"
    };
    for (const QString &key : keysToCopy)
    {
        if (!properties.contains(key)) continue;
        config[key] = QJsonValue::fromVariant(properties.value(key));
    }
    return config;
}

} // namespace

bool ScenarioApplier::applyTerminals(const ScenarioDocument &doc,
                                     ScenarioRegistry &registry,
                                     QString *error)
{
    qCDebug(lcScenario) << "ScenarioApplier::applyTerminals:"
                        << doc.terminals.size() << "terminals";
    for (auto it = doc.terminals.constBegin(); it != doc.terminals.constEnd(); ++it)
    {
        const TerminalPlacement &t = it.value();

        if (t.id.isEmpty())
        {
            qCWarning(lcScenario) << "ScenarioApplier::applyTerminals: terminal with empty id";
            if (error) *error = QStringLiteral("Terminal with empty id");
            return false;
        }
        if (!TerminalTypeDefaults::isValidType(t.type))
        {
            qCWarning(lcScenario) << "ScenarioApplier::applyTerminals: invalid type"
                                  << t.type << "for terminal" << t.id;
            if (error) *error = QStringLiteral("Terminal '%1' has invalid type '%2'")
                                    .arg(t.id, t.type);
            return false;
        }

        // Plan 7: InterfaceSet is now typed QSet<TransportationMode>. No more
        // string intermediate — read straight into the backend's InterfaceMap
        // shape. Defaults (isSet==false) flow through TerminalTypeDefaults's
        // also-typed return.
        using Mode = TransportationTypes::TransportationMode;
        QSet<Mode> land, sea;
        if (t.interfaces.isSet)
        {
            land = t.interfaces.landSide;
            sea  = t.interfaces.seaSide;
        }
        else
        {
            const auto defaults = TerminalTypeDefaults::interfacesFor(t.type);
            land = defaults.first;
            sea  = defaults.second;
        }
        InterfaceConversion::InterfaceMap ifaceMap;
        if (!land.isEmpty())
            ifaceMap.insert(TerminalTypes::TerminalInterface::LAND_SIDE, land);
        if (!sea.isEmpty())
            ifaceMap.insert(TerminalTypes::TerminalInterface::SEA_SIDE, sea);

        const QString display = t.properties.value("Name", t.id).toString();
        QStringList names = { t.id };

        QJsonObject config = buildTerminalConfigJson(t.properties);

        if (t.systemDynamics.enabled)
        {
            QJsonObject modeDelayParams;
            modeDelayParams["ship"]  = t.systemDynamics.shipDelay.toJson();
            modeDelayParams["truck"] = t.systemDynamics.truckDelay.toJson();
            modeDelayParams["train"] = t.systemDynamics.trainDelay.toJson();
            QJsonObject arrivalPenalties;
            arrivalPenalties["ship"]  = t.systemDynamics.shipArrivalPenalty;
            arrivalPenalties["truck"] = t.systemDynamics.truckArrivalPenalty;
            arrivalPenalties["train"] = t.systemDynamics.trainArrivalPenalty;
            QJsonObject sd;
            sd["enabled"]                = true; // always true — block only runs when enabled
            sd["critical_utilization"]   = t.systemDynamics.criticalUtilization;
            sd["congestion_exponent"]    = t.systemDynamics.congestionExponent;
            sd["congestion_sensitivity"] = t.systemDynamics.congestionSensitivity;
            sd["delay_sensitivity"]      = t.systemDynamics.delaySensitivity;
            sd["max_service_rate"]       = t.systemDynamics.maxServiceRate;
            sd["mode_delay_params"]      = modeDelayParams;
            sd["arrival_penalties"]      = arrivalPenalties;
            config["system_dynamics"]    = sd;
        }

        auto *term = new Terminal(names, display, config, ifaceMap, t.region);
        if (!registry.addTerminal(t.id, term))
        {
            qCWarning(lcScenario) << "ScenarioApplier::applyTerminals: duplicate id"
                                  << t.id;
            if (error) *error = QStringLiteral("Duplicate terminal id '%1' in registry").arg(t.id);
            return false;
        }
    }
    qCDebug(lcScenario) << "ScenarioApplier::applyTerminals: success";
    return true;
}

bool ScenarioApplier::applyFleet(const ScenarioDocument &doc,
                                 CargoNetSim::CargoNetSimController &controller,
                                 ScenarioRegistry &registry,
                                 QString *error)
{
    qCDebug(lcScenario) << "ScenarioApplier::applyFleet: trains="
                        << doc.fleet.trainsFiles.size() << "file(s) ships="
                        << doc.fleet.shipsFiles.size()  << "file(s)";
    auto *vehicles = controller.getVehicleController();
    if (!vehicles)
    {
        qCWarning(lcScenario) << "ScenarioApplier::applyFleet: VehicleController unavailable";
        if (error) *error = QStringLiteral("VehicleController unavailable");
        return false;
    }

    for (const QString &path : doc.fleet.trainsFiles)
    {
        QVector<Backend::Train *> loaded =
            Backend::TrainsReader::readTrainsFile(path, vehicles);
        if (loaded.isEmpty())
        {
            qCWarning(lcScenario) << "ScenarioApplier::applyFleet: no trains loaded from" << path;
            if (error) *error = QStringLiteral("Failed to load trains from '%1'").arg(path);
            return false;
        }
        for (Backend::Train *t : loaded)
        {
            if (!vehicles->addTrain(t))
                qCWarning(lcScenario) << "ScenarioApplier::applyFleet: duplicate train id in" << path;
        }
        qCDebug(lcScenario) << "ScenarioApplier::applyFleet: loaded"
                            << loaded.size() << "trains from" << path;
    }

    for (const QString &path : doc.fleet.shipsFiles)
    {
        QVector<Backend::Ship *> loaded =
            Backend::ShipsReader::readShipsFile(path, vehicles);
        if (loaded.isEmpty())
        {
            qCWarning(lcScenario) << "ScenarioApplier::applyFleet: no ships loaded from" << path;
            if (error) *error = QStringLiteral("Failed to load ships from '%1'").arg(path);
            return false;
        }
        for (Backend::Ship *s : loaded)
        {
            if (!vehicles->addShip(s))
                qCWarning(lcScenario) << "ScenarioApplier::applyFleet: duplicate ship id in" << path;
        }
        qCDebug(lcScenario) << "ScenarioApplier::applyFleet: loaded"
                            << loaded.size() << "ships from" << path;
    }

    TruckFleetSpec truck;
    truck.files   = doc.fleet.trucksFiles;
    truck.inline_ = doc.fleet.trucksInline;
    registry.setTruckFleet(truck);

    qCDebug(lcScenario) << "ScenarioApplier::applyFleet: success";
    return true;
}

bool ScenarioApplier::applySettings(const ScenarioDocument &doc,
                                    CargoNetSim::CargoNetSimController &controller,
                                    QString *error)
{
    qCDebug(lcScenario) << "ScenarioApplier::applySettings:"
                        << "endTime=" << (doc.simulation.endTime.has_value()
                                          ? doc.simulation.endTime.value() : -1.0)
                        << "timeStep=" << (doc.simulation.timeStep.has_value()
                                           ? doc.simulation.timeStep.value() : -1)
                        << "fuelTypes=" << doc.simulation.fuelTypes.size();
    auto *cfg = controller.getConfigController();
    if (!cfg)
    {
        qCWarning(lcScenario) << "ScenarioApplier::applySettings: ConfigController unavailable";
        if (error) *error = QStringLiteral("ConfigController unavailable");
        return false;
    }

    const SimulationSettings &s = doc.simulation;

    // Start from config.xml as the base — absent scenario fields inherit these values.
    QVariantMap root           = cfg->getAllParams();
    QVariantMap simulation     = root.value("simulation").toMap();
    QVariantMap carbonTaxes    = root.value("carbon_taxes").toMap();
    QVariantMap transportModes = root.value("transport_modes").toMap();
    QVariantMap fuelEnergy     = root.value("fuel_energy").toMap();
    QVariantMap fuelCarbon     = root.value("fuel_carbon_content").toMap();
    QVariantMap fuelPrices     = root.value("fuel_prices").toMap();

    // Overlay simulation section — only fields present in the YAML.
    if (s.timeStep.has_value())              simulation["time_step"]            = s.timeStep.value();
    if (s.endTime.has_value())               simulation["end_time"]             = s.endTime.value();
    if (s.shortestPathsN.has_value())        simulation["shortest_paths"]       = s.shortestPathsN.value();
    if (s.timeValueOfMoney.has_value())      simulation["time_value_of_money"]  = s.timeValueOfMoney.value();
    if (s.useSpecificTimeValues.has_value()) simulation["use_mode_specific"]    = s.useSpecificTimeValues.value();

    // Overlay carbon taxes.
    if (s.carbonRate.has_value())      carbonTaxes["rate"]             = s.carbonRate.value();
    if (s.shipMultiplier.has_value())  carbonTaxes["ship_multiplier"]  = s.shipMultiplier.value();
    if (s.railMultiplier.has_value())  carbonTaxes["rail_multiplier"]  = s.railMultiplier.value();
    if (s.truckMultiplier.has_value()) carbonTaxes["truck_multiplier"] = s.truckMultiplier.value();

    // Overlay per-mode settings (field-level within each mode).
    auto applyMode = [](QVariantMap &modeMap, const SimulationSettings::Mode &m)
    {
        if (m.speed.has_value())      modeMap[PK::Mode::AverageSpeed]           = m.speed.value();
        if (m.fuelType.has_value())   modeMap[PK::Mode::FuelType]               = m.fuelType.value();
        if (m.fuelRate.has_value())   modeMap[PK::Mode::AverageFuelConsumption] = m.fuelRate.value();
        if (m.containers.has_value()) modeMap[PK::Mode::AverageContainerNumber] = m.containers.value();
        if (m.risk.has_value())       modeMap[PK::Mode::RiskFactor]             = m.risk.value();
        if (m.timeValue.has_value())  modeMap[PK::Mode::TimeValueOfMoney]       = m.timeValue.value();
        if (m.useNetwork.has_value()) modeMap[PK::Mode::UseNetwork]             = m.useNetwork.value();
    };

    QVariantMap shipMap  = transportModes.value("ship").toMap();
    QVariantMap railMap  = transportModes.value("rail").toMap();
    QVariantMap truckMap = transportModes.value("truck").toMap();
    applyMode(shipMap,  s.ship);
    applyMode(railMap,  s.rail);
    applyMode(truckMap, s.truck);
    transportModes["ship"]  = shipMap;
    transportModes["rail"]  = railMap;
    transportModes["truck"] = truckMap;

    // Overlay fuel types (field-level; new fuels added, existing fuels merged).
    for (auto it = s.fuelTypes.constBegin(); it != s.fuelTypes.constEnd(); ++it) {
        const QString &name = it.key();
        const auto    &f    = it.value();
        if (f.energy.has_value()) fuelEnergy[name] = f.energy.value();
        if (f.carbon.has_value()) fuelCarbon[name] = f.carbon.value();
        if (f.price.has_value())  fuelPrices[name] = f.price.value();
    }

    // Commit merged config.
    root["simulation"]          = simulation;
    root["carbon_taxes"]        = carbonTaxes;
    root["transport_modes"]     = transportModes;
    root["fuel_energy"]         = fuelEnergy;
    root["fuel_carbon_content"] = fuelCarbon;
    root["fuel_prices"]         = fuelPrices;

    cfg->updateConfig(root);
    qCDebug(lcScenario) << "ScenarioApplier::applySettings: success";
    return true;
}

bool ScenarioApplier::applyOriginContainers(ScenarioDocument &doc,
                                            QString          *error)
{
    qCDebug(lcScenario) << "ScenarioApplier::applyOriginContainers:"
                        << doc.terminals.size() << "terminals to scan";
    Q_UNUSED(error);

    // Task 0 (Plan 5) shorthand form: every terminal whose properties
    // include `initial_container_count > 0` is an origin, and the applier
    // creates N bare containers for it. Per-container authoring metadata
    // (ids beyond the index-based default, sizes, customVars) comes from
    // the Task 0.1 explicit `containers:` list — handled in a separate
    // applier extension layered on top of this one.
    //
    // Containers start with ONLY `containerCurrentLocation = terminalId`.
    // `containerNextDestinations` stays empty on purpose: the downstream
    // `SimulationRequestBuilder::takeContainersForVehicle` appends
    // segment-level graph-node ids per vehicle at dispatch time. Seeding
    // here (with a terminal id) would corrupt the routing sequence.
    for (auto it = doc.terminals.constBegin();
         it != doc.terminals.constEnd(); ++it)
    {
        const QString &id    = it.key();
        const auto    &props = it.value().properties;
        const int count =
            props.value(PK::Terminal::InitialContainerCount, 0)
                .toInt();
        if (count <= 0) continue;

        QList<ContainerCore::Container *> pool;
        pool.reserve(count);
        for (int i = 0; i < count; ++i)
        {
            auto *c = new ContainerCore::Container();
            c->setContainerID(QStringLiteral("%1_%2").arg(id).arg(i));
            c->setContainerCurrentLocation(id);
            pool.append(c);
        }
        doc.setOriginContainers(id, std::move(pool));
    }
    qCDebug(lcScenario) << "ScenarioApplier::applyOriginContainers: success";
    return true;
}

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
