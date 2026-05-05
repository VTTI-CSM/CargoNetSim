#pragma once

#include "CLI/Subcommand.h"

class QIODevice;

namespace CargoNetSim {
namespace Cli {

/**
 * @brief `cargonetsim-cli preview [--all-errors] <scenario.yml>` — load, validate,
 *        build a preview-linked document and emit it as indented JSON
 *        to stdout.
 *
 * Large validation issue sets are grouped by default. `--all-errors`
 * restores one-line-per-issue output.
 *
 * Plan 5 Task 14. Preview linking is owned by the backend
 * `ScenarioPreviewService`: graph-dependent inputs are loaded into a
 * stand-alone preview registry, auto-rules for linkages / connections /
 * global links are evaluated there, and the results are folded into a
 * copy of the document for JSON emission. The live backend is never
 * touched.
 *
 * Exit codes:
 *   * `Success (0)`          — JSON emitted.
 *   * `ValidationFailed (2)` — parse failed, or validator reported
 *     at least one `Severity::Error`.
 *   * `RunFailed (1)`        — all earlier stages succeeded but
 *     `loadNetworksForPreview` failed (typically a missing network
 *     input file referenced by the YAML).
 *   * `BadArgs (64)`         — no scenario path supplied.
 *
 * Output sinks:
 *   * `outSink` — stdout for the JSON document (test-injection seam).
 *   * `errSink` — stderr for parse errors, validator issues, and
 *                 network-load failures (test-injection seam).
 *   * Both default to nullptr (real stdout / stderr). Injected devices
 *     must outlive the command instance.
 */
class PreviewCommand : public Subcommand
{
public:
    explicit PreviewCommand(QIODevice *outSink = nullptr,
                            QIODevice *errSink = nullptr);

    int execute(const QStringList &args) override;

private:
    QIODevice *m_out;  // not owned; nullptr → stdout
    QIODevice *m_err;  // not owned; nullptr → stderr
};

} // namespace Cli
} // namespace CargoNetSim
