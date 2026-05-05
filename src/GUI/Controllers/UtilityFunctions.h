#pragma once

#include "Backend/Controllers/CargoNetSimController.h"
#include "GUI/Commons/NetworkType.h"
#include "GUI/Items/RegionCenterPoint.h"
#include "GUI/Items/TerminalItem.h"
#include "GUI/Widgets/GraphicsScene.h"
#include <QGraphicsItem>
#include <QMainWindow>
#include <QPointF>
#include <QString>
#include <QToolButton>

namespace CargoNetSim
{
namespace GUI
{

class MapPoint;
class MainWindow;

class UtilitiesFunctions
{
public:
    enum class ConnectionType
    {
        Any,
        Connected,
        NotConnected
    };

    enum class LinkType
    {
        Any,
        Linked,
        NotLinked
    };

    static QList<CargoNetSim::GUI::TerminalItem *>
    getTerminalItems(
        GraphicsScene *scene, const QString &region = "*",
        const QString &terminalType   = "*",
        ConnectionType connectionType = ConnectionType::Any,
        LinkType       linkType       = LinkType::Any);

    static QList<CargoNetSim::GUI::GlobalTerminalItem *>
    getGlobalTerminalItems(GraphicsScene *scene,
                           const QString &region,
                           const QString &terminalType,
                           ConnectionType connectionType,
                           LinkType       linkType);

    static QList<CargoNetSim::GUI::MapPoint *>
    getMapPointsOfTerminal(
        GraphicsScene *scene, TerminalItem *terminal,
        const QString &region      = "*",
        const QString &networkName = "*",
        NetworkType    networkType = NetworkType::Train);

    /**
     * @brief Updates the properties panel with the selected
     * item's properties
     * @ param mainWindow The main window
     * @param item The selected graphics item
     */
    static void
    updatePropertiesPanel(MainWindow    *mainWindow,
                          QGraphicsItem *item);

    /**
     * @brief Hides the properties panel
     * @ param mainWindow The main window
     */
    static void hidePropertiesPanel(MainWindow *mainWindow);

    /**
     * @brief Updates global map items for a region
     * @param mainWindow The main window
     * @param regionName The name of the region to update
     */
    static void
    updateGlobalMapForRegion(MainWindow    *mainWindow,
                             const QString &regionName);

    /// Returns the intersection of source and target interface modes.
    /// Typed enum set — callers compare against TransportationMode values
    /// rather than hand-matching "Truck" / "Rail" / "Ship" strings.
    /// Empty set when either endpoint is unresolvable.
    static QSet<Backend::TransportationTypes::TransportationMode>
    getCommonModes(QGraphicsItem *sourceItem,
                   QGraphicsItem *targetItem);

    static QList<QPair<MapPoint *, MapPoint *>>
    getCommonNetworks(QList<MapPoint *> firstEntries,
                      QList<MapPoint *> secondEntries);

    static QList<QPair<CargoNetSim::GUI::MapPoint *,
                       CargoNetSim::GUI::MapPoint *>>
    getCommonNetworksOfNetworkType(
        QList<CargoNetSim::GUI::MapPoint *> firstEntries,
        QList<CargoNetSim::GUI::MapPoint *> secondEntries,
        NetworkType                         networkType);

    static double
    getApproximateGeoDistance(const QPointF &point1,
                              const QPointF &point2);

    static void getTopShortestPaths(MainWindow *mainWindow,
                                    int         PathsCount);

    static bool processNetworkModeConnection(
        MainWindow                     *mainWindow,
        CargoNetSim::GUI::TerminalItem *sourceTerminal,
        CargoNetSim::GUI::TerminalItem *targetTerminal,
        CargoNetSim::GUI::NetworkType   networkType);

    static void
    linkMapPointToTerminal(MainWindow   *mainWindow,
                           MapPoint     *mapPoint,
                           TerminalItem *terminal);

    static void
    validateSelectedSimulation(MainWindow *mainWindow);

    static void linkSelectedTerminalsToNetwork(
        MainWindow               *mainWindow,
        const QList<NetworkType> &networkTypes);

    static void onLinkTerminalsToNetworkActionTriggered(
        MainWindow *mainWindow);

    static void unlinkSelectedTerminalsToNetwork(
        MainWindow               *mainWindow,
        const QList<NetworkType> &networkTypes);
    static void onUnlinkTerminalsToNetworkActionTriggered(
        MainWindow *mainWindow);

    static void
    openTerminalConnectionSelector(MainWindow *mainWindow);

    static void
    onShowMoveNetworkDialog(MainWindow *mainWindow);
};

} // namespace GUI
} // namespace CargoNetSim
