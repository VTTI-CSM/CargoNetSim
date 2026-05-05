#include "UtilityFunctions.h"
#include "../MainWindow.h"
#include "GUI/Controllers/NetworkController.h"
#include "GUI/Controllers/SceneVisibilityController.h"
#include "GUI/Controllers/TerminalController.h"
#include "GUI/Controllers/ConnectionController.h"
#include "GUI/Items/ConnectionLine.h"
#include "GUI/Items/MapPoint.h"

#include "GUI/Widgets/GraphicsView.h"
#include "GUI/Widgets/NetworkMoveDialog.h"
#include "GUI/Widgets/NetworkSelectionDialog.h"
#include "GUI/Widgets/PropertiesPanel.h"
#include "GUI/Widgets/ShortestPathTable.h"

#include "GUI/Utils/PathFindingWorker.h"
#include "GUI/Widgets/TerminalSelectionDialog.h"

#include "Backend/Application/NetworkViewService.h"
#include "Backend/Application/RouteAuthoringService.h"
#include "Backend/Application/SimulationRunService.h"
#include "Backend/GuiApi/ScenarioContractsApi.h"
#include "Backend/GuiApi/ScenarioDocumentApi.h"
#include "Backend/Scenario/ScenarioRuntime.h"

#include "Backend/Commons/GeoDistance.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Commons/TransportationMode.h"
#include "../Commons/TransportationModeConversion.h"

namespace PK = CargoNetSim::Backend::Scenario::PropertyKeys;

QList<CargoNetSim::GUI::TerminalItem *>
CargoNetSim::GUI::UtilitiesFunctions::getTerminalItems(
    GraphicsScene *scene, const QString &region,
    const QString &terminalType,
    ConnectionType connectionType, LinkType linkType)
{
    qCDebug(lcGuiUtil) << "UtilitiesFunctions::getTerminalItems:"
                       << "region=" << region
                       << "type=" << terminalType;
    // Early check
    if (!scene)
    {
        qCWarning(lcGuiUtil) << "UtilitiesFunctions::getTerminalItems:"
                             << "scene is null";
        return QList<CargoNetSim::GUI::TerminalItem *>();
    }

    // Get all terminals upfront
    QList<TerminalItem *> allTerminals =
        scene->getItemsByType<TerminalItem>();

    // Pre-calculate the connection and link sets only if
    // needed
    QSet<TerminalItem *> connectionLineTerminalItems;
    QSet<TerminalItem *> mapPointsLinkedTerminals;

    if (connectionType != ConnectionType::Any)
    {
        QList<ConnectionLine *> connectionLines =
            scene->getItemsByType<ConnectionLine>();
        for (auto connectionLine : connectionLines)
        {
            if (auto startTerminal =
                    dynamic_cast<TerminalItem *>(
                        connectionLine->startItem()))
                connectionLineTerminalItems.insert(
                    startTerminal);
            if (auto endTerminal =
                    dynamic_cast<TerminalItem *>(
                        connectionLine->endItem()))
                connectionLineTerminalItems.insert(
                    endTerminal);
        }
    }

    if (linkType != LinkType::Any)
    {
        QList<MapPoint *> mapPoints =
            scene->getItemsByType<MapPoint>();
        for (auto mapPoint : mapPoints)
        {
            if (auto linkedTerminal =
                    mapPoint->getLinkedTerminal())
                mapPointsLinkedTerminals.insert(
                    linkedTerminal);
        }
    }

    // Create result container with initial capacity to
    // avoid reallocations
    QList<TerminalItem *> result;
    result.reserve(allTerminals.size());

    const bool anyRegion = region == "*";
    const bool anyType   = terminalType == "*";

    // Filter terminals
    for (auto terminal : allTerminals)
    {
        // Region check
        if (!anyRegion && terminal->getRegion() != region)
        {
            continue;
        }

        // Type check
        if (!anyType
            && terminal->getTerminalType() != terminalType)
        {
            continue;
        }

        // Connection check
        if (connectionType == ConnectionType::Connected
            && !connectionLineTerminalItems.contains(
                terminal))
        {
            continue;
        }
        else if (connectionType
                     == ConnectionType::NotConnected
                 && connectionLineTerminalItems.contains(
                     terminal))
        {
            continue;
        }

        // Link check
        if (linkType == LinkType::Linked
            && !mapPointsLinkedTerminals.contains(terminal))
        {
            continue;
        }
        else if (linkType == LinkType::NotLinked
                 && mapPointsLinkedTerminals.contains(
                     terminal))
        {
            continue;
        }

        result.append(terminal);
    }

    qCDebug(lcGuiUtil) << "UtilitiesFunctions::getTerminalItems:"
                       << "resultCount=" << result.size();
    return result;
}

QList<CargoNetSim::GUI::GlobalTerminalItem *> CargoNetSim::
    GUI::UtilitiesFunctions::getGlobalTerminalItems(
        GraphicsScene *scene, const QString &region,
        const QString &terminalType,
        ConnectionType connectionType, LinkType linkType)
{
    // Early check
    if (!scene)
    {
        return QList<
            CargoNetSim::GUI::GlobalTerminalItem *>();
    }

    // Get all terminals upfront
    QList<GlobalTerminalItem *> allTerminals =
        scene->getItemsByType<GlobalTerminalItem>();

    // Pre-calculate the connection and link sets only if
    // needed
    QSet<GlobalTerminalItem *> connectionLineTerminalItems;
    QSet<TerminalItem *>       mapPointsLinkedTerminals;

    if (connectionType != ConnectionType::Any)
    {
        QList<ConnectionLine *> connectionLines =
            scene->getItemsByType<ConnectionLine>();
        for (auto connectionLine : connectionLines)
        {
            if (auto startTerminal =
                    dynamic_cast<GlobalTerminalItem *>(
                        connectionLine->startItem()))
                connectionLineTerminalItems.insert(
                    startTerminal);
            if (auto endTerminal =
                    dynamic_cast<GlobalTerminalItem *>(
                        connectionLine->endItem()))
                connectionLineTerminalItems.insert(
                    endTerminal);
        }
    }

    if (linkType != LinkType::Any)
    {
        QList<MapPoint *> mapPoints =
            scene->getItemsByType<MapPoint>();
        for (auto mapPoint : mapPoints)
        {
            if (auto linkedTerminal =
                    mapPoint->getLinkedTerminal())
                mapPointsLinkedTerminals.insert(
                    linkedTerminal);
        }
    }

    // Create result container with initial capacity to
    // avoid reallocations
    QList<GlobalTerminalItem *> result;
    result.reserve(allTerminals.size());

    const bool anyRegion = region == "*";
    const bool anyType   = terminalType == "*";

    // Filter terminals
    for (auto terminal : allTerminals)
    {
        // Region check
        if (!anyRegion && terminal->getLinkedTerminalItem()
            && terminal->getLinkedTerminalItem()
                       ->getRegion()
                   != region)
        {
            continue;
        }

        // Type check
        if (!anyType && terminal->getLinkedTerminalItem()
            && terminal->getLinkedTerminalItem()
                       ->getTerminalType()
                   != terminalType)
        {
            continue;
        }

        // Connection check
        if (connectionType == ConnectionType::Connected
            && !connectionLineTerminalItems.contains(
                terminal))
        {
            continue;
        }
        else if (connectionType
                     == ConnectionType::NotConnected
                 && connectionLineTerminalItems.contains(
                     terminal))
        {
            continue;
        }

        // Link check
        if (linkType == LinkType::Linked
            && !mapPointsLinkedTerminals.contains(
                terminal->getLinkedTerminalItem()))
        {
            continue;
        }
        else if (linkType == LinkType::NotLinked
                 && mapPointsLinkedTerminals.contains(
                     terminal->getLinkedTerminalItem()))
        {
            continue;
        }

        result.append(terminal);
    }

    return result;
}

QList<CargoNetSim::GUI::MapPoint *> CargoNetSim::GUI::
    UtilitiesFunctions::getMapPointsOfTerminal(
        GraphicsScene *scene, TerminalItem *terminal,
        const QString &region, const QString &networkName,
        NetworkType networkType)
{
    QList<MapPoint *> result;

    // Early return if terminal is null
    if (!terminal || !scene)
    {
        return result;
    }

    QList<MapPoint *> allMapPoints =
        scene->getItemsByType<MapPoint>();

    const bool anyRegion      = region == "*";
    const bool anyNetworkName = networkName == "*";

    for (const auto &mapPoint : allMapPoints)
    {
        // Skip immediately if not linked to the target
        // terminal
        if (mapPoint->getLinkedTerminal() != terminal)
        {
            continue;
        }

        // Skip if region doesn't match (when not wildcard)
        if (!anyRegion && mapPoint->getRegion() != region)
        {
            continue;
        }

        // Check network type regardless of network name
        if (networkType == NetworkType::Ship)
        {
            qCWarning(lcGuiUtil) << "getMapPointsOfTerminal: Ship network type is not supported";
            throw std::runtime_error(
                "Ship network is not supported yet.");
        }
        Backend::Application::NetworkViewService networkView;
        Backend::NetworkKind kind{};
        const QString        refNetworkName =
            networkView.networkNameOf(
                mapPoint->getReferenceNetwork(), &kind);
        const bool networkTypeMatches =
            !refNetworkName.isEmpty()
            && ((networkType == NetworkType::Train
                 && kind == Backend::NetworkKind::Rail)
                || (networkType == NetworkType::Truck
                    && kind == Backend::NetworkKind::Truck));

        // Skip if network type doesn't match
        if (!networkTypeMatches)
        {
            qCWarning(lcGuiUtil) << "getMapPointsOfTerminal: network type validation failed for mapPoint";
            continue;
        }

        // Check network name only if it's not wildcard
        if (!anyNetworkName
            && refNetworkName != networkName)
        {
            continue;
        }

        // If we got here, all conditions are met
        result.append(mapPoint);
    }

    return result;
}

void CargoNetSim::GUI::UtilitiesFunctions::
    updatePropertiesPanel(MainWindow    *mainWindow,
                          QGraphicsItem *item)
{
    // Early check
    if (!mainWindow)
    {
        return;
    }

    const bool isVisible = (item != nullptr);
    qCDebug(lcGuiUtil) << "updatePropertiesPanel: visible=" << isVisible;

    if (!item)
    {
        // If no item is selected, hide the properties dock
        CargoNetSim::GUI::UtilitiesFunctions::
            hidePropertiesPanel(mainWindow);
    }
    else
    {
        // Show properties dock and update its contents
        mainWindow->propertiesDock_->show();
        mainWindow->propertiesDock_->raise();
        mainWindow->propertiesPanel_->displayProperties(
            item);
    }
}

void CargoNetSim::GUI::UtilitiesFunctions::
    hidePropertiesPanel(MainWindow *mainWindow)
{
    // Early check
    if (!mainWindow)
    {
        return;
    }

    // Show map properties only if on main view or global
    // map tabs
    int currentTab = mainWindow->tabWidget_->currentIndex();
    if (currentTab
        == mainWindow->tabWidget_->indexOf(
            mainWindow->tabWidget_->widget(0)))
    { // Main view tab
        mainWindow->propertiesPanel_->displayProperties(
            nullptr);
        mainWindow->propertiesDock_->show();
        mainWindow->propertiesPanel_
            ->displayMapProperties();
    }
    else
    {
        mainWindow->propertiesDock_->hide();
        mainWindow->propertiesPanel_->displayProperties(
            nullptr);
    }
}

void CargoNetSim::GUI::UtilitiesFunctions::
    updateGlobalMapForRegion(MainWindow    *mainWindow,
                             const QString &regionName)
{
    // Early check
    if (!mainWindow)
    {
        return;
    }

    auto regionTerminals = CargoNetSim::GUI::
        UtilitiesFunctions::getTerminalItems(
            mainWindow->regionScene_, regionName, "*");
    const int terminalCount = regionTerminals.size();
    qCDebug(lcGuiUtil) << "updateGlobalMapForRegion:" << regionName << "terminals:" << terminalCount;
    for (TerminalItem *item : regionTerminals)
    {
        mainWindow->terminalCtrl()->updateGlobalMapItem(item);
    }
}

QSet<CargoNetSim::Backend::TransportationTypes::TransportationMode>
CargoNetSim::GUI::UtilitiesFunctions::getCommonModes(
    QGraphicsItem *sourceItem, QGraphicsItem *targetItem)
{
    using Mode = CargoNetSim::Backend::TransportationTypes::TransportationMode;
    using CargoNetSim::GUI::GlobalTerminalItem;
    using CargoNetSim::GUI::TerminalItem;

    if (!sourceItem || !targetItem)
        return {};

    // Dispatch to the typed accessor regardless of which view class the
    // caller handed us. Plan 7: single extraction point for terminal
    // capabilities.
    auto interfacesOf =
        [](QGraphicsItem *item)
        -> Backend::Scenario::InterfaceConversion::InterfaceMap {
        if (auto *g = dynamic_cast<GlobalTerminalItem *>(item))
            return g->availableInterfaces();
        if (auto *t = dynamic_cast<TerminalItem *>(item))
            return t->availableInterfaces();
        return {};
    };

    // Flatten (LAND_SIDE, SEA_SIDE) → one mode set. The caller only cares
    // "is there a shared transport mode?" — LAND/SEA distinction is
    // irrelevant at this layer.
    auto flatten =
        [](const Backend::Scenario::InterfaceConversion::InterfaceMap &m) {
            QSet<Mode> out;
            for (const auto &set : m) out.unite(set);
            return out;
        };

    const QSet<Mode> sourceModes = flatten(interfacesOf(sourceItem));
    if (sourceModes.isEmpty()) return {};

    const QSet<Mode> targetModes = flatten(interfacesOf(targetItem));
    const int sourceCount = sourceModes.size();
    const int targetCount = targetModes.size();
    qCDebug(lcGuiUtil) << "getCommonModes: source modes:" << sourceCount << "target modes:" << targetCount;
    QSet<Mode> commonModes;
    for (auto m : targetModes)
    {
        if (sourceModes.contains(m)) commonModes.insert(m);
    }

    return commonModes;
}

QList<QPair<CargoNetSim::GUI::MapPoint *,
            CargoNetSim::GUI::MapPoint *>>
CargoNetSim::GUI::UtilitiesFunctions::getCommonNetworks(
    QList<CargoNetSim::GUI::MapPoint *> firstEntries,
    QList<CargoNetSim::GUI::MapPoint *> secondEntries)
{
    QList<QPair<MapPoint *, MapPoint *>> commonNetworkPairs;

    // Early return if either list is empty
    if (firstEntries.isEmpty() || secondEntries.isEmpty())
    {
        return commonNetworkPairs;
    }

    // Group the first list by network reference
    QMap<QObject *, QList<MapPoint *>> networkToPointsMap;
    for (MapPoint *point : firstEntries)
    {
        if (!point)
            continue;

        QObject *network = point->getReferenceNetwork();
        if (network)
        {
            networkToPointsMap[network].append(point);
        }
    }

    const int pairCount = firstEntries.size() * secondEntries.size();
    qCDebug(lcGuiUtil) << "getCommonNetworks: pairs:" << pairCount;

    // Find matches in the second list
    for (MapPoint *secondPoint : secondEntries)
    {
        if (!secondPoint)
            continue;

        QObject *network =
            secondPoint->getReferenceNetwork();
        if (!network
            || !networkToPointsMap.contains(network))
        {
            continue;
        }

        // For each point in first list with matching
        // network, create a pair
        const QList<MapPoint *> &matchingFirstPoints =
            networkToPointsMap[network];
        for (MapPoint *firstPoint : matchingFirstPoints)
        {
            commonNetworkPairs.append(
                QPair<MapPoint *, MapPoint *>(firstPoint,
                                              secondPoint));
        }
    }

    return commonNetworkPairs;
}

QList<QPair<CargoNetSim::GUI::MapPoint *,
            CargoNetSim::GUI::MapPoint *>>
CargoNetSim::GUI::UtilitiesFunctions::
    getCommonNetworksOfNetworkType(
        QList<CargoNetSim::GUI::MapPoint *> firstEntries,
        QList<CargoNetSim::GUI::MapPoint *> secondEntries,
        NetworkType                         networkType)
{
    auto commonNets =
        getCommonNetworks(firstEntries, secondEntries);
    QList<QPair<MapPoint *, MapPoint *>> filteredPairs;

    for (const auto &pair : commonNets)
    {
        MapPoint *firstPoint  = pair.first;
        MapPoint *secondPoint = pair.second;

        if (firstPoint && secondPoint)
        {
            Backend::Application::NetworkViewService
                networkView;
            Backend::NetworkKind kind{};
            const bool recognised =
                !networkView.networkNameOf(
                     firstPoint->getReferenceNetwork(),
                     &kind)
                     .isEmpty();
            const bool matches =
                recognised
                && ((networkType == NetworkType::Train
                     && kind == Backend::NetworkKind::Rail)
                    || (networkType == NetworkType::Truck
                        && kind == Backend::NetworkKind::Truck));
            if (matches)
            {
                filteredPairs.append(pair);
            }
        }
    }

    return filteredPairs;
}

double CargoNetSim::GUI::UtilitiesFunctions::
    getApproximateGeoDistance(const QPointF &point1,
                              const QPointF &point2)
{
    // Delegates to centralized haversine implementation.
    // QPointF convention: x = longitude, y = latitude.
    return CargoNetSim::Backend::Commons::GeoDistance::haversineMeters(
        point1.y(), point1.x(), point2.y(), point2.x());
}

void CargoNetSim::GUI::UtilitiesFunctions::
    getTopShortestPaths(MainWindow *mainWindow,
                        int         PathsCount)
{
    qCDebug(lcGuiUtil) << "UtilitiesFunctions::getTopShortestPaths:"
                       << "count=" << PathsCount;
    if (!mainWindow)
    {
        return;
    }

    if (mainWindow->shortestPathTable_->pathsSize() > 0)
    {
        // Ask the user if they are sure to contunue
        QMessageBox::StandardButton reply =
            QMessageBox::question(
                mainWindow, "Continue?",
                "CargoNetSim has already found shortest "
                "paths between the origin and destination. "
                "Do you want to continue anyway? This will "
                "delete the current results.",
                QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::No)
        {
            return;
        }
    }

    auto *rt = mainWindow->runtime();
    if (!rt)
    {
        mainWindow->showStatusBarError(
            QStringLiteral("No scenario loaded."), 3000);
        return;
    }

    QString applyError;
    if (!rt->ensureApplied(&applyError))
    {
        mainWindow->showStatusBarError(
            applyError.isEmpty()
                ? QStringLiteral(
                      "Scenario could not be applied before path discovery.")
                : applyError,
            5000);
        return;
    }

    // Create a worker and a thread
    QThread           *thread = new QThread();
    PathFindingWorker *worker = new PathFindingWorker();
    auto              &ctl =
        CargoNetSim::CargoNetSimController::getInstance();
    worker->initialize(&rt->document(), &rt->registry(),
                       PathsCount, &ctl);

    // Set up connections
    QObject::connect(thread, &QThread::started, worker,
                     &PathFindingWorker::process);
    QObject::connect(worker, &PathFindingWorker::finished,
                     thread, &QThread::quit);
    QObject::connect(worker, &PathFindingWorker::finished,
                     worker,
                     &PathFindingWorker::deleteLater);
    QObject::connect(thread, &QThread::finished, thread,
                     &QThread::deleteLater);

    // A2 slice 2: discovery + preparation now complete on the worker
    // thread and the runtime becomes the backend-owned source of truth
    // for the prepared path set consumed by the GUI.
    QObject::connect(
        worker, &PathFindingWorker::resultReady, mainWindow,
        [mainWindow](const Backend::Scenario::PreparedPathSet &prepared) {
            auto *runtime = mainWindow->runtime();
            if (!runtime)
            {
                mainWindow->showStatusBarError(
                    "No scenario loaded.", 3000);
                mainWindow->findShortestPathButton_->setEnabled(
                    true);
                mainWindow->stopStatusProgress();
                return;
            }

            runtime->setPreparedPaths(prepared);
            mainWindow->shortestPathTable_->clear();
            mainWindow->shortestPathTable_->setPreparedPaths(
                runtime->preparedPaths(),
                {},
                runtime->preparedPathEligibility());
            mainWindow->shortestPathTableDock_->show();
            mainWindow->findShortestPathButton_->setEnabled(true);
            mainWindow->stopStatusProgress();
        },
        Qt::QueuedConnection);

    // Handle errors
    QObject::connect(
        worker, &PathFindingWorker::error, mainWindow,
        [mainWindow](const QString &message) {
            mainWindow->showStatusBarError(message, 3000);
            mainWindow->findShortestPathButton_->setEnabled(
                true);
            mainWindow->stopStatusProgress();
        },
        Qt::QueuedConnection);

    // Move worker to thread and start
    worker->moveToThread(thread);
    // turn off the find shortest path button
    mainWindow->findShortestPathButton_->setEnabled(false);
    mainWindow->startStatusProgress();

    // Start the thread
    mainWindow->showStatusBarMessage(
        "Finding shortest paths in background...", 3000);
    thread->start();
}

bool CargoNetSim::GUI::UtilitiesFunctions::
    processNetworkModeConnection(
        MainWindow                     *mainWindow,
        CargoNetSim::GUI::TerminalItem *sourceTerminal,
        CargoNetSim::GUI::TerminalItem *targetTerminal,
        CargoNetSim::GUI::NetworkType   networkType)
{

    const auto mode = transportationModeFromNetworkType(networkType);
    if (mode != Backend::TransportationTypes::TransportationMode::Train
        && mode != Backend::TransportationTypes::TransportationMode::Truck)
        return false;  // Ship/Any handled elsewhere (global view)

    if (!mainWindow || !sourceTerminal || !targetTerminal)
        return false;

    if (sourceTerminal == targetTerminal)
        return false;

    if (sourceTerminal->getRegion() != targetTerminal->getRegion())
    {
        mainWindow->showStatusBarError(
            "Terminals are in different regions.",
            3000);
        return false;
    }

    auto *runtime = mainWindow->runtime();
    if (!runtime)
    {
        qCWarning(lcGuiUtil)
            << "processNetworkModeConnection: no ScenarioRuntime";
        return false;
    }

    const QString sourceId = sourceTerminal->getTerminalId();
    const QString targetId = targetTerminal->getTerminalId();
    if (sourceId.isEmpty() || targetId.isEmpty())
    {
        qCWarning(lcGuiUtil)
            << "processNetworkModeConnection: unbound terminal item"
            << "sourceId=" << sourceId << "targetId=" << targetId;
        return false;
    }

    auto &controller =
        CargoNetSim::CargoNetSimController::getInstance();
    Backend::Application::RouteAuthoringService routeAuthoringService(
        &controller);
    const auto routeResult =
        routeAuthoringService.upsertNetworkBackedConnection(
            runtime->document(), sourceId, targetId, mode,
            Backend::Scenario::LinkageSource::Manual);
    if (!routeResult.succeeded())
    {
        qCDebug(lcGuiUtil)
            << "processNetworkModeConnection: backend route authoring failed"
            << sourceId << "->" << targetId
            << "mode=" << static_cast<int>(mode)
            << routeResult.message;
        return false;
    }

    CargoNetSim::GUI::ConnectionLine *connection =
        mainWindow->connectionCtrl()->createConnectionLine(
            sourceTerminal, targetTerminal, mode);
    if (!connection)
    {
        qCWarning(lcGuiUtil)
            << "processNetworkModeConnection: route was authored but no view line was rendered"
            << sourceId << "->" << targetId
            << "mode=" << static_cast<int>(mode);
        return false;
    }

    qCDebug(lcGuiUtil)
        << "processNetworkModeConnection: selected network-backed route"
        << sourceId << "->" << targetId
        << "network=" << routeResult.selectedNetworkName
        << "nodes=" << routeResult.selectedStartNodeId
        << "->" << routeResult.selectedEndNodeId;
    return true;
}

void CargoNetSim::GUI::UtilitiesFunctions::
    linkMapPointToTerminal(MainWindow   *mainWindow,
                           MapPoint     *mapPoint,
                           TerminalItem *terminal)
{
    qCDebug(lcGuiUtil) << "UtilitiesFunctions::linkMapPointToTerminal: begin";
    if (!mainWindow || !mapPoint || !terminal)
    {
        return;
    }

    // Link the terminal to the node
    mapPoint->setLinkedTerminal(terminal);

    // Update the properties panel if this item is currently
    // selected
    if (mainWindow->propertiesPanel_->getCurrentItem()
        == mapPoint)
    {
        mainWindow->propertiesPanel_->displayProperties(
            mapPoint);
    }

    // Force a redraw of the MapPoint to show the terminal
    // icon
    mapPoint->update();

    // Show status message
    mainWindow->showStatusBarMessage(
        "Terminal linked to node successfully", 2000);
}

void CargoNetSim::GUI::UtilitiesFunctions::
    validateSelectedSimulation(MainWindow *mainWindow)
{
    qCDebug(lcGuiUtil) << "UtilitiesFunctions::validateSelectedSimulation: begin";
    if (!mainWindow) return;

    auto *rt = mainWindow->runtime();
    if (!rt)
    {
        mainWindow->showErrorDialog(
            "No scenario loaded. Open a scenario or save the "
            "current work as a scenario first.");
        return;
    }

    const auto selectedPathKeys =
        mainWindow->shortestPathTable_->getCheckedPathKeys();
    if (selectedPathKeys.isEmpty())
    {
        mainWindow->showErrorDialog(
            "No paths selected for validation.");
        return;
    }

    Backend::Application::SimulationRunService runService;
    const auto selectionResult =
        runService.selectAndValidate(
            *rt, selectedPathKeys,
            Backend::Scenario::ExecutionDemandPolicy::
                DuplicateDemandPerSelectedPath);
    mainWindow->shortestPathTable_->setPathEligibility(
        rt->preparedPathEligibility());
    if (!selectionResult.succeeded())
    {
        mainWindow->showErrorDialog(
            selectionResult.message.isEmpty()
                ? QStringLiteral(
                      "Selected paths cannot be simulated with the current backend availability.")
                : selectionResult.message);
        return;
    }

    // The runtime outlives a single Validate click (it lives for the
    // whole loaded scenario), so re-clicking would add another set of
    // lambda connections on top of the previous ones, causing each
    // signal to fire N× after N clicks. Clear any prior runtime →
    // mainWindow connections before re-wiring. Document observers
    // (Task 21) use `doc` as sender, not `rt`, so they are not
    // affected by this disconnect.
    rt->disconnect(mainWindow);
    mainWindow->shortestPathTable_->clearExecutionProgress();

    // Terminal UI state: re-enable the button + stop the progress
    // spinner. Shared by `completed` and `failed` paths.
    auto teardown = [mainWindow] {
        mainWindow->validatePathsButton_->setEnabled(true);
        mainWindow->stopStatusProgress();
    };

    using RT = Backend::Scenario::ScenarioRuntime;
    QObject::connect(rt, &RT::statusMessage, mainWindow,
                     [mainWindow](const QString &msg) {
                         mainWindow->showStatusBarMessage(msg, 3000);
                     },
                     Qt::QueuedConnection);
    QObject::connect(rt, &RT::errorMessage, mainWindow,
                     [mainWindow, teardown](const QString &msg) {
                         mainWindow->showStatusBarError(msg, 3000);
                         teardown();
                     },
                     Qt::QueuedConnection);
    QObject::connect(rt, &RT::failed, mainWindow,
                     [mainWindow, rt, teardown](const QString &msg) {
                         mainWindow->shortestPathTable_
                             ->setExecutionProgress(
                                 rt->progressSnapshot());
                         mainWindow->showStatusBarError(msg, 3000);
                         teardown();
                     },
                     Qt::QueuedConnection);
    QObject::connect(
        rt, &RT::progressSnapshotChanged, mainWindow,
        [mainWindow](
            double,
            const Backend::Scenario::ExecutionProgressSnapshot &snapshot) {
            if (mainWindow->shortestPathTable_)
                mainWindow->shortestPathTable_->setExecutionProgress(
                    snapshot);
        },
        Qt::QueuedConnection);
    QObject::connect(rt, &RT::completed, mainWindow,
                     [mainWindow, rt, teardown] {
                         mainWindow->shortestPathTable_
                             ->setExecutionProgress(
                                 rt->progressSnapshot());

                         // Monetary costs — pre-existing behavior.
                         for (const auto &r : rt->results())
                             mainWindow->shortestPathTable_
                                 ->updateSimulationCosts(
                                     r.canonicalPathKey,
                                     r.totalCost,
                                     r.edgeCosts, r.terminalCosts);

                         mainWindow->shortestPathTable_
                             ->setExecutionResults(
                                 rt->executionResults());
                         mainWindow->shortestPathTable_
                             ->setActualMetrics(
                                 rt->actualPathMetrics());

                         teardown();
                     },
                     Qt::QueuedConnection);

    mainWindow->showStatusBarMessage(
        "Starting simulation validation in background...", 3000);
    mainWindow->startStatusProgress();
    mainWindow->validatePathsButton_->setEnabled(false);
    const auto startResult =
        runService.validateAndStart(*rt);
    if (!startResult.succeeded()
        && startResult.status
               != Backend::Application::SimulationRunServiceStatus::StartFailed)
    {
        teardown();
        mainWindow->showErrorDialog(
            startResult.message.isEmpty()
                ? QStringLiteral(
                      "Selected paths cannot be simulated with the current backend availability.")
                : startResult.message);
    }
}

void CargoNetSim::GUI::UtilitiesFunctions::
    linkSelectedTerminalsToNetwork(
        MainWindow               *mainWindow,
        const QList<NetworkType> &networkTypes)
{
    qCDebug(lcGuiUtil) << "UtilitiesFunctions::linkSelectedTerminalsToNetwork:"
                       << "typeCount=" << networkTypes.size();
    if (!mainWindow || networkTypes.isEmpty())
    {
        return;
    }

    // Get the current scene
    auto scene = mainWindow->regionScene_;
    if (!scene)
    {
        return;
    }

    // Get selected terminal items
    QList<QGraphicsItem *> selectedItems =
        scene->selectedItems();
    QList<TerminalItem *> selectedTerminals;

    for (QGraphicsItem *item : selectedItems)
    {
        if (TerminalItem *terminal =
                dynamic_cast<TerminalItem *>(item))
        {
            selectedTerminals.append(terminal);
        }
    }

    if (selectedTerminals.isEmpty())
    {
        mainWindow->showStatusBarError(
            "No terminals selected.", 3000);
        return;
    }

    // Link each selected terminal to its closest network
    // point
    int successCount = 0;
    for (TerminalItem *terminal : selectedTerminals)
    {
        if (mainWindow->terminalCtrl()
                ->linkTerminalToClosestNetworkPoint(
                    terminal, networkTypes))
        {
            successCount++;
        }
    }

    if (successCount > 0)
    {
        mainWindow->showStatusBarMessage(
            QString("Successfully linked %1 terminal(s) to "
                    "network points")
                .arg(successCount),
            3000);
    }
    else
    {
        mainWindow->showStatusBarError(
            "No terminals could be linked to network "
            "points.",
            3000);
    }
}

void CargoNetSim::GUI::UtilitiesFunctions::
    onLinkTerminalsToNetworkActionTriggered(
        MainWindow *mainWindow)
{
    qCDebug(lcGuiUtil) << "UtilitiesFunctions::onLinkTerminalsToNetworkActionTriggered: begin";
    if (!mainWindow)
    {
        return;
    }
    NetworkSelectionDialog dialog(
        mainWindow, NetworkSelectionDialog::Mode::LinkMode);
    int result = dialog.exec();

    if (result == QDialog::Accepted
        || result == QDialog::Accepted + 1)
    {
        QList<NetworkType> selectedTypes =
            dialog.getSelectedNetworkTypes();

        if (result == QDialog::Accepted)
        {
            // Link selected terminals
            UtilitiesFunctions::
                linkSelectedTerminalsToNetwork(
                    mainWindow, selectedTypes);
        }
        else if (result == QDialog::Accepted + 1)
        {
            // Link all visible terminals
            mainWindow->terminalCtrl()
                ->linkAllVisibleTerminalsToNetwork(
                    selectedTypes);
        }
    }
}

void CargoNetSim::GUI::UtilitiesFunctions::
    unlinkSelectedTerminalsToNetwork(
        MainWindow               *mainWindow,
        const QList<NetworkType> &networkTypes)
{
    qCDebug(lcGuiUtil) << "UtilitiesFunctions::unlinkSelectedTerminalsToNetwork:"
                       << "typeCount=" << networkTypes.size();
    if (!mainWindow || networkTypes.isEmpty())
    {
        return;
    }

    // Get the current scene
    auto scene = mainWindow->regionScene_;
    if (!scene)
    {
        return;
    }

    // Get selected terminal items
    QList<QGraphicsItem *> selectedItems =
        scene->selectedItems();
    QList<TerminalItem *> selectedTerminals;

    for (QGraphicsItem *item : selectedItems)
    {
        if (TerminalItem *terminal =
                dynamic_cast<TerminalItem *>(item))
        {
            selectedTerminals.append(terminal);
        }
    }

    if (selectedTerminals.isEmpty())
    {
        mainWindow->showStatusBarError(
            "No terminals selected.", 3000);
        return;
    }

    // Unlink each selected terminal from network points
    int successCount = 0;
    for (TerminalItem *terminal : selectedTerminals)
    {
        if (mainWindow->terminalCtrl()
                ->unlinkTerminalFromNetworkPoints(
                    terminal, networkTypes))
        {
            successCount++;
        }
    }

    if (successCount > 0)
    {
        mainWindow->showStatusBarMessage(
            QString("Successfully unlinked %1 terminal(s) "
                    "from network points")
                .arg(successCount),
            3000);
    }
    else
    {
        mainWindow->showStatusBarError(
            "No terminals could be unlinked from network "
            "points.",
            3000);
    }
}

void CargoNetSim::GUI::UtilitiesFunctions::
    onUnlinkTerminalsToNetworkActionTriggered(
        MainWindow *mainWindow)
{
    qCDebug(lcGuiUtil) << "UtilitiesFunctions::onUnlinkTerminalsToNetworkActionTriggered: begin";
    NetworkSelectionDialog dialog(
        mainWindow,
        NetworkSelectionDialog::Mode::UnlinkMode);
    dialog.setWindowTitle("Select Network Types to Unlink");

    int result = dialog.exec();

    if (result == QDialog::Accepted
        || result == QDialog::Accepted + 1)
    {
        QList<NetworkType> selectedTypes =
            dialog.getSelectedNetworkTypes();

        if (result == QDialog::Accepted)
        {
            // Unlink selected terminals
            UtilitiesFunctions::
                unlinkSelectedTerminalsToNetwork(
                    mainWindow, selectedTypes);
        }
        else if (result == QDialog::Accepted + 1)
        {
            // Unlink all visible terminals
            mainWindow->terminalCtrl()
                ->unlinkAllVisibleTerminalsToNetwork(
                    selectedTypes);
        }
    }
}

void CargoNetSim::GUI::UtilitiesFunctions::
    openTerminalConnectionSelector(MainWindow *mainWindow)
{
    qCDebug(lcGuiUtil) << "UtilitiesFunctions::openTerminalConnectionSelector: begin";
    if (!mainWindow)
    {
        return;
    }

    TerminalSelectionDialog dialog(mainWindow);

    if (dialog.exec() == QDialog::Accepted)
    {
        QStringList selectedTerminals =
            dialog.getSelectedTerminalNames();
        QStringList selectedConnectionTypes =
            dialog.getSelectedConnectionTypes();

        // Show connections based on filter criteria
        mainWindow->sceneVisibility()->showFilteredConnections(
            mainWindow->isGlobalViewActive(),
            selectedTerminals,
            selectedConnectionTypes);
    }
}

void CargoNetSim::GUI::UtilitiesFunctions::
    onShowMoveNetworkDialog(MainWindow *mainWindow)
{
    qCDebug(lcGuiUtil) << "UtilitiesFunctions::onShowMoveNetworkDialog: begin";
    if (!mainWindow)
        return;

    Backend::Application::NetworkViewService networkView;

    // Get the current region
    const QString currentRegion =
        networkView.currentRegionName();

    if (currentRegion.isEmpty())
    {
        mainWindow->showStatusBarError(
            "No region selected.", 3000);
        return;
    }

    // Check if there are any networks in the region
    bool hasNetworks =
        !networkView.trainNetworkNames(currentRegion).isEmpty()
        || !networkView.truckNetworkNames(currentRegion).isEmpty();

    if (!hasNetworks)
    {
        mainWindow->showStatusBarError(
            "No networks available in the current region.",
            3000);
        return;
    }

    // Determine coordinate system
    bool isUsingProjectedCoords =
        mainWindow->regionView_->isUsingProjectedCoords();

    // Create and show dialog
    NetworkMoveDialog dialog(mainWindow, currentRegion,
                             isUsingProjectedCoords,
                             mainWindow);
    if (dialog.exec() == QDialog::Accepted
        && dialog.hasNetworkSelected())
    {
        QPointF     offset = dialog.getOffset();
        NetworkType networkType =
            dialog.getSelectedNetworkType();
        QString networkName =
            dialog.getSelectedNetworkName();

        // Convert offset to scene coordinates if needed
        if (isUsingProjectedCoords)
        {
            // The offset is in meters, but we need to
            // convert it to scene coordinates
            QPointF originScene = QPointF(0, 0);
            QPointF originGeo =
                mainWindow->regionView_->sceneToWGS84(
                    originScene);
            QPointF originProjected =
                mainWindow->regionView_->convertCoordinates(
                    originGeo, "to_projected");

            QPointF offsetProjected =
                QPointF(originProjected.x() + offset.x(),
                        originProjected.y() + offset.y());
            QPointF offsetGeo =
                mainWindow->regionView_->convertCoordinates(
                    offsetProjected, "to_geodetic");
            QPointF offsetScene =
                mainWindow->regionView_->wgs84ToScene(
                    offsetGeo);

            offset = offsetScene - originScene;
        }
        else
        {
            // The offset is in degrees, directly convert to
            // scene coordinates
            QPointF originScene = QPointF(0, 0);
            QPointF originGeo =
                mainWindow->regionView_->sceneToWGS84(
                    originScene);

            QPointF offsetGeo =
                QPointF(originGeo.x() + offset.x(),
                        originGeo.y() + offset.y());
            QPointF offsetScene =
                mainWindow->regionView_->wgs84ToScene(
                    offsetGeo);

            offset = offsetScene - originScene;
        }

        // Move the network
        NetworkController::moveNetwork(
            mainWindow, networkType, networkName, offset,
            currentRegion);
    }
}
