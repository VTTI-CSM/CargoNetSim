#include "ProjectSerializer.h"
#include "Backend/Commons/LogCategories.h"

#include "Backend/Scenario/ScenarioSerializer.h"

namespace CargoNetSim {
namespace GUI {

std::unique_ptr<Backend::Scenario::ScenarioDocument>
ProjectSerializer::loadProject(const QString &path, QString *error)
{
    qCInfo(lcGui) << "ProjectSerializer::loadProject:" << path;
    auto doc = Backend::Scenario::ScenarioSerializer::fromYaml(path, error);
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
    bool ok = Backend::Scenario::ScenarioSerializer::toYaml(doc, path,
                                                            error);
    if (!ok)
        qCWarning(lcGui) << "ProjectSerializer::saveProject: failed -"
                         << (error ? *error : "unknown error");
    return ok;
}

} // namespace GUI
} // namespace CargoNetSim
