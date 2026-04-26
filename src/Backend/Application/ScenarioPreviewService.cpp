#include "ScenarioPreviewService.h"

#include "Backend/Application/ScenarioPersistenceService.h"
#include "Backend/Scenario/ScenarioDocument.h"
#include "Backend/Scenario/ScenarioLinker.h"
#include "Backend/Scenario/ScenarioRegistry.h"

namespace CargoNetSim
{
namespace Backend
{
namespace Application
{

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

    ScenarioPersistenceService persistenceService;
    result.status = ScenarioPreviewServiceStatus::Success;
    result.previewJson =
        persistenceService.toJson(*document);
    return result;
}

} // namespace Application
} // namespace Backend
} // namespace CargoNetSim
