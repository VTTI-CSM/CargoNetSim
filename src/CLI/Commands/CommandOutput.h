#pragma once

#include <QIODevice>
#include <QTextStream>

#include <cstdio>

namespace CargoNetSim {
namespace Cli {

/**
 * @brief Output-sink dispatcher used by every CLI command that
 *        supports QIODevice-based test injection.
 *
 * Plan 5 Task 12: extracted from `HelpVersion.cpp` (Task 11) so every
 * future command with the same stdout/stderr-vs-injected-QIODevice
 * pattern reuses one implementation. If @p sink is non-null, write
 * @p content through a `QTextStream` attached to it (tests own the
 * device, typically a `QBuffer`). Otherwise write to the C @p fallback
 * stream — normally `stdout` or `stderr`, depending on the command's
 * natural channel.
 *
 * Templated over @p T so both `QString` and `QByteArray` work at call
 * sites (`QTextStream::operator<<` has overloads for both). Inline so
 * each TU emits its own out-of-line body if needed — no shared .cpp.
 */
template <typename T>
inline void streamToOr(QIODevice *sink, FILE *fallback, const T &content)
{
    if (sink)
        QTextStream(sink) << content;
    else
        QTextStream(fallback, QIODevice::WriteOnly) << content;
}

} // namespace Cli
} // namespace CargoNetSim
