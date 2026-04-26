#pragma once

#include <QGraphicsItem>
#include <QColor>
#include <QList>
#include <QMainWindow>
#include <QPointF>
#include <QString>
#include <QToolButton>

#include "GUI/Commons/NetworkType.h"
#include "GUI/Items/MapLine.h"

namespace CargoNetSim
{
namespace Backend
{
struct ShortestPathResult;
}

namespace GUI
{

// Forward declarations
class MainWindow;
class GraphicsScene;
class TerminalItem;

class NetworkController
{
public:
    static QString
    importNetwork(MainWindow          *mainWindow,
                  NetworkType          networkType,
                  const QString       &regionName);

    static bool
    removeNetwork(MainWindow          *mainWindow,
                  NetworkType          networkType,
                  QString             &networkName,
                  const QString       &regionName);

    static bool renameNetwork(
        MainWindow *mainWindow, NetworkType networkType,
        const QString &oldName, const QString &newName,
        const QString &regionName);

    static bool changeNetworkColor(
        MainWindow *mainWindow, NetworkType networkType,
        const QString &networkName, const QColor &newColor,
        const QString &regionName);

    static CargoNetSim::Backend::ShortestPathResult
    findNetworkShortestPath(const QString &regionName,
                            const QString &networkName,
                            NetworkType    networkType,
                            int startNodeId, int endNodeId);

    static bool clearAllNetworks(MainWindow *mainWindow);

    /**
     * @brief Finds the MapLines that correspond to a
     * shortest path between two nodes
     * @param mainWindow Pointer to MainWindow
     * @param regionName Name of the region
     * @param networkName Name of the network
     * @param networkType Type of the network (Train, Truck,
     * Ship)
     * @param startNodeId Starting node ID
     * @param endNodeId Ending node ID
     * @return List of MapLines that form the shortest path
     */
    static QList<MapLine *> getShortestPathMapLines(
        MainWindow *mainWindow, const QString &regionName,
        const QString &networkName, NetworkType networkType,
        int startNodeId, int endNodeId);

    static bool moveNetwork(
        MainWindow *mainWindow, NetworkType networkType,
        const QString &networkName, const QPointF &offset,
        const QString &regionName);

protected:
    static bool
    importTrainNetwork(MainWindow    *mainWindow,
                       const QString &regionName,
                       QString       &networkName);

    static bool
    importTruckNetwork(MainWindow    *mainWindow,
                       const QString &regionName,
                       QString       &networkName);

private:
    static QString getNetworkTypeString(NetworkType type);
};

} // namespace GUI
} // namespace CargoNetSim
