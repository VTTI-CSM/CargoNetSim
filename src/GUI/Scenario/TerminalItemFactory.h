#pragma once

namespace CargoNetSim {
namespace Backend {
namespace Scenario {
struct TerminalPlacement;
} // namespace Scenario
} // namespace Backend

namespace GUI {

class TerminalItem;
class MainWindow;
class GraphicsScene;

namespace Scenario {

/**
 * @brief Build a TerminalItem that is a VIEW of a TerminalPlacement.
 *
 * The factory:
 *   1. Looks up the correct pixmap from IconFactory::createTerminalIcons()
 *      keyed by placement->type.
 *   2. Constructs the TerminalItem (view of the placement; the non-owning
 *      pointer from item to placement is bound by Task 9 via setPlacement).
 *   3. Computes scene position from the placement's PositionMode:
 *      - LatLon: project through MainWindow's regionView when available.
 *      - Scene:  use the raw scene-pixel coordinate.
 *      - NetworkNode: position is resolved by MapPointFactory; factory
 *                     places the item at (0,0) for now.
 *   4. Adds the item to the scene via addItemWithId(id = placement->id).
 *   5. Wires click/position signals via ItemEventBinder when MainWindow
 *      is supplied.
 *
 * Returns nullptr if placement/scene are null or the pixmap lookup fails.
 * mainWindow may be null for headless construction (used in tests and in
 * one-shot mutator paths that bind signals later).
 */
class TerminalItemFactory
{
public:
    static TerminalItem *
    fromPlacement(Backend::Scenario::TerminalPlacement *placement,
                  GraphicsScene                         *scene,
                  MainWindow                            *mainWindow);
};

} // namespace Scenario
} // namespace GUI
} // namespace CargoNetSim
