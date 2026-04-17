#pragma once

#include <QString>

namespace CargoNetSim {
namespace Backend {
class RegionData;
namespace Scenario {
struct NodeLinkage;
} // namespace Scenario
} // namespace Backend

namespace GUI {

class MapPoint;
class MainWindow;
class GraphicsScene;

namespace Scenario {

/**
 * @brief Build a MapPoint that is a VIEW of a NodeLinkage.
 *
 * The factory:
 *   1. Resolves the referenced network from @p regionData by looking up
 *      @p linkage->networkName first as a rail network, then as a truck
 *      network. (NodeLinkage intentionally carries no networkType field
 *      — type is derived from the referenced network per the canonical
 *      schema.)
 *   2. Computes the node's scene-space position using the mode-specific
 *      accessors (train: getX/getY × scales; truck: getXCoordinate/
 *      getYCoordinate × scales × 1000 km→m conversion).
 *   3. Constructs the MapPoint with "Network_ID" set to the linkage's
 *      node ID so that MapPoint::getReferencedNetworkNodeID() returns it.
 *   4. Stores the network pointer via setReferenceNetwork so that
 *      callers (PathFindingWorker and other network-type-aware
 *      consumers) can downcast to the correct network type.
 *   5. Adds the point to the scene and binds its click signals.
 *
 * Returns nullptr when any input is null or the referenced node / network
 * cannot be resolved. @p mainWindow may be null for headless usage.
 */
class MapPointFactory
{
public:
    static MapPoint *
    fromNodeLinkage(Backend::Scenario::NodeLinkage *linkage,
                    Backend::RegionData            *regionData,
                    GraphicsScene                  *scene,
                    MainWindow                     *mainWindow);

    /// Locate an existing MapPoint in @p scene that represents the given
    /// (networkName, nodeId) pair. Matching is O(N) over MapPoints in
    /// the scene; the count is small and this runs only on mutation
    /// observer / Task-16 link-to-closest paths (not per-tick).
    ///
    /// Works for both linked MapPoints (whose `linkageModel()` carries
    /// the canonical networkName) and unlinked MapPoints created by
    /// legacy network-import paths (falls back to downcasting
    /// `getReferenceNetwork()` to read its name).
    ///
    /// Returns nullptr when no match is found.
    static MapPoint *
    findByNetworkAndNode(GraphicsScene *scene,
                         const QString &networkName, int nodeId);
};

} // namespace Scenario
} // namespace GUI
} // namespace CargoNetSim
