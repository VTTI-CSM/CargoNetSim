#include "ProgressReporter.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Commons/TransportationMode.h"

#include <QIODevice>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <cstdio>
#if defined(Q_OS_UNIX)
#include <unistd.h>
#endif

namespace CargoNetSim {
namespace Cli {

namespace {
// 5% of progress OR 2s of wall time — either fires a tick.
constexpr double kPercentThreshold  = 5.0;
constexpr qint64 kElapsedThresholdMs = 2000;

using CargoNetSim::Backend::Scenario::PathLifecycleState;
using CargoNetSim::Backend::Scenario::SegmentLifecycleState;
using CargoNetSim::Backend::TransportationTypes;

bool stderrIsTerminal()
{
#if defined(Q_OS_UNIX)
    return ::isatty(STDERR_FILENO);
#else
    return false;
#endif
}

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

const Backend::Scenario::PathProgressSnapshot *focusedPath(
    const Backend::Scenario::ExecutionProgressSnapshot &snapshot)
{
    const Backend::Scenario::PathProgressSnapshot *fallback = nullptr;
    for (const auto &path : snapshot.paths)
    {
        if (!path.executable)
            continue;
        if (!fallback)
            fallback = &path;
        if (path.lifecycle != PathLifecycleState::Completed)
            return &path;
    }
    return fallback;
}

QString alternativeScope(
    const Backend::Scenario::ExecutionProgressSnapshot &snapshot)
{
    if (snapshot.alternativeCount <= 1
        || snapshot.activeAlternativeIndex < 0)
        return QString();

    return QStringLiteral("alternative=%1/%2")
        .arg(snapshot.activeAlternativeIndex + 1)
        .arg(snapshot.alternativeCount);
}

QString compactSnapshotLine(
    double currentTimeSec,
    const Backend::Scenario::ExecutionProgressSnapshot &snapshot)
{
    QStringList parts;
    parts.append(QStringLiteral("[%1%]")
                     .arg(snapshot.aggregatePercent, 5, 'f', 1));

    const QString alternative = alternativeScope(snapshot);
    if (!alternative.isEmpty())
        parts.append(alternative);

    parts.append(QStringLiteral("t=%1 s")
                     .arg(currentTimeSec, 0, 'f', 1));

    if (const auto *path = focusedPath(snapshot))
    {
        if (path->pathId >= 0)
            parts.append(QStringLiteral("path=%1").arg(path->pathId));
        if (path->rank >= 0)
            parts.append(QStringLiteral("rank=%1").arg(path->rank));
        if (path->activeSegmentIndex >= 0 && path->totalSegments > 0)
        {
            parts.append(QStringLiteral("segment=%1/%2")
                             .arg(path->activeSegmentIndex + 1)
                             .arg(path->totalSegments));
        }
        if (path->activeMode != TransportationTypes::
                TransportationMode::Any)
        {
            parts.append(TransportationTypes::toString(path->activeMode));
        }
    }
    else
    {
        parts.append(QStringLiteral("paths=%1/%2")
                         .arg(snapshot.completedExecutablePaths)
                         .arg(snapshot.executablePathCount));
        parts.append(QStringLiteral("segments=%1/%2")
                         .arg(snapshot.completedExecutableSegments)
                         .arg(snapshot.executableSegmentCount));
    }

    return QStringLiteral("  ") + parts.join(QStringLiteral("  "));
}

QString aggregateProgressLine(double currentTimeSec, double percent)
{
    return QStringLiteral("  [%1%]  t=%2 s")
        .arg(percent,        5, 'f', 1)
        .arg(currentTimeSec, 0, 'f', 1);
}

} // namespace

ProgressReporter::ProgressReporter(bool quiet, QIODevice *out,
                                   ProgressRenderMode renderMode)
    : m_quiet(quiet)
    , m_renderMode(
          renderMode == ProgressRenderMode::Auto
              ? (out == nullptr && stderrIsTerminal()
                     ? ProgressRenderMode::SingleLine
                     : ProgressRenderMode::AppendLines)
              : renderMode)
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
    const QString line = aggregateProgressLine(currentTimeSec, percent);
    if (m_renderMode == ProgressRenderMode::SingleLine)
    {
        *m_out << QStringLiteral("\r\033[2K") << line;
        m_liveLineActive = true;
    }
    else
    {
        *m_out << line << QLatin1Char('\n');
    }
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

    if (m_renderMode == ProgressRenderMode::SingleLine)
    {
        *m_out << QStringLiteral("\r\033[2K")
               << compactSnapshotLine(currentTimeSec, snapshot);
        m_liveLineActive = true;
        m_out->flush();
        return;
    }

    const QString scope = alternativeScope(snapshot).isEmpty()
        ? QString()
        : QStringLiteral("  %1").arg(alternativeScope(snapshot));

    *m_out << QStringLiteral(
                  "  [%1%]  t=%2 s%3  paths=%4/%5  segments=%6/%7\n")
                  .arg(percent,        5, 'f', 1)
                  .arg(currentTimeSec, 0, 'f', 1)
                  .arg(scope)
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
    const QString line = QStringLiteral("  [%1%]  done")
                             .arg(percent, 5, 'f', 1);
    if (m_renderMode == ProgressRenderMode::SingleLine)
    {
        *m_out << QStringLiteral("\r\033[2K") << line
               << QLatin1Char('\n');
        m_liveLineActive = false;
    }
    else
    {
        *m_out << line << QLatin1Char('\n');
    }
    m_out->flush();
}

} // namespace Cli
} // namespace CargoNetSim
