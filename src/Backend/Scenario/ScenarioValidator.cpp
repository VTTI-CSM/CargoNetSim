#include "ScenarioValidator.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Commons/TransportationMode.h"
#include "PropertyKeys.h"
#include "TerminalTypeDefaults.h"

#include <QSet>

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
    for (int i = 0; i < doc.linkages.size(); ++i)
    {
        const NodeLinkage &l = doc.linkages.at(i);
        const QString path = QStringLiteral("linkages[%1]").arg(i);

        if (!doc.terminals.contains(l.terminalId))
        {
            err(out, path + ".terminal",
                QStringLiteral("Terminal '%1' does not exist").arg(l.terminalId));
            continue;
        }
        const QString region = doc.terminals[l.terminalId].region;
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
    for (int i = 0; i < doc.connections.size(); ++i)
    {
        const Connection &c = doc.connections.at(i);
        const QString path = QStringLiteral("connections[%1]").arg(i);

        if (!doc.terminals.contains(c.fromTerminalId))
            err(out, path + ".from",
                QStringLiteral("Terminal '%1' does not exist").arg(c.fromTerminalId));
        if (!doc.terminals.contains(c.toTerminalId))
            err(out, path + ".to",
                QStringLiteral("Terminal '%1' does not exist").arg(c.toTerminalId));

        if (doc.terminals.contains(c.fromTerminalId) &&
            doc.terminals.contains(c.toTerminalId))
        {
            const QString rf = doc.terminals[c.fromTerminalId].region;
            const QString rt = doc.terminals[c.toTerminalId].region;
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
    }
}

void ScenarioValidator::checkGlobalLinks(const ScenarioDocument &doc,
                                         QList<ValidationIssue> &out)
{
    qCDebug(lcScenario) << "ScenarioValidator::checkGlobalLinks:"
                        << doc.globalLinks.size() << "global links";
    auto regionOf = [&](const QString &qualified, QString *foundRegion) -> bool
    {
        const int slash = qualified.indexOf('/');
        if (slash >= 0)
        {
            *foundRegion = qualified.left(slash);
            return doc.regions.contains(*foundRegion);
        }
        const QString id = qualified;
        if (!doc.terminals.contains(id)) return false;
        *foundRegion = doc.terminals[id].region;
        return true;
    };

    for (int i = 0; i < doc.globalLinks.size(); ++i)
    {
        const GlobalLink &g = doc.globalLinks.at(i);
        const QString path = QStringLiteral("global_links[%1]").arg(i);

        QString rf, rt;
        if (!regionOf(g.fromTerminalId, &rf))
            err(out, path + ".from",
                QStringLiteral("Unknown terminal '%1'").arg(g.fromTerminalId));
        if (!regionOf(g.toTerminalId, &rt))
            err(out, path + ".to",
                QStringLiteral("Unknown terminal '%1'").arg(g.toTerminalId));

        if (!rf.isEmpty() && !rt.isEmpty() && rf == rt)
            err(out, path,
                QStringLiteral("global_link endpoints must live in different regions (both in %1)")
                    .arg(rf));

        // Same enum invariant as connections — type system handles
        // the rest.
        if (g.mode == TransportationTypes::TransportationMode::Any)
            err(out, path + ".mode",
                QStringLiteral("mode must be truck, rail, or ship "
                               "(not Any)"));
    }
}

void ScenarioValidator::checkSimulation(const ScenarioDocument &doc,
                                        QList<ValidationIssue> &out)
{
    qCDebug(lcScenario) << "ScenarioValidator::checkSimulation:"
                        << "endTime=" << doc.simulation.endTime
                        << "timeStep=" << doc.simulation.timeStep;
    if (doc.simulation.endTime <= 0.0)
        err(out, QStringLiteral("simulation.end_time"),
            QStringLiteral("end_time must be > 0"));
    if (doc.simulation.timeStep <= 0)
        err(out, QStringLiteral("simulation.time_step"),
            QStringLiteral("time_step must be > 0"));

    auto checkFuelRef = [&](const QString &modeName, const QString &fuel)
    {
        if (fuel.isEmpty()) return;  // empty allowed for test fixtures
        if (!doc.simulation.fuelTypes.contains(fuel))
            err(out, QStringLiteral("simulation.%1.fuel_type").arg(modeName),
                QStringLiteral("fuel_type '%1' is not declared in simulation.fuel_types").arg(fuel));
    };
    checkFuelRef("ship",  doc.simulation.ship.fuelType);
    checkFuelRef("train", doc.simulation.train.fuelType);
    checkFuelRef("truck", doc.simulation.truck.fuelType);
}

void ScenarioValidator::checkDwellTime(const ScenarioDocument &doc,
                                       QList<ValidationIssue> &out)
{
    qCDebug(lcScenario) << "ScenarioValidator::checkDwellTime:"
                        << "method=" << doc.simulation.dwellMethod;
    static const QStringList kValid = { "gamma", "exponential", "normal", "lognormal" };
    const QString method = doc.simulation.dwellMethod;

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

    const QMap<QString, QVariant> &p = doc.simulation.dwellParams;

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
    // An origin terminal is defined by `properties.initial_container_count > 0`
    // (Task 0 retrofit). Each origin MUST carry exactly one destination form:
    // either scalar `destination_terminal` or list `destinations:[{t,f}]`.
    //
    // Rules enforced here (per Plan 5 Task 0 Step 3):
    //   1. mutual exclusion: scalar + list on the same origin → error
    //   2. count > 0 but neither form set → error
    //   3. destination terminal id must exist in doc.terminals
    //   4. origin may not self-reference as destination
    //   5. (list form only) fractions must sum to 1.0 within 1e-6
    //
    // Rule evaluation is per-origin so a single malformed terminal does not
    // mask other issues — every offending id produces at least one issue.
    for (auto it = doc.terminals.constBegin();
         it != doc.terminals.constEnd(); ++it)
    {
        const QString                 &id    = it.key();
        const QMap<QString, QVariant> &props = it.value().properties;
        const int count =
            props.value(PK::Terminal::InitialContainerCount, 0)
                .toInt();
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
