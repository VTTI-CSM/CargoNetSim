#pragma once

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

/// Per-mode BPR volume-delay parameters.
/// Mirrors TerminalSim terminal.h:31-35.
struct ModeDelayParams
{
    double alpha = 0.5;
    double beta  = 2.0;
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
