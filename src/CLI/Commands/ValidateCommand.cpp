#include "ValidateCommand.h"

#include <QIODevice>
#include <QString>

#include <cstdio>

#include "Backend/Application/ScenarioLoadService.h"
#include "Backend/CliApi/ScenarioDocumentApi.h"
#include "Backend/Commons/LogCategories.h"
#include "CLI/Commands/CommandOutput.h"
#include "CLI/Commands/IssueFormatter.h"
#include "CLI/ExitCodes.h"

namespace CargoNetSim {
namespace Cli {

ValidateCommand::ValidateCommand(QIODevice *errSink)
    : m_err(errSink)
{
}

int ValidateCommand::execute(const QStringList &args)
{
    qCInfo(lcCli) << "ValidateCommand::execute: entry, args =" << args;

    if (args.isEmpty())
    {
        qCWarning(lcCli) << "ValidateCommand::execute: missing scenario path";
        streamToOr(m_err, stderr,
                   QStringLiteral("validate: missing scenario.yml path\n"));
        return static_cast<int>(ExitCode::BadArgs);
    }

    const QString path = args.first();
    qCInfo(lcCli) << "ValidateCommand::execute: scenario path =" << path;

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
        qCCritical(lcCli) << "ValidateCommand::execute: parse failed —"
                          << path << suffix;
        streamToOr(m_err, stderr,
                   QStringLiteral("validate: failed to parse %1%2\n")
                       .arg(path, suffix));
        return static_cast<int>(ExitCode::ValidationFailed);
    }

    if (parseResult.document)
    {
        qCDebug(lcCli) << "ValidateCommand::execute: parsed doc — regions ="
                       << parseResult.document->regions.size()
                       << ", terminals =" << parseResult.document->terminals.size()
                       << ", linkages =" << parseResult.document->linkages.size();
    }

    // Format every issue via the shared helper — same line format
    // the `preview` command uses. Warnings are reported but do not
    // change the exit code (per spec §8.3); only Error severity
    // bumps the exit to ValidationFailed.
    bool          hasError = false;
    const QString buffer   =
        formatValidationIssues(parseResult.issues, &hasError);
    if (!buffer.isEmpty())
        streamToOr(m_err, stderr, buffer);

    qCInfo(lcCli) << "ValidateCommand::execute: validation"
                  << (hasError ? "FAILED" : "PASSED")
                  << "— issues =" << parseResult.issues.size();

    return hasError
        ? static_cast<int>(ExitCode::ValidationFailed)
        : static_cast<int>(ExitCode::Success);
}

} // namespace Cli
} // namespace CargoNetSim
