#pragma once

#include <QJsonObject>
#include <QString>
#include <memory>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{
class ScenarioDocument;
}

namespace Application
{

class ScenarioPersistenceService
{
public:
    ScenarioPersistenceService() = default;

    std::unique_ptr<Scenario::ScenarioDocument>
    loadYaml(const QString &path, QString *error = nullptr) const;

    bool saveYaml(const Scenario::ScenarioDocument &doc,
                  const QString                    &path,
                  QString                          *error = nullptr) const;

    QJsonObject toJson(const Scenario::ScenarioDocument &doc) const;
};

} // namespace Application
} // namespace Backend
} // namespace CargoNetSim
