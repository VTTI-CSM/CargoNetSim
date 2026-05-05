#include "ScenarioValidator.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Commons/TransportationMode.h"
#include "PropertyKeys.h"
#include "RouteMetricUnits.h"
#include "ScenarioEndpointResolver.h"
#include "TerminalTypeDefaults.h"

#include <QMap>
#include <QSet>
#include <QVariant>

#include <cmath>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

namespace PK = PropertyKeys;

namespace
{

void err(QList<ValidationIssue> &out, const QString &path, const QString &msg)
{
    ValidationIssue i;
    i.severity = ValidationIssue::Error;
    i.path     = path;
    i.message  = msg;
    out.append(i);
}

void warn(QList<ValidationIssue> &out, const QString &path, const QString &msg)
{
    ValidationIssue i;
    i.severity = ValidationIssue::Warning;
    i.path     = path;
    i.message  = msg;
    out.append(i);
}

QPair<QSet<TransportationTypes::TransportationMode>,
      QSet<TransportationTypes::TransportationMode>>
interfacesForTerminal(const TerminalPlacement &terminal)
{
    if (terminal.interfaces.isSet)
        return {terminal.interfaces.landSide,
                terminal.interfaces.seaSide};
    return TerminalTypeDefaults::interfacesFor(terminal.type);
}

bool terminalSupportsMode(
    const TerminalPlacement &terminal,
    TransportationTypes::TransportationMode mode)
{
    if (mode == TransportationTypes::TransportationMode::Any)
        return false;

    const auto interfaces = interfacesForTerminal(terminal);
    return interfaces.first.contains(mode)
           || interfaces.second.contains(mode);
}

QString modeLabel(TransportationTypes::TransportationMode mode)
{
    const QString label = transportationModeToString(mode);
    return label.isEmpty() ? QStringLiteral("Any") : label;
}

bool isNumericVariant(const QVariant &value)
{
    bool ok = false;
    const double numeric = value.toDouble(&ok);
    return ok && std::isfinite(numeric);
}

void checkCanonicalRouteMetrics(const QVariantMap &properties,
                                const QString     &path,
                                QList<ValidationIssue> &out)
{
    for (const QString &key : RouteMetricUnits::routeMetricKeys())
    {
        const QString metricPath =
            path + QStringLiteral(".properties.") + key;
        if (!properties.contains(key))
        {
            err(out, metricPath,
                QStringLiteral("missing canonical route metric '%1'")
                    .arg(key));
            continue;
        }

        if (!isNumericVariant(properties.value(key)))
        {
            err(out, metricPath,
                QStringLiteral("canonical route metric '%1' must be numeric and finite")
                    .arg(key));
        }
    }
}

} // namespace

QList<ValidationIssue> ScenarioValidator::validate(const ScenarioDocument &doc)
{
    qCInfo(lcScenario) << "ScenarioValidator::validate: starting validation";
    QList<ValidationIssue> out;
    qCDebug(lcScenario) << "ScenarioValidator::validate: checkRegions";
    checkRegions(doc, out);
    qCDebug(lcScenario) << "ScenarioValidator::validate: checkTerminals";
    checkTerminals(doc, out);
    qCDebug(lcScenario) << "ScenarioValidator::validate: checkLinkages";
    checkLinkages(doc, out);
    qCDebug(lcScenario) << "ScenarioValidator::validate: checkConnections";
    checkConnections(doc, out);
    qCDebug(lcScenario) << "ScenarioValidator::validate: checkGlobalLinks";
    checkGlobalLinks(doc, out);
    qCDebug(lcScenario) << "ScenarioValidator::validate: checkSimulation";
    checkSimulation(doc, out);
    qCDebug(lcScenario) << "ScenarioValidator::validate: checkDwellTime";
    checkDwellTime(doc, out);
    qCDebug(lcScenario) << "ScenarioValidator::validate: checkOriginContainers";
    checkOriginContainers(doc, out);
    qCInfo(lcScenario) << "ScenarioValidator::validate: complete,"
                       << out.size() << "issues found";
    return out;
}

void ScenarioValidator::checkRegions(const ScenarioDocument &doc,
                                     QList<ValidationIssue> &out)
{
    qCDebug(lcScenario) << "ScenarioValidator::checkRegions:"
                        << doc.regions.size() << "regions";
    for (auto it = doc.regions.constBegin(); it != doc.regions.constEnd(); ++it)
    {
        if (it.key() != it.value().name)
            err(out, QStringLiteral("regions[%1]").arg(it.key()),
                QStringLiteral("Map key '%1' does not match RegionSpec.name '%2'")
                    .arg(it.key(), it.value().name));

        // Network uniqueness per region is already guaranteed by
        // ScenarioDocument::addNetwork (QMap keyed by name). But an invariant
        // check against a hand-built doc is cheap:
        QSet<QString> seen;
        QSet<QString> railNames;
        QSet<QString> truckNames;
        for (auto nit = it.value().networks.constBegin();
             nit != it.value().networks.constEnd(); ++nit)
        {
            const auto &n = nit.value();
            const QString networkPath =
                QStringLiteral("regions[%1].networks[%2]")
                    .arg(it.key(), nit.key());
            if (nit.key() != n.name)
                err(out, networkPath,
                    QStringLiteral(
                        "Map key '%1' does not match NetworkSpec.name '%2'")
                        .arg(nit.key(), n.name));
            if (n.name.trimmed().isEmpty())
                err(out, networkPath,
                    QStringLiteral("network name is required"));
            if (!std::isfinite(n.referencePoint.x)
                || !std::isfinite(n.referencePoint.y))
            {
                err(out, networkPath + QStringLiteral(".reference_point"),
                    QStringLiteral(
                        "reference_point coordinates must be numeric and finite"));
            }

            auto requireFile = [&](const QString &role) {
                if (n.files.value(role).trimmed().isEmpty())
                {
                    err(out,
                        networkPath
                            + QStringLiteral(".files.%1").arg(role),
                        QStringLiteral(
                            "required network file '%1' is missing")
                            .arg(role));
                }
            };
            if (n.type == NetworkSpec::Type::Rail)
            {
                requireFile(QStringLiteral("nodes"));
                requireFile(QStringLiteral("links"));
            }
            else if (n.type == NetworkSpec::Type::Truck)
            {
                requireFile(QStringLiteral("config"));
            }

            if (seen.contains(n.name))
                err(out, QStringLiteral("regions[%1].networks").arg(it.key()),
                    QStringLiteral("Duplicate network name '%1'").arg(n.name));
            seen.insert(n.name);

            // Track per-kind to detect rail/truck collisions below.
            if (n.type == NetworkSpec::Type::Rail)
                railNames.insert(n.name);
            else if (n.type == NetworkSpec::Type::Truck)
                truckNames.insert(n.name);
        }

        // Rail and truck networks MUST have distinct names within a region.
        // A collision would make `RegionData::findNetworkByName` silently
        // prefer rail, which is a quiet bug. Fail loud at load time.
        for (const QString &clash : railNames)
        {
            if (truckNames.contains(clash))
                err(out,
                    QStringLiteral("regions[%1].networks").arg(it.key()),
                    QStringLiteral(
                        "Network name '%1' is declared as both rail and "
                        "truck in region '%2' — names must be unique "
                        "across kinds")
                        .arg(clash, it.key()));
        }
    }
}

void ScenarioValidator::checkTerminals(const ScenarioDocument &doc,
                                       QList<ValidationIssue> &out)
{
    qCDebug(lcScenario) << "ScenarioValidator::checkTerminals:"
                        << doc.terminals.size() << "terminals";
    for (auto it = doc.terminals.constBegin(); it != doc.terminals.constEnd(); ++it)
    {
        const TerminalPlacement &t = it.value();

        if (it.key() != t.id)
            err(out, QStringLiteral("terminals[%1]").arg(it.key()),
                QStringLiteral("Map key '%1' does not match TerminalPlacement.id '%2'")
                    .arg(it.key(), t.id));

        if (!TerminalTypeDefaults::isValidType(t.type))
            err(out, QStringLiteral("terminals[%1].type").arg(it.key()),
                QStringLiteral("Terminal type '%1' is not valid (allowed: %2)")
                    .arg(t.type,
                         TerminalTypeDefaults::allTypes().join(
                             QStringLiteral(", "))));

        if (!doc.regions.contains(t.region))
            err(out, QStringLiteral("terminals[%1].region").arg(it.key()),
                QStringLiteral("Region '%1' does not exist").arg(t.region));

        if (t.mode == TerminalPlacement::PositionMode::NetworkNode)
        {
            if (t.networkRef.isEmpty() || t.nodeId < 0)
                err(out, QStringLiteral("terminals[%1].position").arg(it.key()),
                    QStringLiteral("mode=network_node requires network and node_id >= 0"));
            else if (doc.regions.contains(t.region) &&
                     !doc.regions[t.region].networks.contains(t.networkRef))
                err(out, QStringLiteral("terminals[%1].position.network").arg(it.key()),
                    QStringLiteral("Network '%1' not found in region '%2'")
                        .arg(t.networkRef, t.region));
        }

        if (t.interfaces.isSet)
        {
            // The enum type already restricts to {Ship, Truck, Train, Any}.
            // Residual invariant: land is truck-or-train, sea is ship-only,
            // and Any is never valid in an interface set.
            using Mode = TransportationTypes::TransportationMode;
            for (auto m : t.interfaces.landSide)
                if (m != Mode::Truck && m != Mode::Train)
                    err(out, QStringLiteral("terminals[%1].interfaces.land_side").arg(it.key()),
                        QStringLiteral("Invalid land_side mode '%1'")
                            .arg(interfaceModeCanonicalString(m)));
            for (auto m : t.interfaces.seaSide)
                if (m != Mode::Ship)
                    err(out, QStringLiteral("terminals[%1].interfaces.sea_side").arg(it.key()),
                        QStringLiteral("Invalid sea_side mode '%1'")
                            .arg(interfaceModeCanonicalString(m)));
        }
    }
}

void ScenarioValidator::checkLinkages(const ScenarioDocument &doc,
                                      QList<ValidationIssue> &out)
{
    qCDebug(lcScenario) << "ScenarioValidator::checkLinkages:"
                        << doc.linkages.size() << "linkages";
    QSet<QString> seen;
    QMap<QString, QString> activeNodeOwners;

    for (int i = 0; i < doc.linkages.size(); ++i)
    {
        const NodeLinkage &l = doc.linkages.at(i);
        const QString path = QStringLiteral("linkages[%1]").arg(i);
        const QString key =
            l.terminalId + QLatin1Char('|')
            + l.networkName + QLatin1Char('|')
            + QString::number(l.nodeId);

        if (seen.contains(key))
            err(out, path,
                QStringLiteral("Duplicate linkage for the same "
                               "(terminal, network, node_id) identity"));
        else
            seen.insert(key);

        if (!doc.terminals.contains(l.terminalId))
        {
            err(out, path + ".terminal",
                QStringLiteral("Terminal '%1' does not exist").arg(l.terminalId));
            continue;
        }
        const QString region = doc.terminals[l.terminalId].region;
        if (!l.excluded)
        {
            const QString nodeKey =
                region + QLatin1Char('|')
                + l.networkName + QLatin1Char('|')
                + QString::number(l.nodeId);
            const auto ownerIt =
                activeNodeOwners.constFind(nodeKey);
            if (ownerIt != activeNodeOwners.constEnd()
                && *ownerIt != l.terminalId)
            {
                err(out, path,
                    QStringLiteral("Network node '%1' node_id %2 in region '%3' is already linked to terminal '%4'")
                        .arg(l.networkName)
                        .arg(l.nodeId)
                        .arg(region, *ownerIt));
            }
            else if (ownerIt == activeNodeOwners.constEnd())
            {
                activeNodeOwners.insert(nodeKey, l.terminalId);
            }
        }
        if (!doc.regions.contains(region)) continue;
        if (!doc.regions[region].networks.contains(l.networkName))
            err(out, path + ".network",
                QStringLiteral("Network '%1' not found in region '%2' (owner of terminal '%3')")
                    .arg(l.networkName, region, l.terminalId));
        if (l.nodeId < 0)
            err(out, path + ".node_id",
                QStringLiteral("node_id must be >= 0"));
    }
}

void ScenarioValidator::checkConnections(const ScenarioDocument &doc,
                                         QList<ValidationIssue> &out)
{
    qCDebug(lcScenario) << "ScenarioValidator::checkConnections:"
                        << doc.connections.size() << "connections";
    QSet<QString> seen;
    for (int i = 0; i < doc.connections.size(); ++i)
    {
        const Connection &c = doc.connections.at(i);
        const QString path = QStringLiteral("connections[%1]").arg(i);
        const QString key =
            c.fromTerminalId + QLatin1Char('|')
            + c.toTerminalId + QLatin1Char('|')
            + QString::number(static_cast<int>(c.mode));

        if (seen.contains(key))
            err(out, path,
                QStringLiteral("Duplicate connection for the same (from, to, mode) route identity"));
        else
            seen.insert(key);

        if (!doc.terminals.contains(c.fromTerminalId))
            err(out, path + ".from",
                QStringLiteral("Terminal '%1' does not exist").arg(c.fromTerminalId));
        if (!doc.terminals.contains(c.toTerminalId))
            err(out, path + ".to",
                QStringLiteral("Terminal '%1' does not exist").arg(c.toTerminalId));

        if (doc.terminals.contains(c.fromTerminalId) &&
            doc.terminals.contains(c.toTerminalId))
        {
            const TerminalPlacement &fromTerminal =
                doc.terminals[c.fromTerminalId];
            const TerminalPlacement &toTerminal =
                doc.terminals[c.toTerminalId];
            const QString rf = fromTerminal.region;
            const QString rt = toTerminal.region;
            if (rf != rt)
                err(out, path,
                    QStringLiteral("Endpoints must be in the same region (%1 vs %2); "
                                   "use global_links for cross-region").arg(rf, rt));
            else if (!c.region.isEmpty() && c.region != rf)
                err(out, path + ".region",
                    QStringLiteral("connection.region '%1' does not match the "
                                   "endpoints' region '%2' (Connection.region is "
                                   "a redundant cross-check field; drop it or keep "
                                   "it consistent)").arg(c.region, rf));
            if (c.mode != TransportationTypes::TransportationMode::Any)
            {
                if (!terminalSupportsMode(fromTerminal, c.mode))
                    err(out, path + ".from",
                        QStringLiteral("Terminal '%1' does not support route mode '%2'")
                            .arg(c.fromTerminalId, modeLabel(c.mode)));
                if (!terminalSupportsMode(toTerminal, c.mode))
                    err(out, path + ".to",
                        QStringLiteral("Terminal '%1' does not support route mode '%2'")
                            .arg(c.toTerminalId, modeLabel(c.mode)));
            }
        }
        // Connection.mode is now a strongly-typed enum
        // (TransportationTypes::TransportationMode); the type system
        // rules out any invalid value at compile time. `Any` is the
        // only enum value that doesn't correspond to a concrete mode;
        // flag it as invalid for Connections (which must pick one).
        if (c.mode == TransportationTypes::TransportationMode::Any)
            err(out, path + ".mode",
                QStringLiteral("mode must be truck, rail, or ship "
                               "(not Any)"));

        checkCanonicalRouteMetrics(c.properties, path, out);
    }
}

void ScenarioValidator::checkGlobalLinks(const ScenarioDocument &doc,
                                         QList<ValidationIssue> &out)
{
    qCDebug(lcScenario) << "ScenarioValidator::checkGlobalLinks:"
                        << doc.globalLinks.size() << "global links";
    QSet<QString> seen;

    for (int i = 0; i < doc.globalLinks.size(); ++i)
    {
        const GlobalLink &g = doc.globalLinks.at(i);
        const QString path = QStringLiteral("global_links[%1]").arg(i);
        const QString key =
            g.fromTerminalId + QLatin1Char('|')
            + g.toTerminalId + QLatin1Char('|')
            + QString::number(static_cast<int>(g.mode));

        if (seen.contains(key))
            err(out, path,
                QStringLiteral("Duplicate global_link for the same (from, to, mode) route identity"));
        else
            seen.insert(key);

        const auto fromEndpoint =
            resolveTerminalEndpoint(doc, g.fromTerminalId);
        const auto toEndpoint =
            resolveTerminalEndpoint(doc, g.toTerminalId);
        const TerminalPlacement *fromTerminal = fromEndpoint.terminal;
        const TerminalPlacement *toTerminal = toEndpoint.terminal;
        const QString rf = fromEndpoint.region;
        const QString rt = toEndpoint.region;

        if (!fromTerminal)
            err(out, path + ".from",
                QStringLiteral("Unknown terminal '%1'").arg(g.fromTerminalId));
        if (!toTerminal)
            err(out, path + ".to",
                QStringLiteral("Unknown terminal '%1'").arg(g.toTerminalId));

        if (!rf.isEmpty() && !rt.isEmpty() && rf == rt)
            err(out, path,
                QStringLiteral("global_link endpoints must live in different regions (both in %1)")
                    .arg(rf));
        if (g.mode != TransportationTypes::TransportationMode::Any)
        {
            if (fromTerminal && !terminalSupportsMode(*fromTerminal, g.mode))
                err(out, path + ".from",
                    QStringLiteral("Terminal '%1' does not support global link mode '%2'")
                        .arg(g.fromTerminalId, modeLabel(g.mode)));
            if (toTerminal && !terminalSupportsMode(*toTerminal, g.mode))
                err(out, path + ".to",
                    QStringLiteral("Terminal '%1' does not support global link mode '%2'")
                        .arg(g.toTerminalId, modeLabel(g.mode)));
        }

        // Same enum invariant as connections — type system handles
        // the rest.
        if (g.mode == TransportationTypes::TransportationMode::Any)
            err(out, path + ".mode",
                QStringLiteral("mode must be truck, rail, or ship "
                               "(not Any)"));

        checkCanonicalRouteMetrics(g.properties, path, out);
    }
}

void ScenarioValidator::checkSimulation(const ScenarioDocument &doc,
                                        QList<ValidationIssue> &out)
{
    const auto endTime = doc.simulation.endTimeUnits();
    const auto timeStep = doc.simulation.timeStepUnits();
    qCDebug(lcScenario) << "ScenarioValidator::checkSimulation:"
                        << "endTime="
                        << (endTime.has_value()
                                ? endTime->value()
                                : -1.0)
                        << "timeStep="
                        << (timeStep.has_value()
                                ? timeStep->value()
                                : -1.0);

    if (doc.simulation.endTime.has_value() && doc.simulation.endTime.value() <= 0.0)
        err(out, QStringLiteral("simulation.end_time"),
            QStringLiteral("end_time must be > 0"));
    if (doc.simulation.timeStep.has_value() && doc.simulation.timeStep.value() <= 0)
        err(out, QStringLiteral("simulation.time_step"),
            QStringLiteral("time_step must be > 0"));

    auto checkFuelRef = [&](const QString &modeName,
                            const std::optional<QString> &fuel)
    {
        if (!fuel.has_value() || fuel.value().isEmpty()) return;
        if (!doc.simulation.fuelTypes.contains(fuel.value()))
            err(out, QStringLiteral("simulation.%1.fuel_type").arg(modeName),
                QStringLiteral("fuel_type '%1' is not declared in simulation.fuel_types")
                    .arg(fuel.value()));
    };
    checkFuelRef("ship",  doc.simulation.ship.fuelType);
    checkFuelRef("rail",  doc.simulation.rail.fuelType);
    checkFuelRef("truck", doc.simulation.truck.fuelType);
}

void ScenarioValidator::checkDwellTime(const ScenarioDocument &doc,
                                       QList<ValidationIssue> &out)
{
    // Nothing to validate when neither method nor params are set in the YAML.
    if (!doc.simulation.dwellMethod.has_value() && !doc.simulation.dwellParams.has_value())
        return;

    qCDebug(lcScenario) << "ScenarioValidator::checkDwellTime:"
                        << "method=" << doc.simulation.dwellMethod.value_or("(unset)");

    if (!doc.simulation.dwellMethod.has_value()) {
        err(out, QStringLiteral("simulation.dwell_time.method"),
            QStringLiteral("dwell_time.parameters is set but dwell_time.method is absent"));
        return;
    }

    static const QStringList kValid = { "gamma", "exponential", "normal", "lognormal" };
    const QString method = doc.simulation.dwellMethod.value_or(QString());

    if (!kValid.contains(method))
    {
        err(out, QStringLiteral("simulation.dwell_time.method"),
            QStringLiteral("method '%1' must be one of: gamma, exponential, normal, lognormal")
                .arg(method));
        return;
    }

    QStringList required;
    if      (method == "gamma")       required = { "shape", "scale" };
    else if (method == "exponential") required = { "scale" };
    else if (method == "normal")      required = { "mean", "std_dev" };
    else if (method == "lognormal")   required = { "mean", "sigma" };

    const QMap<QString, QVariant> p =
        doc.simulation.dwellParams.value_or(QMap<QString, QVariant>{});

    for (const QString &key : required)
    {
        if (!p.contains(key))
        {
            err(out, QStringLiteral("simulation.dwell_time.parameters"),
                QStringLiteral("method '%1' requires parameter '%2'").arg(method, key));
            continue;
        }
        bool ok = false;
        double v = p.value(key).toDouble(&ok);
        if (!ok || v <= 0.0)
            err(out, QStringLiteral("simulation.dwell_time.parameters.%1").arg(key),
                QStringLiteral("parameter '%1' must be a positive double").arg(key));
    }

    // Unknown keys → warning (never silently dropped per spec §7.1).
    for (auto it = p.constBegin(); it != p.constEnd(); ++it)
    {
        if (!required.contains(it.key()))
            warn(out, QStringLiteral("simulation.dwell_time.parameters"),
                 QStringLiteral("unknown parameter '%1' for method '%2' (ignored)")
                     .arg(it.key(), method));
    }
}

void ScenarioValidator::checkOriginContainers(
    const ScenarioDocument &doc, QList<ValidationIssue> &out)
{
    qCDebug(lcScenario) << "ScenarioValidator::checkOriginContainers:"
                        << doc.terminals.size() << "terminals to scan";
    // An origin terminal is defined by `properties.initial_container_count > 0`.
    // Each origin MUST carry exactly one destination form:
    // either scalar `destination_terminal` or list `destinations:[{t,f}]`.
    //
    // Rules enforced here:
    //   1. mutual exclusion: scalar + list on the same origin → error
    //   2. count > 0 but neither form set → error
    //   3. destination terminal id must exist in doc.terminals
    //   4. origin may not self-reference as destination
    //   5. destination ids must be unique within an origin route list
    //   6. (list form only) fractions must be positive and sum to 1.0
    //      within 1e-6
    //
    // Rule evaluation is per-origin so a single malformed terminal does not
    // mask other issues — every offending id produces at least one issue.
    for (auto it = doc.terminals.constBegin();
         it != doc.terminals.constEnd(); ++it)
    {
        const QString                 &id    = it.key();
        const QMap<QString, QVariant> &props = it.value().properties;
        const bool hasInitialCount =
            props.contains(PK::Terminal::InitialContainerCount);
        bool countOk = true;
        const int count =
            props.value(PK::Terminal::InitialContainerCount, 0)
                .toInt(&countOk);
        if (hasInitialCount && (!countOk || count < 0))
        {
            err(out,
                QStringLiteral("terminals[%1].initial_container_count")
                    .arg(id),
                QStringLiteral("initial_container_count must be a non-negative integer"));
            continue;
        }
        if (count <= 0) continue;

        const QString path = QStringLiteral("terminals[%1]").arg(id);

        const bool hasScalar =
            props.contains(PK::Terminal::DestinationTerminal);
        const bool hasList =
            props.contains(PK::Terminal::Destinations);

        if (hasScalar && hasList)
        {
            err(out, path,
                QStringLiteral("destination_terminal and destinations "
                               "are mutually exclusive"));
        }
        if (!hasScalar && !hasList)
        {
            err(out, path,
                QStringLiteral("initial_container_count > 0 requires "
                               "destination_terminal or destinations"));
            continue;  // nothing else to check for this origin
        }

        // Single lookup through destinationsFor() — the Document is the
        // one place that knows how to normalize both scalar and list
        // forms. Re-implementing the parse here would duplicate that
        // logic (violates DRY).
        const auto routes = doc.destinationsFor(id);
        double fractionSum = 0.0;
        QSet<QString> seenDestinations;
        for (const auto &r : routes)
        {
            fractionSum += r.fraction;
            if (r.terminal == id)
                err(out, path,
                    QStringLiteral(
                        "origin '%1' cannot self-reference as destination")
                        .arg(id));
            if (!doc.terminals.contains(r.terminal))
                err(out, path,
                    QStringLiteral("unknown destination terminal '%1'")
                        .arg(r.terminal));
            if (seenDestinations.contains(r.terminal))
                err(out, path,
                    QStringLiteral("duplicate destination terminal '%1'")
                        .arg(r.terminal));
            seenDestinations.insert(r.terminal);
            if (hasList && r.fraction <= 0.0)
                err(out, path,
                    QStringLiteral(
                        "destination '%1' fraction must be positive")
                        .arg(r.terminal));
        }
        if (hasList && qAbs(fractionSum - 1.0) > 1e-6)
            err(out, path,
                QStringLiteral("destinations fractions must sum to 1.0 "
                               "(got %1)")
                    .arg(fractionSum));
    }

    // Role-mismatch warnings: scan all terminals independent of container count.
    for (auto it = doc.terminals.constBegin();
         it != doc.terminals.constEnd(); ++it)
    {
        const QString             &id       = it.key();
        const TerminalPlacement   &terminal = it.value();
        const int count =
            terminal.properties
                .value(PK::Terminal::InitialContainerCount, 0)
                .toInt();

        if (terminal.role == TerminalPlacement::TerminalRole::Origin
            && count == 0)
        {
            warn(out, QStringLiteral("terminals[%1]").arg(id),
                 QStringLiteral("Terminal '%1' has role 'origin'"
                                " but no containers configured")
                     .arg(id));
        }
        if (terminal.role == TerminalPlacement::TerminalRole::Destination
            && !doc.isDestination(id))
        {
            warn(out, QStringLiteral("terminals[%1]").arg(id),
                 QStringLiteral("Terminal '%1' has role"
                                " 'destination' but no origin"
                                " routes containers to it")
                     .arg(id));
        }
    }
}

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
