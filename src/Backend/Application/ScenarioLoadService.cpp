#include "ScenarioLoadService.h"

#include <QObject>

#include "Backend/Commons/LogCategories.h"
#include "Backend/Scenario/ScenarioDocument.h"
#include "Backend/Scenario/ScenarioRuntime.h"
#include "Backend/Scenario/ScenarioSerializer.h"
#include "Backend/Scenario/ScenarioValidator.h"

namespace CargoNetSim
{
namespace Backend
{
namespace Application
{

namespace
{

bool hasValidationErrors(
    const QList<Scenario::ValidationIssue> &issues)
{
    for (const auto &issue : issues)
    {
        if (issue.severity == Scenario::ValidationIssue::Error)
            return true;
    }
    return false;
}

QString firstValidationErrorMessage(
    const QList<Scenario::ValidationIssue> &issues)
{
    for (const auto &issue : issues)
    {
        if (issue.severity == Scenario::ValidationIssue::Error)
            return issue.message;
    }
    return QString();
}

} // namespace

ScenarioParseServiceResult ScenarioLoadService::parseAndValidateYaml(
    const QString &scenarioPath) const
{
    ScenarioParseServiceResult result;

    if (scenarioPath.trimmed().isEmpty())
    {
        result.status = ScenarioLoadServiceStatus::InvalidInput;
        result.message = QStringLiteral("Scenario path is empty");
        return result;
    }

    QString parseError;
    auto document = Scenario::ScenarioSerializer::fromYaml(
        scenarioPath, &parseError);
    if (!document)
    {
        result.status = ScenarioLoadServiceStatus::ParseFailed;
        result.message = parseError.isEmpty()
            ? QStringLiteral("Failed to parse scenario")
            : parseError;
        return result;
    }

    result.issues = Scenario::ScenarioValidator::validate(*document);
    if (hasValidationErrors(result.issues))
    {
        result.status = ScenarioLoadServiceStatus::ValidationFailed;
        const QString firstError =
            firstValidationErrorMessage(result.issues);
        result.message = firstError.isEmpty()
            ? QStringLiteral("Scenario validation failed")
            : firstError;
        return result;
    }

    result.status = ScenarioLoadServiceStatus::Success;
    result.document = std::move(document);
    return result;
}

ScenarioLoadServiceResult ScenarioLoadService::loadFromYaml(
    const QString &scenarioPath) const
{
    ScenarioLoadServiceResult result;
    auto parseResult = parseAndValidateYaml(scenarioPath);
    result.issues = parseResult.issues;
    result.message = parseResult.message;
    result.status = parseResult.status;

    if (!parseResult.succeeded())
        return result;

    return loadValidatedDocument(std::move(parseResult.document));
}

ScenarioLoadServiceResult ScenarioLoadService::loadFromDocument(
    std::unique_ptr<Scenario::ScenarioDocument> document) const
{
    ScenarioLoadServiceResult result;

    if (!document)
    {
        result.status = ScenarioLoadServiceStatus::InvalidInput;
        result.message = QStringLiteral("Scenario document is null");
        return result;
    }

    result.issues = Scenario::ScenarioValidator::validate(*document);
    if (hasValidationErrors(result.issues))
    {
        result.status = ScenarioLoadServiceStatus::ValidationFailed;
        const QString firstError =
            firstValidationErrorMessage(result.issues);
        result.message = firstError.isEmpty()
            ? QStringLiteral("Scenario validation failed")
            : firstError;
        return result;
    }

    return loadValidatedDocument(std::move(document));
}

ScenarioLoadServiceResult ScenarioLoadService::loadValidatedDocument(
    std::unique_ptr<Scenario::ScenarioDocument> document) const
{
    ScenarioLoadServiceResult result;

    if (!document)
    {
        result.status = ScenarioLoadServiceStatus::InvalidInput;
        result.message = QStringLiteral("Scenario document is null");
        return result;
    }

    auto runtime =
        std::make_unique<Scenario::ScenarioRuntime>(std::move(document));
    QString runtimeLoadError;
    const auto runtimeFailureConnection = QObject::connect(
        runtime.get(), &Scenario::ScenarioRuntime::failed,
        runtime.get(),
        [&runtimeLoadError](const QString &message) {
            runtimeLoadError = message;
        },
        Qt::DirectConnection);

    const bool loaded = runtime->load();
    QObject::disconnect(runtimeFailureConnection);

    if (!loaded)
    {
        result.status = ScenarioLoadServiceStatus::RuntimeLoadFailed;
        result.message = runtimeLoadError.isEmpty()
            ? QStringLiteral("Scenario runtime load failed")
            : runtimeLoadError;
        qCWarning(lcScenario)
            << "ScenarioLoadService::loadFromDocument:"
            << result.message;
        return result;
    }

    result.status = ScenarioLoadServiceStatus::Success;
    result.runtime = std::move(runtime);
    return result;
}

} // namespace Application
} // namespace Backend
} // namespace CargoNetSim
