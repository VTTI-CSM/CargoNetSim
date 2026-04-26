#pragma once

#include <QList>
#include <QObject>
#include <QPointF>
#include <QString>

#include "Backend/GuiApi/ScenarioDocumentApi.h"
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

    /// Reconcile the global-scene mirror for @p terminal against the
    /// terminal's current state ("Show on Global Map" property, position,
    /// region). Creates, moves, or removes the GlobalTerminalItem as
    /// needed. The one entry point to invoke on add/change events.
    void updateGlobalMapItem(TerminalItem *terminal);

    /// Unconditionally remove the GlobalTerminalItem (if any) registered
    /// under @p terminalId from the global scene. The only entry point
    /// to invoke on the remove event: the bound TerminalItem may already
    /// have been destroyed by the regionScene observer, so lookup goes
    /// by terminal id through the global scene's registry, not through
    /// TerminalItem::getGlobalTerminalItem().
    ///
    /// This keeps the "a GlobalTerminalItem exists iff the bound
    /// TerminalItem exists AND show-on-global-map is true" invariant
    /// owned entirely by TerminalController — callers only signal
    /// lifecycle events, never mutate the mirror directly.
    void removeGlobalMirror(const QString &terminalId);

    /// Reposition every existing GlobalTerminalItem mirror whose bound
    /// TerminalItem lives in @p regionName. Sibling of
    /// updateGlobalMapItem: that one reconciles a single terminal on
    /// `terminalAdded`/`terminalChanged`; this one handles the
    /// `regionChanged` case where the region's globalPosition or
    /// localOrigin moved and every mirror in the region has to follow.
    /// Does NOT create or remove mirrors — lifecycle stays with
    /// updateGlobalMapItem / removeGlobalMirror. No-ops for regions
    /// that have no mirrored terminals.
    void refreshGlobalMirrorsForRegion(const QString &regionName);

    TerminalItem *createTerminalAtPoint(
        const QString &region,
        const QString &terminalType,
        const QPointF &point,
        Backend::Scenario::TerminalPlacement::TerminalRole role =
            Backend::Scenario::TerminalPlacement::TerminalRole::Transit);

    bool updateTerminalPositionByGlobalPosition(
        TerminalItem *terminal,
        QPointF       globalGeoPos);

    /**
     * @brief Convert a global WGS84 point to the given region's local
     *        lat/lon (x = lon, y = lat).
     *
     * The conversion subtracts the region's shared-origin offset
     * (Shared − local origin) from the global point, mirroring the
     * inverse of the offset that `sceneToWGS84` applies in the region
     * view. Returns the input unchanged if the region center cannot be
     * resolved.
     */
    QPointF globalToLocalLatLon(const QString &region,
                                const QPointF &globalLatLon) const;

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
