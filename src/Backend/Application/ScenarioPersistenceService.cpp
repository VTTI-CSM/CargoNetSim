#include "ScenarioPersistenceService.h"

#include "Backend/Scenario/ScenarioSerializer.h"

namespace CargoNetSim
{
namespace Backend
{
namespace Application
{

std::unique_ptr<Scenario::ScenarioDocument>
ScenarioPersistenceService::loadYaml(const QString &path,
                                     QString       *error) const
{
    return Scenario::ScenarioSerializer::fromYaml(path, error);
}

bool ScenarioPersistenceService::saveYaml(
    const Scenario::ScenarioDocument &doc,
    const QString                    &path,
    QString                          *error) const
{
    return Scenario::ScenarioSerializer::toYaml(doc, path,
                                                error);
}

QJsonObject ScenarioPersistenceService::toJson(
    const Scenario::ScenarioDocument &doc) const
{
    return Scenario::ScenarioSerializer::toJson(doc);
}

} // namespace Application
} // namespace Backend
} // namespace CargoNetSim
