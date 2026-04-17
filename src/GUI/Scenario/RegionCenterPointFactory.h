#pragma once

namespace CargoNetSim {
namespace Backend {
namespace Scenario {
struct RegionSpec;
} // namespace Scenario
} // namespace Backend

namespace GUI {

class RegionCenterPoint;
class MainWindow;
class GraphicsScene;

namespace Scenario {

/**
 * @brief Build a RegionCenterPoint that is a VIEW of a RegionSpec.
 *
 * The factory:
 *   1. Builds the legacy properties map (keys: "Region", "Latitude",
 *      "Longitude", "Shared Latitude", "Shared Longitude") mirroring
 *      what ViewController::updateTerminalGlobalPosition reads.
 *   2. Parses RegionSpec.color ("#RRGGBB") into a QColor.
 *   3. Constructs the RegionCenterPoint.
 *   4. Adds it to the scene with its auto-generated UUID.
 *   5. Wires click signals via ItemEventBinder (currently a no-op for
 *      RegionCenterPoint — hook kept for pattern uniformity).
 *   6. Publishes the pointer under the legacy GUI-runtime key
 *      ("regionCenterPoint") via RegionDataController, so callers using
 *      `getVariableAs<RegionCenterPoint*>("regionCenterPoint", ...)`
 *      continue to resolve correctly during the Plan-4 transition.
 *
 * Returns nullptr when @p region or @p scene is null. @p mainWindow may
 * be null for headless usage.
 */
class RegionCenterPointFactory
{
public:
    static RegionCenterPoint *
    fromRegionSpec(Backend::Scenario::RegionSpec *region,
                   GraphicsScene                 *scene,
                   MainWindow                    *mainWindow);
};

} // namespace Scenario
} // namespace GUI
} // namespace CargoNetSim
