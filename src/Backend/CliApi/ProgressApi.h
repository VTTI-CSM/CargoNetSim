#pragma once

// Stable backend-owned execution progress contracts intentionally exposed to
// the CLI for progress rendering. Keep CLI code on this facade instead of
// including scenario execution internals directly.

#include "Backend/Scenario/ExecutionPlanTypes.h"
