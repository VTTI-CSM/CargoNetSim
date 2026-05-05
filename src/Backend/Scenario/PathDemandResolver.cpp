#include "PathDemandResolver.h"

#include "Backend/Commons/LogCategories.h"
#include "Backend/Models/Path.h"
#include "ScenarioDocument.h"

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{
namespace PathDemandResolver
{

namespace
{

QString resolvedOriginId(const CargoNetSim::Backend::Path &path)
{
    return !path.getOriginId().isEmpty() ? path.getOriginId()
                                         : path.getStartTerminal();
}

QString resolvedDestinationId(const CargoNetSim::Backend::Path &path)
{
    return !path.getDestinationId().isEmpty() ? path.getDestinationId()
                                              : path.getEndTerminal();
}

} // namespace

int previewContainerCount(const ScenarioDocument &document,
                          const QString          &originId,
                          const QString          &destinationId)
{
    const int total = document.originContainerCount(originId);
    if (total <= 0)
        return 0;

    const auto routes = document.destinationsFor(originId);
    for (const auto &route : routes)
    {
        if (route.terminal == destinationId)
            return qRound(total * route.fraction);
    }

    qCDebug(lcScenario)
        << "PathDemandResolver::previewContainerCount: destination"
        << destinationId << "not in destinations for" << originId
        << "- using full container count" << total;
    return total;
}

int previewContainerCount(const ScenarioDocument           &document,
                          const CargoNetSim::Backend::Path &path)
{
    return previewContainerCount(
        document, resolvedOriginId(path), resolvedDestinationId(path));
}

} // namespace PathDemandResolver
} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
