#include "NetworkDrawingController.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Controllers/RegionDataController.h"
#include "Backend/Scenario/NetworkLookup.h"
#include "GUI/Controllers/TerminalController.h"
#include "GUI/Controllers/UtilityFunctions.h"
#include "GUI/Items/MapLine.h"
#include "GUI/Items/MapPoint.h"
#include "GUI/Items/TerminalItem.h"
#include "GUI/MainWindow.h"
#include "GUI/Scenario/ItemEventBinder.h"
#include "Backend/Scenario/ScenarioRuntime.h"
#include "GUI/Scenario/ScenarioMutator.h"
#include "GUI/Utils/ColorUtils.h"
#include "GUI/Widgets/GraphicsScene.h"
#include "GUI/Widgets/GraphicsView.h"
#include "StatusReporter.h"

#include <QApplication>

namespace CargoNetSim
{
namespace GUI
{

NetworkDrawingController::NetworkDrawingController(
    GraphicsScene      *regionScene,
    GraphicsView       *regionView,
    TerminalController *terminalCtrl,
    MainWindow         *mainWindow,
    StatusReporter     *status,
    QObject            *parent)
    : QObject(parent)
    , m_regionScene(regionScene)
    , m_regionView(regionView)
    , m_terminalCtrl(terminalCtrl)
    , m_mainWindow(mainWindow)
    , m_status(status)
{
    qCDebug(lcGuiView)
        << "NetworkDrawingController: created";
}

// ── public ──────────────────────────────────────────────

void NetworkDrawingController::drawNetwork(
    Backend::RegionData *regionData,
    NetworkType          networkType,
    QString             &networkName,
    bool                 skipTerminalCreation)
{
    qCInfo(lcGuiView) << "NetworkDrawingController::drawNetwork:"
                      << "network=" << networkName
                      << "type=" << static_cast<int>(networkType);

    if (!regionData)
    {
        qCWarning(lcGuiView)
            << "NetworkDrawingController::drawNetwork:"
            << "null regionData, returning";
        return;
    }

    QString regionName = regionData->getRegion();
    QColor  linksColor = ColorUtils::getRandomColor();

    m_status->storeButtons();
    m_status->disableButtons();
    m_status->startProgress();

    if (networkType == NetworkType::Train)
    {
        auto network =
            regionData->getTrainNetwork(networkName);
        drawTrainNetwork(network, regionName, linksColor,
                         skipTerminalCreation, networkName);
    }
    else if (networkType == NetworkType::Truck)
    {
        auto network =
            regionData->getTruckNetworkConfig(networkName);
        drawTruckNetwork(network, regionName, linksColor);
    }

    m_status->restoreButtons();
    m_status->stopProgress();
}

void NetworkDrawingController::removeNetwork(
    NetworkType          networkType,
    Backend::RegionData *regionData,
    QString             &networkName)
{
    qCDebug(lcGuiView)
        << "NetworkDrawingController::removeNetwork:"
        << "network=" << networkName
        << "type=" << static_cast<int>(networkType);

    if (!regionData)
    {
        qCWarning(lcGuiView)
            << "NetworkDrawingController::removeNetwork:"
            << "null regionData, returning";
        return;
    }

    QString regionName = regionData->getRegion();

    if (networkType == NetworkType::Train)
    {
        Backend::TrainClient::NeTrainSimNetwork *network =
            regionData->getTrainNetwork(networkName);
        if (!network)
        {
            qCWarning(lcGuiView)
                << "NetworkDrawingController::removeNetwork:"
                << "train network not found";
            return;
        }
        for (auto &node : network->getNodes())
        {
            if (!node)
                continue;

            MapPoint *point =
                m_regionScene->getItemById<MapPoint>(
                    node->getInternalUniqueID());
            if (!point)
                continue;

            TerminalItem *terminal =
                point->getLinkedTerminal();

            point->setProperty("Show on Global Map", false);
            m_terminalCtrl->updateGlobalMapItem(terminal);

            m_regionScene->removeItemWithId<MapPoint>(
                node->getInternalUniqueID());
        }
        for (auto &link : network->getLinks())
        {
            if (!link)
                continue;

            m_regionScene->removeItemWithId<MapLine>(
                link->getInternalUniqueID());
        }
    }
    else if (networkType == NetworkType::Truck)
    {
        Backend::TruckClient::IntegrationNetwork *network =
            regionData->getTruckNetwork(networkName);
        if (!network)
        {
            qCWarning(lcGuiView)
                << "NetworkDrawingController::removeNetwork:"
                << "truck network not found";
            return;
        }
        for (auto &node : network->getNodes())
        {
            MapPoint *point =
                m_regionScene->getItemById<MapPoint>(
                    node->getInternalUniqueID());
            if (!point)
                continue;

            TerminalItem *terminal =
                point->getLinkedTerminal();

            point->setProperty("Show on Global Map", false);
            m_terminalCtrl->updateGlobalMapItem(terminal);

            m_regionScene->removeItemWithId<MapPoint>(
                node->getInternalUniqueID());
        }
        for (auto &link : network->getLinks())
        {
            m_regionScene->removeItemWithId<MapLine>(
                link->getInternalUniqueID());
        }
    }
}

bool NetworkDrawingController::moveNetworkItems(
    NetworkType    networkType,
    const QString &networkName,
    const QPointF &offset,
    const QString &regionName)
{
    if (!m_regionScene)
    {
        qCWarning(lcGuiView)
            << "NetworkDrawingController::moveNetworkItems:"
            << "null scene, returning false";
        return false;
    }

    qCDebug(lcGuiView)
        << "NetworkDrawingController::moveNetworkItems:"
        << "network=" << networkName
        << "offset=" << offset;

    bool usingProjectedCoords =
        m_regionView->isUsingProjectedCoords();

    QList<MapPoint *> mapPoints =
        m_regionScene->getItemsByType<MapPoint>();
    QList<MapLine *> mapLines =
        m_regionScene->getItemsByType<MapLine>();

    int itemsUpdated = 0;

    for (MapPoint *point : mapPoints)
    {
        if (point->getRegion() != regionName)
            continue;

        QObject *refNetwork = point->getReferenceNetwork();
        if (!refNetwork)
            continue;

        Backend::NetworkKind kind{};
        const QString name =
            Backend::Scenario::NetworkLookup::networkNameOf(
                refNetwork, &kind);
        const bool isTargetNetwork =
            (name == networkName)
            && ((networkType == NetworkType::Train
                 && kind == Backend::NetworkKind::Rail)
                || (networkType == NetworkType::Truck
                    && kind == Backend::NetworkKind::Truck));

        if (!isTargetNetwork)
            continue;

        QPointF currentPos = point->getSceneCoordinate();
        QPointF newPos     = currentPos + offset;

        point->setSceneCoordinate(newPos);
        point->updateProperties(
            {{"x", newPos.x()}, {"y", newPos.y()}});

        TerminalItem *linkedTerminal =
            point->getLinkedTerminal();
        if (linkedTerminal)
        {
            linkedTerminal->setPos(newPos);
            m_terminalCtrl->updateGlobalMapItem(linkedTerminal);
        }

        itemsUpdated++;
    }

    for (MapLine *line : mapLines)
    {
        if (line->getRegion() != regionName)
            continue;

        QObject *refNetwork = line->getReferenceNetwork();
        if (!refNetwork)
            continue;

        Backend::NetworkKind kind{};
        const QString name =
            Backend::Scenario::NetworkLookup::networkNameOf(
                refNetwork, &kind);
        const bool isTargetNetwork =
            (name == networkName)
            && ((networkType == NetworkType::Train
                 && kind == Backend::NetworkKind::Rail)
                || (networkType == NetworkType::Truck
                    && kind == Backend::NetworkKind::Truck));

        if (!isTargetNetwork)
            continue;

        QPointF currentStart = line->getStartPoint();
        QPointF currentEnd   = line->getEndPoint();

        QPointF newStart = currentStart + offset;
        QPointF newEnd   = currentEnd + offset;

        line->setPoints(newStart, newEnd);

        itemsUpdated++;
    }

    if (itemsUpdated > 0)
    {
        m_regionScene->update();
        return true;
    }

    return false;
}

// ── private ─────────────────────────────────────────────

void NetworkDrawingController::drawTrainNetwork(
    Backend::TrainClient::NeTrainSimNetwork *network,
    QString &regionName, QColor &linksColor,
    bool skipTerminalCreation,
    const QString &networkName)
{
    qCDebug(lcRail) << "[RailDraw] drawTrainNetwork start"
             << "region=" << regionName
             << "network=" << (network ? "ok" : "null");
    qCDebug(lcRail) << "[RailDraw] setUsingProjectedCoords(true)";
    m_regionView->setUsingProjectedCoords(true);
    qCDebug(lcRail) << "[RailDraw] coordinatesNeedUpdate begin";
    emit coordinatesNeedUpdate();
    qCDebug(lcRail) << "[RailDraw] coordinatesNeedUpdate ok";

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
        MapPoint *point = drawNode(
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

        if (!skipTerminalCreation && point && node->isTerminal())
        {
            qCDebug(lcRail)
                << "[RailDraw]   calling createTerminalAtPoint";
            auto terminal =
                m_terminalCtrl->createTerminalAtPoint(
                    regionName,
                    "Intermodal Land Terminal",
                    point->getSceneCoordinate());
            qCDebug(lcRail)
                << "[RailDraw]   createTerminalAtPoint returned"
                << (terminal ? "terminal" : "null");

            point->setLinkedTerminal(terminal);
            if (terminal && m_mainWindow->runtime())
            {
                GUI::Scenario::ScenarioMutator::linkTerminalToNode(
                    &m_mainWindow->runtime()->document(),
                    terminal->sceneRegistryKey(),
                    networkName,
                    node->getUserId(),
                    Backend::Scenario::LinkageSource::Manual);
                qCDebug(lcRail) << "[RailDraw]   NodeLinkage written"
                                << "terminal=" << terminal->sceneRegistryKey()
                                << "nodeId="   << node->getUserId();
            }
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
    for (auto &link : linksVec)
    {
        auto sourceNode = link->getFromNode();
        auto destNode   = link->getToNode();
        if (!sourceNode || !destNode)
        {
            qCCritical(lcRail) << "[RailDraw] link has null"
                           " endpoint, linkIdx=" << linkIdx
                        << " userId=" << link->getUserId();
        }

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

        auto line = drawLink(
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
    m_regionView->fitInView(
        m_regionScene->itemsBoundingRect(),
        Qt::KeepAspectRatio);
    qCDebug(lcRail) << "[RailDraw] fitInView ok";

    m_status->showMessage(
        QString("Train network imported successfully."));
    qCDebug(lcRail) << "[RailDraw] drawTrainNetwork done";
}

void NetworkDrawingController::drawTruckNetwork(
    Backend::TruckClient::IntegrationSimulationConfig
            *networkConfig,
    QString &regionName, QColor &linksColor)
{
    if (!networkConfig)
    {
        qCWarning(lcGuiView)
            << "NetworkDrawingController::drawTruckNetwork:"
            << "null networkConfig, returning";
        return;
    }

    m_regionView->setUsingProjectedCoords(true);
    emit coordinatesNeedUpdate();

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

        auto point = drawNode(
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

        auto to =
            network->getNode(link->getDownstreamNodeId());
        auto from =
            network->getNode(link->getUpstreamNodeId());
        if (!to || !from)
        {
            qCWarning(lcGuiView)
                << "NetworkDrawingController::drawTruckNetwork:"
                << "node lookup returned null for nodeId";
            continue;
        }

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

        auto line = drawLink(
            QString::number(link->getLinkId()),
            link->getInternalUniqueID(),
            projectedSourcePoint, projectedDestPoint,
            regionName, linksColor, properties);

        line->setReferenceNetwork(network);
    }

    // Fit the view to the scene
    m_regionView->fitInView(
        m_regionScene->itemsBoundingRect(),
        Qt::KeepAspectRatio);

    m_status->showMessage(
        QString("Truck network imported successfully."));
}

MapPoint *NetworkDrawingController::drawNode(
    const QString &networkNodeID,
    const QString &nodeUniqueID,
    QPointF        projectedPoint,
    QString       &regionName,
    QColor         color,
    const QMap<QString, QVariant> &properties)
{
    if (auto *existing = m_regionScene->getItemById<MapPoint>(nodeUniqueID))
        return existing;

    qCDebug(lcGuiView)
        << "NetworkDrawingController::drawNode:"
        << "x=" << projectedPoint.x()
        << "y=" << projectedPoint.y();

    QPointF geodeticPoint =
        m_regionView->convertCoordinates(
            projectedPoint, "to_geodetic");
    QPointF scenePoint =
        m_regionView->wgs84ToScene(geodeticPoint);

    MapPoint *point =
        new MapPoint(networkNodeID, scenePoint, regionName,
                     "circle", nullptr, properties);

    Scenario::ItemEventBinder::bindMapPoint(
        point, m_mainWindow);

    point->setProperty("NodeID", nodeUniqueID);
    point->setColor(color);

    m_regionScene->addItemWithId(point, nodeUniqueID);

    return point;
}

MapLine *NetworkDrawingController::drawLink(
    const QString &networkNodeID,
    const QString &linkUniqueID,
    QPointF        projectedStartPoint,
    QPointF        projectedEndPoint,
    QString       &regionName,
    QColor         color,
    const QMap<QString, QVariant> &properties)
{
    MapLine *line = nullptr;
    try
    {
        QPointF sourceGeodetic =
            m_regionView->convertCoordinates(
                projectedStartPoint, "to_geodetic");
        QPointF destGeodetic =
            m_regionView->convertCoordinates(
                projectedEndPoint, "to_geodetic");
        QPointF sourceScenePoint =
            m_regionView->wgs84ToScene(sourceGeodetic);
        QPointF destScenePoint =
            m_regionView->wgs84ToScene(destGeodetic);

        line = new MapLine(networkNodeID, sourceScenePoint,
                           destScenePoint, regionName,
                           properties);

        MainWindow *mw = m_mainWindow;
        QObject::connect(
            line, &MapLine::clicked,
            [mw](MapLine *clickedLine) {
                UtilitiesFunctions::updatePropertiesPanel(
                    mw, clickedLine);
            });

        line->setProperty("LinkID", linkUniqueID);
        line->setColor(color);

        m_regionScene->addItemWithId(line, linkUniqueID);
    }
    catch (const std::exception &e)
    {
        qCWarning(lcGuiView)
            << "NetworkDrawingController::drawLink: error:"
            << e.what();
        m_status->showError(
            QString("Failed to draw link: %1")
                .arg(e.what()));
    }

    return line;
}

} // namespace GUI
} // namespace CargoNetSim
