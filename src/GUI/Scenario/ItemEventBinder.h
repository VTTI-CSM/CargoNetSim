#pragma once

namespace CargoNetSim {
namespace GUI {

class MainWindow;
class TerminalItem;
class MapPoint;
class ConnectionLine;
class GlobalTerminalItem;
class RegionCenterPoint;
class BackgroundPhotoItem;

namespace Scenario {

/**
 * @brief Shared QObject::connect wiring for scene items.
 *
 * Every connect() that previously lived inline inside ViewController
 * factory code moves here. One location to update when MainWindow slot
 * names change, and one location for both the ViewController creation
 * paths and the future SceneRepopulator factory paths to call.
 *
 * Returns the number of connections established (useful for tests and
 * for verifying the signal wiring was applied).
 * When mainWindow is nullptr, returns 0 without making connections.
 */
class ItemEventBinder
{
public:
    static int bindTerminalItem(TerminalItem *item,
                                MainWindow   *mainWindow);
    static int bindMapPoint(MapPoint   *item,
                            MainWindow *mainWindow);
    static int bindConnectionLine(ConnectionLine *line,
                                  MainWindow     *mainWindow);
    static int bindGlobalTerminalItem(GlobalTerminalItem *item,
                                      MainWindow *mainWindow);
    static int bindRegionCenterPoint(RegionCenterPoint *item,
                                     MainWindow        *mainWindow);
    static int bindBackgroundPhotoItem(BackgroundPhotoItem *item,
                                       MainWindow          *mainWindow);
};

} // namespace Scenario
} // namespace GUI
} // namespace CargoNetSim
