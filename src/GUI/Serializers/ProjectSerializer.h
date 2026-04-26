#pragma once

#include <QString>
#include <memory>

namespace CargoNetSim {
namespace Backend {
namespace Scenario {
class ScenarioDocument;
} // namespace Scenario
} // namespace Backend

namespace GUI {

/**
 * @brief Thin GUI wrapper over backend scenario persistence operations.
 *
 * Kept under the legacy "ProjectSerializer" name because the GUI menu
 * action has historically been labelled "Open / Save Project". New
 * callers should prefer the backend persistence service directly; this
 * class exists so `BasicButtonController::openScenario` /
 * `saveScenario` (Task 19) have a single GUI-layer entry point for
 * YAML round-trip that callers can extend with GUI-specific concerns
 * (status-bar messages, recent-files list, etc.) without reaching into
 * the backend.
 *
 * Both methods are stateless / static. Errors are reported via an
 * optional out-parameter so callers can surface the message to the
 * user; passing nullptr is also valid and suppresses the message.
 */
class ProjectSerializer
{
public:
    /// Load a scenario YAML into a new `ScenarioDocument`. Returns
    /// nullptr on failure; if @p error is non-null, it is populated
    /// with a human-readable reason.
    static std::unique_ptr<Backend::Scenario::ScenarioDocument>
    loadProject(const QString &path, QString *error = nullptr);

    /// Save a `ScenarioDocument` to YAML. Returns false on failure; if
    /// @p error is non-null, it is populated with the reason.
    static bool
    saveProject(const Backend::Scenario::ScenarioDocument &doc,
                const QString                             &path,
                QString                                   *error = nullptr);
};

} // namespace GUI
} // namespace CargoNetSim
