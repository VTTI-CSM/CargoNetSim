#include "GUI/Controllers/ViewController.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "GUI/Controllers/NetworkController.h"
#include "GUI/Controllers/ToolbarController.h"
#include "GUI/Items/BackgroundPhotoItem.h"
#include "GUI/Items/ConnectionLine.h"
#include "GUI/Items/MapLine.h"
#include "GUI/Items/MapPoint.h"
#include "GUI/MainWindow.h"
#include "GUI/Utils/ColorUtils.h"
#include "GUI/Utils/IconCreator.h"
#include "GUI/Widgets/GraphicsScene.h"
#include "GUI/Widgets/GraphicsView.h"
#include "GUI/Widgets/InterfaceSelectionDialog.h"
#include "GUI/Widgets/PropertiesPanel.h"
#include "GUI/Widgets/ShortestPathTable.h"
#include "UtilityFunctions.h"
#include <QLoggingCategory>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qfiledialog.h>

namespace
{
Q_LOGGING_CATEGORY(lcRail, "cargonetsim.rail")
}

void CargoNetSim::GUI::ViewController::
    updateSceneVisibility(MainWindow *mainWindow)
{
    QString currentRegion =
        CargoNetSim::CargoNetSimController::getInstance()
            .getRegionDataController()
            ->getCurrentRegion();
    // Update scene visibility based on settings
    GraphicsScene *scene = mainWindow->regionScene_;

    for (auto item : scene->items())
    {
        // Check if item is a terminal
        TerminalItem *terminal =
            dynamic_cast<TerminalItem *>(item);
        if (terminal)
        {
            terminal->setVisible(terminal->getRegion()
                                 == currentRegion);
        }

        // Check if item is a connection line
        ConnectionLine *connectionLine =
            dynamic_cast<ConnectionLine *>(item);
        if (connectionLine)
        {
            connectionLine->setVisible(
                connectionLine->getRegion()
                == currentRegion);
        }

        // Check if item is a region center
        RegionCenterPoint *regionCenter =
            dynamic_cast<RegionCenterPoint *>(item);
        if (regionCenter)
        {
            regionCenter->setVisible(
                regionCenter->getRegion() == currentRegion);
        }

        // Check if item is a map point
        MapPoint *mapPoint = dynamic_cast<MapPoint *>(item);
        if (mapPoint)
        {
            mapPoint->setVisible(mapPoint->getRegion()
                                 == currentRegion);
        }

        // Check if item is a map line
        MapLine *mapLine = dynamic_cast<MapLine *>(item);
        if (mapLine)
        {
            mapLine->setVisible(mapLine->getRegion()
                                == currentRegion);
        }

        BackgroundPhotoItem *backgroundPhoto =
            dynamic_cast<BackgroundPhotoItem *>(item);
        if (backgroundPhoto)
        {
            backgroundPhoto->setVisible(
                backgroundPhoto->getRegion()
                == currentRegion);
        }
    }
}

void CargoNetSim::GUI::ViewController::updateGlobalMapItem(
    MainWindow *main_window, TerminalItem *terminal)
{
    qCDebug(lcRail) << "[RailDraw]       updateGlobalMapItem"
                       " begin";
    if (!terminal)
    {
        return;
    }
    auto props = terminal->getProperties();
    bool show =
        props.contains("Show on Global Map")
            ? props.value("Show on Global Map").toBool()
            : true;
    qCDebug(lcRail)
        << "[RailDraw]       updateGlobalMapItem show=" << show;

    if (show)
    {
        auto regionData =
            CargoNetSim::CargoNetSimController::
                getInstance()
                    .getRegionDataController()
                    ->getRegionData(terminal->getRegion());

        if (!regionData)
        {
            qCDebug(lcRail) << "[RailDraw]       "
                               "regionData null, return";
            return;
        }

        auto regionCenterPoint =
            regionData->getVariableAs<RegionCenterPoint *>(
                "regionCenterPoint", nullptr);

        if (!regionCenterPoint)
        {
            qCDebug(lcRail)
                << "[RailDraw]       regionCenterPoint"
                   " null, return";
            return;
        }

        if (terminal->getGlobalTerminalItem())
        {
            qCDebug(lcRail)
                << "[RailDraw]       existing global item,"
                   " updating position";
            // Update the global terminal item position
            CargoNetSim::GUI::ViewController::
                updateTerminalGlobalPosition(
                    main_window, regionCenterPoint,
                    terminal);
        }
        else
        {
            qCDebug(lcRail)
                << "[RailDraw]       creating"
                   " GlobalTerminalItem";
            // Create the global terminal item
            QPixmap pixmap       = terminal->getPixmap();
            auto global_terminal = new GlobalTerminalItem(
                pixmap, terminal, nullptr);
            qCDebug(lcRail)
                << "[RailDraw]       GlobalTerminalItem"
                   " constructed, addItemWithId begin";

            // Add to the view
            main_window->globalMapView_->getScene()
                ->addItemWithId(global_terminal,
                                global_terminal->getID());
            qCDebug(lcRail)
                << "[RailDraw]       addItemWithId ok";
            terminal->setGlobalTerminalItem(
                global_terminal);

            QObject::connect(
                terminal, &TerminalItem::positionChanged,
                [main_window, regionCenterPoint,
                 terminal]() {
                    if (!regionCenterPoint)
                    {
                        return;
                    }
                    CargoNetSim::GUI::ViewController::
                        updateTerminalGlobalPosition(
                            main_window, regionCenterPoint,
                            terminal);
                });

            qCDebug(lcRail)
                << "[RailDraw]       calling"
                   " updateTerminalGlobalPosition";
            // Explicitly set its position
            updateTerminalGlobalPosition(
                main_window, regionCenterPoint, terminal);
            qCDebug(lcRail)
                << "[RailDraw]       "
                   "updateTerminalGlobalPosition ok";
        }
    }
    else
    {
        qCDebug(lcRail)
            << "[RailDraw]       show=false else-branch,"
               " checking existing global item";
        // Remove the global terminal item
        if (terminal->getGlobalTerminalItem())
        {
            qCDebug(lcRail)
                << "[RailDraw]       existing global"
                   " item present, removing";
            GlobalTerminalItem *item =
                terminal->getGlobalTerminalItem();
            terminal->setGlobalTerminalItem(
                nullptr); // First detach from terminal
            main_window->globalMapView_->getScene()
                ->removeItemWithId<GlobalTerminalItem>(
                    item->getID()); // Then remove from
                                    // scene
            qCDebug(lcRail)
                << "[RailDraw]       removeItemWithId ok";
        }
        else
        {
            qCDebug(lcRail)
                << "[RailDraw]       no existing global"
                   " item, nothing to remove";
        }
    }
    qCDebug(lcRail)
        << "[RailDraw]       updateGlobalMapItem end";
}

void CargoNetSim::GUI::ViewController::
    updateTerminalGlobalPosition(
        MainWindow        *main_window,
        RegionCenterPoint *regionCenterPoint,
        TerminalItem      *terminal)
{
    // Check if the regionCenterPoint is not nullptr
    if (!regionCenterPoint || !terminal || !main_window)
    {
        return;
    }

    auto props = regionCenterPoint->getProperties();
    auto center_shared_lat =
        props.contains("Shared Latitude")
            ? props.value("Shared Latitude").toDouble()
            : 0.0;
    auto center_shared_lon =
        props.contains("Shared Longitude")
            ? props.value("Shared Longitude").toDouble()
            : 0;

    auto center_lon =
        props.contains("Longitude")
            ? props.value("Longitude").toDouble()
            : 0;
    auto center_lat =
        props.contains("Latitude")
            ? props.value("Latitude").toDouble()
            : 0;

    // Get terminal's coordinates in region view
    auto out = main_window->regionView_->sceneToWGS84(
        terminal->pos());

    double terminal_lon = out.x();
    double terminal_lat = out.y();

    // Calculate terminal's offset from region
    // center Calculate the deltas (terminal
    // relative to center)
    double delta_lat = terminal_lat - center_lat;
    double delta_lon = terminal_lon - center_lon;

    // Apply these deltas to the shared coordinates
    double item_global_view_lon =
        center_shared_lon + delta_lon;
    double item_global_view_lat =
        center_shared_lat + delta_lat;

    // Update the global terminal item
    GlobalTerminalItem *globalITem =
        terminal->getGlobalTerminalItem();
    if (!globalITem)
    {
        return;
    }

    // Important: Ensure we're using the correct coordinate
    // transformation
    QPointF globalPos =
        main_window->globalMapView_->wgs84ToScene(QPointF(
            item_global_view_lon, item_global_view_lat));
    // Set position directly to avoid any signal/slot
    // cascading issues
    globalITem->setPos(globalPos);
}

bool CargoNetSim::GUI::ViewController::
    updateTerminalPositionByGlobalPosition(
        MainWindow *mainWindow, TerminalItem *terminal,
        QPointF globalGeoPos)
{
    if (!mainWindow || !terminal
        || !terminal->getGlobalTerminalItem())
    {
        return false;
    }

    QString currentRegion = terminal->getRegion();

    // Get the region center point
    auto regionCenterPoint =
        CargoNetSim::CargoNetSimController::getInstance()
            .getRegionDataController()
            ->getRegionData(currentRegion)
            ->getVariableAs<RegionCenterPoint *>(
                "regionCenterPoint", nullptr);

    if (!regionCenterPoint)
    {
        return false;
    }

    auto props = regionCenterPoint->getProperties();

    auto center_lon =
        props.contains("Longitude")
            ? props.value("Longitude").toDouble()
            : 0;
    auto center_lat =
        props.contains("Latitude")
            ? props.value("Latitude").toDouble()
            : 0;

    QPointF terminalGeoPos =
        mainWindow->regionView_->sceneToWGS84(
            terminal->pos());

    // Calculate the delta (how far the terminal is from its
    // region center)
    double delta_lat = terminalGeoPos.y() - center_lat;
    double delta_lon = terminalGeoPos.x() - center_lon;

    // Calculate what the shared coordinates need to be to
    // position the terminal at the target location
    double new_shared_lat = globalGeoPos.y() - delta_lat;
    double new_shared_lon = globalGeoPos.x() - delta_lon;

    regionCenterPoint->setProperty("Shared Latitude",
                                   new_shared_lat);
    regionCenterPoint->setProperty("Shared Longitude",
                                   new_shared_lon);

    // Update the terminal position
    CargoNetSim::GUI::UtilitiesFunctions::
        updateGlobalMapForRegion(mainWindow, currentRegion);

    return true;
}

void CargoNetSim::GUI::ViewController::flashPathLines(
    MainWindow *mainWindow, int pathId)
{
    if (!mainWindow || !mainWindow->shortestPathTable_)
    {
        return;
    }

    // Get the selected path data
    const ShortestPathsTable::PathData *pathData =
        mainWindow->shortestPathTable_->getDataByPathId(
            pathId);

    if (!pathData || !pathData->path)
    {
        qWarning()
            << "Cannot flash path: Invalid path data for ID"
            << pathId;
        return;
    }

    // Get segments and terminals from the path
    const QList<Backend::PathSegment *> &segments =
        pathData->path->getSegments();
    const QList<Backend::Terminal *> &terminals =
        pathData->path->getTerminalsInPath();

    // Process each segment
    for (int i = 0; i < segments.size(); ++i)
    {
        Backend::PathSegment *segment = segments[i];
        if (!segment)
            continue;

        // Get terminals for this segment
        Backend::Terminal *startTerminal = terminals[i];
        Backend::Terminal *endTerminal   = terminals[i + 1];

        if (!startTerminal || !endTerminal)
        {
            qWarning() << "Cannot flash path: Missing "
                          "terminals for segment"
                       << i;
            continue;
        }

        // Find corresponding terminal items in the scene
        TerminalItem *startTerminalItem = nullptr;
        TerminalItem *endTerminalItem   = nullptr;

        for (auto terminal :
             mainWindow->regionScene_
                 ->getItemsByType<TerminalItem>())
        {
            QString terminalName =
                terminal->getProperty("Name").toString();

            if (terminalName
                == startTerminal->getDisplayName())
            {
                startTerminalItem = terminal;
            }
            else if (terminalName
                     == endTerminal->getDisplayName())
            {
                endTerminalItem = terminal;
            }

            if (startTerminalItem && endTerminalItem)
                break;
        }

        if (!startTerminalItem || !endTerminalItem)
        {
            qWarning() << "Cannot flash path: Unable to "
                          "find terminal items for segment"
                       << i;
            continue;
        }

        // Get transportation mode
        Backend::TransportationTypes::TransportationMode
                mode = segment->getMode();
        QString segmentModeText =
            Backend::TransportationTypes::toString(mode);
        segmentModeText = segmentModeText == "Train"
                              ? "Rail"
                              : segmentModeText;

        // Find connection line between these terminals
        ConnectionLine *connection = nullptr;
        for (auto line :
             mainWindow->regionScene_
                 ->getItemsByType<ConnectionLine>())
        {
            if (((line->startItem() == startTerminalItem
                  && line->endItem() == endTerminalItem)
                 || (line->startItem() == endTerminalItem
                     && line->endItem()
                            == startTerminalItem))
                && line->connectionType()
                       == segmentModeText)
            {
                connection = line;
                break;
            }
        }

        if (!connection)
        {
            qWarning() << "Cannot flash path: Unable to "
                          "find connection line for segment"
                       << i;
            continue;
        }

        // If ship mode, flash the connection line only
        if (mode
            == Backend::TransportationTypes::
                TransportationMode::Ship)
        {
            // Flash the connection line
            connection->flash(
                true,
                QColor(Qt::blue)); // Blue for ship
        }
        // For train or truck, flash the network map lines
        else
        {
            // Determine network type
            NetworkType networkType;
            if (mode
                == Backend::TransportationTypes::
                    TransportationMode::Train)
            {
                networkType = NetworkType::Train;
                // Choose dark gray for train
                QColor flashColor = QColor(80, 80, 80);
            }
            else if (mode
                     == Backend::TransportationTypes::
                         TransportationMode::Truck)
            {
                networkType = NetworkType::Truck;
                // Choose magenta for truck
                QColor flashColor = QColor(255, 0, 255);
            }

            QString regionName =
                startTerminalItem->getRegion();

            // Get map points for both terminals
            QList<MapPoint *> sourcePoints =
                UtilitiesFunctions::getMapPointsOfTerminal(
                    mainWindow->regionScene_,
                    startTerminalItem, regionName, "*",
                    networkType);

            QList<MapPoint *> targetPoints =
                UtilitiesFunctions::getMapPointsOfTerminal(
                    mainWindow->regionScene_,
                    endTerminalItem, regionName, "*",
                    networkType);

            // Find matching network points
            auto networkPairs = UtilitiesFunctions::
                getCommonNetworksOfNetworkType(sourcePoints,
                                               targetPoints,
                                               networkType);

            if (networkPairs.isEmpty())
            {
                qWarning()
                    << "Cannot flash path: No common "
                       "network points found for segment"
                    << i;
                continue;
            }

            // Take the first pair
            MapPoint *sourcePoint =
                networkPairs.first().first;
            MapPoint *targetPoint =
                networkPairs.first().second;

            if (!sourcePoint || !targetPoint)
            {
                qWarning() << "Cannot flash path: Invalid "
                              "network points";
                continue;
            }

            // Get network information
            QObject *networkObj =
                sourcePoint->getReferenceNetwork();
            if (!networkObj)
            {
                qWarning() << "Cannot flash path: Unable "
                              "to get reference network";
                continue;
            }

            QString networkName;
            if (networkType == NetworkType::Train)
            {
                auto trainNet = qobject_cast<
                    Backend::TrainClient::NeTrainSimNetwork
                        *>(networkObj);
                if (trainNet)
                    networkName =
                        trainNet->getNetworkName();
            }
            else
            {
                auto truckNet = qobject_cast<
                    Backend::TruckClient::IntegrationNetwork
                        *>(networkObj);
                if (truckNet)
                    networkName =
                        truckNet->getNetworkName();
            }

            if (networkName.isEmpty())
            {
                qWarning() << "Cannot flash path: Unable "
                              "to determine network name";
                continue;
            }

            // Get node IDs
            QString sourceNodeId =
                sourcePoint->getReferencedNetworkNodeID();
            QString targetNodeId =
                targetPoint->getReferencedNetworkNodeID();

            bool validSourceID, validTargetID;
            int  sourceID =
                sourceNodeId.toInt(&validSourceID);
            int targetID =
                targetNodeId.toInt(&validTargetID);

            if (!validSourceID || !validTargetID)
            {
                qWarning() << "Cannot flash path: Invalid "
                              "node IDs";
                continue;
            }

            // Get map lines for the shortest path
            QList<MapLine *> pathMapLines =
                NetworkController::getShortestPathMapLines(
                    mainWindow, regionName, networkName,
                    networkType, sourceID, targetID);

            // Flash each map line with appropriate color
            QColor flashColor =
                (networkType == NetworkType::Train)
                    ? QColor(Qt::darkGray) // Dark gray for
                                           // train
                    : QColor(
                          Qt::magenta); // Magenta for truck

            for (MapLine *mapLine : pathMapLines)
            {
                mapLine->flash(false, flashColor);
            }
        }
    }
}

bool CargoNetSim::GUI::ViewController::
    linkTerminalToClosestNetworkPoint(
        MainWindow *mainWindow, TerminalItem *terminal,
        const QList<NetworkType> &networkTypes)
{
    if (!mainWindow || !terminal || networkTypes.isEmpty())
    {
        return false;
    }

    QString           region = terminal->getRegion();
    QList<MapPoint *> networkPoints;
    QMap<NetworkType, QList<MapPoint *>>
        alreadyLinkedPointsByType;

    // Get all map points in the scene
    QList<MapPoint *> allMapPoints =
        mainWindow->regionScene_
            ->getItemsByType<MapPoint>();

    // Filter map points by network type and region
    for (MapPoint *point : allMapPoints)
    {
        if (point->getRegion() != region)
        {
            continue;
        }

        QObject *network = point->getReferenceNetwork();
        if (!network)
        {
            continue;
        }

        NetworkType matchedType =
            NetworkType::Train; // Default, will be
                                // overwritten
        bool isMatchingType = false;

        if (networkTypes.contains(NetworkType::Train))
        {
            if (dynamic_cast<
                    Backend::TrainClient::NeTrainSimNetwork
                        *>(network))
            {
                isMatchingType = true;
                matchedType    = NetworkType::Train;
            }
        }

        if (networkTypes.contains(NetworkType::Truck))
        {
            if (dynamic_cast<
                    Backend::TruckClient::IntegrationNetwork
                        *>(network))
            {
                isMatchingType = true;
                matchedType    = NetworkType::Truck;
            }
        }

        if (isMatchingType)
        {
            // Check if this point is already linked to a
            // terminal
            if (point->getLinkedTerminal())
            {
                alreadyLinkedPointsByType[matchedType]
                    .append(point);
            }
            else
            {
                networkPoints.append(point);
            }
        }
    }

    // Process events to keep UI responsive
    QApplication::processEvents();

    if (networkPoints.isEmpty())
    {
        QString networkTypeStr;
        if (networkTypes.size() == 1)
        {
            networkTypeStr =
                networkTypes.first() == NetworkType::Train
                    ? "train"
                    : "truck";
        }
        else
        {
            networkTypeStr = "transport";
        }

        mainWindow->showStatusBarError(
            QString("No available %1 network points found "
                    "in region '%2'")
                .arg(networkTypeStr)
                .arg(region),
            3000);
        return false;
    }

    // Check if terminal is already linked to a network
    // point of any requested type
    for (NetworkType type : networkTypes)
    {
        const QList<MapPoint *> &typeLinkedPoints =
            alreadyLinkedPointsByType[type];
        for (MapPoint *point : typeLinkedPoints)
        {
            if (point->getLinkedTerminal() == terminal)
            {
                QString networkTypeStr =
                    type == NetworkType::Train ? "train"
                                               : "truck";
                // // This terminal is already linked to a
                // // network point of this type
                // mainWindow->showStatusBarError(
                //     QString("Terminal '%1' is already "
                //             "linked to a %2 network
                //             point.")
                //         .arg(terminal->getProperty("Name")
                //                  .toString())
                //         .arg(networkTypeStr),
                //     3000);
                return false;
            }
        }

        // Process events to keep UI responsive
        QApplication::processEvents();
    }

    // Find the closest network point
    MapPoint *closestPoint = nullptr;
    qreal minDistance = std::numeric_limits<qreal>::max();

    QPointF terminalPos = terminal->pos();
    for (MapPoint *point : networkPoints)
    {
        QPointF pointPos = point->pos();
        qreal   distance =
            QLineF(terminalPos, pointPos).length();

        if (distance < minDistance)
        {
            minDistance  = distance;
            closestPoint = point;
        }
    }

    if (closestPoint)
    {
        // Link the terminal to the closest point
        UtilitiesFunctions::linkMapPointToTerminal(
            mainWindow, closestPoint, terminal);

        // Get the network name and type for display
        QString  networkName    = "Unknown Network";
        QString  networkTypeStr = "transport";
        QObject *network =
            closestPoint->getReferenceNetwork();
        if (auto trainNet = dynamic_cast<
                Backend::TrainClient::NeTrainSimNetwork *>(
                network))
        {
            networkName    = trainNet->getNetworkName();
            networkTypeStr = "train";
        }
        else if (auto truckNet =
                     dynamic_cast<Backend::TruckClient::
                                      IntegrationNetwork *>(
                         network))
        {
            networkName    = truckNet->getNetworkName();
            networkTypeStr = "truck";
        }

        mainWindow->showStatusBarMessage(
            QString("Terminal '%1' successfully linked to "
                    "%2 network '%3'")
                .arg(terminal->getProperty("Name")
                         .toString())
                .arg(networkTypeStr)
                .arg(networkName),
            3000);

        return true;
    }

    mainWindow->showStatusBarError(
        "Failed to find a suitable network point to link.",
        3000);
    return false;
}

void CargoNetSim::GUI::ViewController::
    linkAllVisibleTerminalsToNetwork(
        MainWindow               *mainWindow,
        const QList<NetworkType> &networkTypes)
{
    if (!mainWindow || networkTypes.isEmpty())
    {
        mainWindow->showStatusBarError(
            "No network types selected for linking.", 3000);
        return;
    }

    ToolbarController::storeButtonStates(mainWindow);
    ToolbarController::disableAllButtons(mainWindow);
    mainWindow->startStatusProgress();

    // Get the current scene
    auto scene = mainWindow->regionScene_;
    if (!scene)
    {
        mainWindow->showStatusBarError(
            "No active scene found.", 3000);
        ToolbarController::restoreButtonStates(mainWindow);
        mainWindow->stopStatusProgress();
        return;
    }

    // Get current region
    QString currentRegion =
        CargoNetSim::CargoNetSimController::getInstance()
            .getRegionDataController()
            ->getCurrentRegion();

    // Get all visible terminal items in the current region
    QList<TerminalItem *> allTerminals =
        scene->getItemsByType<TerminalItem>();
    QList<TerminalItem *> visibleTerminals;

    for (TerminalItem *terminal : allTerminals)
    {
        if (terminal && terminal->isVisible()
            && terminal->getRegion() == currentRegion)
        {
            visibleTerminals.append(terminal);
        }
    }

    if (visibleTerminals.isEmpty())
    {
        mainWindow->showStatusBarError(
            QString(
                "No visible terminals found in region '%1'")
                .arg(currentRegion),
            3000);
        ToolbarController::restoreButtonStates(mainWindow);
        mainWindow->stopStatusProgress();
        return;
    }

    // Create a dialog to confirm the operation
    QMessageBox msgBox(mainWindow);
    msgBox.setWindowTitle("Link Terminals to Network");
    msgBox.setText(
        QString(
            "This will link all %1 visible terminals in "
            "region '%2' to their closest network points.")
            .arg(visibleTerminals.size())
            .arg(currentRegion));

    QString networkTypesStr;
    if (networkTypes.contains(NetworkType::Train)
        && networkTypes.contains(NetworkType::Truck))
    {
        networkTypesStr = "train and truck";
    }
    else if (networkTypes.contains(NetworkType::Train))
    {
        networkTypesStr = "train";
    }
    else if (networkTypes.contains(NetworkType::Truck))
    {
        networkTypesStr = "truck";
    }

    msgBox.setInformativeText(
        QString("Selected network types: %1\n\nDo you want "
                "to continue?")
            .arg(networkTypesStr));
    msgBox.setStandardButtons(QMessageBox::Yes
                              | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);

    int result = msgBox.exec();
    if (result != QMessageBox::Yes)
    {
        ToolbarController::restoreButtonStates(mainWindow);
        mainWindow->stopStatusProgress();
        return;
    }

    // Link each terminal to its closest network point
    int successCount = 0;
    int i            = 0;

    for (TerminalItem *terminal : visibleTerminals)
    {

        if (linkTerminalToClosestNetworkPoint(
                mainWindow, terminal, networkTypes))
        {
            successCount++;

            // Process events to keep UI responsive
            QApplication::processEvents();
        }

        // Process events to keep UI responsive
        QApplication::processEvents();
    }

    // Display results
    if (successCount > 0)
    {
        mainWindow->showStatusBarMessage(
            QString(
                "Successfully linked %1 of %2 terminals to "
                "their closest network points")
                .arg(successCount)
                .arg(visibleTerminals.size()),
            5000);
    }
    else
    {
        mainWindow->showStatusBarError(
            "No terminals could be linked to network "
            "points.",
            3000);
    }

    ToolbarController::restoreButtonStates(mainWindow);
    mainWindow->stopStatusProgress();
}

void CargoNetSim::GUI::ViewController::flashTerminalItems(
    QList<TerminalItem *> terminals, bool evenIfHidden)
{
    for (auto terminal : terminals)
    {
        terminal->flash(evenIfHidden);

        // Process events to keep UI responsive
        QApplication::processEvents();
    }
}

CargoNetSim::GUI::TerminalItem *
CargoNetSim::GUI::ViewController::createTerminalAtPoint(
    MainWindow *mainWindow, const QString &region,
    const QString &terminalType, const QPointF &point)
{
    qCDebug(lcRail) << "[RailDraw]     createTerminal: type="
                    << terminalType << "region=" << region
                    << "point=" << point;
    // Create a new terminal item
    QMap<QString, QPixmap> terminalIcons =
        IconFactory::createTerminalIcons();
    qCDebug(lcRail)
        << "[RailDraw]     createTerminal: icons loaded,"
           " keys="
        << terminalIcons.keys();
    QPixmap pixmap = terminalIcons.value(terminalType);
    qCDebug(lcRail)
        << "[RailDraw]     createTerminal: pixmap isNull="
        << pixmap.isNull();
    auto terminal = new TerminalItem(pixmap, {}, region,
                                     nullptr, terminalType);
    qCDebug(lcRail)
        << "[RailDraw]     createTerminal: TerminalItem"
           " constructed";
    terminal->setPos(point);
    qCDebug(lcRail)
        << "[RailDraw]     createTerminal: setPos ok,"
           " addItemWithId begin";
    mainWindow->regionScene_->addItemWithId(
        terminal, terminal->getID());
    qCDebug(lcRail)
        << "[RailDraw]     createTerminal: addItemWithId ok";

    // update the TerminalItem visibility
    terminal->setVisible(
        CargoNetSim::CargoNetSimController::getInstance()
            .getRegionDataController()
            ->getCurrentRegion()
        == region);
    qCDebug(lcRail)
        << "[RailDraw]     createTerminal: setVisible ok";

    // Update the Global Map Item visibility
    updateGlobalMapItem(mainWindow, terminal);
    qCDebug(lcRail)
        << "[RailDraw]     createTerminal: updateGlobalMapItem"
           " ok";

    // Connections
    QObject::connect(
        terminal, &TerminalItem::positionChanged,
        [mainWindow, terminal]() {
            CargoNetSim::GUI::ViewController::
                updateGlobalMapItem(mainWindow, terminal);
        });
    QObject::connect(
        terminal, &TerminalItem::clicked,
        [mainWindow](TerminalItem *terminal) {
            UtilitiesFunctions::updatePropertiesPanel(
                mainWindow, terminal);
        });
    QObject::connect(
        terminal, &TerminalItem::clicked, mainWindow,
        &MainWindow::handleTerminalNodeLinking);
    QObject::connect(
        terminal, &TerminalItem::clicked, mainWindow,
        &MainWindow::handleTerminalNodeUnlinking);

    return terminal;
}

void CargoNetSim::GUI::ViewController::drawNetwork(
    MainWindow *mainWindow, Backend::RegionData *regionData,
    NetworkType networkType, QString &networkName)
{
    QString regionName = regionData->getRegion();
    QColor  linksColor = ColorUtils::getRandomColor();

    ToolbarController::storeButtonStates(mainWindow);
    ToolbarController::disableAllButtons(mainWindow);
    mainWindow->startStatusProgress();

    // Get the network data
    if (networkType == NetworkType::Train)
    {
        auto network =
            regionData->getTrainNetwork(networkName);
        CargoNetSim::GUI::ViewController::drawTrainNetwork(
            mainWindow, network, regionName, linksColor);
    }
    else if (networkType == NetworkType::Truck)
    {
        auto network =
            regionData->getTruckNetworkConfig(networkName);
        CargoNetSim::GUI::ViewController::drawTruckNetwork(
            mainWindow, network, regionName, linksColor);
    }

    ToolbarController::restoreButtonStates(mainWindow);
    mainWindow->stopStatusProgress();
}

void CargoNetSim::GUI::ViewController::
    changeNetworkVisibility(MainWindow    *mainWindow,
                            const QString &networkName,
                            const bool     isVisible)
{
    if (!mainWindow || !mainWindow->regionScene_)
    {
        return;
    }

    // Common function to check network name and set
    // visibility
    auto checkNetworkAndSetVisibility =
        [&networkName,
         isVisible](const QString &itemNetworkName,
                    QGraphicsItem *graphicsItem) {
            if (itemNetworkName == networkName)
            {
                graphicsItem->setVisible(isVisible);
            }
        };

    auto mapPoints = mainWindow->regionScene_
                         ->getItemsByType<MapPoint>();
    auto mapLines =
        mainWindow->regionScene_->getItemsByType<MapLine>();

    for (auto mapPoint : mapPoints)
    {
        QObject *referencedNet =
            mapPoint->getReferenceNetwork();
        if (!referencedNet)
            continue;

        // Try to cast to train network
        if (auto *trainNet = dynamic_cast<
                Backend::TrainClient::NeTrainSimNetwork *>(
                referencedNet))
        {
            checkNetworkAndSetVisibility(
                trainNet->getNetworkName(), mapPoint);
        }
        // Try to cast to truck network
        else if (auto *truckNet =
                     dynamic_cast<Backend::TruckClient::
                                      IntegrationNetwork *>(
                         referencedNet))
        {
            checkNetworkAndSetVisibility(
                truckNet->getNetworkName(), mapPoint);
        }
    }

    for (auto mapLine : mapLines)
    {
        QObject *referencedNet =
            mapLine->getReferenceNetwork();
        if (!referencedNet)
            continue;

        // Try to cast to train network
        if (auto *trainNet = dynamic_cast<
                Backend::TrainClient::NeTrainSimNetwork *>(
                referencedNet))
        {
            checkNetworkAndSetVisibility(
                trainNet->getNetworkName(), mapLine);
        }
        // Try to cast to truck network
        else if (auto *truckNet =
                     dynamic_cast<Backend::TruckClient::
                                      IntegrationNetwork *>(
                         referencedNet))
        {
            checkNetworkAndSetVisibility(
                truckNet->getNetworkName(), mapLine);
        }
    }
}

void CargoNetSim::GUI::ViewController::renameRegion(
    MainWindow *mainWindow, const QString &oldRegionName,
    const QString &newName)
{
    if (!mainWindow || !mainWindow->regionScene_)
    {
        return;
    }

    QGraphicsScene *scene = mainWindow->regionScene_;
    for (QGraphicsItem *item : scene->items())
    {
        if (MapPoint *mapPoint =
                dynamic_cast<MapPoint *>(item))
        {
            if (mapPoint->getRegion() == oldRegionName)
            {
                mapPoint->setRegion(newName);
            }
        }
        else if (MapLine *mapLine =
                     dynamic_cast<MapLine *>(item))
        {
            if (mapLine->getRegion() == oldRegionName)
            {
                mapLine->setRegion(newName);
            }
        }
        else if (RegionCenterPoint *regionCenter =
                     dynamic_cast<RegionCenterPoint *>(
                         item))
        {
            if (regionCenter->getRegion() == oldRegionName)
            {
                regionCenter->setRegion(newName);
            }
        }
        else if (TerminalItem *terminal =
                     dynamic_cast<TerminalItem *>(item))
        {
            if (terminal->getRegion() == oldRegionName)
            {
                terminal->setRegion(newName);
            }
        }
        else if (ConnectionLine *connectionLine =
                     dynamic_cast<ConnectionLine *>(item))
        {
            if (connectionLine->getRegion()
                == oldRegionName)
            {
                connectionLine->setRegion(newName);
            }
        }
        else if (BackgroundPhotoItem *backgroundPhoto =
                     dynamic_cast<BackgroundPhotoItem *>(
                         item))
        {
            if (backgroundPhoto->getRegion()
                == oldRegionName)
            {
                backgroundPhoto->setRegion(newName);
            }
        }
    }
    updateSceneVisibility(mainWindow);
}

void CargoNetSim::GUI::ViewController::changeNetworkColor(
    MainWindow *mainWindow, const QString &networkName,
    const QColor newColor)
{
    if (!mainWindow)
    {
        return;
    }

    QColor newDarkerColor =
        newColor.darker(150); // 150% darker for MapPoints

    auto mapPoints = mainWindow->regionScene_
                         ->getItemsByType<MapPoint>();
    auto mapLines =
        mainWindow->regionScene_->getItemsByType<MapLine>();

    for (auto mapPoint : mapPoints)
    {
        Backend::TrainClient::NeTrainSimNetwork *trainNet =
            dynamic_cast<
                Backend::TrainClient::NeTrainSimNetwork *>(
                mapPoint->getReferenceNetwork());
        Backend::TruckClient::IntegrationNetwork *truckNet =
            dynamic_cast<
                Backend::TruckClient::IntegrationNetwork *>(
                mapPoint->getReferenceNetwork());

        if (trainNet)
        {
            if (trainNet->getNetworkName() == networkName)
            {
                mapPoint->setColor(newDarkerColor);
            }
        }
        else if (truckNet)
        {
            if (truckNet->getNetworkName() == networkName)
            {
                mapPoint->setColor(newDarkerColor);
            }
        }
    }

    for (auto mapLine : mapLines)
    {
        Backend::TrainClient::NeTrainSimNetwork *trainNet =
            dynamic_cast<
                Backend::TrainClient::NeTrainSimNetwork *>(
                mapLine->getReferenceNetwork());
        Backend::TruckClient::IntegrationNetwork *truckNet =
            dynamic_cast<
                Backend::TruckClient::IntegrationNetwork *>(
                mapLine->getReferenceNetwork());

        if (trainNet)
        {
            if (trainNet->getNetworkName() == networkName)
            {
                mapLine->setColor(newColor);
            }
        }
        else if (truckNet)
        {
            if (truckNet->getNetworkName() == networkName)
            {
                mapLine->setColor(newColor);
            }
        }
    }
}

void CargoNetSim::GUI::ViewController::drawTrainNetwork(
    MainWindow                              *mainWindow,
    Backend::TrainClient::NeTrainSimNetwork *network,
    QString &regionName, QColor &linksColor)
{
    qCDebug(lcRail) << "[RailDraw] drawTrainNetwork start"
             << "region=" << regionName
             << "network=" << (network ? "ok" : "null");
    qCDebug(lcRail) << "[RailDraw] setUsingProjectedCoords(true)";
    mainWindow->regionView_->setUsingProjectedCoords(true);
    qCDebug(lcRail) << "[RailDraw] updateAllCoordinates begin";
    mainWindow->updateAllCoordinates();
    qCDebug(lcRail) << "[RailDraw] updateAllCoordinates ok";

    // Define node color
    QColor nodesColor = QColor(linksColor);
    nodesColor.setHsv(nodesColor.hue(),
                      nodesColor.saturation(),
                      nodesColor.value() * 0.7);

    // set the network Color
    network->setVariable("color", linksColor);

    auto nodes = network->getNodes();
    qCDebug(lcRail) << "[RailDraw] drawing nodes, count="
             << nodes.size();
    // Draw the train network
    int nodeIdx = 0;
    for (auto &node : nodes)
    {
        qCDebug(lcRail)
            << "[RailDraw] node idx=" << nodeIdx
            << " userId=" << node->getUserId()
            << " isTerminal=" << node->isTerminal()
            << " x=" << node->getX()
            << " y=" << node->getY()
            << " xScale=" << node->getXScale()
            << " yScale=" << node->getYScale();

        QMap<QString, QVariant> properties = {
            {"Is_terminal", node->isTerminal()},
            {"Dwell_time", node->getDwellTime()},
            {"Description", node->getDescription()}};

        QPointF projectedPoint =
            QPointF(node->getX() * node->getXScale(),
                    node->getY() * node->getYScale());

        qCDebug(lcRail) << "[RailDraw]   calling drawNode"
                        << "projected=" << projectedPoint;
        MapPoint *point =
            CargoNetSim::GUI::ViewController::drawNode(
                mainWindow,
                QString::number(node->getUserId()),
                node->getInternalUniqueID(), projectedPoint,
                regionName, nodesColor, properties);
        qCDebug(lcRail) << "[RailDraw]   drawNode returned"
                        << (point ? "point" : "null");

        if (!point)
        {
            qCCritical(lcRail) << "[RailDraw] drawNode returned"
                           " nullptr at nodeIdx=" << nodeIdx
                        << " userId=" << node->getUserId();
        }
        point->setReferenceNetwork(network);

        // Link terminal to point
        if (point && node->isTerminal())
        {
            qCDebug(lcRail)
                << "[RailDraw]   calling createTerminalAtPoint";
            auto terminal =
                ViewController::createTerminalAtPoint(
                    mainWindow, regionName,
                    "Intermodal Land Terminal",
                    point->getSceneCoordinate());
            qCDebug(lcRail)
                << "[RailDraw]   createTerminalAtPoint returned"
                << (terminal ? "terminal" : "null");

            point->setLinkedTerminal(terminal);
            qCDebug(lcRail)
                << "[RailDraw]   setLinkedTerminal ok";
        }
        ++nodeIdx;
    }
    qCDebug(lcRail) << "[RailDraw] nodes drawn";

    // Process events to keep UI responsive
    QApplication::processEvents();

    auto linksVec = network->getLinks();
    qCDebug(lcRail) << "[RailDraw] drawing links, count="
             << linksVec.size();
    int linkIdx = 0;
    // Draw the train network links
    for (auto &link : linksVec)
    {
        // Get the source and destination nodes
        auto sourceNode = link->getFromNode();
        auto destNode   = link->getToNode();
        if (!sourceNode || !destNode)
        {
            qCCritical(lcRail) << "[RailDraw] link has null"
                           " endpoint, linkIdx=" << linkIdx
                        << " userId=" << link->getUserId();
        }

        // Create the source and destination points
        QPointF projectedSourcePoint = QPointF(
            sourceNode->getX() * sourceNode->getXScale(),
            sourceNode->getY() * sourceNode->getYScale());

        QPointF projectedDestPoint = QPointF(
            destNode->getX() * destNode->getXScale(),
            destNode->getY() * destNode->getYScale());

        QMap<QString, QVariant> properties = {
            {"Length", link->getLength()},
            {"MaxSpeed",
             link->getMaxSpeed() * link->getSpeedScale()}};

        auto line =
            CargoNetSim::GUI::ViewController::drawLink(
                mainWindow,
                QString::number(link->getUserId()),
                link->getInternalUniqueID(),
                projectedSourcePoint, projectedDestPoint,
                regionName, linksColor, properties);

        if (!line)
        {
            qCCritical(lcRail) << "[RailDraw] drawLink returned"
                           " nullptr at linkIdx=" << linkIdx
                        << " userId=" << link->getUserId();
        }
        line->setReferenceNetwork(network);
        ++linkIdx;
    }
    qCDebug(lcRail) << "[RailDraw] links drawn";

    // Fit the view to the scene
    qCDebug(lcRail) << "[RailDraw] fitInView begin";
    mainWindow->regionView_->fitInView(
        mainWindow->regionScene_->itemsBoundingRect(),
        Qt::KeepAspectRatio);
    qCDebug(lcRail) << "[RailDraw] fitInView ok";

    mainWindow->showStatusBarMessage(
        QString("Train network imported successfully."));
    qCDebug(lcRail) << "[RailDraw] drawTrainNetwork done";
}

void CargoNetSim::GUI::ViewController::drawTruckNetwork(
    MainWindow *mainWindow,
    Backend::TruckClient::IntegrationSimulationConfig
            *networkConfig,
    QString &regionName, QColor &linksColor)
{
    if (!mainWindow)
    {
        return;
    }

    mainWindow->regionView_->setUsingProjectedCoords(true);
    mainWindow->updateAllCoordinates();

    // Get the network object
    auto network = networkConfig->getNetwork();

    // Define node color
    QColor nodesColor = QColor(linksColor);
    nodesColor.setHsv(nodesColor.hue(),
                      nodesColor.saturation(),
                      nodesColor.value() * 0.7);

    // set the network Color
    network->setVariable("color", linksColor);

    for (auto &node : network->getNodes())
    {

        QMap<QString, QVariant> properties = {
            {"Description", node->getDescription()}};

        auto point =
            CargoNetSim::GUI::ViewController::drawNode(
                mainWindow,
                QString::number(node->getNodeId()),
                node->getInternalUniqueID(),
                QPointF(node->getXCoordinate()
                            * node->getXScale()
                            * 1000.0, // km to m
                        node->getYCoordinate()
                            * node->getYScale()
                            * 1000.0), // km to m
                regionName, nodesColor, properties);

        point->setReferenceNetwork(network);
    }

    // Process events to keep UI responsive
    QApplication::processEvents();

    for (auto &link : network->getLinks())
    {
        QMap<QString, QVariant> properties = {
            {"ReferenceNetworkID", link->getLinkId()},
            {"Length", link->getLength()
                           * link->getLengthScale()
                           * 1000.0}, // km to m
            {"FreeFlowTime",
             link->getFreeSpeed() * link->getSpeedScale()},
            {"NoOfLanes", link->getLanes()}};

        // Get the source and destination nodes
        auto to =
            network->getNode(link->getDownstreamNodeId());
        auto from =
            network->getNode(link->getUpstreamNodeId());
        if (!to || !from)
        {
            continue;
        }

        // Create the source and destination projected
        // points
        QPointF projectedSourcePoint = QPointF(
            from->getXCoordinate() * from->getXScale()
                * 1000.0, // km to m
            from->getYCoordinate() * from->getYScale()
                * 1000.0); // km to m
        QPointF projectedDestPoint =
            QPointF(to->getXCoordinate() * to->getXScale()
                        * 1000.0, // km to m
                    to->getYCoordinate() * to->getYScale()
                        * 1000.0); // km to m

        auto line =
            CargoNetSim::GUI::ViewController::drawLink(
                mainWindow,
                QString::number(link->getLinkId()),
                link->getInternalUniqueID(),
                projectedSourcePoint, projectedDestPoint,
                regionName, linksColor, properties);

        line->setReferenceNetwork(network);
    }

    // Fit the view to the scene
    mainWindow->regionView_->fitInView(
        mainWindow->regionScene_->itemsBoundingRect(),
        Qt::KeepAspectRatio);

    mainWindow->showStatusBarMessage(
        QString("Truck network imported successfully."));
}

CargoNetSim::GUI::MapPoint *
CargoNetSim::GUI::ViewController::drawNode(
    MainWindow *mainWindow, const QString &networkNodeID,
    const QString &nodeUniqueID, QPointF projectedPoint,
    QString &regionName, QColor color,
    const QMap<QString, QVariant> &properties)
{

    QPointF geodeticPoint =
        mainWindow->regionView_->convertCoordinates(
            projectedPoint, "to_geodetic");
    QPointF scenePoint =
        mainWindow->regionView_->wgs84ToScene(
            geodeticPoint);

    // Create projected coordinate point
    MapPoint *point =
        new MapPoint(networkNodeID, scenePoint, regionName,
                     "circle", nullptr, properties);

    QObject::connect(
        point, &MapPoint::clicked,
        [mainWindow](MapPoint *point) {
            UtilitiesFunctions::updatePropertiesPanel(
                mainWindow, point);
        });

    QObject::connect(
        point, &MapPoint::clicked, mainWindow,
        &MainWindow::handleTerminalNodeLinking);

    QObject::connect(
        point, &MapPoint::clicked, mainWindow,
        &MainWindow::handleTerminalNodeUnlinking);

    point->setProperty("NodeID", nodeUniqueID);
    point->setColor(color); // Set node color

    mainWindow->regionScene_->addItemWithId(
        point, nodeUniqueID); // Add node to scene

    return point;
}

CargoNetSim::GUI::MapLine *
CargoNetSim::GUI::ViewController::drawLink(
    MainWindow *mainWindow, const QString &networkNodeID,
    const QString &linkUniqueID,
    QPointF projectedStartPoint, QPointF projectedEndPoint,
    QString &regionName, QColor color,
    const QMap<QString, QVariant> &properties)
{
    MapLine *line = nullptr;
    try
    {
        // Convert the points to geodetic coordinates
        QPointF sourceGeodetic =
            mainWindow->regionView_->convertCoordinates(
                projectedStartPoint, "to_geodetic");
        QPointF destGeodetic =
            mainWindow->regionView_->convertCoordinates(
                projectedEndPoint, "to_geodetic");
        QPointF sourceScenePoint =
            mainWindow->regionView_->wgs84ToScene(
                sourceGeodetic);
        QPointF destScenePoint =
            mainWindow->regionView_->wgs84ToScene(
                destGeodetic);

        // Create the link
        line = new MapLine(networkNodeID, sourceScenePoint,
                           destScenePoint, regionName,
                           properties);

        QObject::connect(
            line, &MapLine::clicked,
            [mainWindow](MapLine *line) {
                UtilitiesFunctions::updatePropertiesPanel(
                    mainWindow, line);
            });

        line->setProperty("LinkID", linkUniqueID);

        line->setColor(color); // Set link color

        mainWindow->regionScene_->addItemWithId(
            line, linkUniqueID); // Add link to scene
    }
    catch (const std::exception &e)
    {
        qWarning() << "Error in drawLink:" << e.what();

        QMessageBox::warning(
            mainWindow, "Error",
            QString("Failed to draw link: %1")
                .arg(e.what()));
    }

    return line;
}

void CargoNetSim::GUI::ViewController::removeNetwork(
    MainWindow *mainWindow, NetworkType networkType,
    Backend::RegionData *regionData, QString &networkName)
{
    QString regionName = regionData->getRegion();

    if (networkType == NetworkType::Train)
    {
        Backend::TrainClient::NeTrainSimNetwork *network =
            regionData->getTrainNetwork(networkName);
        if (!network)
        {
            return;
        }
        for (auto &node : network->getNodes())
        {
            if (!node)
            {
                continue;
            }
            // Get the point
            MapPoint *point =
                mainWindow->regionScene_
                    ->getItemById<MapPoint>(
                        node->getInternalUniqueID());
            // Check if we got the correct point
            if (!point)
            {
                continue;
            }

            // Get the associated terminal of the point
            TerminalItem *terminal =
                point->getLinkedTerminal();

            // Set it to not appear on global map
            point->setProperty("Show on Global Map", false);
            // Update the Global Map to delete the global
            // terminal item from the global map
            CargoNetSim::GUI::ViewController::
                updateGlobalMapItem(mainWindow, terminal);

            // Remove the item
            mainWindow->regionScene_
                ->removeItemWithId<MapPoint>(
                    node->getInternalUniqueID());
        }
        for (auto &link : network->getLinks())
        {
            if (!link)
            {
                continue;
            }
            mainWindow->regionScene_
                ->removeItemWithId<MapLine>(
                    link->getInternalUniqueID());
        }
    }
    else if (networkType == NetworkType::Truck)
    {
        Backend::TruckClient::IntegrationNetwork *network =
            regionData->getTruckNetwork(networkName);
        for (auto &node : network->getNodes())
        {
            // Get the point
            MapPoint *point =
                mainWindow->regionScene_
                    ->getItemById<MapPoint>(
                        node->getInternalUniqueID());

            // Get the associated terminal of the point
            TerminalItem *terminal =
                point->getLinkedTerminal();

            // Set it to not appear on global map
            point->setProperty("Show on Global Map", false);
            // Update the Global Map to delete the global
            // terminal item from the global map
            CargoNetSim::GUI::ViewController::
                updateGlobalMapItem(mainWindow, terminal);

            mainWindow->regionScene_
                ->removeItemWithId<MapPoint>(
                    node->getInternalUniqueID());
        }
        for (auto &link : network->getLinks())
        {
            mainWindow->regionScene_
                ->removeItemWithId<MapLine>(
                    link->getInternalUniqueID());
        }
    }
}

void CargoNetSim::GUI::ViewController::addBackgroundPhoto(
    CargoNetSim::GUI::MainWindow *mainWindow)
{
    try
    {
        // Open file dialog (using non-native dialog for
        // consistency)
        QString fileName = QFileDialog::getOpenFileName(
            nullptr, "Select Background Photo", "",
            "Images (*.png *.jpg *.bmp)", nullptr,
            QFileDialog::DontUseNativeDialog);

        if (fileName.isEmpty())
        {
            return;
        }

        QPixmap pixmap(fileName);
        if (pixmap.isNull())
        {
            QMessageBox::warning(mainWindow, "Error",
                                 "Failed to load image.");
            return;
        }

        // Check which tab is currently active
        if (mainWindow->tabWidget_->currentWidget()
            == mainWindow->tabWidget_->widget(0))
        { // Main view tab
            // Create a new BackgroundPhotoItem for the main
            // view
            QString currentRegion =
                CargoNetSim::CargoNetSimController::
                    getInstance()
                        .getRegionDataController()
                        ->getCurrentRegion();
            BackgroundPhotoItem *background =
                new BackgroundPhotoItem(pixmap,
                                        currentRegion);
            QObject::connect(
                background, &BackgroundPhotoItem::clicked,
                [mainWindow](BackgroundPhotoItem *item) {
                    UtilitiesFunctions::
                        updatePropertiesPanel(mainWindow,
                                              item);
                });
            QObject::connect(
                background,
                &BackgroundPhotoItem::positionChanged,
                [background,
                 mainWindow](const QPointF &pos) {
                    if (mainWindow->propertiesPanel_
                            ->getCurrentItem()
                        == background)
                    {
                        mainWindow->propertiesPanel_
                            ->updatePositionFields(pos);
                    }
                });

            // Place the photo at the center of the main
            // view
            QPointF viewCenter =
                mainWindow->regionView_->mapToScene(
                    mainWindow->regionView_->viewport()
                        ->rect()
                        .center());

            double lat, lon;
            auto   wgsPoint =
                mainWindow->regionView_->sceneToWGS84(
                    viewCenter);
            lat = wgsPoint.x();
            lon = wgsPoint.y();
            background->getProperties()["Latitude"] =
                QString::number(lat, 'f', 6);
            background->getProperties()["Longitude"] =
                QString::number(lon, 'f', 6);
            background->setPos(viewCenter);

            mainWindow->regionScene_->addItemWithId(
                background, background->getID());

            CargoNetSim::CargoNetSimController::
                getInstance()
                    .getRegionDataController()
                    ->setRegionVariable(
                        currentRegion,
                        "backgroundPhotoItem",
                        QVariant::fromValue(background));
        }
        else
        { // Global map tab
            // Create a new BackgroundPhotoItem for the
            // global map
            BackgroundPhotoItem *background =
                new BackgroundPhotoItem(pixmap, "global");
            QObject::connect(
                background, &BackgroundPhotoItem::clicked,
                [mainWindow](BackgroundPhotoItem *item) {
                    UtilitiesFunctions::
                        updatePropertiesPanel(mainWindow,
                                              item);
                });
            QObject::connect(
                background,
                &BackgroundPhotoItem::positionChanged,
                [background,
                 &mainWindow](const QPointF &pos) {
                    if (mainWindow->propertiesPanel_
                            ->getCurrentItem()
                        == background)
                    {
                        mainWindow->propertiesPanel_
                            ->updatePositionFields(pos);
                    }
                });

            // Place the photo at the center of the global
            // map view
            QPointF viewCenter =
                mainWindow->globalMapView_->mapToScene(
                    mainWindow->globalMapView_->viewport()
                        ->rect()
                        .center());

            double lat, lon;
            auto   wgsPoint =
                mainWindow->regionView_->sceneToWGS84(
                    viewCenter);
            lon = wgsPoint.x();
            lat = wgsPoint.y();
            background->getProperties()["Latitude"] =
                QString::number(lat, 'f', 6);
            background->getProperties()["Longitude"] =
                QString::number(lon, 'f', 6);
            background->setPos(viewCenter);

            mainWindow->globalMapScene_->addItemWithId(
                background, background->getID());
            CargoNetSim::CargoNetSimController::
                getInstance()
                    .getRegionDataController()
                    ->setGlobalVariable(
                        "globalBackgroundPhotoItem",
                        QVariant::fromValue(background));
        }
    }
    catch (const std::exception &e)
    {
        qWarning() << "Error in addBackgroundPhoto:"
                   << e.what();
        QMessageBox::warning(
            mainWindow, "Error",
            QString("Failed to add background photo: %1")
                .arg(e.what()));
    }
}

bool CargoNetSim::GUI::ViewController::
    checkExistingConnection(MainWindow    *mainWindow,
                            QGraphicsItem *startItem,
                            QGraphicsItem *endItem,
                            const QString &connectionType)
{
    // Early validation check
    if (!startItem || !endItem || !mainWindow)
    {
        return false;
    }

    // Determine which scene to use based on item types
    QList<ConnectionLine *> viewConnectionLines;

    const bool isRegionStart =
        dynamic_cast<TerminalItem *>(startItem) != nullptr;
    const bool isRegionEnd =
        dynamic_cast<TerminalItem *>(endItem) != nullptr;

    if (isRegionStart && isRegionEnd)
    {
        // Both are region items
        viewConnectionLines =
            mainWindow->regionScene_
                ->getItemsByType<ConnectionLine>();
    }
    else
    {
        const bool isGlobalStart =
            dynamic_cast<GlobalTerminalItem *>(startItem)
            != nullptr;
        const bool isGlobalEnd =
            dynamic_cast<GlobalTerminalItem *>(endItem)
            != nullptr;

        if (isGlobalStart && isGlobalEnd)
        {
            // Both are global items
            viewConnectionLines =
                mainWindow->globalMapView_->getScene()
                    ->getItemsByType<ConnectionLine>();
        }
        else
        {
            // Mixed types or unrecognized types
            return false;
        }
    }

    if (viewConnectionLines.empty())
    {
        return false;
    }

    // Look for matching connection
    for (const ConnectionLine *line : viewConnectionLines)
    {
        // Check connection type first as it's likely faster
        // than pointer comparison
        if (line->connectionType() != connectionType)
        {
            continue;
        }

        // Then check if terminals match (in either
        // direction)
        if ((line->startItem() == startItem
             && line->endItem() == endItem)
            || (line->startItem() == endItem
                && line->endItem() == startItem))
        {
            return true;
        }
    }

    return false;
}

CargoNetSim::GUI::ConnectionLine *
CargoNetSim::GUI::ViewController::createConnectionLine(
    MainWindow *mainWindow, QGraphicsItem *startItem,
    QGraphicsItem *endItem, const QString &connectionType)
{
    if (!mainWindow || !startItem || !endItem)
    {
        return nullptr; // Early validation check
    }

    // Check if a connection of the same type already
    // exists between the terminals
    if (CargoNetSim::GUI::ViewController::
            checkExistingConnection(mainWindow, startItem,
                                    endItem,
                                    connectionType))
    {
        // mainWindow->showStatusBarError(
        //     "A connection of this type already exists.",
        //     3000);
        return nullptr;
    }

    auto SP = dynamic_cast<TerminalItem *>(startItem);
    auto EP = dynamic_cast<TerminalItem *>(endItem);

    if (SP && EP && SP->getRegion() == EP->getRegion())
    {
        auto line = new ConnectionLine(
            SP, EP, connectionType, {}, SP->getRegion());
        mainWindow->regionScene_->addItemWithId(
            line, line->getID());

        // Connect the clicked signal to update properties
        // panel
        QObject::connect(
            line, &ConnectionLine::clicked,
            [mainWindow](ConnectionLine *line) {
                UtilitiesFunctions::updatePropertiesPanel(
                    mainWindow, line);
            });

        return line;
    }
    else if (SP && EP && SP->getRegion() != EP->getRegion())
    {
        mainWindow->showStatusBarError(
            "Cannot create a connection between two "
            "different regions in region view.",
            3000);
        return nullptr;
    }
    else if (!SP && !EP)
    {
        auto SPG =
            dynamic_cast<GlobalTerminalItem *>(startItem);
        auto EPG =
            dynamic_cast<GlobalTerminalItem *>(endItem);

        if (SPG && EPG && SPG != EPG)
        {
            if (SPG->getLinkedTerminalItem()->getRegion()
                == EPG->getLinkedTerminalItem()
                       ->getRegion())
            {
                mainWindow->showStatusBarError(
                    "Cannot link terminals in the same "
                    "region in global map.",
                    3000);
                return nullptr;
            }

            // Create the connection line and add it to the
            // scene
            auto line = new ConnectionLine(
                SPG, EPG, connectionType, {}, "Global");
            mainWindow->globalMapScene_->addItemWithId(
                line, line->getID());

            // Connect the clicked signal to update
            // properties panel
            QObject::connect(
                line, &ConnectionLine::clicked,
                [mainWindow](ConnectionLine *line) {
                    UtilitiesFunctions::
                        updatePropertiesPanel(mainWindow,
                                              line);
                });

            return line;
        }
        else if (SPG && EPG && SPG == EPG)
        {
            mainWindow->showStatusBarError(
                "Cannot link a terminal to itself.", 3000);
        }
    }

    return nullptr;
}

bool CargoNetSim::GUI::ViewController::removeConnectionLine(
    MainWindow                       *mainWindow,
    CargoNetSim::GUI::ConnectionLine *connectionLine)
{
    // Early validation check
    if (!mainWindow || !connectionLine)
    {
        return false;
    }

    try
    {
        // Determine which scene the connection belongs to
        GraphicsScene *scene = nullptr;

        // Check if it's a connection in the region view
        if (connectionLine->getRegion() != "Global")
        {
            scene = mainWindow->regionScene_;
        }
        else
        {
            // It's a global connection
            scene = mainWindow->globalMapScene_;
        }

        if (!scene)
        {
            return false;
        }

        // Get the connection ID
        QString connectionID = connectionLine->getID();

        // Remove the item from the scene
        if (scene->removeItemWithId<ConnectionLine>(
                connectionID))
        {
            // Show success message
            mainWindow->showStatusBarMessage(
                QString("Connection removed successfully."),
                2000);

            // If the connection was being displayed in the
            // properties panel, clear it
            if (mainWindow->propertiesPanel_
                    ->getCurrentItem()
                == connectionLine)
            {
                UtilitiesFunctions::hidePropertiesPanel(
                    mainWindow);
            }

            return true;
        }
        else
        {
            // If removal failed, show error
            mainWindow->showStatusBarError(
                QString("Failed to remove connection."),
                3000);
            return false;
        }
    }
    catch (const std::exception &e)
    {
        // Handle any exceptions that might occur
        qWarning() << "Error removing connection line:"
                   << e.what();
        mainWindow->showStatusBarError(
            QString("Error removing connection: %1")
                .arg(e.what()),
            3000);
        return false;
    }
}

void CargoNetSim::GUI::ViewController::
    connectVisibleTerminalsByNetworks(
        MainWindow *mainWindow)
{
    if (!mainWindow)
    {
        return;
    }

    auto vehicleController =
        CargoNetSim::CargoNetSimController::getInstance()
            .getVehicleController();

    if (vehicleController->getAllShips().isEmpty())
    {
        mainWindow->showStatusBarError(
            "No ships available! Load ships first!", 3000);
        return;
    }
    if (vehicleController->getAllTrains().isEmpty())
    {
        mainWindow->showStatusBarError(
            "No trains available! Load trains first!",
            3000);
        return;
    }

    // Store the current button states and disable all
    ToolbarController::storeButtonStates(mainWindow);
    ToolbarController::disableAllButtons(mainWindow);
    mainWindow->startStatusProgress();

    // Process events to keep UI responsive
    QApplication::processEvents();

    // Check if the region view is active
    bool isGlobalView = mainWindow->isGlobalViewActive();
    GraphicsScene *currentScene =
        isGlobalView ? mainWindow->globalMapScene_
                     : mainWindow->regionScene_;

    QString                     currentRegion = "";
    QList<TerminalItem *>       terminals;
    QList<GlobalTerminalItem *> globalTerminals;
    QSet<QString>               visibleTerminalTypes;
    QSet<QString>               availableNetworks;

    TerminalItem *originTerminal =
        UtilitiesFunctions::getOriginTerminal(mainWindow);
    if (!originTerminal)
    {
        mainWindow->showStatusBarError(
            "Origin is not present in the region view!",
            3000);

        // Restore toolbar button states
        ToolbarController::restoreButtonStates(mainWindow);
        mainWindow->stopStatusProgress();
        return;
    }

    QVariant containersVar =
        originTerminal->getProperty("Containers");
    if (containersVar.canConvert<
            QList<ContainerCore::Container *>>())
    {
        QList<ContainerCore::Container *> containers =
            containersVar
                .value<QList<ContainerCore::Container *>>();
        if (containers.empty())
        {
            mainWindow->showStatusBarError(
                "No containers at origin!", 3000);
            ToolbarController::restoreButtonStates(
                mainWindow);
            mainWindow->stopStatusProgress();
            return; // Early return if no containers
        }
    }
    else
    {
        // Early return if container property doesn't
        // convert
        mainWindow->showStatusBarError(
            "Invalid container format at origin!", 3000);

        // Restore toolbar button states
        ToolbarController::restoreButtonStates(mainWindow);
        mainWindow->stopStatusProgress();
        return;
    }

    // Process events to keep UI responsive
    QApplication::processEvents();

    if (isGlobalView)
    {
        globalTerminals = CargoNetSim::GUI::
            UtilitiesFunctions::getGlobalTerminalItems(
                mainWindow->globalMapScene_, "*", "*",
                UtilitiesFunctions::ConnectionType::Any,
                UtilitiesFunctions::LinkType::Any);

        // Collect terminal types
        for (auto terminal : globalTerminals)
        {
            if (terminal->getLinkedTerminalItem())
            {
                visibleTerminalTypes.insert(
                    terminal->getLinkedTerminalItem()
                        ->getTerminalType());
            }
        }

        // Add Ship network type
        availableNetworks.insert("Ship");
    }
    else
    {
        currentRegion = CargoNetSim::CargoNetSimController::
                            getInstance()
                                .getRegionDataController()
                                ->getCurrentRegion();
        terminals = CargoNetSim::GUI::UtilitiesFunctions::
            getTerminalItems(
                mainWindow->regionScene_, currentRegion,
                "*",
                UtilitiesFunctions::ConnectionType::Any,
                UtilitiesFunctions::LinkType::Any);

        // Collect terminal types
        for (auto terminal : terminals)
        {
            visibleTerminalTypes.insert(
                terminal->getTerminalType());
        }

        // Process events to keep UI responsive
        QApplication::processEvents();

        // Check available networks in the current region
        auto regionData =
            CargoNetSim::CargoNetSimController::
                getInstance()
                    .getRegionDataController()
                    ->getCurrentRegionData();

        if (regionData)
        {
            if (!regionData->getTrainNetworks().isEmpty())
            {
                availableNetworks.insert("Rail");
            }

            if (!regionData->getTruckNetworks().isEmpty())
            {
                availableNetworks.insert("Truck");
            }
        }
    }

    if ((terminals.empty() && !isGlobalView)
        || (globalTerminals.empty() && isGlobalView))
    {
        QString msgHndler =
            isGlobalView ? "view" : "region";
        QString mssg = QString("There is no terminal "
                               "in the current %1")
                           .arg(msgHndler);
        mainWindow->showStatusBarError(mssg, 3000);

        // Restore toolbar button states
        ToolbarController::restoreButtonStates(mainWindow);
        mainWindow->stopStatusProgress();
        return;
    }
    else if ((terminals.size() == 1 && !isGlobalView)
             || (globalTerminals.size() == 1
                 && isGlobalView))
    {
        QString msgHndler =
            isGlobalView ? "view" : "region";
        QString mssg = QString("There is only one terminal "
                               "in the current %1.")
                           .arg(msgHndler);
        mainWindow->showStatusBarError(mssg, 3000);

        // Restore toolbar button states
        ToolbarController::restoreButtonStates(mainWindow);
        mainWindow->stopStatusProgress();
        return;
    }

    // If no available networks, show error and return
    if (availableNetworks.isEmpty())
    {
        mainWindow->showStatusBarError(
            "No available network types found for "
            "connecting terminals.",
            3000);

        // Restore toolbar button states
        ToolbarController::restoreButtonStates(mainWindow);
        mainWindow->stopStatusProgress();
        return;
    }

    // Process events to keep UI responsive
    QApplication::processEvents();

    // Show network selection dialog with NetworkSelection
    // type
    InterfaceSelectionDialog dialog(
        availableNetworks, visibleTerminalTypes,
        InterfaceSelectionDialog::NetworkSelection,
        mainWindow);

    if (dialog.exec() != QDialog::Accepted)
    {

        // Restore toolbar button states
        ToolbarController::restoreButtonStates(mainWindow);
        mainWindow->stopStatusProgress();
        return; // User cancelled
    }

    // Get selected networks using the new method
    QList<QString> selectedNetworks =
        dialog.getSelectedNetworkTypes();

    // Get included terminal types
    QMap<QString, bool> includedTerminalTypes =
        dialog.getIncludedTerminalTypes();

    if (selectedNetworks.isEmpty())
    {
        mainWindow->showStatusBarMessage(
            "No network types selected for connection.",
            3000);

        // Restore toolbar button states
        ToolbarController::restoreButtonStates(mainWindow);
        mainWindow->stopStatusProgress();
        return;
    }

    // Check if any terminal types are selected
    bool anyTerminalTypeSelected = false;
    for (auto it = includedTerminalTypes.begin();
         it != includedTerminalTypes.end(); ++it)
    {
        if (it.value())
        {
            anyTerminalTypeSelected = true;
            break;
        }
    }

    if (!anyTerminalTypeSelected)
    {
        mainWindow->showStatusBarMessage(
            "No terminal types selected for connection.",
            3000);

        // Restore toolbar button states
        ToolbarController::restoreButtonStates(mainWindow);
        mainWindow->stopStatusProgress();
        return;
    }

    // Process events to keep UI responsive
    QApplication::processEvents();

    bool anyConnectionCreated = false;
    bool errorOccurred        = false;
    int  processCount =
        0; // Counter to determine when to process events

    if (!isGlobalView)
    {
        for (auto &sourceTerminal : terminals)
        {
            // Skip if terminal type is not included
            if (!includedTerminalTypes.value(
                    sourceTerminal->getTerminalType(),
                    true))
            {
                continue;
            }

            for (auto &targetTerminal : terminals)
            {
                if (sourceTerminal == targetTerminal)
                {
                    continue;
                }

                // Skip if terminal type is not included
                if (!includedTerminalTypes.value(
                        targetTerminal->getTerminalType(),
                        true))
                {
                    continue;
                }

                // Process events periodically to keep UI
                // responsive
                if (++processCount % 10
                    == 0) // Process every 10 iterations
                {
                    QApplication::processEvents();
                }

                QList<QString> commonModes =
                    UtilitiesFunctions::getCommonModes(
                        sourceTerminal, targetTerminal);

                // Process Rail connections in region view
                // if selected
                if (selectedNetworks.contains("Rail")
                    && commonModes.contains("Rail")
                    && !CargoNetSim::CargoNetSimController::
                            getInstance()
                                .getRegionDataController()
                                ->getCurrentRegionData()
                                ->getTrainNetworks()
                                .isEmpty())
                {
                    bool isConnected = UtilitiesFunctions::
                        processNetworkModeConnection(
                            mainWindow, sourceTerminal,
                            targetTerminal,
                            NetworkType::Train);
                    if (isConnected)
                    {
                        anyConnectionCreated = true;
                    }
                }

                // Process Truck connections in region view
                // if selected
                if (selectedNetworks.contains("Truck")
                    && commonModes.contains("Truck")
                    && !CargoNetSim::CargoNetSimController::
                            getInstance()
                                .getRegionDataController()
                                ->getCurrentRegionData()
                                ->getTruckNetworks()
                                .isEmpty())
                {
                    bool isConnected = UtilitiesFunctions::
                        processNetworkModeConnection(
                            mainWindow, sourceTerminal,
                            targetTerminal,
                            NetworkType::Truck);
                    if (isConnected)
                    {
                        anyConnectionCreated = true;
                    }
                }
            }
        }
    }
    else // Global view
    {
        for (auto &sourceTerminal : globalTerminals)
        {
            // Skip if terminal type is not included
            TerminalItem *sourceLinkedTerminal =
                sourceTerminal->getLinkedTerminalItem();
            if (!sourceLinkedTerminal
                || !includedTerminalTypes.value(
                    sourceLinkedTerminal->getTerminalType(),
                    true))
            {
                continue;
            }

            // Break outer loop if error occurred
            if (errorOccurred)
                break;

            for (auto &targetTerminal : globalTerminals)
            {
                // Skip if terminal type is not included
                TerminalItem *targetLinkedTerminal =
                    targetTerminal->getLinkedTerminalItem();
                if (!targetLinkedTerminal
                    || !includedTerminalTypes.value(
                        targetLinkedTerminal
                            ->getTerminalType(),
                        true))
                {
                    continue;
                }

                // Break inner loop if error occurred
                if (errorOccurred)
                    break;

                if (sourceTerminal == targetTerminal)
                {
                    continue;
                }

                // Process events periodically to keep UI
                // responsive
                if (++processCount % 10
                    == 0) // Process every 10 iterations
                {
                    QApplication::processEvents();
                }

                QList<QString> commonModes =
                    UtilitiesFunctions::getCommonModes(
                        sourceTerminal, targetTerminal);

                for (const QString &mode : commonModes)
                {
                    // Break mode loop if error occurred
                    if (errorOccurred)
                        break;

                    // Skip if this network type wasn't
                    // selected
                    if (!selectedNetworks.contains(mode))
                    {
                        continue;
                    }

                    QString connectionType;
                    if (mode.toLower() == "ship")
                        connectionType = "Ship";

                    if (!connectionType.isEmpty())
                    {
                        auto connectionLine =
                            ViewController::
                                createConnectionLine(
                                    mainWindow,
                                    sourceTerminal,
                                    targetTerminal,
                                    connectionType);
                        if (connectionLine)
                        {
                            // Set connection properties
                            // calculate them on the fly as
                            // if there is a shortest path
                            // between the terminals
                            CargoNetSim::Backend::
                                ShortestPathResult result;

                            // Get geographic coordinates
                            // for both terminals
                            QPointF sourceGeoPoint =
                                mainWindow->globalMapView_
                                    ->sceneToWGS84(
                                        sourceTerminal
                                            ->pos());
                            QPointF targetGeoPoint =
                                mainWindow->globalMapView_
                                    ->sceneToWGS84(
                                        targetTerminal
                                            ->pos());

                            // Calculate the distance using
                            // geographic coordinates
                            result.totalLength =
                                UtilitiesFunctions::
                                    getApproximateGeoDistance(
                                        sourceGeoPoint,
                                        targetGeoPoint);
                            result.optimizationCriterion =
                                "distance";

                            NetworkType networkType =
                                NetworkType::Ship;

                            bool propertiesSet =
                                UtilitiesFunctions::
                                    setConnectionProperties(
                                        mainWindow,
                                        connectionLine,
                                        result,
                                        networkType);
                            if (!propertiesSet)
                            {
                                CargoNetSim::GUI::
                                    ViewController::
                                        removeConnectionLine(
                                            mainWindow,
                                            connectionLine);
                                // Set flag to exit all
                                // loops
                                errorOccurred = true;
                                break;
                            }

                            anyConnectionCreated = true;
                        }
                    }
                }
            }

            // Process events for each source terminal to
            // ensure responsiveness
            QApplication::processEvents();
        }
    }

    // Final process events before showing result message
    QApplication::processEvents();

    if (anyConnectionCreated)
    {
        mainWindow->showStatusBarMessage(
            "Terminal connections created based on "
            "selected networks and terminal types.");
    }
    else if (!errorOccurred)
    {
        mainWindow->showStatusBarMessage(
            "No new connections were created.", 3000);
    }

    // Restore toolbar button states
    ToolbarController::restoreButtonStates(mainWindow);
    mainWindow->stopStatusProgress();
}

void CargoNetSim::GUI::ViewController::
    connectVisibleTerminalsByInterfaces(
        MainWindow *mainWindow)
{
    if (!mainWindow)
    {
        return;
    }

    // Store the current button states and disable all
    ToolbarController::storeButtonStates(mainWindow);
    ToolbarController::disableAllButtons(mainWindow);
    mainWindow->startStatusProgress();

    // Process events to keep UI responsive
    QApplication::processEvents();

    // Get the current view and scene based on which tab is
    // active
    bool isGlobalView =
        mainWindow->tabWidget_->currentIndex() == 0 ? false
                                                    : true;
    GraphicsScene *currentScene =
        isGlobalView ? mainWindow->globalMapScene_
                     : mainWindow->regionScene_;

    // Get all visible terminals in the current view
    QList<QGraphicsItem *> visibleTerminals;

    // Track which terminal types are present in the current
    // view
    QSet<QString> visibleTerminalTypes;

    // Process events to keep UI responsive
    QApplication::processEvents();

    if (isGlobalView)
    {
        QList<GlobalTerminalItem *> allTerminals =
            currentScene
                ->getItemsByType<GlobalTerminalItem>();
        // Filter visible GlobalTerminalItems
        for (auto terminal : allTerminals)
        {
            if (terminal && terminal->isVisible())
            {
                visibleTerminals.append(terminal);

                // Get the terminal type
                TerminalItem *linkedTerminal =
                    terminal->getLinkedTerminalItem();
                if (linkedTerminal)
                {
                    visibleTerminalTypes.insert(
                        linkedTerminal->getTerminalType());
                }
            }
        }
    }
    else
    {
        QList<TerminalItem *> allTerminals =
            currentScene->getItemsByType<TerminalItem>();
        // Filter visible TerminalItems in current region
        QString currentRegion =
            CargoNetSim::CargoNetSimController::
                getInstance()
                    .getRegionDataController()
                    ->getCurrentRegion();

        for (auto terminal : allTerminals)
        {
            if (terminal && terminal->isVisible()
                && terminal->getRegion() == currentRegion)
            {
                visibleTerminals.append(terminal);
                visibleTerminalTypes.insert(
                    terminal->getTerminalType());
            }
        }
    }

    // Process events to keep UI responsive
    QApplication::processEvents();

    if (visibleTerminals.empty())
    {
        mainWindow->showStatusBarError(
            "No visible terminals found in the current "
            "view.",
            3000);

        // Restore toolbar button states
        ToolbarController::restoreButtonStates(mainWindow);
        mainWindow->stopStatusProgress();
        return;
    }

    // Find all available common interfaces
    QSet<QString> availableInterfaces;
    int           processCount =
        0; // Counter to determine when to process events

    for (int i = 0; i < visibleTerminals.size(); ++i)
    {
        for (int j = i + 1; j < visibleTerminals.size();
             ++j)
        {
            // Process events periodically to keep UI
            // responsive
            if (++processCount % 10
                == 0) // Process every 10 iterations
            {
                QApplication::processEvents();
            }

            // Get the source and target terminals
            QGraphicsItem *sourceItem = visibleTerminals[i];
            QGraphicsItem *targetItem = visibleTerminals[j];

            // Get common modes
            QList<QString> commonModes =
                UtilitiesFunctions::getCommonModes(
                    sourceItem, targetItem);
            for (const QString &mode : commonModes)
            {
                if (!mode.isEmpty())
                {
                    availableInterfaces.insert(mode);
                }
            }
        }
    }

    // Process events to keep UI responsive
    QApplication::processEvents();

    // If no common interfaces, show error and return
    if (availableInterfaces.isEmpty())
    {
        mainWindow->showStatusBarError(
            "No common interfaces found between terminals.",
            3000);

        // Restore toolbar button states
        ToolbarController::restoreButtonStates(mainWindow);
        mainWindow->stopStatusProgress();
        return;
    }

    // Show interface selection dialog with only visible
    // terminal types
    InterfaceSelectionDialog dialog(
        availableInterfaces, visibleTerminalTypes,
        InterfaceSelectionDialog::InterfaceSelection,
        mainWindow);
    if (dialog.exec() != QDialog::Accepted)
    {
        // Restore toolbar button states
        ToolbarController::restoreButtonStates(mainWindow);
        mainWindow->stopStatusProgress();
        return; // User cancelled
    }

    // Get selected interfaces
    QList<QString> selectedInterfaces =
        dialog.getSelectedInterfaces();

    // Get included terminal types
    QMap<QString, bool> includedTerminalTypes =
        dialog.getIncludedTerminalTypes();

    // Get coordinate distance flag
    bool useCoordinateDistance =
        dialog.useCoordinateDistance();

    if (selectedInterfaces.isEmpty())
    {
        mainWindow->showStatusBarMessage(
            "No interfaces selected for connection.", 3000);

        // Restore toolbar button states
        ToolbarController::restoreButtonStates(mainWindow);
        mainWindow->stopStatusProgress();
        return;
    }

    // Check if any terminal types are selected
    bool anyTerminalTypeSelected = false;
    for (auto it = includedTerminalTypes.begin();
         it != includedTerminalTypes.end(); ++it)
    {
        if (it.value())
        {
            anyTerminalTypeSelected = true;
            break;
        }
    }

    if (!anyTerminalTypeSelected)
    {
        mainWindow->showStatusBarMessage(
            "No terminal types selected for connection.",
            3000);

        // Restore toolbar button states
        ToolbarController::restoreButtonStates(mainWindow);
        mainWindow->stopStatusProgress();
        return;
    }

    // If using coordinate distance, perform additional
    // checks
    if (useCoordinateDistance)
    {
        // Check origin terminal has containers
        TerminalItem *originTerminal =
            UtilitiesFunctions::getOriginTerminal(
                mainWindow);
        int containerCount = 0;

        if (originTerminal)
        {
            QVariant containersVar =
                originTerminal->getProperty("Containers");
            if (containersVar.canConvert<
                    QList<ContainerCore::Container *>>())
            {
                QList<ContainerCore::Container *>
                    containers = containersVar.value<QList<
                        ContainerCore::Container *>>();
                containerCount = containers.size();
            }
        }

        if (!originTerminal)
        {
            mainWindow->showStatusBarError(
                "No origin terminal is found in the map!",
                3000);

            // Restore toolbar button states
            ToolbarController::restoreButtonStates(
                mainWindow);
            mainWindow->stopStatusProgress();
            return;
        }
        if (containerCount == 0)
        {
            mainWindow->showStatusBarError(
                "No containers are found in the origin "
                "terminal!",
                3000);

            // Restore toolbar button states
            ToolbarController::restoreButtonStates(
                mainWindow);
            mainWindow->stopStatusProgress();
            return;
        }

        // Check vehicles are imported
        auto vehicleController =
            CargoNetSim::CargoNetSimController::
                getInstance()
                    .getVehicleController();

        if (vehicleController->getAllShips().isEmpty())
        {
            mainWindow->showStatusBarError(
                "No ships available! Load ships first.",
                3000);

            // Restore toolbar button states
            ToolbarController::restoreButtonStates(
                mainWindow);
            mainWindow->stopStatusProgress();
            return;
        }

        if (vehicleController->getAllTrains().isEmpty())
        {
            mainWindow->showStatusBarError(
                "No trains available! Load trains first.",
                3000);

            // Restore toolbar button states
            ToolbarController::restoreButtonStates(
                mainWindow);
            mainWindow->stopStatusProgress();
            return;
        }
    }

    // Process events to keep UI responsive
    QApplication::processEvents();

    // Connect terminals based on selected interfaces
    int connectionsCreated = 0;
    processCount           = 0; // Reset counter

    for (int i = 0; i < visibleTerminals.size(); ++i)
    {
        for (int j = i + 1; j < visibleTerminals.size();
             ++j)
        {
            // Skip if the same terminal
            if (i == j)
            {
                continue;
            }

            // Process events periodically to keep UI
            // responsive
            if (++processCount % 10
                == 0) // Process every 10 iterations
            {
                QApplication::processEvents();
            }

            // Get the source and target terminals
            QGraphicsItem *sourceItem = visibleTerminals[i];
            QGraphicsItem *targetItem = visibleTerminals[j];

            QList<QString> commonModes =
                UtilitiesFunctions::getCommonModes(
                    sourceItem, targetItem);

            // Check if the terminal types are included
            bool    skipConnection = false;
            QString sourceType;
            QString targetType;

            // Get source terminal type
            if (TerminalItem *source =
                    qgraphicsitem_cast<TerminalItem *>(
                        sourceItem))
            {
                sourceType = source->getTerminalType();
            }
            else if (GlobalTerminalItem *globalSource =
                         qgraphicsitem_cast<
                             GlobalTerminalItem *>(
                             sourceItem))
            {
                TerminalItem *linkedSource =
                    globalSource->getLinkedTerminalItem();
                if (linkedSource)
                {
                    sourceType =
                        linkedSource->getTerminalType();
                }
            }

            // Get target terminal type
            if (TerminalItem *target =
                    qgraphicsitem_cast<TerminalItem *>(
                        targetItem))
            {
                targetType = target->getTerminalType();
            }
            else if (GlobalTerminalItem *globalTarget =
                         qgraphicsitem_cast<
                             GlobalTerminalItem *>(
                             targetItem))
            {
                TerminalItem *linkedTarget =
                    globalTarget->getLinkedTerminalItem();
                if (linkedTarget)
                {
                    targetType =
                        linkedTarget->getTerminalType();
                }
            }

            // Skip if either terminal type is not included
            if ((!sourceType.isEmpty()
                 && !includedTerminalTypes.value(sourceType,
                                                 true))
                || (!targetType.isEmpty()
                    && !includedTerminalTypes.value(
                        targetType, true)))
            {
                skipConnection = true;
            }

            if (!skipConnection)
            {
                // Create connections for each selected
                // common mode
                for (const QString &mode : commonModes)
                {
                    if (!mode.isEmpty()
                        && selectedInterfaces.contains(
                            mode))
                    {
                        ConnectionLine *connection =
                            ViewController::
                                createConnectionLine(
                                    mainWindow, sourceItem,
                                    targetItem, mode);
                        if (connection)
                        {
                            // If using coordinate distance,
                            // set properties based on
                            // geographic distance
                            if (useCoordinateDistance)
                            {
                                // Get geographic
                                // coordinates for both
                                // terminals
                                QPointF sourcePos;
                                QPointF targetPos;

                                if (isGlobalView)
                                {
                                    // In global view, get
                                    // geographic
                                    // coordinates directly
                                    sourcePos =
                                        mainWindow
                                            ->globalMapView_
                                            ->sceneToWGS84(
                                                sourceItem
                                                    ->pos());
                                    targetPos =
                                        mainWindow
                                            ->globalMapView_
                                            ->sceneToWGS84(
                                                targetItem
                                                    ->pos());
                                }
                                else
                                {
                                    // In region view, get
                                    // geographic
                                    // coordinates based on
                                    // current coordinate
                                    // system
                                    sourcePos =
                                        mainWindow
                                            ->regionView_
                                            ->sceneToWGS84(
                                                sourceItem
                                                    ->pos());
                                    targetPos =
                                        mainWindow
                                            ->regionView_
                                            ->sceneToWGS84(
                                                targetItem
                                                    ->pos());
                                }

                                // Calculate distance
                                double distanceMeters =
                                    UtilitiesFunctions::
                                        getApproximateGeoDistance(
                                            sourcePos,
                                            targetPos);

                                // Set up a
                                // ShortestPathResult to
                                // pass to
                                // setConnectionProperties
                                CargoNetSim::Backend::
                                    ShortestPathResult
                                        result;
                                result.totalLength =
                                    distanceMeters;
                                result
                                    .optimizationCriterion =
                                    "distance";

                                // Determine network type
                                // based on connection mode
                                NetworkType networkType;
                                if (mode == "Rail")
                                {
                                    networkType =
                                        NetworkType::Train;
                                }
                                else if (mode == "Truck")
                                {
                                    networkType =
                                        NetworkType::Truck;
                                }
                                else if (mode == "Ship")
                                {
                                    networkType =
                                        NetworkType::Ship;
                                }
                                else
                                {
                                    // Default to ship for
                                    // any other mode
                                    networkType =
                                        NetworkType::Ship;
                                }

                                // Set connection properties
                                UtilitiesFunctions::
                                    setConnectionProperties(
                                        mainWindow,
                                        connection, result,
                                        networkType, false);
                            }

                            connectionsCreated++;
                        }
                    }
                }
            }
        }

        // Process events after processing each source
        // terminal
        QApplication::processEvents();
    }

    // Process events before showing final message
    QApplication::processEvents();

    if (connectionsCreated > 0)
    {
        mainWindow->showStatusBarMessage(
            QString("Created %1 terminal connections based "
                    "on selected interfaces.")
                .arg(connectionsCreated),
            3000);
    }
    else
    {
        mainWindow->showStatusBarMessage(
            "No new connections were created.", 3000);
    }

    // Restore toolbar button states
    ToolbarController::restoreButtonStates(mainWindow);
    mainWindow->stopStatusProgress();
}

CargoNetSim::GUI::RegionCenterPoint *
CargoNetSim::GUI::ViewController::createRegionCenter(
    MainWindow *mainWindow, const QString &regionName,
    const QColor &color, const QPointF pos,
    const bool keepVisible)
{
    RegionCenterPoint *centerPoint =
        new RegionCenterPoint(regionName, color);
    QObject::connect(
        centerPoint, &RegionCenterPoint::clicked,
        [mainWindow](RegionCenterPoint *item) {
            UtilitiesFunctions::updatePropertiesPanel(
                mainWindow, item);
        });

    // Add position change connection
    QObject::connect(
        centerPoint, &RegionCenterPoint::coordinatesChanged,
        [regionName, mainWindow](QPointF newGeopoint) {
            PropertiesPanel *propertiesPanel =
                mainWindow->propertiesPanel_;
            UtilitiesFunctions::updateGlobalMapForRegion(
                mainWindow, regionName);
            propertiesPanel->updateCoordinateFields(
                newGeopoint);
        });

    QObject::connect(
        centerPoint, &RegionCenterPoint::propertiesChanged,
        [regionName, mainWindow]() {
            UtilitiesFunctions::updateGlobalMapForRegion(
                mainWindow, regionName);
        });

    centerPoint->setPos(pos);
    mainWindow->regionScene_->addItemWithId(
        centerPoint, centerPoint->getID());
    CargoNetSim::CargoNetSimController::getInstance()
        .getRegionDataController()
        ->setRegionVariable(
            regionName, "regionCenterPoint",
            QVariant::fromValue(centerPoint));

    // update visibility
    centerPoint->setVisible(keepVisible);
    return centerPoint;
}

bool CargoNetSim::GUI::ViewController::
    unlinkTerminalFromNetworkPoints(
        MainWindow *mainWindow, TerminalItem *terminal,
        const QList<NetworkType> &networkTypes)
{
    if (!mainWindow || !terminal || networkTypes.isEmpty())
    {
        return false;
    }

    QString           region = terminal->getRegion();
    QList<MapPoint *> linkedPoints;

    // Get all map points in the scene
    QList<MapPoint *> allMapPoints =
        mainWindow->regionScene_
            ->getItemsByType<MapPoint>();

    // Find map points linked to this terminal with matching
    // network types
    for (MapPoint *point : allMapPoints)
    {
        if (point->getRegion() != region
            || point->getLinkedTerminal() != terminal)
        {
            continue;
        }

        QObject *network = point->getReferenceNetwork();
        if (!network)
        {
            continue;
        }

        bool    isMatchingType = false;
        QString networkTypeStr = "unknown";

        if (networkTypes.contains(NetworkType::Train))
        {
            if (dynamic_cast<
                    Backend::TrainClient::NeTrainSimNetwork
                        *>(network))
            {
                isMatchingType = true;
                networkTypeStr = "train";
            }
        }

        if (networkTypes.contains(NetworkType::Truck))
        {
            if (dynamic_cast<
                    Backend::TruckClient::IntegrationNetwork
                        *>(network))
            {
                isMatchingType = true;
                networkTypeStr = "truck";
            }
        }

        if (isMatchingType)
        {
            linkedPoints.append(point);
        }
    }

    if (linkedPoints.isEmpty())
    {
        // Determine which network types were selected for
        // more informative message
        QStringList typeNames;
        if (networkTypes.contains(NetworkType::Train))
            typeNames.append("train");
        if (networkTypes.contains(NetworkType::Truck))
            typeNames.append("truck");

        QString typesStr = typeNames.join(" or ");

        mainWindow->showStatusBarError(
            QString("Terminal '%1' is not linked to any %2 "
                    "network points.")
                .arg(terminal->getProperty("Name")
                         .toString())
                .arg(typesStr),
            3000);
        return false;
    }

    // Unlink all matching points
    int unlinkCount = 0;
    for (MapPoint *point : linkedPoints)
    {
        // Get network info for the message
        QString  networkName    = "Unknown Network";
        QString  networkTypeStr = "unknown";
        QObject *network = point->getReferenceNetwork();

        if (auto trainNet = dynamic_cast<
                Backend::TrainClient::NeTrainSimNetwork *>(
                network))
        {
            networkName    = trainNet->getNetworkName();
            networkTypeStr = "train";
        }
        else if (auto truckNet =
                     dynamic_cast<Backend::TruckClient::
                                      IntegrationNetwork *>(
                         network))
        {
            networkName    = truckNet->getNetworkName();
            networkTypeStr = "truck";
        }

        // Unlink the terminal from the point
        point->setLinkedTerminal(nullptr);
        unlinkCount++;

        // Show detailed message for each unlink operation
        mainWindow->showStatusBarMessage(
            QString("Terminal '%1' unlinked from %2 "
                    "network '%3'")
                .arg(terminal->getProperty("Name")
                         .toString())
                .arg(networkTypeStr)
                .arg(networkName),
            1500); // Shorter display time since there might
                   // be multiple messages
    }

    // Show summary message
    if (unlinkCount > 0)
    {
        mainWindow->showStatusBarMessage(
            QString("Successfully unlinked terminal '%1' "
                    "from %2 network point(s)")
                .arg(terminal->getProperty("Name")
                         .toString())
                .arg(unlinkCount),
            3000);

        // Update the properties panel if this item is
        // currently selected
        if (mainWindow->propertiesPanel_->getCurrentItem()
            == terminal)
        {
            mainWindow->propertiesPanel_->displayProperties(
                terminal);
        }

        return true;
    }

    return false;
}

void CargoNetSim::GUI::ViewController::
    unlinkAllVisibleTerminalsToNetwork(
        MainWindow               *mainWindow,
        const QList<NetworkType> &networkTypes)
{
    if (!mainWindow || networkTypes.isEmpty())
    {
        mainWindow->showStatusBarError(
            "No network types selected for unlinking.",
            3000);
        return;
    }

    // Store the current button states and disable all
    ToolbarController::storeButtonStates(mainWindow);
    ToolbarController::disableAllButtons(mainWindow);
    mainWindow->startStatusProgress();

    // Get the current scene
    auto scene = mainWindow->regionScene_;
    if (!scene)
    {
        // Restore toolbar button states
        ToolbarController::restoreButtonStates(mainWindow);
        mainWindow->stopStatusProgress();
        return;
    }

    // Get current region
    QString currentRegion =
        CargoNetSim::CargoNetSimController::getInstance()
            .getRegionDataController()
            ->getCurrentRegion();

    // Get all visible terminal items in the current region
    QList<TerminalItem *> allTerminals =
        scene->getItemsByType<TerminalItem>();
    QList<TerminalItem *> visibleTerminals;

    for (TerminalItem *terminal : allTerminals)
    {
        if (terminal && terminal->isVisible()
            && terminal->getRegion() == currentRegion)
        {
            visibleTerminals.append(terminal);
        }
    }

    if (visibleTerminals.isEmpty())
    {
        mainWindow->showStatusBarError(
            QString(
                "No visible terminals found in region '%1'")
                .arg(currentRegion),
            3000);

        // Restore toolbar button states
        ToolbarController::restoreButtonStates(mainWindow);
        mainWindow->stopStatusProgress();
        return;
    }

    // Create a dialog to confirm the operation
    QMessageBox msgBox(mainWindow);
    msgBox.setWindowTitle("Unlink Terminals from Network");
    msgBox.setText(
        QString("This will unlink all %1 visible terminals "
                "in region '%2' from network points.")
            .arg(visibleTerminals.size())
            .arg(currentRegion));

    QString networkTypesStr;
    if (networkTypes.contains(NetworkType::Train)
        && networkTypes.contains(NetworkType::Truck))
    {
        networkTypesStr = "train and truck";
    }
    else if (networkTypes.contains(NetworkType::Train))
    {
        networkTypesStr = "train";
    }
    else if (networkTypes.contains(NetworkType::Truck))
    {
        networkTypesStr = "truck";
    }

    msgBox.setInformativeText(
        QString("Selected network types: %1\n\nDo you want "
                "to continue?")
            .arg(networkTypesStr));
    msgBox.setStandardButtons(QMessageBox::Yes
                              | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);

    int result = msgBox.exec();
    if (result != QMessageBox::Yes)
    {
        // Restore toolbar button states
        ToolbarController::restoreButtonStates(mainWindow);
        mainWindow->stopStatusProgress();
        return;
    }

    // Unlink each terminal from network points
    int successCount = 0;
    int i            = 0;

    for (TerminalItem *terminal : visibleTerminals)
    {

        if (unlinkTerminalFromNetworkPoints(
                mainWindow, terminal, networkTypes))
        {
            successCount++;
        }

        // Process events to keep UI responsive
        QApplication::processEvents();
    }

    // Display results
    if (successCount > 0)
    {
        mainWindow->showStatusBarMessage(
            QString("Successfully unlinked %1 of %2 "
                    "terminals from network points")
                .arg(successCount)
                .arg(visibleTerminals.size()),
            5000);
    }
    else
    {
        mainWindow->showStatusBarError(
            "No terminals were unlinked from network "
            "points.",
            3000);
    }
    // Restore toolbar button states
    ToolbarController::restoreButtonStates(mainWindow);
    mainWindow->stopStatusProgress();
}

void CargoNetSim::GUI::ViewController::
    showFilteredConnections(
        MainWindow        *mainWindow,
        const QStringList &terminalNames,
        const QStringList &connectionTypes)
{
    if (!mainWindow || terminalNames.isEmpty()
        || connectionTypes.isEmpty())
    {
        return;
    }

    // Determine if we're in global or region view
    bool isGlobalView    = mainWindow->isGlobalViewActive();
    GraphicsScene *scene = isGlobalView
                               ? mainWindow->globalMapScene_
                               : mainWindow->regionScene_;

    // Get all connection lines in the scene
    QList<ConnectionLine *> connectionLines =
        scene->getItemsByType<ConnectionLine>();

    // Find all the specified terminals
    QList<QGraphicsItem *> selectedTerminals;

    if (isGlobalView)
    {
        // Get all global terminals in the scene
        QList<GlobalTerminalItem *> globalTerminals =
            scene->getItemsByType<GlobalTerminalItem>();

        // Find the specified global terminals by name
        for (GlobalTerminalItem *terminal : globalTerminals)
        {
            TerminalItem *linkedTerminal =
                terminal->getLinkedTerminalItem();
            if (linkedTerminal)
            {
                QString name =
                    linkedTerminal->getProperty("Name")
                        .toString();
                if (terminalNames.contains(name))
                {
                    selectedTerminals.append(terminal);
                }
            }
        }
    }
    else
    {
        // Get all terminals in the scene
        QList<TerminalItem *> terminals =
            scene->getItemsByType<TerminalItem>();

        // Get the current region
        QString currentRegion =
            CargoNetSim::CargoNetSimController::
                getInstance()
                    .getRegionDataController()
                    ->getCurrentRegion();

        // Find the specified terminals by name in the
        // current region
        for (TerminalItem *terminal : terminals)
        {
            if (terminal->getRegion() == currentRegion)
            {
                QString name = terminal->getProperty("Name")
                                   .toString();
                if (terminalNames.contains(name))
                {
                    selectedTerminals.append(terminal);
                }
            }
        }
    }

    // Show error message if no terminals found
    if (selectedTerminals.isEmpty())
    {
        mainWindow->showStatusBarError(
            "Could not find any of the selected terminals "
            "in the current view.",
            3000);
        return;
    }

    // Hide all connection lines first
    for (ConnectionLine *line : connectionLines)
    {
        line->setVisible(false);
    }

    // Show only connection lines that meet both criteria:
    // 1. Connect two of the selected terminals
    // 2. Are of a selected type
    int connectionsFound = 0;

    for (ConnectionLine *line : connectionLines)
    {
        // Check if the connection type is selected
        if (!connectionTypes.contains(
                line->connectionType()))
        {
            continue;
        }

        // Check if both endpoints are in the selected
        // terminals list
        QGraphicsItem *startItem = line->startItem();
        QGraphicsItem *endItem   = line->endItem();

        bool startIsSelected =
            selectedTerminals.contains(startItem);
        bool endIsSelected =
            selectedTerminals.contains(endItem);

        if (startIsSelected && endIsSelected)
        {
            line->setVisible(true);
            connectionsFound++;
        }
    }

    // Show status bar message
    if (connectionsFound > 0)
    {
        QString message =
            QString("Showing %1 connection(s) between %2 "
                    "terminal(s) of types: %3")
                .arg(connectionsFound)
                .arg(terminalNames.size())
                .arg(connectionTypes.join(", "));
        mainWindow->showStatusBarMessage(message, 5000);
    }
    else
    {
        mainWindow->showStatusBarMessage(
            "No connections found matching the selected "
            "criteria.",
            3000);
    }
}

bool CargoNetSim::GUI::ViewController::moveNetworkItems(
    MainWindow *mainWindow, NetworkType networkType,
    const QString &networkName, const QPointF &offset,
    const QString &regionName)
{
    if (!mainWindow || !mainWindow->regionScene_)
        return false;

    GraphicsScene *scene = mainWindow->regionScene_;

    // Check if we're using projected coordinates
    bool usingProjectedCoords =
        mainWindow->regionView_->isUsingProjectedCoords();

    // Get all network map points and lines
    QList<MapPoint *> mapPoints =
        scene->getItemsByType<MapPoint>();
    QList<MapLine *> mapLines =
        scene->getItemsByType<MapLine>();

    int itemsUpdated = 0;

    // Process map points
    for (MapPoint *point : mapPoints)
    {
        // Skip points that don't belong to this region or
        // network
        if (point->getRegion() != regionName)
            continue;

        QObject *refNetwork = point->getReferenceNetwork();
        if (!refNetwork)
            continue;

        // Check if this point belongs to the target network
        bool isTargetNetwork = false;

        if (networkType == NetworkType::Train)
        {
            auto trainNet = qobject_cast<
                Backend::TrainClient::NeTrainSimNetwork *>(
                refNetwork);
            if (trainNet
                && trainNet->getNetworkName()
                       == networkName)
                isTargetNetwork = true;
        }
        else if (networkType == NetworkType::Truck)
        {
            auto truckNet = qobject_cast<
                Backend::TruckClient::IntegrationNetwork *>(
                refNetwork);
            if (truckNet
                && truckNet->getNetworkName()
                       == networkName)
                isTargetNetwork = true;
        }

        if (!isTargetNetwork)
            continue;

        // Update point position
        QPointF currentPos = point->getSceneCoordinate();
        QPointF newPos     = currentPos + offset;

        // Update both scene position and properties
        point->setSceneCoordinate(newPos);
        point->updateProperties(
            {{"x", newPos.x()}, {"y", newPos.y()}});

        // Update any linked terminal
        TerminalItem *linkedTerminal =
            point->getLinkedTerminal();
        if (linkedTerminal)
        {
            linkedTerminal->setPos(newPos);
            // Update the global terminal item position if
            // needed
            updateGlobalMapItem(mainWindow, linkedTerminal);
        }

        itemsUpdated++;
    }

    // Process map lines
    for (MapLine *line : mapLines)
    {
        // Skip lines that don't belong to this region or
        // network
        if (line->getRegion() != regionName)
            continue;

        QObject *refNetwork = line->getReferenceNetwork();
        if (!refNetwork)
            continue;

        // Check if this line belongs to the target network
        bool isTargetNetwork = false;

        if (networkType == NetworkType::Train)
        {
            auto trainNet = qobject_cast<
                Backend::TrainClient::NeTrainSimNetwork *>(
                refNetwork);
            if (trainNet
                && trainNet->getNetworkName()
                       == networkName)
                isTargetNetwork = true;
        }
        else if (networkType == NetworkType::Truck)
        {
            auto truckNet = qobject_cast<
                Backend::TruckClient::IntegrationNetwork *>(
                refNetwork);
            if (truckNet
                && truckNet->getNetworkName()
                       == networkName)
                isTargetNetwork = true;
        }

        if (!isTargetNetwork)
            continue;

        // Update line start and end points
        QPointF currentStart = line->getStartPoint();
        QPointF currentEnd   = line->getEndPoint();

        QPointF newStart = currentStart + offset;
        QPointF newEnd   = currentEnd + offset;

        line->setPoints(newStart, newEnd);

        itemsUpdated++;
    }

    if (itemsUpdated > 0)
    {
        // Force scene update
        scene->update();
        return true;
    }

    return false;
}
