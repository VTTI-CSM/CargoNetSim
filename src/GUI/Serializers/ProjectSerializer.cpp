#include "ProjectSerializer.h"
#include "Backend/Application/ScenarioPersistenceService.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/GuiApi/ScenarioDocumentApi.h"

namespace CargoNetSim {
namespace GUI {

std::unique_ptr<Backend::Scenario::ScenarioDocument>
ProjectSerializer::loadProject(const QString &path, QString *error)
{
    qCInfo(lcGui) << "ProjectSerializer::loadProject:" << path;
    auto doc = Backend::Application::ScenarioPersistenceService()
                   .loadYaml(path, error);
    if (!doc)
        qCWarning(lcGui) << "ProjectSerializer::loadProject: failed -"
                         << (error ? *error : "unknown error");
    return doc;
}

bool ProjectSerializer::saveProject(
    const Backend::Scenario::ScenarioDocument &doc,
    const QString &path, QString *error)
{
    qCInfo(lcGui) << "ProjectSerializer::saveProject:" << path;
    bool ok = Backend::Application::ScenarioPersistenceService()
                  .saveYaml(doc, path, error);
    if (!ok)
        qCWarning(lcGui) << "ProjectSerializer::saveProject: failed -"
                         << (error ? *error : "unknown error");
    return ok;
}

} // namespace GUI
} // namespace CargoNetSim
