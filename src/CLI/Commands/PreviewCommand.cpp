#include "PreviewCommand.h"

#include <QIODevice>
#include <QJsonDocument>
#include <QString>
#include <QStringList>

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

namespace {

struct PreviewOptions
{
    QString scenarioPath;
    bool    allErrors = false;
};

bool parsePreviewArgs(const QStringList &args,
                      PreviewOptions   *options,
                      QString          *error)
{
    QStringList positional;
    for (const QString &arg : args)
    {
        if (arg == QLatin1String("--all-errors"))
        {
            options->allErrors = true;
            continue;
        }
        if (arg.startsWith(QLatin1Char('-')))
        {
            *error = QStringLiteral(
                "preview: unsupported flag '%1' (supported: --all-errors)\n")
                .arg(arg);
            return false;
        }
        positional.append(arg);
    }

    if (positional.isEmpty())
    {
        *error = QStringLiteral("preview: missing scenario.yml path\n");
        return false;
    }

    if (positional.size() != 1)
    {
        *error = QStringLiteral(
            "preview: expected exactly one scenario.yml path\n");
        return false;
    }

    options->scenarioPath = positional.first();
    return true;
}

} // namespace

PreviewCommand::PreviewCommand(QIODevice *outSink, QIODevice *errSink)
    : m_out(outSink)
    , m_err(errSink)
{
}

int PreviewCommand::execute(const QStringList &args)
{
    qCInfo(lcCli) << "PreviewCommand::execute: entry, args =" << args;

    PreviewOptions options;
    QString parseError;
    if (!parsePreviewArgs(args, &options, &parseError))
    {
        qCWarning(lcCli) << "PreviewCommand::execute: bad arguments —"
                         << parseError.trimmed();
        streamToOr(m_err, stderr, parseError);
        return static_cast<int>(ExitCode::BadArgs);
    }

    const QString path = options.scenarioPath;
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

    // Emit validation diagnostics to stderr so users see why preview failed.
    // Large issue sets are summarized unless the caller requests --all-errors.
    // Warnings are printed but don't change the exit code.
    bool          hasError = false;
    const QString issueBuf =
        formatValidationIssues(
            parseResult.issues, &hasError,
            validationIssueFormatOptions(options.allErrors));
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
        if (previewResult.status
            == Backend::Application::ScenarioPreviewServiceStatus::
                   ValidationFailed)
        {
            const QString issueBuffer =
                formatValidationIssues(
                    previewResult.issues, nullptr,
                    validationIssueFormatOptions(options.allErrors));
            if (!issueBuffer.isEmpty())
                streamToOr(m_err, stderr, issueBuffer);
            if (issueBuffer.isEmpty())
            {
                streamToOr(m_err, stderr,
                           QStringLiteral(
                               "preview: resolved scenario validation failed: %1\n")
                               .arg(previewResult.message));
            }
            return static_cast<int>(ExitCode::ValidationFailed);
        }
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
