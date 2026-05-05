#include "ValidateCommand.h"

#include <QIODevice>
#include <QString>
#include <QStringList>

#include <cstdio>

#include "Backend/Application/ScenarioLoadService.h"
#include "Backend/CliApi/ScenarioDocumentApi.h"
#include "Backend/Commons/LogCategories.h"
#include "CLI/Commands/CommandOutput.h"
#include "CLI/Commands/IssueFormatter.h"
#include "CLI/ExitCodes.h"

namespace CargoNetSim {
namespace Cli {

namespace {

struct ValidateOptions
{
    QString scenarioPath;
    bool    allErrors = false;
};

bool parseValidateArgs(const QStringList &args,
                       ValidateOptions  *options,
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
                "validate: unsupported flag '%1' (supported: --all-errors)\n")
                .arg(arg);
            return false;
        }
        positional.append(arg);
    }

    if (positional.isEmpty())
    {
        *error = QStringLiteral("validate: missing scenario.yml path\n");
        return false;
    }

    if (positional.size() != 1)
    {
        *error = QStringLiteral(
            "validate: expected exactly one scenario.yml path\n");
        return false;
    }

    options->scenarioPath = positional.first();
    return true;
}

} // namespace

ValidateCommand::ValidateCommand(QIODevice *errSink)
    : m_err(errSink)
{
}

int ValidateCommand::execute(const QStringList &args)
{
    qCInfo(lcCli) << "ValidateCommand::execute: entry, args =" << args;

    ValidateOptions options;
    QString parseError;
    if (!parseValidateArgs(args, &options, &parseError))
    {
        qCWarning(lcCli) << "ValidateCommand::execute: bad arguments —"
                         << parseError.trimmed();
        streamToOr(m_err, stderr, parseError);
        return static_cast<int>(ExitCode::BadArgs);
    }

    const QString path = options.scenarioPath;
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

    // Format issues via the shared helper so validate/preview/run/discover
    // all use the same summary-vs-full-output policy. Warnings are reported
    // but do not change the exit code; only Error severity fails validation.
    bool          hasError = false;
    const QString buffer   =
        formatValidationIssues(
            parseResult.issues, &hasError,
            validationIssueFormatOptions(options.allErrors));
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
