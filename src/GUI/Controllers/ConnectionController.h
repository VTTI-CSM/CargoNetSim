#pragma once

#include "Backend/Commons/TransportationMode.h"
#include <QObject>
#include <QVariantMap>
#include <optional>

class QGraphicsItem;

namespace CargoNetSim
{
namespace Backend {
namespace Scenario {
class ScenarioRuntime;
} // namespace Scenario
} // namespace Backend

namespace GUI
{

class ConnectionLine;
class GraphicsScene;
class MainWindow;
class StatusReporter;

class ConnectionController : public QObject
{
    Q_OBJECT

public:
    explicit ConnectionController(
        GraphicsScene  *regionScene,
        GraphicsScene  *globalMapScene,
        MainWindow     *mainWindow,
        StatusReporter *status,
        QObject        *parent = nullptr);

    /**
     * @brief Checks if a connection of the same type
     * already exists between two terminals
     */
    bool checkExistingConnection(
        QGraphicsItem *startItem,
        QGraphicsItem *endItem,
        Backend::TransportationTypes::TransportationMode
            connectionType);

    /**
     * @brief Creates a connection line between two terminals.
     * Mutates the ScenarioDocument through the backend route-authoring
     * service. Unbound view-only endpoints are rejected.
     */
    ConnectionLine *createConnectionLine(
        QGraphicsItem *startItem,
        QGraphicsItem *endItem,
        Backend::TransportationTypes::TransportationMode
            connectionType,
        std::optional<QVariantMap> canonicalProperties =
            std::nullopt);

    /**
     * @brief Removes a connection line from its scene.
     */
    bool removeConnectionLine(ConnectionLine *connectionLine);

    /**
     * @brief Bulk auto-connect visible terminals that share
     * a network (Rail / Truck / Ship).
     */
    void connectVisibleTerminalsByNetworks(
        const QString &currentRegion,
        bool           isGlobalView);

    /**
     * @brief Bulk auto-connect visible terminals that share
     * common interfaces (modes).
     */
    void connectVisibleTerminalsByInterfaces(
        const QString &currentRegion,
        bool           isGlobalView);

public slots:
    void setRuntime(
        Backend::Scenario::ScenarioRuntime *rt);

signals:
    void connectionRemoved(QGraphicsItem *item);
    void connectionCreated(QGraphicsItem *item);
    void terminalUnlinked(QGraphicsItem *item);

private:
    GraphicsScene  *m_regionScene;
    GraphicsScene  *m_globalMapScene;
    MainWindow     *m_mainWindow;
    StatusReporter *m_status;
    Backend::Scenario::ScenarioRuntime *m_runtime = nullptr;
};

} // namespace GUI
} // namespace CargoNetSim
