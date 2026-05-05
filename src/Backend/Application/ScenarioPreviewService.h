#pragma once

#include "Backend/Scenario/ValidationIssue.h"

#include <QList>
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

enum class ScenarioPreviewServiceStatus
{
    Success,
    InvalidInput,
    NetworkLoadFailed,
    ValidationFailed
};

struct ScenarioPreviewServiceResult
{
    ScenarioPreviewServiceStatus status =
        ScenarioPreviewServiceStatus::InvalidInput;
    QString                                  message;
    QList<Scenario::ValidationIssue>        issues;
    QJsonObject                             previewJson;

    bool succeeded() const
    {
        return status == ScenarioPreviewServiceStatus::Success;
    }
};

class ScenarioPreviewService
{
public:
    ScenarioPreviewService() = default;

    ScenarioPreviewServiceResult buildPreviewJson(
        std::unique_ptr<Scenario::ScenarioDocument> document) const;
};

} // namespace Application
} // namespace Backend
} // namespace CargoNetSim
