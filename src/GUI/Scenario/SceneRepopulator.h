#pragma once

namespace CargoNetSim {
namespace Backend {
namespace Scenario {
class ScenarioDocument;
} // namespace Scenario
} // namespace Backend

namespace GUI {

class MainWindow;
class GraphicsScene;

namespace Scenario {

/**
 * @brief Tear down the region / global scenes and rebuild them as views
 *        over the given ScenarioDocument.
 *
 * Rebuild order:
 *   1. Both scenes are cleared via GraphicsScene::clearAll (registry-safe).
 *   2. RegionSpec → RegionCenterPoint via RegionCenterPointFactory.
 *   3. NodeLinkage → MapPoint via MapPointFactory (per owning region's
 *      live RegionData).
 *   4. TerminalPlacement → TerminalItem via TerminalItemFactory.
 *   5. Link each MapPoint back to its TerminalItem by looking up the
 *      linkage's (networkName, nodeId) in a per-run QHash index.
 *   6. Connection / GlobalLink → ConnectionLine (Task 4 — deferred).
 *
 * Idempotent. Null-safe (null doc or null scenes → no-op early return).
 */
class SceneRepopulator
{
public:
    /// MainWindow-flavoured convenience: reads scenes + view from
    /// `mainWindow->regionScene()` / `globalMapScene()`. The call-site
    /// that `ViewController::onDocumentReset` (Task 21) and `File → Open
    /// Scenario` (Task 19) use.
    static void repopulate(Backend::Scenario::ScenarioDocument *doc,
                           MainWindow                          *mainWindow);

    /// Lower-level overload that operates on scenes directly, without a
    /// MainWindow. Kept public so:
    ///   (a) unit tests can exercise the full rebuild pipeline without
    ///       bringing up the heavyweight MainWindow QWidget hierarchy, and
    ///   (b) future non-GUI callers (headless thumbnailer, server-side
    ///       preview renderer) can reuse the same orchestration logic.
    /// Factories internally treat a null @p mainWindow as "no event
    /// wiring, no visibility update" — safe for headless use.
    static void repopulate(Backend::Scenario::ScenarioDocument *doc,
                           GraphicsScene                       *regionScene,
                           GraphicsScene                       *globalScene,
                           MainWindow                          *mainWindow);
};

} // namespace Scenario
} // namespace GUI
} // namespace CargoNetSim
