#include "PreparedPathEligibilityService.h"

#include <QStringList>

#include "Backend/Commons/TransportationMode.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Controllers/VehicleController.h"
#include "Backend/Models/Path.h"
#include "Backend/Models/PathSegment.h"
#include "PathPreparationService.h"
#include "SimulatorCommandAvailability.h"

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

namespace
{

QString displayIdentity(const PreparedPathRecord &record)
{
    if (record.path)
        return QStringLiteral("Path %1").arg(record.path->getPathId());
    if (!record.executionPathKey.isEmpty())
        return record.executionPathKey;
    return QStringLiteral("Selected path");
}

} // namespace

SimulatorAvailability
PreparedPathEligibilityService::currentAvailability()
{
    SimulatorAvailability availability;

    auto *controller = CargoNetSim::CargoNetSimController::instance();
    if (!controller)
        return availability;

    availability.terminalAvailable =
        isCommandAvailable(controller->getTerminalClient());
    availability.trainAvailable =
        isCommandAvailable(controller->getTrainClient());
    availability.shipAvailable =
        isCommandAvailable(controller->getShipClient());
    availability.truckAvailable =
        isCommandAvailable(controller->getTruckManager());

    auto *vehicles = controller->getVehicleController();
    availability.trainFleetAvailable =
        vehicles && vehicles->trainCount() > 0;
    availability.shipFleetAvailable =
        vehicles && vehicles->shipCount() > 0;
    availability.truckFleetAvailable =
        availability.truckAvailable;
    return availability;
}

PreparedPathRequirements
PreparedPathEligibilityService::requirementsFor(
    const CargoNetSim::Backend::Path &path)
{
    PreparedPathRequirements requirements;

    for (const auto *segment : path.getSegments())
    {
        if (!segment)
            continue;

        switch (segment->getMode())
        {
        case TransportationTypes::TransportationMode::Train:
            requirements.trainRequired = true;
            break;
        case TransportationTypes::TransportationMode::Ship:
            requirements.shipRequired = true;
            break;
        case TransportationTypes::TransportationMode::Truck:
            requirements.truckRequired = true;
            break;
        case TransportationTypes::TransportationMode::Any:
        default:
            break;
        }
    }

    return requirements;
}

PreparedPathEligibility
PreparedPathEligibilityService::evaluate(
    const PreparedPathRequirements &requirements,
    const SimulatorAvailability    &availability)
{
    PreparedPathEligibility eligibility;
    QStringList             blocking;

    if (requirements.terminalRequired
        && !availability.terminalAvailable)
    {
        blocking.append(
            QStringLiteral("TerminalSim consumer is unavailable"));
    }
    if (requirements.trainRequired && !availability.trainAvailable)
    {
        blocking.append(
            QStringLiteral("NeTrainSim consumer is unavailable"));
    }
    if (requirements.trainRequired
        && !availability.trainFleetAvailable)
    {
        blocking.append(QStringLiteral(
            "train fleet is empty; load at least one train before simulation"));
    }
    if (requirements.shipRequired && !availability.shipAvailable)
    {
        blocking.append(
            QStringLiteral("ShipNetSim consumer is unavailable"));
    }
    if (requirements.shipRequired && !availability.shipFleetAvailable)
    {
        blocking.append(QStringLiteral(
            "ship fleet is empty; load at least one ship before simulation"));
    }
    if (requirements.truckRequired)
    {
        blocking.append(
            QStringLiteral("truck-backed execution is not supported in Track A1"));
    }

    if (!blocking.isEmpty())
    {
        eligibility.selectable = false;
        eligibility.simulatable = false;
        eligibility.blockingReason =
            blocking.join(QStringLiteral("; "));
    }

    return eligibility;
}

PreparedPathEligibility
PreparedPathEligibilityService::evaluate(
    const PreparedPathRecord  &record,
    const SimulatorAvailability &availability)
{
    return evaluate(record.requirements, availability);
}

QHash<QString, PreparedPathEligibility>
PreparedPathEligibilityService::evaluateAll(
    const PreparedPathSet     &preparedPaths,
    const SimulatorAvailability &availability)
{
    QHash<QString, PreparedPathEligibility> eligibilityByIdentity;

    for (const auto &record : preparedPaths.records())
    {
        if (record.executionPathKey.isEmpty())
            continue;
        eligibilityByIdentity.insert(
            record.executionPathKey, evaluate(record, availability));
    }

    return eligibilityByIdentity;
}

bool PreparedPathEligibilityService::validateSelection(
    const PreparedPathSet     &preparedPaths,
    const QVector<QString>    &executionPathKeys,
    const SimulatorAvailability &availability,
    QString                  *err)
{
    if (executionPathKeys.isEmpty())
    {
        if (err)
        {
            *err = QStringLiteral(
                "No paths selected for simulation");
        }
        return false;
    }

    QHash<QString, const PreparedPathRecord *> byIdentity;
    for (const auto &record : preparedPaths.records())
    {
        if (!record.executionPathKey.isEmpty())
            byIdentity.insert(record.executionPathKey, &record);
    }

    for (const auto &executionPathKey : executionPathKeys)
    {
        const auto it = byIdentity.constFind(executionPathKey);
        if (it == byIdentity.constEnd())
        {
            if (err)
            {
                *err = QStringLiteral(
                           "Unknown execution path key: %1")
                           .arg(executionPathKey);
            }
            return false;
        }

        const PreparedPathEligibility eligibility =
            evaluate(*it.value(), availability);
        if (!eligibility.simulatable)
        {
            if (err)
            {
                *err = QStringLiteral("%1 cannot be simulated: %2")
                           .arg(displayIdentity(*it.value()),
                                eligibility.blockingReason);
            }
            return false;
        }
    }

    if (err)
        err->clear();
    return true;
}

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
