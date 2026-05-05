#include "ScenarioEndpointResolver.h"

#include "ScenarioDocument.h"

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

ResolvedTerminalEndpoint resolveTerminalEndpoint(
    const ScenarioDocument &document,
    const QString          &endpoint)
{
    auto resolveUnqualified =
        [&](const QString &terminalId) -> ResolvedTerminalEndpoint
    {
        const auto it = document.terminals.constFind(terminalId);
        if (it == document.terminals.constEnd())
            return {};

        ResolvedTerminalEndpoint resolved;
        resolved.found = true;
        resolved.terminalId = it.key();
        resolved.region = it->region;
        resolved.terminal = &it.value();
        return resolved;
    };

    if (const auto resolved = resolveUnqualified(endpoint);
        resolved.found)
    {
        return resolved;
    }

    const int slash = endpoint.indexOf(QLatin1Char('/'));
    if (slash < 0)
        return {};

    const QString endpointRegion = endpoint.left(slash);
    const QString terminalId = endpoint.mid(slash + 1);
    auto resolved = resolveUnqualified(terminalId);
    if (!resolved.found || resolved.region != endpointRegion)
        return {};

    resolved.region = endpointRegion;
    return resolved;
}

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
