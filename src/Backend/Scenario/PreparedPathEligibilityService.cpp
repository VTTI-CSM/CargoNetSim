#include "PreparedPathEligibilityService.h"

#include <QStringList>

#include "Backend/Commons/TransportationMode.h"
#include "Backend/Controllers/CargoNetSimController.h"
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
    if (!record.pathIdentity.isEmpty())
        return record.pathIdentity;
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
    if (requirements.shipRequired && !availability.shipAvailable)
    {
        blocking.append(
            QStringLiteral("ShipNetSim consumer is unavailable"));
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
        if (record.pathIdentity.isEmpty())
            continue;
        eligibilityByIdentity.insert(
            record.pathIdentity, evaluate(record, availability));
    }

    return eligibilityByIdentity;
}

bool PreparedPathEligibilityService::validateSelection(
    const PreparedPathSet     &preparedPaths,
    const QVector<QString>    &pathIdentities,
    const SimulatorAvailability &availability,
    QString                  *err)
{
    if (pathIdentities.isEmpty())
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
        if (!record.pathIdentity.isEmpty())
            byIdentity.insert(record.pathIdentity, &record);
    }

    for (const auto &pathIdentity : pathIdentities)
    {
        const auto it = byIdentity.constFind(pathIdentity);
        if (it == byIdentity.constEnd())
        {
            if (err)
            {
                *err = QStringLiteral(
                           "Unknown prepared path identity: %1")
                           .arg(pathIdentity);
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
