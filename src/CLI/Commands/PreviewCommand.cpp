#include "PreviewCommand.h"

#include <QIODevice>
#include <QJsonDocument>
#include <QString>

#include <cstdio>

#include "Backend/Commons/LogCategories.h"
#include "Backend/Scenario/ScenarioLinker.h"
#include "Backend/Scenario/ScenarioRegistry.h"
#include "Backend/Scenario/ScenarioSerializer.h"
#include "Backend/Scenario/ScenarioValidator.h"
#include "CLI/Commands/CommandOutput.h"
#include "CLI/Commands/IssueFormatter.h"
#include "CLI/ExitCodes.h"

namespace CargoNetSim {
namespace Cli {

PreviewCommand::PreviewCommand(QIODevice *outSink, QIODevice *errSink)
    : m_out(outSink)
    , m_err(errSink)
{
}

int PreviewCommand::execute(const QStringList &args)
{
    using namespace Backend::Scenario;

    qCInfo(lcCli) << "PreviewCommand::execute: entry, args =" << args;

    if (args.isEmpty())
    {
        qCWarning(lcCli) << "PreviewCommand::execute: missing scenario path";
        streamToOr(m_err, stderr,
                   QStringLiteral("preview: missing scenario.yml path\n"));
        return static_cast<int>(ExitCode::BadArgs);
    }

    const QString path = args.first();
    qCInfo(lcCli) << "PreviewCommand::execute: scenario path =" << path;

    // ---- Parse -----------------------------------------------------------
    qCDebug(lcCli) << "PreviewCommand::execute: parsing YAML...";
    QString parseErr;
    auto    doc = ScenarioSerializer::fromYaml(path, &parseErr);
    if (!doc)
    {
        const QString suffix = parseErr.isEmpty()
            ? QString()
            : QStringLiteral(": ") + parseErr;
        qCCritical(lcCli) << "PreviewCommand::execute: parse failed —"
                          << path << suffix;
        streamToOr(m_err, stderr,
                   QStringLiteral("preview: failed to parse %1%2\n")
                       .arg(path, suffix));
        return static_cast<int>(ExitCode::ValidationFailed);
    }

    qCDebug(lcCli) << "PreviewCommand::execute: parsed — regions ="
                   << doc->regions.size()
                   << ", terminals =" << doc->terminals.size();

    // ---- Validate --------------------------------------------------------
    // Emit every issue to stderr so users see *why* preview failed,
    // not a silent exit code. Warnings are printed but don't change
    // the exit code (same rule as ValidateCommand / spec §8.3).
    bool          hasError = false;
    const auto    issues   = ScenarioValidator::validate(*doc);
    const QString issueBuf =
        formatValidationIssues(issues, &hasError);
    if (!issueBuf.isEmpty())
        streamToOr(m_err, stderr, issueBuf);
    if (hasError)
    {
        qCCritical(lcCli) << "PreviewCommand::execute: validation failed, issues =" << issues.size();
        return static_cast<int>(ExitCode::ValidationFailed);
    }
    qCDebug(lcCli) << "PreviewCommand::execute: validation passed, issues =" << issues.size();

    // ---- Load networks for preview --------------------------------------
    // Preview-only: the linker writes graph objects into this local
    // registry instead of mutating the live RegionDataController. The
    // registry is destroyed when `execute` returns — nothing leaks
    // into the rest of the process.
    qCDebug(lcCli) << "PreviewCommand::execute: loading networks for preview...";
    ScenarioRegistry registry;
    QString          loadErr;
    if (!ScenarioLinker::loadNetworksForPreview(*doc, registry, &loadErr))
    {
        qCCritical(lcCli) << "PreviewCommand::execute: network load failed —" << loadErr;
        streamToOr(m_err, stderr,
                   QStringLiteral(
                       "preview: failed to load networks: %1\n")
                       .arg(loadErr));
        return static_cast<int>(ExitCode::RunFailed);
    }

    // ---- Resolve auto-rules + fold into the doc -------------------------
    // Mutation is intentional and preview-only: the emitted JSON
    // reflects what `run` would see. Not round-trippable — re-parsing
    // the JSON would double-count any auto-derived entries. The doc
    // is thrown away after we serialise it.
    qCDebug(lcCli) << "PreviewCommand::execute: resolving linkages, connections, global links...";
    doc->linkages    = ScenarioLinker::resolveLinkages   (*doc, registry);
    doc->connections = ScenarioLinker::resolveConnections(*doc, registry);
    doc->globalLinks = ScenarioLinker::resolveGlobalLinks(*doc, registry);

    qCDebug(lcCli) << "PreviewCommand::execute: linked doc — linkages ="
                   << doc->linkages.size()
                   << ", connections =" << doc->connections.size()
                   << ", globalLinks =" << doc->globalLinks.size();

    // ---- Emit JSON to stdout --------------------------------------------
    const auto json = ScenarioSerializer::toJson(*doc);
    streamToOr(m_out, stdout,
               QJsonDocument(json).toJson(QJsonDocument::Indented));
    qCInfo(lcCli) << "PreviewCommand::execute: JSON output written to stdout";
    return static_cast<int>(ExitCode::Success);
}

} // namespace Cli
} // namespace CargoNetSim
