#pragma once

#include <QElapsedTimer>
#include <QTextStream>

#include <memory>

class QIODevice;

namespace CargoNetSim {
namespace Cli {

/**
 * @brief Rate-limited progress ticks for the CLI, streamed to stderr.
 *
 * Plan 5 Task 8. Consumes `ScenarioRuntime::progressChanged(current,
 * percent)` via a Qt signal connection in `RunCommand` (Task 17) and
 * prints a short status line every 5% of progress OR every 2 seconds
 * of wall time, whichever comes first. Spec §8.3.
 *
 * Output format (stderr, one line per tick):
 * @code
 *   [ 15.0%]  t=14400.0 s
 * @endcode
 * and one terminal line when the simulation ends:
 * @code
 *   [100.0%]  done
 * @endcode
 *
 * Stateful: tracks the last emitted percent and the last emission
 * wall-clock so the rate-limit predicate can suppress tick-storms
 * when the simulator updates rapidly. State is private and bounded;
 * no thread-safety is provided — the expected use is from a single
 * (main) thread, driven off Qt's queued-connection dispatch.
 *
 * Quiet mode (`quiet=true`) suppresses EVERY emission including the
 * final `done` line; callers that want silence pipe `2>/dev/null`
 * or construct with `quiet=true`.
 *
 * Testing seam: passing a non-null `QIODevice *out` routes emissions
 * to that device instead of stderr. Tests own the device (typically
 * a `QBuffer`); the reporter stores it by pointer and does NOT take
 * ownership. Production callers use the default (wraps stderr).
 */
class ProgressReporter
{
public:
    /// @param quiet  Suppress all output when true.
    /// @param out    Optional output sink. `nullptr` (default) wraps
    ///               stderr — the production path. Non-null is a
    ///               test-only injection point; the device must
    ///               outlive the reporter.
    explicit ProgressReporter(bool quiet = false, QIODevice *out = nullptr);

    /// Emit a progress tick at @p percent (0..100) if the rate-limit
    /// predicate allows. Always emits on the first call (post-ctor).
    /// @p currentTimeSec is the simulation clock, not wall time.
    void report(double currentTimeSec, double percent);

    /// Force a final line regardless of rate limiting. Still honours
    /// quiet mode. Use exactly once per simulation, after the last
    /// `report()`, so the last line on stderr reflects the terminal
    /// progress value.
    void emitFinal(double percent);

private:
    bool                         m_quiet;
    double                       m_lastPercent = -100.0;  // first call always emits
    QElapsedTimer                m_lastEmitTimer;
    std::unique_ptr<QTextStream> m_out;
};

} // namespace Cli
} // namespace CargoNetSim
