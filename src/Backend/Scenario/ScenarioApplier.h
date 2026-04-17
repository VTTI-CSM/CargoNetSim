#pragma once

#include "ScenarioDocument.h"
#include "ScenarioRegistry.h"
#include <QString>

namespace CargoNetSim
{
class CargoNetSimController;

namespace Backend
{
namespace Scenario
{

/// Materializes a validated ScenarioDocument into the backend controllers
/// and populates a ScenarioRegistry with the constructed Backend::Terminal
/// objects and the truck-fleet spec. After 2026-04-14 (Task 0 retrofit),
/// `apply` also constructs each origin terminal's `ContainerCore::Container`
/// pool and installs it onto the document via
/// `ScenarioDocument::setOriginContainers(id, …)` — hence the non-const
/// `ScenarioDocument &` parameter.
///
/// Stateless: all methods are static. Idempotent: clears existing state
/// first so repeated calls produce identical results.
///
/// On failure, returns false and writes the reason into *error (if non-null).
/// The backend controller may be in a partially-mutated state — callers
/// should treat a failed apply as fatal and not attempt to simulate.
class ScenarioApplier
{
public:
    static bool apply(ScenarioDocument &doc,
                      CargoNetSim::CargoNetSimController &controller,
                      ScenarioRegistry &registry,
                      QString *error = nullptr);

private:
    static void clearAll(CargoNetSim::CargoNetSimController &controller,
                         ScenarioRegistry &registry);

    static bool applyRegions         (const ScenarioDocument &doc,
                                      CargoNetSim::CargoNetSimController &controller,
                                      QString *error);
    static bool applyNetworks        (const ScenarioDocument &doc,
                                      CargoNetSim::CargoNetSimController &controller,
                                      QString *error);
    static bool applyTerminals       (const ScenarioDocument &doc,
                                      ScenarioRegistry &registry,
                                      QString *error);
    static bool applyFleet           (const ScenarioDocument &doc,
                                      CargoNetSim::CargoNetSimController &controller,
                                      ScenarioRegistry &registry,
                                      QString *error);
    static bool applySettings        (const ScenarioDocument &doc,
                                      CargoNetSim::CargoNetSimController &controller,
                                      QString *error);

    /// Materializes the origin container pools described by each
    /// terminal's `properties.initial_container_count` into
    /// `ScenarioDocument::m_containersByTerminal` (via the per-terminal
    /// setter). Must run AFTER `applyTerminals` because terminal ids are
    /// referenced by their own property map. Non-const `doc` by design
    /// — this is the only mutator among the apply helpers.
    static bool applyOriginContainers(ScenarioDocument &doc,
                                      QString *error);
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
