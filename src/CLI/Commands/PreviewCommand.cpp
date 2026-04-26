#include "PreviewCommand.h"

#include <QIODevice>
#include <QJsonDocument>
#include <QString>

#include <cstdio>

#include "Backend/Application/ScenarioLoadService.h"
#include "Backend/Application/ScenarioPreviewService.h"
#include "Backend/CliApi/ScenarioDocumentApi.h"
#include "Backend/Commons/LogCategories.h"
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

    // ---- Parse + validate ------------------------------------------------
    qCDebug(lcCli) << "PreviewCommand::execute: parsing YAML...";
    Backend::Application::ScenarioLoadService loadService;
    auto parseResult = loadService.parseAndValidateYaml(path);
    if (parseResult.status
        == Backend::Application::ScenarioLoadServiceStatus::ParseFailed
        || parseResult.status
               == Backend::Application::ScenarioLoadServiceStatus::InvalidInput)
    {
        const QString suffix = parseResult.message.isEmpty()
            ? QString()
            : QStringLiteral(": ") + parseResult.message;
        qCCritical(lcCli) << "PreviewCommand::execute: parse failed —"
                          << path << suffix;
        streamToOr(m_err, stderr,
                   QStringLiteral("preview: failed to parse %1%2\n")
                       .arg(path, suffix));
        return static_cast<int>(ExitCode::ValidationFailed);
    }

    // Emit every issue to stderr so users see *why* preview failed,
    // not a silent exit code. Warnings are printed but don't change
    // the exit code (same rule as ValidateCommand / spec §8.3).
    bool          hasError = false;
    const QString issueBuf =
        formatValidationIssues(parseResult.issues, &hasError);
    if (!issueBuf.isEmpty())
        streamToOr(m_err, stderr, issueBuf);
    if (!parseResult.succeeded())
    {
        qCCritical(lcCli) << "PreviewCommand::execute: validation failed, issues ="
                          << parseResult.issues.size();
        return static_cast<int>(ExitCode::ValidationFailed);
    }
    auto doc = std::move(parseResult.document);
    qCDebug(lcCli) << "PreviewCommand::execute: parsed — regions ="
                   << doc->regions.size()
                   << ", terminals =" << doc->terminals.size();
    qCDebug(lcCli) << "PreviewCommand::execute: validation passed, issues ="
                   << parseResult.issues.size();

    // ---- Resolve preview document + emit JSON ---------------------------
    qCDebug(lcCli)
        << "PreviewCommand::execute: building preview JSON...";
    Backend::Application::ScenarioPreviewService previewService;
    const auto previewResult =
        previewService.buildPreviewJson(std::move(doc));
    if (!previewResult.succeeded())
    {
        qCCritical(lcCli)
            << "PreviewCommand::execute: preview build failed —"
            << previewResult.message;
        streamToOr(m_err, stderr,
                   QStringLiteral(
                       "preview: failed to load networks: %1\n")
                       .arg(previewResult.message));
        return static_cast<int>(ExitCode::RunFailed);
    }

    streamToOr(m_out, stdout,
               QJsonDocument(previewResult.previewJson)
                   .toJson(QJsonDocument::Indented));
    qCInfo(lcCli) << "PreviewCommand::execute: JSON output written to stdout";
    return static_cast<int>(ExitCode::Success);
}

} // namespace Cli
} // namespace CargoNetSim
