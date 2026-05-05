#pragma once

#include <QList>
#include <QString>
#include <memory>

#include "Backend/Scenario/ValidationIssue.h"

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{
class ScenarioDocument;
class ScenarioRuntime;
}

namespace Application
{

enum class ScenarioLoadServiceStatus
{
    Success,
    InvalidInput,
    ParseFailed,
    ValidationFailed,
    RuntimeLoadFailed
};

struct ScenarioLoadServiceResult
{
    ScenarioLoadServiceStatus                    status =
        ScenarioLoadServiceStatus::RuntimeLoadFailed;
    QString                                      message;
    QList<Scenario::ValidationIssue>             issues;
    std::unique_ptr<Scenario::ScenarioRuntime>   runtime;

    bool succeeded() const
    {
        return status == ScenarioLoadServiceStatus::Success
               && runtime != nullptr;
    }
};

struct ScenarioParseServiceResult
{
    ScenarioLoadServiceStatus                    status =
        ScenarioLoadServiceStatus::ParseFailed;
    QString                                      message;
    QList<Scenario::ValidationIssue>             issues;
    std::unique_ptr<Scenario::ScenarioDocument>  document;

    bool succeeded() const
    {
        return status == ScenarioLoadServiceStatus::Success
               && document != nullptr;
    }
};

class ScenarioLoadService
{
public:
    ScenarioLoadService() = default;

    ScenarioParseServiceResult parseAndValidateYaml(
        const QString &scenarioPath) const;

    ScenarioLoadServiceResult loadFromYaml(
        const QString &scenarioPath) const;

    ScenarioLoadServiceResult loadFromDocument(
        std::unique_ptr<Scenario::ScenarioDocument> document) const;

    ScenarioLoadServiceResult loadValidatedDocument(
        std::unique_ptr<Scenario::ScenarioDocument> document) const;
};

} // namespace Application
} // namespace Backend
} // namespace CargoNetSim
