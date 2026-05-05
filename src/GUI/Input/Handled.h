#pragma once

#include <cstdint>

namespace CargoNetSim::GUI::Input {

/**
 * Dispatch verdict returned by every handler in the input pipeline.
 *
 * Three values (not two) because "handler doesn't care" and "handler actively
 * refuses this input" are semantically different:
 *
 *   Yes         — event consumed; stop propagation, do not call Qt default.
 *   PassThrough — handler does not care; let the next layer try, and ultimately
 *                 fall through to Qt's default implementation.
 *   Reject      — input is actively wrong for the current mode; caller should
 *                 beep + status-bar message, and stop propagation.
 */
enum class Handled : uint8_t {
    Yes,
    PassThrough,
    Reject
};

} // namespace CargoNetSim::GUI::Input
