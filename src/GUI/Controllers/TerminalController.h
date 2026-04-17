#pragma once

#include <QList>
#include <QObject>
#include <QPointF>
#include <QString>

#include "Backend/Scenario/TerminalPlacement.h"
#include "GUI/Commons/NetworkType.h"

namespace CargoNetSim
{
namespace Backend {
namespace Scenario {
class ScenarioRuntime;
} // namespace Scenario
} // namespace Backend

namespace GUI
{

class GraphicsScene;
class GraphicsView;
class MainWindow;
class StatusReporter;
class TerminalItem;
class RegionCenterPoint;

class TerminalController : public QObject
{
    Q_OBJECT

public:
    explicit TerminalController(
        GraphicsScene  *regionScene,
        GraphicsView   *regionView,
        GraphicsScene  *globalMapScene,
        GraphicsView   *globalMapView,
        MainWindow     *mainWindow,
        StatusReporter *status,
        QObject        *parent = nullptr);

    void updateGlobalMapItem(TerminalItem *terminal);

    TerminalItem *createTerminalAtPoint(
        const QString &region,
        const QString &terminalType,
        const QPointF &point,
        Backend::Scenario::TerminalPlacement::TerminalRole role =
            Backend::Scenario::TerminalPlacement::TerminalRole::Transit);

    bool updateTerminalPositionByGlobalPosition(
        TerminalItem *terminal,
        QPointF       globalGeoPos);

    bool linkTerminalToClosestNetworkPoint(
        TerminalItem             *terminal,
        const QList<NetworkType> &networkTypes);

    void linkAllVisibleTerminalsToNetwork(
        const QList<NetworkType> &networkTypes);

    bool unlinkTerminalFromNetworkPoints(
        TerminalItem             *terminal,
        const QList<NetworkType> &networkTypes);

    void unlinkAllVisibleTerminalsToNetwork(
        const QList<NetworkType> &networkTypes);

public slots:
    void setRuntime(
        Backend::Scenario::ScenarioRuntime *rt);

private:
    void updateTerminalGlobalPosition(
        RegionCenterPoint *regionCenterPoint,
        TerminalItem      *terminal);

    GraphicsScene  *m_regionScene;
    GraphicsView   *m_regionView;
    GraphicsScene  *m_globalMapScene;
    GraphicsView   *m_globalMapView;
    MainWindow     *m_mainWindow;
    StatusReporter *m_status;
    Backend::Scenario::ScenarioRuntime *m_runtime = nullptr;
};

} // namespace GUI
} // namespace CargoNetSim
