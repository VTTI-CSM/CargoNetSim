#pragma once

#include "AutoLinkRule.h"
#include "ScenarioDocument.h"
#include "ScenarioRegistry.h"
#include <QList>
#include <QStringList>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

/// Stateless auto-link resolver. Given a document (which carries each
/// region's strategy + auto_rules + manual + exclude lists) and a registry
/// (which owns in-memory network graphs + terminal objects), produce the
/// effective linkage / connection / global-link sets.
///
/// Strategy semantics (per region, also applied to global_links):
///   Manual : return manual entries verbatim (exclude is ignored).
///   Auto   : return only rule-derived entries (manual ignored).
///   Hybrid : union of manual + rule-derived, minus anything whose
///            (terminalId, network, nodeId) matches an exclude entry.
class ScenarioLinker
{
public:
    // Introspection (used by tests + CLI `preview`).
    static QStringList linkageRuleNames();
    static QStringList connectionRuleNames();
    static QStringList globalRuleNames();

    // Main entry points.
    static QList<NodeLinkage> resolveLinkages   (const ScenarioDocument &doc,
                                                 const ScenarioRegistry &registry);
    static QList<Connection>  resolveConnections(const ScenarioDocument &doc,
                                                 const ScenarioRegistry &registry);
    static QList<GlobalLink>  resolveGlobalLinks(const ScenarioDocument &doc,
                                                 const ScenarioRegistry &registry);

    /// Headless network loader used by the CLI `preview` subcommand: loads
    /// each region's rail + truck networks into stand-alone graph objects
    /// inside the registry (no RegionData mutation). Returns false on any
    /// missing file; writes reason to *error.
    static bool loadNetworksForPreview(const ScenarioDocument &doc,
                                       ScenarioRegistry &registry,
                                       QString *error = nullptr);
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
