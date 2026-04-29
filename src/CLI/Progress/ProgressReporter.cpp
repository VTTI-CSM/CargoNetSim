#include "ProgressReporter.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Commons/TransportationMode.h"

#include <QIODevice>
#include <QString>
#include <QStringList>

#include <cstdio>

namespace CargoNetSim {
namespace Cli {

namespace {
// 5% of progress OR 2s of wall time — either fires a tick.
constexpr double kPercentThreshold  = 5.0;
constexpr qint64 kElapsedThresholdMs = 2000;

using CargoNetSim::Backend::Scenario::PathLifecycleState;
using CargoNetSim::Backend::Scenario::SegmentLifecycleState;

QString pathLifecycleName(PathLifecycleState lifecycle)
{
    switch (lifecycle)
    {
    case PathLifecycleState::Pending:
        return QStringLiteral("pending");
    case PathLifecycleState::Running:
        return QStringLiteral("running");
    case PathLifecycleState::WaitingForTerminalHandoff:
        return QStringLiteral("terminal-handoff");
    case PathLifecycleState::WaitingForTerminalProcessing:
        return QStringLiteral("terminal-processing");
    case PathLifecycleState::ReadyForNextSegment:
        return QStringLiteral("ready-next");
    case PathLifecycleState::Paused:
        return QStringLiteral("paused");
    case PathLifecycleState::Skipped:
        return QStringLiteral("skipped");
    case PathLifecycleState::Completed:
        return QStringLiteral("completed");
    case PathLifecycleState::Failed:
        return QStringLiteral("failed");
    }
    return QStringLiteral("unknown");
}

QString segmentLifecycleName(SegmentLifecycleState lifecycle)
{
    switch (lifecycle)
    {
    case SegmentLifecycleState::Pending:
        return QStringLiteral("pending");
    case SegmentLifecycleState::Dispatched:
        return QStringLiteral("dispatched");
    case SegmentLifecycleState::VehicleRunning:
        return QStringLiteral("running");
    case SegmentLifecycleState::VehicleArrived:
        return QStringLiteral("arrived");
    case SegmentLifecycleState::UnloadCompleted:
        return QStringLiteral("unloaded");
    case SegmentLifecycleState::TerminalHandoffCompleted:
        return QStringLiteral("handoff");
    case SegmentLifecycleState::TerminalProcessing:
        return QStringLiteral("terminal");
    case SegmentLifecycleState::ReadyForPickup:
        return QStringLiteral("ready");
    case SegmentLifecycleState::Skipped:
        return QStringLiteral("skipped");
    case SegmentLifecycleState::Completed:
        return QStringLiteral("completed");
    case SegmentLifecycleState::Failed:
        return QStringLiteral("failed");
    }
    return QStringLiteral("unknown");
}

QString focusKeyFor(
    const Backend::Scenario::ExecutionProgressSnapshot &snapshot)
{
    QStringList parts;
    for (const auto &path : snapshot.paths)
    {
        if (!path.executable)
            continue;

        parts.append(QStringLiteral("%1:%2:%3")
                         .arg(path.pathIdentity,
                              QString::number(path.activeSegmentIndex),
                              pathLifecycleName(path.lifecycle)));
    }
    return parts.join(QLatin1Char('|'));
}

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

void ProgressReporter::setVerbose(bool verbose)
{
    m_verbose = verbose;
}

void ProgressReporter::report(double currentTimeSec, double percent)
{
    if (m_quiet) return;

    const bool movedEnough =
        (percent - m_lastPercent) >= kPercentThreshold;
    const bool elapsedEnough =
        m_verbose
        && m_lastEmitTimer.elapsed() >= kElapsedThresholdMs;
    if (!movedEnough && !elapsedEnough) return;

    m_lastPercent = percent;
    m_lastEmitTimer.restart();
    *m_out << QStringLiteral("  [%1%]  t=%2 s\n")
                  .arg(percent,        5, 'f', 1)
                  .arg(currentTimeSec, 0, 'f', 1);
    m_out->flush();
}

void ProgressReporter::reportSnapshot(
    double currentTimeSec,
    const Backend::Scenario::ExecutionProgressSnapshot &snapshot)
{
    if (m_quiet) return;

    const double percent = snapshot.aggregatePercent;
    const QString focusKey =
        m_verbose ? focusKeyFor(snapshot) : QString();
    const bool movedEnough =
        (percent - m_lastPercent) >= kPercentThreshold;
    const bool focusChanged =
        m_verbose && focusKey != m_lastFocusKey;
    const bool elapsedEnough =
        m_verbose
        && m_lastEmitTimer.elapsed() >= kElapsedThresholdMs;

    if (!movedEnough && !focusChanged && !elapsedEnough)
        return;

    m_lastPercent = percent;
    m_lastFocusKey = focusKey;
    m_lastEmitTimer.restart();

    *m_out << QStringLiteral(
                  "  [%1%]  t=%2 s  paths=%3/%4  segments=%5/%6\n")
                  .arg(percent,        5, 'f', 1)
                  .arg(currentTimeSec, 0, 'f', 1)
                  .arg(snapshot.completedExecutablePaths)
                  .arg(snapshot.executablePathCount)
                  .arg(snapshot.completedExecutableSegments)
                  .arg(snapshot.executableSegmentCount);

    if (m_verbose)
    {
        for (const auto &path : snapshot.paths)
        {
            if (!path.executable)
            {
                *m_out << QStringLiteral(
                              "    path rank=%1  skipped  %2\n")
                              .arg(path.rank)
                              .arg(path.message);
                continue;
            }

            const auto modeName =
                Backend::TransportationTypes::toString(
                    path.activeMode);
            QString segmentStatus =
                path.activeSegmentIndex >= 0
                    ? QStringLiteral("%1 %2 -> %3 segment %4/%5")
                          .arg(modeName,
                               path.activeStartTerminalId,
                               path.activeEndTerminalId)
                          .arg(path.activeSegmentIndex + 1)
                          .arg(path.totalSegments)
                    : QStringLiteral("no active segment");

            QString lifecycle = pathLifecycleName(path.lifecycle);
            for (const auto &segment : path.segments)
            {
                if (segment.segmentIndex == path.activeSegmentIndex)
                {
                    lifecycle =
                        segmentLifecycleName(segment.lifecycle);
                    break;
                }
            }

            *m_out << QStringLiteral(
                          "    path rank=%1  %2%  %3  %4  %5\n")
                          .arg(path.rank)
                          .arg(path.percent, 5, 'f', 1)
                          .arg(lifecycle,
                               segmentStatus,
                               path.pathIdentity);
        }
    }

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
