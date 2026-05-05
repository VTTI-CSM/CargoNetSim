#include "ScenarioSerializer.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Commons/TransportationMode.h"
#include "RouteMetricUnits.h"
#include "TerminalTypeDefaults.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTextStream>
#include <yaml-cpp/yaml.h>
#include <sstream>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

namespace
{

// ----- small JSON helpers (file-local, no leakage into public API) -----

QJsonArray stringListToArray(const QStringList &src)
{
    QJsonArray a;
    for (const QString &s : src) a.append(s);
    return a;
}

QStringList arrayToStringList(const QJsonArray &src)
{
    QStringList out;
    for (const QJsonValue &v : src) out << v.toString();
    return out;
}

QJsonObject latLonToJson(const LatLon &p)
{
    QJsonObject o;
    o["latitude"]  = p.latitude;
    o["longitude"] = p.longitude;
    return o;
}

LatLon jsonToLatLon(const QJsonObject &o)
{
    return { o.value("latitude").toDouble(), o.value("longitude").toDouble() };
}

QJsonObject point2DToJson(const Point2D &p)
{
    QJsonObject o; o["x"] = p.x; o["y"] = p.y; return o;
}

Point2D jsonToPoint2D(const QJsonObject &o)
{
    return { o.value("x").toDouble(), o.value("y").toDouble() };
}

QJsonObject variantMapToJson(const QMap<QString, QVariant> &m)
{
    return QJsonObject::fromVariantMap(m);
}

QMap<QString, QVariant> jsonToVariantMap(const QJsonObject &o)
{
    return o.toVariantMap();
}

QSet<TransportationTypes::TransportationMode>
interfaceModesFromStrings(const QStringList &values,
                          const QString &context)
{
    QSet<TransportationTypes::TransportationMode> modes;
    for (const QString &value : values)
    {
        bool ok = false;
        const auto mode = interfaceModeFromCanonicalString(value, &ok);
        if (ok && mode != TransportationTypes::TransportationMode::Any)
        {
            modes.insert(mode);
        }
        else
        {
            qCWarning(lcScenario)
                << "ScenarioSerializer:" << context
                << "unknown terminal interface mode skipped:" << value;
        }
    }
    return modes;
}

TerminalPlacement::InterfaceSet
interfaceSetFromJsonObject(const QJsonObject &iface)
{
    TerminalPlacement::InterfaceSet result;
    result.isSet = true;
    result.landSide =
        interfaceModesFromStrings(
            arrayToStringList(iface.value("land_side").toArray()),
            QStringLiteral("interfaces.land_side"));
    result.seaSide =
        interfaceModesFromStrings(
            arrayToStringList(iface.value("sea_side").toArray()),
            QStringLiteral("interfaces.sea_side"));
    return result;
}

// ----- YAML ↔ QJsonValue bridge -----
//
// Strategy: convert YAML ↔ QJsonDocument once, then delegate to toJson/fromJson.
// Keeps the JSON path as the single source of truth for the schema — YAML
// support is purely a format translation layer.

QJsonValue yamlNodeToJsonValue(const YAML::Node &node);

QJsonObject yamlMapToJsonObject(const YAML::Node &map)
{
    QJsonObject o;
    for (auto it = map.begin(); it != map.end(); ++it)
    {
        const QString key = QString::fromStdString(it->first.as<std::string>());
        o[key] = yamlNodeToJsonValue(it->second);
    }
    return o;
}

QJsonArray yamlSeqToJsonArray(const YAML::Node &seq)
{
    QJsonArray a;
    for (const YAML::Node &child : seq) a.append(yamlNodeToJsonValue(child));
    return a;
}

QJsonValue yamlNodeToJsonValue(const YAML::Node &node)
{
    switch (node.Type())
    {
    case YAML::NodeType::Null:   return QJsonValue();
    case YAML::NodeType::Scalar:
    {
        const std::string s = node.Scalar();
        // Try int, then double, then bool, finally string.
        // Both numeric parses must consume the ENTIRE scalar, otherwise
        // "0.6" would match std::stoll (returning 0, pos=1) and silently
        // collapse to integer zero, breaking fractional fields like
        // destination fractions and fuel rates.
        try
        {
            size_t pos = 0;
            qint64 i = static_cast<qint64>(std::stoll(s, &pos));
            if (pos == s.size()) return QJsonValue(i);
        } catch (...) {}
        try
        {
            size_t pos = 0;
            double d = std::stod(s, &pos);
            if (pos == s.size()) return QJsonValue(d);
        } catch (...) {}
        if (s == "true" || s == "True")  return QJsonValue(true);
        if (s == "false" || s == "False") return QJsonValue(false);
        return QJsonValue(QString::fromStdString(s));
    }
    case YAML::NodeType::Sequence: return yamlSeqToJsonArray(node);
    case YAML::NodeType::Map:      return yamlMapToJsonObject(node);
    case YAML::NodeType::Undefined:
    default:                       return QJsonValue();
    }
}

YAML::Node jsonValueToYamlNode(const QJsonValue &v)
{
    YAML::Node out;
    switch (v.type())
    {
    case QJsonValue::Null:   out = YAML::Node(YAML::NodeType::Null); break;
    case QJsonValue::Bool:   out = v.toBool(); break;
    case QJsonValue::Double:
        if (v.toDouble() == static_cast<double>(v.toInt()))
            out = v.toInt();
        else
            out = v.toDouble();
        break;
    case QJsonValue::String: out = v.toString().toStdString(); break;
    case QJsonValue::Array:
    {
        out = YAML::Node(YAML::NodeType::Sequence);
        for (const QJsonValue &child : v.toArray()) out.push_back(jsonValueToYamlNode(child));
        break;
    }
    case QJsonValue::Object:
    {
        out = YAML::Node(YAML::NodeType::Map);
        QJsonObject o = v.toObject();
        for (auto it = o.constBegin(); it != o.constEnd(); ++it)
            out[it.key().toStdString()] = jsonValueToYamlNode(it.value());
        break;
    }
    case QJsonValue::Undefined: out = YAML::Node(YAML::NodeType::Null); break;
    }
    return out;
}

// ----- Relative path resolution -----
//
// Walks regions[*].networks[*].files.* and fleet.*.files[], replacing every
// relative path with an absolute path anchored on yamlDir.
void resolvePathsRelativeTo(ScenarioDocument &doc, const QString &yamlDir)
{
    const QDir anchor(yamlDir);
    auto resolve = [&](QString &p)
    {
        if (p.isEmpty()) return;
        QString normalized = QDir::fromNativeSeparators(p);
        if (QDir::isAbsolutePath(normalized)) { p = normalized; return; }
        p = QDir::cleanPath(anchor.filePath(normalized));
    };

    for (RegionSpec &r : doc.regions)
        for (NetworkSpec &n : r.networks)
            for (auto it = n.files.begin(); it != n.files.end(); ++it)
                resolve(it.value());

    for (QString &p : doc.fleet.trainsFiles) resolve(p);
    for (QString &p : doc.fleet.shipsFiles)  resolve(p);
    for (QString &p : doc.fleet.trucksFiles) resolve(p);

}

// ----- per-struct serializers -----

QJsonObject networkSpecToJson(const NetworkSpec &n)
{
    QJsonObject o;
    o["name"]            = n.name;
    o["type"]            = networkKindToString(n.type);
    o["reference_point"] = point2DToJson(n.referencePoint);
    QJsonObject files;
    for (auto it = n.files.constBegin(); it != n.files.constEnd(); ++it)
        files[it.key()] = it.value();
    o["files"] = files;
    return o;
}

NetworkSpec networkSpecFromJson(const QJsonObject &o)
{
    NetworkSpec n;
    n.name = o.value("name").toString();
    bool ok = false;
    n.type = networkKindFromString(o.value("type").toString(), &ok);
    n.referencePoint = jsonToPoint2D(o.value("reference_point").toObject());
    QJsonObject files = o.value("files").toObject();
    for (auto it = files.constBegin(); it != files.constEnd(); ++it)
        n.files.insert(it.key(), it.value().toString());
    return n;
}

QJsonObject terminalPlacementToJson(const TerminalPlacement &t)
{
    QJsonObject o;
    o["id"]                 = t.id;
    o["type"]               = t.type;
    if (t.role != TerminalPlacement::TerminalRole::Transit)
        o["role"] = roleToString(t.role);
    o["region"] = t.region;

    QMap<QString, QVariant> properties = t.properties;
    properties.remove(QStringLiteral("Available Interfaces"));
    o["properties"] = variantMapToJson(properties);

    QJsonObject position;
    position["mode"] = positionModeToString(t.mode);
    switch (t.mode)
    {
    case TerminalPlacement::PositionMode::NetworkNode:
        position["network"] = t.networkRef;
        position["node_id"] = t.nodeId;
        break;
    case TerminalPlacement::PositionMode::LatLon:
        position["latitude"]  = t.latLon.latitude;
        position["longitude"] = t.latLon.longitude;
        break;
    case TerminalPlacement::PositionMode::Scene:
        position["x"] = t.scenePos.x;
        position["y"] = t.scenePos.y;
        break;
    }
    o["position"] = position;

    if (t.interfaces.isSet)
    {
        auto typedSetToArray =
            [](const QSet<TransportationTypes::TransportationMode> &modes) {
                QJsonArray arr;
                for (auto m : modes)
                {
                    const QString canonical = interfaceModeCanonicalString(m);
                    if (!canonical.isEmpty()) arr.append(canonical);
                }
                return arr;
            };
        QJsonObject iface;
        iface["land_side"] = typedSetToArray(t.interfaces.landSide);
        iface["sea_side"]  = typedSetToArray(t.interfaces.seaSide);
        o["interfaces"] = iface;
    }

    // system_dynamics is serialized only when enabled == true; otherwise the
    // key is simply absent (round-trip-stable because fromJson treats a
    // missing key as "disabled + all defaults").
    if (t.systemDynamics.enabled)
    {
        QJsonObject sd;
        sd["enabled"]               = t.systemDynamics.enabled;
        sd["critical_utilization"]  = t.systemDynamics.criticalUtilization;
        sd["congestion_exponent"]   = t.systemDynamics.congestionExponent;
        sd["congestion_sensitivity"]= t.systemDynamics.congestionSensitivity;
        sd["delay_sensitivity"]     = t.systemDynamics.delaySensitivity;
        sd["max_service_rate"]      = t.systemDynamics.maxServiceRate;
        sd["ship_delay"]            = t.systemDynamics.shipDelay.toJson();
        sd["truck_delay"]           = t.systemDynamics.truckDelay.toJson();
        sd["train_delay"]           = t.systemDynamics.trainDelay.toJson();
        sd["ship_arrival_penalty"]  = t.systemDynamics.shipArrivalPenalty;
        sd["truck_arrival_penalty"] = t.systemDynamics.truckArrivalPenalty;
        sd["train_arrival_penalty"] = t.systemDynamics.trainArrivalPenalty;
        o["system_dynamics"] = sd;
    }
    return o;
}

TerminalPlacement terminalPlacementFromJson(const QJsonObject &o)
{
    TerminalPlacement t;
    t.id     = o.value("id").toString();
    t.type   = o.value("type").toString();
    t.region     = o.value("region").toString();
    t.properties = jsonToVariantMap(o.value("properties").toObject());
    t.properties.remove(QStringLiteral("Available Interfaces"));

    if (o.contains("role"))
    {
        bool ok = false;
        t.role  = roleFromString(
            o.value("role").toString(), &ok);
        if (!ok)
            qCWarning(lcScenario)
                << "terminalPlacementFromJson:"
                << "unknown role value:"
                << o.value("role").toString();
    }

    QJsonObject position = o.value("position").toObject();
    bool ok = false;
    t.mode = positionModeFromString(position.value("mode").toString(), &ok);
    switch (t.mode)
    {
    case TerminalPlacement::PositionMode::NetworkNode:
        t.networkRef = position.value("network").toString();
        t.nodeId     = position.value("node_id").toInt(-1);
        break;
    case TerminalPlacement::PositionMode::LatLon:
        t.latLon.latitude  = position.value("latitude").toDouble();
        t.latLon.longitude = position.value("longitude").toDouble();
        break;
    case TerminalPlacement::PositionMode::Scene:
        t.scenePos.x = position.value("x").toDouble();
        t.scenePos.y = position.value("y").toDouble();
        break;
    }

    if (o.contains("interfaces"))
    {
        t.interfaces =
            interfaceSetFromJsonObject(o.value("interfaces").toObject());
    }

    if (o.contains("system_dynamics"))
    {
        auto modeDelayFromJson = [](const QJsonObject &o, ModeDelayParams def)
        {
            ModeDelayParams p;
            p.alpha = o.value("alpha").toDouble(def.alpha);
            p.beta  = o.value("beta").toDouble(def.beta);
            return p;
        };
        QJsonObject sd = o.value("system_dynamics").toObject();
        const SystemDynamicsSpec def;          // defaults mirror TerminalSim
        t.systemDynamics.enabled              = sd.value("enabled").toBool(true);
        t.systemDynamics.criticalUtilization  = sd.value("critical_utilization").toDouble(def.criticalUtilization);
        t.systemDynamics.congestionExponent   = sd.value("congestion_exponent").toDouble(def.congestionExponent);
        t.systemDynamics.congestionSensitivity= sd.value("congestion_sensitivity").toDouble(def.congestionSensitivity);
        t.systemDynamics.delaySensitivity     = sd.value("delay_sensitivity").toDouble(def.delaySensitivity);
        t.systemDynamics.maxServiceRate       = sd.value("max_service_rate").toDouble(def.maxServiceRate);
        t.systemDynamics.shipDelay  = modeDelayFromJson(sd.value("ship_delay").toObject(),  def.shipDelay);
        t.systemDynamics.truckDelay = modeDelayFromJson(sd.value("truck_delay").toObject(), def.truckDelay);
        t.systemDynamics.trainDelay = modeDelayFromJson(sd.value("train_delay").toObject(), def.trainDelay);
        t.systemDynamics.shipArrivalPenalty   = sd.value("ship_arrival_penalty").toDouble(def.shipArrivalPenalty);
        t.systemDynamics.truckArrivalPenalty  = sd.value("truck_arrival_penalty").toDouble(def.truckArrivalPenalty);
        t.systemDynamics.trainArrivalPenalty  = sd.value("train_arrival_penalty").toDouble(def.trainArrivalPenalty);
    }
    return t;
}

QJsonObject nodeLinkageToJson(const NodeLinkage &l)
{
    QJsonObject o;
    o["terminal"] = l.terminalId;
    o["network"]  = l.networkName;
    o["node_id"]  = l.nodeId;
    o["source"]   = (l.source == LinkageSource::Auto) ? "auto" : "manual";
    o["excluded"] = l.excluded;
    return o;
}

NodeLinkage nodeLinkageFromJson(const QJsonObject &o)
{
    NodeLinkage l;
    l.terminalId  = o.value("terminal").toString();
    l.networkName = o.value("network").toString();
    l.nodeId      = o.value("node_id").toInt(-1);
    l.source      = (o.value("source").toString() == "auto")
                        ? LinkageSource::Auto : LinkageSource::Manual;
    l.excluded    = o.value("excluded").toBool(false);
    return l;
}

QJsonObject connectionToJson(const Connection &c)
{
    QJsonObject o;
    o["from"]       = c.fromTerminalId;
    o["to"]         = c.toTerminalId;
    o["mode"]       = transportationModeToString(c.mode);
    o["region"]     = c.region;
    o["properties"] = variantMapToJson(
        RouteMetricUnits::serializedPropertiesFromCanonical(
            c.properties));
    o["source"]     = (c.source == LinkageSource::Auto) ? "auto" : "manual";
    return o;
}

Connection connectionFromJson(const QJsonObject &o)
{
    Connection c;
    c.fromTerminalId = o.value("from").toString();
    c.toTerminalId   = o.value("to").toString();
    c.mode           =
        transportationModeFromString(o.value("mode").toString());
    c.region         = o.value("region").toString();
    c.properties     =
        RouteMetricUnits::canonicalPropertiesFromSerialized(
            jsonToVariantMap(o.value("properties").toObject()),
            ScenarioSerializer::kSchemaVersion);
    c.source         = (o.value("source").toString() == "auto")
                           ? LinkageSource::Auto : LinkageSource::Manual;
    return c;
}

QJsonObject globalLinkToJson(const GlobalLink &g)
{
    QJsonObject o;
    o["from"]       = g.fromTerminalId;
    o["to"]         = g.toTerminalId;
    o["mode"]       = transportationModeToString(g.mode);
    o["properties"] = variantMapToJson(
        RouteMetricUnits::serializedPropertiesFromCanonical(
            g.properties));
    o["source"]     = (g.source == LinkageSource::Auto) ? "auto" : "manual";
    return o;
}

GlobalLink globalLinkFromJson(const QJsonObject &o)
{
    GlobalLink g;
    g.fromTerminalId = o.value("from").toString();
    g.toTerminalId   = o.value("to").toString();
    g.mode           =
        transportationModeFromString(o.value("mode").toString());
    g.properties     =
        RouteMetricUnits::canonicalPropertiesFromSerialized(
            jsonToVariantMap(o.value("properties").toObject()),
            ScenarioSerializer::kSchemaVersion);
    g.source         = (o.value("source").toString() == "auto")
                           ? LinkageSource::Auto : LinkageSource::Manual;
    return g;
}

// The serializer emits linkages / connections / global_links at scenario top
// level, not nested inside each region. This diverges from the spec §7 YAML
// example (which groups them under region for readability) but keeps fromJson
// simpler — no back-references — and matches how ScenarioDocument stores
// them. A future plan may add the per-region YAML layout for human authoring.
// Per-region strategy + auto_rules DO stay embedded in regionSpecToJson below
// because they're region-scoped configuration.
QJsonObject regionSpecToJson(const RegionSpec &r)
{
    QJsonObject o;
    o["name"]            = r.name;
    o["color"]           = r.color;
    o["local_origin"]    = latLonToJson(r.localOrigin);
    o["global_position"] = latLonToJson(r.globalPosition);

    QJsonArray networks;
    for (const NetworkSpec &n : r.networks.values())
        networks.append(networkSpecToJson(n));
    o["networks"] = networks;

    // linkageStrategyToString / linkageStrategyFromString avoid QTest::toString
    // ADL hijacking; see LinkageStrategy.h for the rationale.
    QJsonObject linkagesObj;
    linkagesObj["strategy"]   = linkageStrategyToString(r.linkageStrategy);
    linkagesObj["auto_rules"] = stringListToArray(r.linkageAutoRules);
    linkagesObj["manual"]     = QJsonArray{};   // scenario-top-level instead
    linkagesObj["exclude"]    = QJsonArray{};
    o["linkages"] = linkagesObj;

    QJsonObject connectionsObj;
    connectionsObj["strategy"]   = linkageStrategyToString(r.connectionStrategy);
    connectionsObj["auto_rules"] = stringListToArray(r.connectionAutoRules);
    connectionsObj["manual"]     = QJsonArray{};
    connectionsObj["exclude"]    = QJsonArray{};
    o["connections"] = connectionsObj;

    return o;
}

QJsonObject simulationSettingsToJson(const SimulationSettings &s)
{
    auto modeToJson = [](const SimulationSettings::Mode &m)
    {
        QJsonObject o;
        if (const auto speed = m.speedUnits())
            o["speed"] = speed->value();
        if (m.fuelType.has_value())   o["fuel_type"]   = m.fuelType.value();
        if (m.fuelRate.has_value())   o["fuel_rate"]   = m.fuelRate.value();
        if (m.containers.has_value()) o["containers"]  = m.containers.value();
        if (m.locomotives.has_value()) o["locomotives"] = m.locomotives.value();
        if (const auto risk = m.riskUnits())
            o["risk"] = risk->value();
        if (m.timeValue.has_value())  o["time_value"]  = m.timeValue.value();
        if (m.useNetwork.has_value()) o["use_network"] = m.useNetwork.value();
        return o;
    };

    QJsonObject o;
    if (const auto timeStep = s.timeStepUnits())
        o["time_step"] = static_cast<int>(timeStep->value());
    if (const auto endTime = s.endTimeUnits())
        o["end_time"] = endTime->value();
    if (s.shortestPathsN.has_value())        o["shortest_paths_n"]         = s.shortestPathsN.value();
    if (s.timeValueOfMoney.has_value())      o["time_value_of_money"]      = s.timeValueOfMoney.value();
    if (s.useSpecificTimeValues.has_value()) o["use_specific_time_values"] = s.useSpecificTimeValues.value();
    if (s.carbonRate.has_value())            o["carbon_rate"]              = s.carbonRate.value();

    if (s.shipMultiplier.has_value() || s.railMultiplier.has_value() || s.truckMultiplier.has_value()) {
        QJsonObject mult;
        if (s.shipMultiplier.has_value())  mult["ship"]  = s.shipMultiplier.value();
        if (s.railMultiplier.has_value())  mult["rail"]  = s.railMultiplier.value();
        if (s.truckMultiplier.has_value()) mult["truck"] = s.truckMultiplier.value();
        o["multipliers"] = mult;
    }

    QJsonObject shipObj  = modeToJson(s.ship);
    QJsonObject railObj  = modeToJson(s.rail);
    QJsonObject truckObj = modeToJson(s.truck);
    if (!shipObj.isEmpty())  o["ship"]  = shipObj;
    if (!railObj.isEmpty())  o["rail"]  = railObj;
    if (!truckObj.isEmpty()) o["truck"] = truckObj;

    if (!s.fuelTypes.isEmpty()) {
        QJsonObject fuels;
        for (auto it = s.fuelTypes.constBegin(); it != s.fuelTypes.constEnd(); ++it)
        {
            QJsonObject f;
            if (it.value().energy.has_value()) f["energy"] = it.value().energy.value();
            if (it.value().carbon.has_value()) f["carbon"] = it.value().carbon.value();
            if (it.value().price.has_value())  f["price"]  = it.value().price.value();
            fuels[it.key()] = f;
        }
        o["fuel_types"] = fuels;
    }

    if (s.dwellMethod.has_value() || s.dwellParams.has_value()) {
        QJsonObject dwell;
        if (s.dwellMethod.has_value()) dwell["method"] = s.dwellMethod.value();
        if (s.dwellParams.has_value()) dwell["parameters"] = variantMapToJson(s.dwellParams.value());
        o["dwell_time"] = dwell;
    }

    return o;
}

SimulationSettings simulationSettingsFromJson(const QJsonObject &o)
{
    auto modeFromJson = [](const QJsonObject &o)
    {
        SimulationSettings::Mode m;
        if (o.contains("speed"))
            m.setSpeed(Units::kilometersPerHour(
                o.value("speed").toDouble()));
        if (o.contains("fuel_type"))   m.fuelType   = o.value("fuel_type").toString();
        if (o.contains("fuel_rate"))   m.fuelRate   = o.value("fuel_rate").toDouble();
        if (o.contains("containers"))  m.containers = o.value("containers").toInt();
        if (o.contains("locomotives")) m.locomotives = o.value("locomotives").toInt();
        if (o.contains("risk"))
            m.setRisk(Units::scalar(
                o.value("risk").toDouble()));
        if (o.contains("time_value"))  m.timeValue  = o.value("time_value").toDouble();
        if (o.contains("use_network")) m.useNetwork = o.value("use_network").toBool();
        return m;
    };

    SimulationSettings s;
    if (o.contains("time_step"))
        s.setTimeStep(Units::seconds(
            o.value("time_step").toDouble()));
    if (o.contains("end_time"))
        s.setEndTime(Units::seconds(
            o.value("end_time").toDouble()));
    if (o.contains("shortest_paths_n"))
        s.shortestPathsN = o.value("shortest_paths_n").toInt();
    if (o.contains("time_value_of_money"))
        s.timeValueOfMoney = o.value("time_value_of_money").toDouble();
    if (o.contains("use_specific_time_values"))
        s.useSpecificTimeValues = o.value("use_specific_time_values").toBool();
    if (o.contains("carbon_rate"))
        s.carbonRate = o.value("carbon_rate").toDouble();

    if (o.contains("multipliers")) {
        QJsonObject mult = o.value("multipliers").toObject();
        if (mult.contains("ship"))
            s.shipMultiplier = mult.value("ship").toDouble();
        if (mult.contains("rail") || mult.contains("train")) {
            const QString railKey = mult.contains("rail") ? QStringLiteral("rail") : QStringLiteral("train");
            s.railMultiplier = mult.value(railKey).toDouble();
        }
        if (mult.contains("truck"))
            s.truckMultiplier = mult.value("truck").toDouble();
    }

    if (o.contains("ship"))  s.ship  = modeFromJson(o.value("ship").toObject());
    if (o.contains("rail") || o.contains("train"))
        s.rail = modeFromJson(
            o.contains("rail") ? o.value("rail").toObject()
                               : o.value("train").toObject());
    if (o.contains("truck")) s.truck = modeFromJson(o.value("truck").toObject());

    if (o.contains("fuel_types")) {
        QJsonObject fuels = o.value("fuel_types").toObject();
        for (auto it = fuels.constBegin(); it != fuels.constEnd(); ++it)
        {
            QJsonObject f = it.value().toObject();
            SimulationSettings::Fuel fuel;
            if (f.contains("energy")) fuel.energy = f.value("energy").toDouble();
            if (f.contains("carbon")) fuel.carbon = f.value("carbon").toDouble();
            if (f.contains("price"))  fuel.price  = f.value("price").toDouble();
            s.fuelTypes.insert(it.key(), fuel);
        }
    }

    if (o.contains("dwell_time")) {
        QJsonObject dwell = o.value("dwell_time").toObject();
        if (dwell.contains("method"))
            s.dwellMethod = dwell.value("method").toString();
        if (dwell.contains("parameters"))
            s.dwellParams = jsonToVariantMap(dwell.value("parameters").toObject());
    }

    return s;
}

QJsonObject outputSpecToJson(const OutputSpec &o)
{
    QJsonObject j;
    j["directory"]          = o.directory;
    j["formats"]            = stringListToArray(o.formats);
    j["container_tracking"] = o.containerTracking;
    return j;
}

OutputSpec outputSpecFromJson(const QJsonObject &j)
{
    OutputSpec o;
    o.directory         = j.value("directory").toString("./results");
    o.formats           = arrayToStringList(j.value("formats").toArray());
    if (o.formats.isEmpty()) o.formats = { QStringLiteral("json") };
    o.containerTracking = j.value("container_tracking").toBool(true);
    return o;
}

QJsonArray inlineJsonArray(const QList<QJsonObject> &items)
{
    QJsonArray a;
    for (const QJsonObject &o : items) a.append(o);
    return a;
}

QList<QJsonObject> jsonArrayToInline(const QJsonArray &a)
{
    QList<QJsonObject> out;
    for (const QJsonValue &v : a) out.append(v.toObject());
    return out;
}

QJsonObject fleetSpecToJson(const FleetSpec &f)
{
    auto one = [](const QStringList &files, const QList<QJsonObject> &inl)
    {
        QJsonObject o;
        o["files"]  = stringListToArray(files);
        o["inline"] = inlineJsonArray(inl);
        return o;
    };
    QJsonObject o;
    o["trains"] = one(f.trainsFiles, f.trainsInline);
    o["ships"]  = one(f.shipsFiles,  f.shipsInline);
    o["trucks"] = one(f.trucksFiles, f.trucksInline);
    return o;
}

FleetSpec fleetSpecFromJson(const QJsonObject &j)
{
    FleetSpec f;
    auto one = [&j](const QString &key, QStringList &files, QList<QJsonObject> &inl)
    {
        const QJsonObject o = j.value(key).toObject();
        files = arrayToStringList(o.value("files").toArray());
        inl   = jsonArrayToInline(o.value("inline").toArray());
    };
    one("trains", f.trainsFiles, f.trainsInline);
    one("ships",  f.shipsFiles,  f.shipsInline);
    one("trucks", f.trucksFiles, f.trucksInline);
    return f;
}

} // namespace

QJsonObject ScenarioSerializer::toJson(const ScenarioDocument &doc)
{
    qCDebug(lcScenario) << "ScenarioSerializer::toJson: begin serialization";
    QJsonObject root;
    root["schema_version"] = kSchemaVersion;
    root["simulation"]     = simulationSettingsToJson(doc.simulation);
    root["output"]         = outputSpecToJson(doc.output);
    root["fleet"]          = fleetSpecToJson(doc.fleet);

    QJsonArray regions;
    for (const RegionSpec &r : doc.regions.values())
        regions.append(regionSpecToJson(r));
    root["regions"] = regions;

    // Terminals live at scenario top level (flat layout, keyed by id).
    // Per-region nesting (spec §7 YAML example) is intentionally not used
    // in this plan — see self-review gap note.
    QJsonArray terminals;
    for (const TerminalPlacement &t : doc.terminals.values())
        terminals.append(terminalPlacementToJson(t));
    root["terminals"] = terminals;

    // linkages are stored scenario-wide
    QJsonArray linkages;
    for (const NodeLinkage &l : doc.linkages) linkages.append(nodeLinkageToJson(l));
    root["linkages"] = linkages;

    QJsonArray connections;
    for (const Connection &c : doc.connections) connections.append(connectionToJson(c));
    root["connections"] = connections;

    QJsonArray globalLinks;
    for (const GlobalLink &g : doc.globalLinks) globalLinks.append(globalLinkToJson(g));
    root["global_links"] = globalLinks;

    QJsonArray comparisonSnapshots;
    for (const QJsonObject &snapshot : doc.comparisonSnapshots)
        comparisonSnapshots.append(snapshot);
    root["comparison_snapshots"] = comparisonSnapshots;

    // Scenario-scope global-link metadata is emitted beside `global_links`
    // so the route array remains a pure list of links.
    root["global_link_strategy"]    = linkageStrategyToString(doc.globalLinkStrategy);
    root["global_link_auto_rules"]  = stringListToArray(doc.globalLinkAutoRules);
    root["global_link_auto_rule_params"] = QJsonObject::fromVariantMap(doc.globalLinkAutoRuleParams);

    qCDebug(lcScenario) << "ScenarioSerializer::toJson: complete -"
                        << doc.regions.size() << "regions,"
                        << doc.terminals.size() << "terminals,"
                        << doc.linkages.size() << "linkages,"
                        << doc.connections.size() << "connections,"
                        << doc.globalLinks.size() << "global links";
    return root;
}

std::unique_ptr<ScenarioDocument>
ScenarioSerializer::fromJson(const QJsonObject &j)
{
    qCDebug(lcScenario) << "ScenarioSerializer::fromJson: begin";
    const int version = j.value("schema_version").toInt(1);
    if (version != kSchemaVersion)
    {
        qCWarning(lcScenario) << "ScenarioSerializer::fromJson: schema version mismatch"
                              << version << "!=" << kSchemaVersion;
        return nullptr;
    }

    auto doc = std::make_unique<ScenarioDocument>();

    doc->simulation = simulationSettingsFromJson(j.value("simulation").toObject());
    doc->output     = outputSpecFromJson(j.value("output").toObject());
    doc->fleet      = fleetSpecFromJson(j.value("fleet").toObject());

    // Regions first (networks are nested)
    QJsonArray regions = j.value("regions").toArray();
    for (const QJsonValue &rv : regions)
    {
        QJsonObject ro = rv.toObject();
        RegionSpec r;
        r.name           = ro.value("name").toString();
        r.color          = ro.value("color").toString();
        r.localOrigin    = jsonToLatLon(ro.value("local_origin").toObject());
        r.globalPosition = jsonToLatLon(ro.value("global_position").toObject());

        QJsonObject linkagesObj = ro.value("linkages").toObject();
        bool ok = false;
        r.linkageStrategy = linkageStrategyFromString(
            linkagesObj.value("strategy").toString("manual"), &ok);
        r.linkageAutoRules = arrayToStringList(
            linkagesObj.value("auto_rules").toArray());

        QJsonObject connectionsObj = ro.value("connections").toObject();
        r.connectionStrategy = linkageStrategyFromString(
            connectionsObj.value("strategy").toString("manual"), &ok);
        r.connectionAutoRules = arrayToStringList(
            connectionsObj.value("auto_rules").toArray());

        if (!doc->addRegion(r)) return nullptr;

        QJsonArray networks = ro.value("networks").toArray();
        for (const QJsonValue &nv : networks)
        {
            NetworkSpec n = networkSpecFromJson(nv.toObject());
            if (!doc->addNetwork(r.name, n)) return nullptr;
        }
    }

    // Terminals (depend on regions existing)
    QJsonArray terminalsArr = j.value("terminals").toArray();
    // Terminals can also be embedded per-region under "terminals" — accept
    // both layouts. Here we support the flat layout; per-region layout is a
    // Task 13 (YAML) concern when yaml-cpp re-assembles them.
    for (const QJsonValue &tv : terminalsArr)
    {
        TerminalPlacement t = terminalPlacementFromJson(tv.toObject());
        if (!doc->addTerminal(t)) return nullptr;
    }

    // Linkages / connections / global_links are authored relationship
    // records. Preserve every parsed record here, even if it violates an
    // authoring invariant, so ScenarioValidator can report the exact invalid
    // row instead of losing user data during deserialization. Interactive
    // editing still goes through ScenarioDocument's strict mutators.
    QJsonArray linkages = j.value("linkages").toArray();
    for (const QJsonValue &lv : linkages)
        doc->linkages.append(nodeLinkageFromJson(lv.toObject()));

    QJsonArray connections = j.value("connections").toArray();
    for (const QJsonValue &cv : connections)
        doc->connections.append(connectionFromJson(cv.toObject()));

    QJsonArray globalLinks = j.value("global_links").toArray();
    for (const QJsonValue &gv : globalLinks)
        doc->globalLinks.append(globalLinkFromJson(gv.toObject()));

    const QJsonArray comparisonSnapshots =
        j.value("comparison_snapshots").toArray();
    for (const QJsonValue &snapshotValue : comparisonSnapshots)
    {
        if (!snapshotValue.isObject())
            continue;
        doc->comparisonSnapshots.append(snapshotValue.toObject());
    }

    // Absent scenario-scope global-link metadata defaults to Manual + empty.
    if (j.contains("global_link_strategy"))
    {
        bool glOk = false;
        doc->globalLinkStrategy = linkageStrategyFromString(
            j.value("global_link_strategy").toString("manual"), &glOk);
    }
    doc->globalLinkAutoRules = arrayToStringList(
        j.value("global_link_auto_rules").toArray());
    doc->globalLinkAutoRuleParams =
        j.value("global_link_auto_rule_params").toObject().toVariantMap();

    qCDebug(lcScenario) << "ScenarioSerializer::fromJson: complete -"
                        << doc->regions.size() << "regions,"
                        << doc->terminals.size() << "terminals,"
                        << doc->linkages.size() << "linkages,"
                        << doc->connections.size() << "connections,"
                        << doc->globalLinks.size() << "global links";
    return doc;
}

bool ScenarioSerializer::toYaml(const ScenarioDocument &doc,
                                const QString &path, QString *error)
{
    qCInfo(lcScenario) << "ScenarioSerializer::toYaml: writing to" << path;
    QJsonObject root = toJson(doc);
    YAML::Node  yaml = jsonValueToYamlNode(QJsonValue(root));

    std::stringstream buf;
    buf << yaml;

    QFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qCWarning(lcScenario) << "ScenarioSerializer::toYaml: failed to open file"
                              << path << "-" << out.errorString();
        if (error) *error = out.errorString();
        return false;
    }
    const std::string text = buf.str();
    if (out.write(text.data(), static_cast<qint64>(text.size())) < 0)
    {
        qCWarning(lcScenario) << "ScenarioSerializer::toYaml: write failed -"
                              << out.errorString();
        if (error) *error = out.errorString();
        return false;
    }
    qCInfo(lcScenario) << "ScenarioSerializer::toYaml: success";
    return true;
}

std::unique_ptr<ScenarioDocument>
ScenarioSerializer::fromYaml(const QString &path, QString *error)
{
    qCInfo(lcScenario) << "ScenarioSerializer::fromYaml: loading" << path;
    QFile in(path);
    if (!in.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qCWarning(lcScenario) << "ScenarioSerializer::fromYaml: cannot open file"
                              << path << "-" << in.errorString();
        if (error) *error = in.errorString();
        return nullptr;
    }
    const QByteArray raw = in.readAll();
    qCDebug(lcScenario) << "ScenarioSerializer::fromYaml: read"
                        << raw.size() << "bytes";

    YAML::Node yaml;
    try
    {
        yaml = YAML::Load(std::string(raw.constData(),
                                      static_cast<size_t>(raw.size())));
    }
    catch (const YAML::Exception &e)
    {
        qCCritical(lcScenario) << "ScenarioSerializer::fromYaml: YAML parse exception -"
                               << e.what();
        if (error) *error = QString::fromStdString(e.what());
        return nullptr;
    }

    qCDebug(lcScenario) << "ScenarioSerializer::fromYaml: YAML parsed, converting to JSON";
    QJsonValue  asValue = yamlNodeToJsonValue(yaml);
    if (!asValue.isObject())
    {
        qCWarning(lcScenario) << "ScenarioSerializer::fromYaml: top-level YAML is not a mapping";
        if (error) *error = QStringLiteral("Top-level YAML must be a mapping.");
        return nullptr;
    }

    std::unique_ptr<ScenarioDocument> doc = fromJson(asValue.toObject());
    if (!doc)
    {
        qCWarning(lcScenario) << "ScenarioSerializer::fromYaml: fromJson returned null";
        if (error) *error = QStringLiteral("Schema validation failed during fromJson.");
        return nullptr;
    }

    const QString yamlDir = QFileInfo(path).absolutePath();
    resolvePathsRelativeTo(*doc, yamlDir);
    qCInfo(lcScenario) << "ScenarioSerializer::fromYaml: complete -"
                       << doc->regions.size() << "regions,"
                       << doc->terminals.size() << "terminals,"
                       << doc->linkages.size() << "linkages,"
                       << doc->connections.size() << "connections,"
                       << doc->globalLinks.size() << "global links";
    return doc;
}

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
