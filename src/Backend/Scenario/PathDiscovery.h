#pragma once

#include <QList>
#include <QString>

namespace CargoNetSim {
namespace Backend {

class Path;

namespace Scenario {

class ScenarioDocument;
class ScenarioRegistry;

/**
 * @brief Backend-pure path discovery over a `ScenarioDocument`.
 *
 * Replaces the scene-walking logic that previously lived in
 * `GUI/Utils/PathFindingWorker`. Given a validated + applied
 * `ScenarioDocument` (Plans 1-2) and its `ScenarioRegistry` of
 * pre-constructed `Backend::Terminal*`, this class drives the
 * TerminalSim graph server to compute the top-N shortest paths for
 * every `(origin, destination)` pair the document declares.
 *
 * Inputs come from the document alone — no scene, no MainWindow, no
 * GUI types. That makes the same implementation usable from the CLI
 * (`cargonetsim-cli run`) and from the GUI (via `PathFindingWorker`,
 * which becomes a thin QObject wrapper in Plan 5 Task 5).
 *
 * Stateless: the class holds no members. All dependencies are either
 * injected via `findTopPaths` (document + registry) or obtained from
 * `CargoNetSimController::getInstance()` at call time (TerminalSim
 * client + ConfigController weights). Callers must ensure the
 * controller is initialized and `startAll()` has completed.
 */
class PathDiscovery
{
public:
    /**
     * @brief Submit the scenario's terminals + routes to TerminalSim
     *        and return the top-N shortest paths for every declared
     *        `(origin, destination)` pair.
     *
     * Origins are every terminal with at least one seeded container
     * (`ScenarioDocument::originTerminalIds()`); destinations come
     * from each origin's `destinationsFor()` list (Task 0). An empty
     * document yields an empty result with no error.
     *
     * @param doc       Source of terminals, connections, global links,
     *                  and origin/destination declarations.
     * @param registry  Source of `Backend::Terminal*` instances
     *                  (populated by `ScenarioApplier::applyTerminals`).
     * @param n         Number of shortest paths to request per pair.
     * @param err       On failure, filled with a human-readable
     *                  message. Caller can pass nullptr if the
     *                  distinction between "empty success" and
     *                  "failed" is not needed.
     *
     * @return Aggregated `Path*` list across all pairs (caller owns
     *         each pointer — `Path` is a QObject and can be reparented
     *         or deleted via `qDeleteAll`). Empty list with @p err set
     *         on failure; empty list with @p err untouched when the
     *         document declares no origins (not an error — used by
     *         headless unit tests).
     */
    QList<CargoNetSim::Backend::Path *> findTopPaths(
        const ScenarioDocument &doc,
        const ScenarioRegistry &registry,
        int                     n,
        QString                *err);
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
