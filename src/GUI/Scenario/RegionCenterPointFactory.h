#pragma once

#include <QString>

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
 *      ("regionCenterPoint") via NetworkViewService, so callers using
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

    /// Locate the RegionCenterPoint bound to @p regionName. The scene
    /// keys items by stable UUID (so rename doesn't break the
    /// registry); this is the correct entry point for callers that
    /// only have the region name. Returns nullptr when no matching
    /// center exists. Matches the sibling pattern
    /// `ConnectionLineFactory::findRegionConnection` /
    /// `MapPointFactory::findByNetworkAndNode`.
    static RegionCenterPoint *
    findByName(GraphicsScene *scene, const QString &regionName);
};

} // namespace Scenario
} // namespace GUI
} // namespace CargoNetSim
