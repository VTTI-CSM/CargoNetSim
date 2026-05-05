#include "ScenarioPreviewService.h"

#include "Backend/Application/ScenarioPersistenceService.h"
#include "Backend/Scenario/ScenarioDocument.h"
#include "Backend/Scenario/ScenarioLinker.h"
#include "Backend/Scenario/ScenarioRegistry.h"
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
        if (issue.severity
            == Scenario::ValidationIssue::Severity::Error)
        {
            return true;
        }
    }
    return false;
}

QString firstValidationError(
    const QList<Scenario::ValidationIssue> &issues)
{
    for (const auto &issue : issues)
    {
        if (issue.severity
            == Scenario::ValidationIssue::Severity::Error)
        {
            return issue.message;
        }
    }
    return QString();
}

} // namespace

ScenarioPreviewServiceResult
ScenarioPreviewService::buildPreviewJson(
    std::unique_ptr<Scenario::ScenarioDocument> document) const
{
    ScenarioPreviewServiceResult result;

    if (!document)
    {
        result.status = ScenarioPreviewServiceStatus::InvalidInput;
        result.message = QStringLiteral("Scenario document is null");
        return result;
    }

    Scenario::ScenarioRegistry registry;
    QString                    loadError;
    if (!Scenario::ScenarioLinker::loadNetworksForPreview(
            *document, registry, &loadError))
    {
        result.status = ScenarioPreviewServiceStatus::NetworkLoadFailed;
        result.message = loadError.isEmpty()
            ? QStringLiteral("Failed to load preview networks")
            : loadError;
        return result;
    }

    document->linkages =
        Scenario::ScenarioLinker::resolveLinkages(*document,
                                                  registry);
    document->connections =
        Scenario::ScenarioLinker::resolveConnections(*document,
                                                     registry);
    document->globalLinks =
        Scenario::ScenarioLinker::resolveGlobalLinks(*document,
                                                     registry);

    result.issues = Scenario::ScenarioValidator::validate(*document);
    if (hasValidationErrors(result.issues))
    {
        result.status =
            ScenarioPreviewServiceStatus::ValidationFailed;
        const QString firstError =
            firstValidationError(result.issues);
        result.message = firstError.isEmpty()
            ? QStringLiteral("Resolved preview scenario validation failed")
            : firstError;
        return result;
    }

    ScenarioPersistenceService persistenceService;
    result.status = ScenarioPreviewServiceStatus::Success;
    result.previewJson =
        persistenceService.toJson(*document);
    return result;
}

} // namespace Application
} // namespace Backend
} // namespace CargoNetSim
