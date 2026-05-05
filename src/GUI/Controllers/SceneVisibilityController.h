#pragma once

#include <QColor>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

namespace CargoNetSim
{
namespace GUI
{

class GraphicsScene;
class StatusReporter;
class TerminalItem;

class SceneVisibilityController : public QObject
{
    Q_OBJECT

public:
    explicit SceneVisibilityController(
        GraphicsScene  *regionScene,
        GraphicsScene  *globalMapScene,
        StatusReporter *status,
        QObject        *parent = nullptr);

    /// Iterates region scene items,
    /// shows/hides by current region.
    void updateSceneVisibility();

    /// Shows only connections matching the given terminal
    /// names and connection types.
    /// @param isGlobalView true when the global map is active
    void showFilteredConnections(
        bool               isGlobalView,
        const QStringList &terminalNames,
        const QStringList &connectionTypes);

    /// Toggles visibility of network items by name.
    void changeNetworkVisibility(const QString &networkName,
                                 bool           isVisible);

    /// Recolors network items by name.
    void changeNetworkColor(const QString &networkName,
                            const QColor  &newColor);

    /// Flashes the given terminal items.
    static void
    flashTerminalItems(QList<TerminalItem *> terminals,
                       bool evenIfHidden = false);

protected:
    GraphicsScene  *m_regionScene;
    GraphicsScene  *m_globalMapScene;
    StatusReporter *m_status;
};

} // namespace GUI
} // namespace CargoNetSim
