#pragma once

#include <QObject>
#include <QColor>
#include <QPointF>
#include <QString>
#include <QMap>
#include <QVariant>

#include "GUI/Commons/NetworkType.h"

namespace CargoNetSim
{

namespace Backend
{
namespace TrainClient
{
class NeTrainSimNetwork;
}
namespace TruckClient
{
class IntegrationSimulationConfig;
}

} // namespace Backend

namespace GUI
{

class GraphicsScene;
class GraphicsView;
class TerminalController;
class MainWindow;
class StatusReporter;
class MapPoint;
class MapLine;

class NetworkDrawingController : public QObject
{
    Q_OBJECT

public:
    explicit NetworkDrawingController(
        GraphicsScene        *regionScene,
        GraphicsView         *regionView,
        TerminalController   *terminalCtrl,
        MainWindow           *mainWindow,
        StatusReporter       *status,
        QObject              *parent = nullptr);

    void drawNetwork(const QString      &regionName,
                     NetworkType         networkType,
                     const QString      &networkName,
                     bool                 skipTerminalCreation = false);

    void removeNetwork(NetworkType    networkType,
                       const QString &regionName,
                       const QString &networkName);

    bool moveNetworkItems(NetworkType    networkType,
                          const QString &networkName,
                          const QPointF &offset,
                          const QString &regionName);

signals:
    void coordinatesNeedUpdate();

private:
    void drawTrainNetwork(
        Backend::TrainClient::NeTrainSimNetwork *network,
        const QString &regionName, QColor &linksColor,
        bool skipTerminalCreation,
        const QString &networkName);

    void drawTruckNetwork(
        Backend::TruckClient::IntegrationSimulationConfig
                *networkConfig,
        const QString &regionName, QColor &linksColor);

    MapPoint *drawNode(const QString &networkNodeID,
                       const QString &nodeUniqueID,
                       QPointF        projectedPoint,
                       const QString &regionName,
                       QColor         color,
                       const QMap<QString, QVariant> &properties =
                           QMap<QString, QVariant>());

    MapLine *drawLink(const QString &networkNodeID,
                      const QString &linkUniqueID,
                      QPointF        projectedStartPoint,
                      QPointF        projectedEndPoint,
                      const QString &regionName,
                      QColor         color,
                      const QMap<QString, QVariant> &properties =
                          QMap<QString, QVariant>());

    GraphicsScene      *m_regionScene;
    GraphicsView       *m_regionView;
    TerminalController *m_terminalCtrl;
    MainWindow         *m_mainWindow;
    StatusReporter     *m_status;
};

} // namespace GUI
} // namespace CargoNetSim
