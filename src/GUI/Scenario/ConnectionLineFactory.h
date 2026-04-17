#pragma once

#include "Backend/Commons/TransportationMode.h"
#include <QString>

namespace CargoNetSim {
namespace Backend {
namespace Scenario {
struct Connection;
struct GlobalLink;
} // namespace Scenario
} // namespace Backend

namespace GUI {

class ConnectionLine;
class MainWindow;
class GraphicsScene;

namespace Scenario {

/**
 * @brief Build a ConnectionLine view from a scenario Connection or
 *        GlobalLink.
 *
 * The two entry points mirror the discriminated union that a
 * ConnectionLine carries (Task 11): region-local Connection ⊕
 * cross-region GlobalLink. Each looks up the start/end endpoints by id
 * from the matching scene, constructs the view, binds the model via the
 * Task-11 setter, adds to the scene, and wires click signals through
 * ItemEventBinder.
 *
 * GlobalTerminalItem lookup is NOT by scene-registry id (those are
 * auto-UUIDs) — it's by the linked TerminalItem's id, which is the
 * canonical document-level identifier after Task 9.
 *
 * Returns nullptr when any input is null or the endpoints can't be
 * resolved in the scene.
 */
class ConnectionLineFactory
{
public:
    static ConnectionLine *
    fromConnection(Backend::Scenario::Connection *connection,
                   GraphicsScene                 *regionScene,
                   MainWindow                    *mainWindow);

    static ConnectionLine *
    fromGlobalLink(Backend::Scenario::GlobalLink *link,
                   GraphicsScene                 *globalScene,
                   MainWindow                    *mainWindow);

    /// Locate a region-level ConnectionLine in @p scene whose
    /// connectionModel matches (from, to, mode). Returns nullptr if no
    /// such line is bound. Used by Task 14/21 observers and by
    /// ViewController's post-mutator lookup for the line it just
    /// asked the mutator to create (Task 15).
    static ConnectionLine *
    findRegionConnection(
        GraphicsScene *scene,
        const QString &fromTerminalId,
        const QString &toTerminalId,
        Backend::TransportationTypes::TransportationMode mode);

    /// GlobalLink counterpart of `findRegionConnection`. Matches on the
    /// ConnectionLine's globalLinkModel.
    static ConnectionLine *
    findGlobalLink(
        GraphicsScene *scene,
        const QString &fromTerminalId,
        const QString &toTerminalId,
        Backend::TransportationTypes::TransportationMode mode);
};

} // namespace Scenario
} // namespace GUI
} // namespace CargoNetSim
