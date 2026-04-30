#include "TerminalGraphBootstrap.h"

#include "Backend/Clients/TerminalClient/TerminalSimulationClient.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Commons/TransportationMode.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Controllers/ConfigController.h"
#include "Backend/Models/PathSegment.h"
#include "Backend/Models/Terminal.h"
#include "Connection.h"
#include "GlobalLink.h"
#include "RouteMetricUnits.h"
#include "ScenarioDocument.h"
#include "ScenarioEndpointResolver.h"
#include "ScenarioRegistry.h"
#include "SimulatorCommandAvailability.h"

#include <QtAlgorithms>
#include <QStringList>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

namespace
{

using Mode = TransportationTypes::TransportationMode;

QString makeSegmentId(const QString &from, const QString &to,
                      Mode           mode)
{
    return QStringLiteral("%1->%2:%3")
        .arg(from, to, transportationModeToString(mode));
}

QString operationContext(const QString &context)
{
    return context.isEmpty()
        ? QStringLiteral("TerminalGraphBootstrap")
        : context;
}

void setError(QString *error, const QString &message)
{
    if (error)
        *error = message;
}

} // namespace

bool TerminalGraphBootstrap::resetAndLoad(
    const ScenarioDocument &document,
    const ScenarioRegistry &registry,
    CargoNetSim::CargoNetSimController &controller,
    QString                *error,
    const QString          &context)
{
    const QString op = operationContext(context);
    auto *terminalClient = controller.getTerminalClient();
    if (!terminalClient)
    {
        const QString message =
            QStringLiteral("TerminalSim client unavailable");
        qCWarning(lcScenario) << op << message;
        setError(error, message);
        return false;
    }

    if (!isCommandAvailable(terminalClient))
    {
        const QString message =
            QStringLiteral("TerminalSim command queue is unavailable");
        qCWarning(lcScenario) << op << message;
        setError(error, message);
        return false;
    }

    if (!terminalClient->resetServer())
    {
        const QString message =
            QStringLiteral("Failed to reset terminal server");
        qCWarning(lcScenario) << op << message;
        setError(error, message);
        return false;
    }

    auto *config = controller.getConfigController();
    if (!config)
    {
        const QString message =
            QStringLiteral("ConfigController unavailable");
        qCWarning(lcScenario) << op << message;
        setError(error, message);
        return false;
    }

    if (!terminalClient->setCostFunctionParameters(
            config->getCostFunctionWeights()))
    {
        const QString message =
            QStringLiteral("Failed to configure TerminalSim cost weights");
        qCWarning(lcScenario) << op << message;
        setError(error, message);
        return false;
    }

    QList<Terminal *> terminals;
    terminals.reserve(document.terminals.size());
    for (auto it = document.terminals.constBegin();
         it != document.terminals.constEnd(); ++it)
    {
        if (Terminal *terminal = registry.terminal(it.key()))
            terminals.append(terminal);
    }

    if (!terminalClient->addTerminals(terminals))
    {
        const QString message =
            QStringLiteral("Failed to add terminals to TerminalSim");
        qCWarning(lcScenario) << op << message;
        setError(error, message);
        return false;
    }

    QList<PathSegment *> routes;
    routes.reserve(document.connections.size()
                   + document.globalLinks.size());
    for (const auto &connection : document.connections)
    {
        const QStringList missingKeys =
            RouteMetricUnits::missingCanonicalRouteMetricKeys(
                connection.properties);
        if (!missingKeys.isEmpty())
        {
            qCWarning(lcScenario)
                << op
                << "regional connection"
                << connection.fromTerminalId
                << "->" << connection.toTerminalId
                << "is missing canonical route metrics"
                << missingKeys;
        }
        routes.append(new PathSegment(
            makeSegmentId(connection.fromTerminalId,
                          connection.toTerminalId,
                          connection.mode),
            connection.fromTerminalId,
            connection.toTerminalId,
            connection.mode,
            RouteMetricUnits::routeAttributesFromCanonical(
                connection.properties)));
    }
    for (const auto &globalLink : document.globalLinks)
    {
        const auto fromEndpoint =
            resolveTerminalEndpoint(document, globalLink.fromTerminalId);
        const auto toEndpoint =
            resolveTerminalEndpoint(document, globalLink.toTerminalId);
        if (!fromEndpoint.found || !toEndpoint.found)
        {
            const QString message =
                QStringLiteral("Global link endpoint cannot be resolved: %1 -> %2")
                    .arg(globalLink.fromTerminalId, globalLink.toTerminalId);
            qCWarning(lcScenario) << op << message;
            setError(error, message);
            qDeleteAll(routes);
            return false;
        }

        const QString fromTerminalId = fromEndpoint.terminalId;
        const QString toTerminalId = toEndpoint.terminalId;
        const QStringList missingKeys =
            RouteMetricUnits::missingCanonicalRouteMetricKeys(
                globalLink.properties);
        if (!missingKeys.isEmpty())
        {
            qCWarning(lcScenario)
                << op
                << "global link"
                << globalLink.fromTerminalId
                << "->" << globalLink.toTerminalId
                << "canonical=" << fromTerminalId
                << "->" << toTerminalId
                << "is missing canonical route metrics"
                << missingKeys;
        }
        routes.append(new PathSegment(
            makeSegmentId(fromTerminalId,
                          toTerminalId,
                          globalLink.mode),
            fromTerminalId,
            toTerminalId,
            globalLink.mode,
            RouteMetricUnits::routeAttributesFromCanonical(
                globalLink.properties)));
    }

    const bool routesOk = terminalClient->addRoutes(routes);
    qDeleteAll(routes);
    if (!routesOk)
    {
        const QString message =
            QStringLiteral("Failed to add routes to TerminalSim");
        qCWarning(lcScenario) << op << message;
        setError(error, message);
        return false;
    }

    if (error)
        error->clear();
    qCInfo(lcScenario)
        << op
        << "TerminalSim baseline loaded"
        << "terminals=" << terminals.size()
        << "routes="
        << (document.connections.size() + document.globalLinks.size());
    return true;
}

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
