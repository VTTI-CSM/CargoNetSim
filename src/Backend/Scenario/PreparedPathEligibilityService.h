#pragma once

#include <QHash>
#include <QVector>

#include "PreparedPathStatus.h"

namespace CargoNetSim
{
namespace Backend
{
class Path;
namespace Scenario
{

struct PreparedPathRecord;
class PreparedPathSet;

class PreparedPathEligibilityService
{
public:
    static SimulatorAvailability currentAvailability();

    static PreparedPathRequirements requirementsFor(
        const CargoNetSim::Backend::Path &path);

    static PreparedPathEligibility evaluate(
        const PreparedPathRequirements &requirements,
        const SimulatorAvailability    &availability);

    static PreparedPathEligibility evaluate(
        const PreparedPathRecord  &record,
        const SimulatorAvailability &availability);

    static QHash<QString, PreparedPathEligibility> evaluateAll(
        const PreparedPathSet     &preparedPaths,
        const SimulatorAvailability &availability);

    static bool validateSelection(
        const PreparedPathSet     &preparedPaths,
        const QVector<QString>    &executionPathKeys,
        const SimulatorAvailability &availability,
        QString                  *err = nullptr);
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
