#pragma once

#include "ScenarioDocument.h"
#include "ValidationIssue.h"
#include <QList>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

/// Stateless validator. Returns a list of issues; callers filter by severity.
class ScenarioValidator
{
public:
    static QList<ValidationIssue> validate(const ScenarioDocument &doc);

private:
    static void checkRegions         (const ScenarioDocument &doc, QList<ValidationIssue> &out);
    static void checkTerminals       (const ScenarioDocument &doc, QList<ValidationIssue> &out);
    static void checkLinkages        (const ScenarioDocument &doc, QList<ValidationIssue> &out);
    static void checkConnections     (const ScenarioDocument &doc, QList<ValidationIssue> &out);
    static void checkGlobalLinks     (const ScenarioDocument &doc, QList<ValidationIssue> &out);
    static void checkSimulation      (const ScenarioDocument &doc, QList<ValidationIssue> &out);
    static void checkDwellTime       (const ScenarioDocument &doc, QList<ValidationIssue> &out);
    static void checkOriginContainers(const ScenarioDocument &doc, QList<ValidationIssue> &out);
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
