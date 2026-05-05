#include "SceneVisibilityController.h"
#include "Backend/Application/NetworkViewService.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Commons/TransportationMode.h"
#include "GUI/Items/BackgroundPhotoItem.h"
#include "GUI/Items/ConnectionLine.h"
#include "GUI/Items/GlobalTerminalItem.h"
#include "GUI/Items/MapLine.h"
#include "GUI/Items/MapPoint.h"
#include "GUI/Items/RegionCenterPoint.h"
#include "GUI/Items/TerminalItem.h"
#include "GUI/Widgets/GraphicsScene.h"
#include "StatusReporter.h"
#include <QApplication>

namespace CargoNetSim
{
namespace GUI
{

SceneVisibilityController::SceneVisibilityController(
    GraphicsScene  *regionScene,
    GraphicsScene  *globalMapScene,
    StatusReporter *status,
    QObject        *parent)
    : QObject(parent)
    , m_regionScene(regionScene)
    , m_globalMapScene(globalMapScene)
    , m_status(status)
{
    qCDebug(lcGuiView)
        << "SceneVisibilityController: created";
}

// ─── updateSceneVisibility ──────────────────────────────

void SceneVisibilityController::updateSceneVisibility()
{
    qCDebug(lcGuiView)
        << "SceneVisibilityController::updateSceneVisibility:"
           " begin";
    if (!m_regionScene)
    {
        qCWarning(lcGuiView)
            << "SceneVisibilityController::"
               "updateSceneVisibility: null scene";
        return;
    }

    Backend::Application::NetworkViewService networkView;
    const QString currentRegion =
        networkView.currentRegionName();

    for (auto item : m_regionScene->items())
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

// ─── showFilteredConnections ────────────────────────────

void SceneVisibilityController::showFilteredConnections(
    bool               isGlobalView,
    const QStringList &terminalNames,
    const QStringList &connectionTypes)
{
    qCDebug(lcGuiView)
        << "SceneVisibilityController::"
           "showFilteredConnections: begin"
        << "global=" << isGlobalView;

    if (terminalNames.isEmpty()
        || connectionTypes.isEmpty())
    {
        qCWarning(lcGuiView)
            << "SceneVisibilityController::"
               "showFilteredConnections: empty input";
        return;
    }

    GraphicsScene *scene = isGlobalView
                               ? m_globalMapScene
                               : m_regionScene;
    if (!scene)
    {
        qCWarning(lcGuiView)
            << "SceneVisibilityController::"
               "showFilteredConnections: null scene";
        return;
    }

    // Get all connection lines in the scene
    QList<ConnectionLine *> connectionLines =
        scene->getItemsByType<ConnectionLine>();

    // Find all the specified terminals
    QList<QGraphicsItem *> selectedTerminals;

    if (isGlobalView)
    {
        QList<GlobalTerminalItem *> globalTerminals =
            scene->getItemsByType<GlobalTerminalItem>();

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
        QList<TerminalItem *> terminals =
            scene->getItemsByType<TerminalItem>();

        Backend::Application::NetworkViewService networkView;
        const QString currentRegion =
            networkView.currentRegionName();

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

    if (selectedTerminals.isEmpty())
    {
        qCDebug(lcGuiView)
            << "SceneVisibilityController::"
               "showFilteredConnections: no terminals found";
        m_status->showError(
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
        const QString lineTypeStr =
            Backend::transportationModeToString(
                line->connectionType());
        bool typeSelected = false;
        for (const QString &t : connectionTypes)
        {
            if (t.compare(lineTypeStr, Qt::CaseInsensitive)
                == 0)
            {
                typeSelected = true;
                break;
            }
        }
        if (!typeSelected) continue;

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

    if (connectionsFound > 0)
    {
        QString message =
            QString("Showing %1 connection(s) between %2 "
                    "terminal(s) of types: %3")
                .arg(connectionsFound)
                .arg(terminalNames.size())
                .arg(connectionTypes.join(", "));
        m_status->showMessage(message, 5000);
    }
    else
    {
        m_status->showMessage(
            "No connections found matching the selected "
            "criteria.",
            3000);
    }
}

// ─── changeNetworkVisibility ────────────────────────────

void SceneVisibilityController::changeNetworkVisibility(
    const QString &networkName, bool isVisible)
{
    qCDebug(lcGuiView)
        << "SceneVisibilityController::"
           "changeNetworkVisibility:"
        << networkName << "visible=" << isVisible;

    if (!m_regionScene)
    {
        qCWarning(lcGuiView)
            << "SceneVisibilityController::"
               "changeNetworkVisibility: null scene";
        return;
    }

    auto checkNetworkAndSetVisibility =
        [&networkName,
         isVisible](const QString &itemNetworkName,
                    QGraphicsItem *graphicsItem) {
            if (itemNetworkName == networkName)
            {
                graphicsItem->setVisible(isVisible);
            }
        };

    auto mapPoints =
        m_regionScene->getItemsByType<MapPoint>();
    auto mapLines =
        m_regionScene->getItemsByType<MapLine>();

    for (auto mapPoint : mapPoints)
    {
        Backend::Application::NetworkViewService networkView;
        const QString name =
            networkView.networkNameOf(
                mapPoint->getReferenceNetwork());
        if (!name.isEmpty())
            checkNetworkAndSetVisibility(name, mapPoint);
    }

    for (auto mapLine : mapLines)
    {
        Backend::Application::NetworkViewService networkView;
        const QString name =
            networkView.networkNameOf(
                mapLine->getReferenceNetwork());
        if (!name.isEmpty())
            checkNetworkAndSetVisibility(name, mapLine);
    }
}

// ─── changeNetworkColor ─────────────────────────────────

void SceneVisibilityController::changeNetworkColor(
    const QString &networkName, const QColor &newColor)
{
    qCDebug(lcGuiView)
        << "SceneVisibilityController::changeNetworkColor:"
        << networkName << "color=" << newColor.name();

    if (!m_regionScene)
    {
        qCWarning(lcGuiView)
            << "SceneVisibilityController::"
               "changeNetworkColor: null scene";
        return;
    }

    QColor newDarkerColor =
        newColor.darker(150); // 150% darker for MapPoints

    auto mapPoints =
        m_regionScene->getItemsByType<MapPoint>();
    auto mapLines =
        m_regionScene->getItemsByType<MapLine>();

    for (auto mapPoint : mapPoints)
    {
        Backend::Application::NetworkViewService networkView;
        if (networkView.networkNameOf(
                mapPoint->getReferenceNetwork())
            == networkName)
        {
            mapPoint->setColor(newDarkerColor);
        }
    }

    for (auto mapLine : mapLines)
    {
        Backend::Application::NetworkViewService networkView;
        if (networkView.networkNameOf(
                mapLine->getReferenceNetwork())
            == networkName)
        {
            mapLine->setColor(newColor);
        }
    }
}

// ─── flashTerminalItems ─────────────────────────────────

void SceneVisibilityController::flashTerminalItems(
    QList<TerminalItem *> terminals, bool evenIfHidden)
{
    qCDebug(lcGuiView)
        << "SceneVisibilityController::flashTerminalItems:"
        << "count=" << terminals.size();

    for (auto terminal : terminals)
    {
        terminal->flash(evenIfHidden);
        QApplication::processEvents();
    }
}

} // namespace GUI
} // namespace CargoNetSim
