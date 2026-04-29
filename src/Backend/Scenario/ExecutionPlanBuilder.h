#pragma once

#include <QList>
#include <QString>
#include <QVector>

#include "ExecutionPlanTypes.h"
#include "PathAllocation.h"

namespace CargoNetSim
{
namespace Backend
{

class Path;

namespace Scenario
{

class ScenarioDocument;

enum class ExecutionPlanBuildStatus
{
    Success,
    InvalidInput
};

struct ExecutionPlanBuildResult
{
    ExecutionPlanBuildStatus status =
        ExecutionPlanBuildStatus::InvalidInput;
    ScenarioExecutionPlan plan;
    QString               errorMessage;

    bool isSuccess() const
    {
        return status == ExecutionPlanBuildStatus::Success;
    }
};

class ExecutionPlanBuilder
{
public:
    explicit ExecutionPlanBuilder(const ScenarioDocument &document);

    ExecutionPlanBuildResult build(
        const QList<CargoNetSim::Backend::Path *> &paths,
        const QVector<QString>                    &pathIdentities,
        const PathAllocation                      &allocation,
        const QString                             &executionId,
        ExecutionDemandPolicy demandPolicy =
            ExecutionDemandPolicy::AllocatedOnly) const;

private:
    const ScenarioDocument &m_document;
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
