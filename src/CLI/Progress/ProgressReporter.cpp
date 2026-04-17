#include "ProgressReporter.h"
#include "Backend/Commons/LogCategories.h"

#include <QIODevice>
#include <QString>

#include <cstdio>

namespace CargoNetSim {
namespace Cli {

namespace {
// 5% of progress OR 2s of wall time — either fires a tick.
constexpr double kPercentThreshold  = 5.0;
constexpr qint64 kElapsedThresholdMs = 2000;
} // namespace

ProgressReporter::ProgressReporter(bool quiet, QIODevice *out)
    : m_quiet(quiet)
    , m_out(out
                ? std::make_unique<QTextStream>(out)
                : std::make_unique<QTextStream>(stderr,
                                                QIODevice::WriteOnly))
{
    qCDebug(lcCli) << "ProgressReporter::ProgressReporter: quiet" << quiet;
    m_lastEmitTimer.start();
}

void ProgressReporter::report(double currentTimeSec, double percent)
{
    if (m_quiet) return;

    const bool movedEnough =
        (percent - m_lastPercent) >= kPercentThreshold;
    const bool elapsedEnough =
        m_lastEmitTimer.elapsed() >= kElapsedThresholdMs;
    if (!movedEnough && !elapsedEnough) return;

    m_lastPercent = percent;
    m_lastEmitTimer.restart();
    *m_out << QStringLiteral("  [%1%]  t=%2 s\n")
                  .arg(percent,        5, 'f', 1)
                  .arg(currentTimeSec, 0, 'f', 1);
    m_out->flush();
}

void ProgressReporter::emitFinal(double percent)
{
    qCDebug(lcCli) << "ProgressReporter::emitFinal: percent" << percent;
    if (m_quiet) return;
    *m_out << QStringLiteral("  [%1%]  done\n")
                  .arg(percent, 5, 'f', 1);
    m_out->flush();
}

} // namespace Cli
} // namespace CargoNetSim
