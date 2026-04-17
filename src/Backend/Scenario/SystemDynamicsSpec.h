#pragma once

#include "ModeDelayParams.h"

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

/// Optional per-terminal system-dynamics / BPR congestion params.
/// Defaults mirror TerminalSim terminal.h:43-61 exactly.
struct SystemDynamicsSpec
{
    bool   enabled              = false;
    double criticalUtilization  = 0.7;
    double congestionExponent   = 2.0;
    double congestionSensitivity = 1.0;
    double delaySensitivity     = 0.5;
    double maxServiceRate       = 100.0;

    ModeDelayParams shipDelay  { 0.5, 2.0 };
    ModeDelayParams truckDelay { 0.3, 2.5 };
    ModeDelayParams trainDelay { 0.8, 3.0 };

    double shipArrivalPenalty  = 14400.0;
    double truckArrivalPenalty =  1800.0;
    double trainArrivalPenalty =  7200.0;
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
