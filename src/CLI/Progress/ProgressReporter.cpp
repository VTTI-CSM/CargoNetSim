#include "ProgressReporter.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Commons/TransportationMode.h"

#include <QIODevice>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#if defined(Q_OS_UNIX)
#include <sys/ioctl.h>
#include <unistd.h>
#endif
#if defined(Q_OS_WIN)
#include <windows.h>
#endif

namespace CargoNetSim {
namespace Cli {

namespace {
// 5% of progress OR a renderer-specific wall interval — either fires a tick.
constexpr double kPercentThreshold  = 5.0;
constexpr qint64 kElapsedThresholdMs = 2000;
constexpr qint64 kAppendSnapshotThresholdMs = 30000;
constexpr int    kFallbackTerminalColumns = 120;

using CargoNetSim::Backend::Scenario::PathLifecycleState;
using CargoNetSim::Backend::Scenario::SegmentLifecycleState;
using CargoNetSim::Backend::TransportationTypes;

bool stderrIsTerminal()
{
#if defined(Q_OS_UNIX)
    return ::isatty(STDERR_FILENO);
#elif defined(Q_OS_WIN)
    return ::GetFileType(::GetStdHandle(STD_ERROR_HANDLE))
        == FILE_TYPE_CHAR;
#else
    return false;
#endif
}

bool stderrSupportsAnsi()
{
#if defined(Q_OS_WIN)
    HANDLE handle = ::GetStdHandle(STD_ERROR_HANDLE);
    if (handle == INVALID_HANDLE_VALUE)
        return false;

    DWORD mode = 0;
    if (!::GetConsoleMode(handle, &mode))
        return false;

    if ((mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0)
        return true;

    return ::SetConsoleMode(
        handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#else
    return stderrIsTerminal();
#endif
}

int detectedTerminalColumns()
{
#if defined(Q_OS_UNIX)
    winsize size{};
    if (::ioctl(STDERR_FILENO, TIOCGWINSZ, &size) == 0
        && size.ws_col > 0)
    {
        return static_cast<int>(size.ws_col);
    }
#elif defined(Q_OS_WIN)
    CONSOLE_SCREEN_BUFFER_INFO info{};
    HANDLE handle = ::GetStdHandle(STD_ERROR_HANDLE);
    if (handle != INVALID_HANDLE_VALUE
        && ::GetConsoleScreenBufferInfo(handle, &info))
    {
        return info.srWindow.Right - info.srWindow.Left + 1;
    }
#endif

    const QByteArray columns = qgetenv("COLUMNS");
    bool ok = false;
    const int envColumns = QString::fromLatin1(columns).toInt(&ok);
    return ok && envColumns > 0 ? envColumns
                                : kFallbackTerminalColumns;
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
                         .arg(path.executionPathKey,
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

QString pathStateCode(PathLifecycleState lifecycle)
{
    switch (lifecycle)
    {
    case PathLifecycleState::Pending:
        return QStringLiteral("P");
    case PathLifecycleState::Running:
        return QStringLiteral("R");
    case PathLifecycleState::WaitingForTerminalHandoff:
        return QStringLiteral("H");
    case PathLifecycleState::WaitingForTerminalProcessing:
        return QStringLiteral("T");
    case PathLifecycleState::ReadyForNextSegment:
        return QStringLiteral("N");
    case PathLifecycleState::Paused:
        return QStringLiteral("Z");
    case PathLifecycleState::Skipped:
        return QStringLiteral("S");
    case PathLifecycleState::Completed:
        return QStringLiteral("C");
    case PathLifecycleState::Failed:
        return QStringLiteral("F");
    }
    return QStringLiteral("?");
}

QString modeCode(TransportationTypes::TransportationMode mode)
{
    switch (mode)
    {
    case TransportationTypes::TransportationMode::Train:
        return QStringLiteral("TRN");
    case TransportationTypes::TransportationMode::Ship:
        return QStringLiteral("SHP");
    case TransportationTypes::TransportationMode::Truck:
        return QStringLiteral("TRK");
    case TransportationTypes::TransportationMode::Any:
        return QStringLiteral("-");
    }
    return QStringLiteral("-");
}

QString compactNumber(double value, int decimals = 1)
{
    const double absValue = std::abs(value);
    struct Suffix
    {
        double scale;
        const char *suffix;
    };
    static constexpr Suffix suffixes[] = {
        {1.0e9, "B"},
        {1.0e6, "M"},
        {1.0e3, "K"},
    };

    for (const auto &suffix : suffixes)
    {
        if (absValue >= suffix.scale)
        {
            return QStringLiteral("%1%2")
                .arg(value / suffix.scale, 0, 'f', decimals)
                .arg(QLatin1String(suffix.suffix));
        }
    }

    return QString::number(value, 'f', absValue < 10.0 ? 1 : 0);
}

QString compactMoney(double value, bool available)
{
    return available ? compactNumber(value) : QStringLiteral("-");
}

QString compactDistance(double valueKm, bool available)
{
    return available ? compactNumber(valueKm) : QStringLiteral("-");
}

QString compactHours(double valueHours, bool available)
{
    if (!available)
        return QStringLiteral("-");

    if (valueHours >= 72.0)
    {
        const int days = static_cast<int>(valueHours / 24.0);
        const int hours = static_cast<int>(valueHours) % 24;
        return QStringLiteral("%1d%2h").arg(days).arg(hours);
    }

    return QString::number(valueHours, 'f', valueHours < 10.0 ? 1 : 0);
}

QString compactSimTime(double seconds)
{
    const double hours = seconds / 3600.0;
    if (hours >= 24.0)
    {
        const int days = static_cast<int>(hours / 24.0);
        const int remainingHours = static_cast<int>(hours) % 24;
        return QStringLiteral("%1d %2h").arg(days).arg(remainingHours);
    }
    if (hours >= 1.0)
        return QStringLiteral("%1h").arg(hours, 0, 'f', 1);
    return QStringLiteral("%1m").arg(seconds / 60.0, 0, 'f', 0);
}

bool isRunningPath(const Backend::Scenario::PathProgressSnapshot &path)
{
    return path.executable
        && path.lifecycle == PathLifecycleState::Running;
}

QString spinnerDots(int frame)
{
    return QString(frame % 3 + 1, QLatin1Char('.'));
}

QString pathStateIndicator(
    const Backend::Scenario::PathProgressSnapshot &path,
    int spinnerFrame)
{
    return isRunningPath(path)
        ? spinnerDots(spinnerFrame)
        : pathStateCode(path.lifecycle);
}

struct ProgressColumn
{
    QString key;
    QString header;
    int width = 0;
    int dropPriority = 0;
    bool rightAlign = false;
};

int tableWidth(const QVector<ProgressColumn> &columns)
{
    int width = qMax(0, columns.size() - 1);
    for (const auto &column : columns)
        width += column.width;
    return width;
}

QVector<ProgressColumn> selectedColumns(int terminalColumns)
{
    QVector<ProgressColumn> columns = {
        {QStringLiteral("path"), QStringLiteral("Path"), 4, 0, true},
        {QStringLiteral("rank"), QStringLiteral("Rk"),   3, 1, true},
        {QStringLiteral("state"), QStringLiteral("S"),   3, 0, false},
        {QStringLiteral("seg"), QStringLiteral("Seg"),   5, 0, false},
        {QStringLiteral("mode"), QStringLiteral("Md"),   3, 0, false},
        {QStringLiteral("network"), QStringLiteral("Network"), 12, 4, false},
        {QStringLiteral("predCost"), QStringLiteral("P$"), 7, 0, true},
        {QStringLiteral("actualCost"), QStringLiteral("A$"), 7, 0, true},
        {QStringLiteral("predKm"), QStringLiteral("Pkm"), 6, 0, true},
        {QStringLiteral("actualKm"), QStringLiteral("Akm"), 6, 0, true},
        {QStringLiteral("predHours"), QStringLiteral("Ph"), 6, 2, true},
        {QStringLiteral("actualHours"), QStringLiteral("Ah"), 6, 2, true},
        {QStringLiteral("containers"), QStringLiteral("Cnt"), 4, 1, true},
    };

    for (int priority = 4;
         priority > 0 && tableWidth(columns) > terminalColumns;
         --priority)
    {
        for (int i = columns.size() - 1;
             i >= 0 && tableWidth(columns) > terminalColumns;
             --i)
        {
            if (columns[i].dropPriority == priority)
                columns.removeAt(i);
        }
    }

    return columns;
}

QString fitCell(QString value, int width, bool rightAlign)
{
    if (width <= 0)
        return QString();

    if (value.size() > width)
    {
        if (width == 1)
            value = value.left(1);
        else
            value = value.left(width - 1) + QLatin1Char('~');
    }

    return rightAlign ? value.rightJustified(width, QLatin1Char(' '))
                      : value.leftJustified(width, QLatin1Char(' '));
}

QString cellValue(
    const ProgressColumn &column,
    const Backend::Scenario::PathProgressSnapshot &path,
    int spinnerFrame)
{
    if (column.key == QLatin1String("path"))
        return path.pathId >= 0 ? QString::number(path.pathId)
                                : QStringLiteral("-");
    if (column.key == QLatin1String("rank"))
        return path.rank >= 0 ? QString::number(path.rank)
                              : QStringLiteral("-");
    if (column.key == QLatin1String("state"))
        return pathStateIndicator(path, spinnerFrame);
    if (column.key == QLatin1String("seg"))
    {
        return path.activeSegmentIndex >= 0 && path.totalSegments > 0
            ? QStringLiteral("%1/%2")
                  .arg(path.activeSegmentIndex + 1)
                  .arg(path.totalSegments)
            : QStringLiteral("-");
    }
    if (column.key == QLatin1String("mode"))
        return modeCode(path.activeMode);
    if (column.key == QLatin1String("network"))
        return path.activeNetworkName.isEmpty()
            ? QStringLiteral("-")
            : path.activeNetworkName;
    if (column.key == QLatin1String("predCost"))
        return compactMoney(path.predictedTotalCostUsd,
                            path.predictedTotalCostUsd > 0.0);
    if (column.key == QLatin1String("actualCost"))
        return compactMoney(path.actualTotalCostUsd,
                            path.actualCostsAvailable);
    if (column.key == QLatin1String("predKm"))
        return compactDistance(path.predictedDistanceKm,
                               path.predictedDistanceKm > 0.0);
    if (column.key == QLatin1String("actualKm"))
        return compactDistance(path.actualDistanceKm,
                               path.actualMetricsAvailable);
    if (column.key == QLatin1String("predHours"))
        return compactHours(path.predictedTravelTimeHours,
                            path.predictedTravelTimeHours > 0.0);
    if (column.key == QLatin1String("actualHours"))
        return compactHours(path.actualTravelTimeHours,
                            path.actualMetricsAvailable);
    if (column.key == QLatin1String("containers"))
        return QString::number(qMax(0, path.effectiveContainerCount));
    return QString();
}

QString renderRow(
    const QVector<ProgressColumn> &columns,
    const Backend::Scenario::PathProgressSnapshot *path,
    int spinnerFrame)
{
    QStringList cells;
    for (const auto &column : columns)
    {
        const QString value = path
            ? cellValue(column, *path, spinnerFrame)
            : column.header;
        cells.append(fitCell(value, column.width, column.rightAlign));
    }
    return cells.join(QLatin1Char(' '));
}

QString separatorRow(const QVector<ProgressColumn> &columns)
{
    QStringList cells;
    for (const auto &column : columns)
        cells.append(QString(column.width, QLatin1Char('-')));
    return cells.join(QLatin1Char(' '));
}

QStringList renderProgressTable(
    double currentTimeSec,
    const Backend::Scenario::ExecutionProgressSnapshot &snapshot,
    int spinnerFrame,
    int terminalColumns)
{
    const auto columns = selectedColumns(qMax(50, terminalColumns));
    QStringList lines;
    QString title = QStringLiteral(
        "[%1%] CargoNetSim t=%2 | paths=%3/%4 | segments=%5/%6")
        .arg(snapshot.aggregatePercent, 5, 'f', 1)
        .arg(compactSimTime(currentTimeSec))
        .arg(snapshot.completedExecutablePaths)
        .arg(snapshot.executablePathCount)
        .arg(snapshot.completedExecutableSegments)
        .arg(snapshot.executableSegmentCount);

    const QString alternative = alternativeScope(snapshot);
    if (!alternative.isEmpty())
        title += QStringLiteral(" | %1").arg(alternative);

    lines.append(title.left(qMax(1, terminalColumns)));
    lines.append(renderRow(columns, nullptr, spinnerFrame));
    lines.append(separatorRow(columns));
    for (const auto &path : snapshot.paths)
        lines.append(renderRow(columns, &path, spinnerFrame));

    lines.append(QStringLiteral(
        "S: . .. ...=running P=pending H=handoff T=terminal N=next C=done S=skipped F=failed"));
    lines.append(QStringLiteral(
        "A$ and Akm are accrued reported actuals so far."));

    for (auto &line : lines)
    {
        if (line.size() > terminalColumns)
            line = line.left(qMax(1, terminalColumns));
    }

    return lines;
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
                         && stderrSupportsAnsi()
                     ? ProgressRenderMode::LiveTable
                     : (out == nullptr
                            ? ProgressRenderMode::AppendTable
                            : ProgressRenderMode::AppendLines))
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

    m_lastSnapshot = snapshot;
    m_lastSnapshotTimeSec = currentTimeSec;
    m_hasLastSnapshot = true;

    const double percent = snapshot.aggregatePercent;
    const QString focusKey =
        m_verbose ? focusKeyFor(snapshot) : QString();
    const bool movedEnough =
        (percent - m_lastPercent) >= kPercentThreshold;
    const bool focusChanged =
        m_verbose && focusKey != m_lastFocusKey;
    const bool liveTableElapsedEnough =
        m_renderMode == ProgressRenderMode::LiveTable
        && m_lastEmitTimer.elapsed() >= kElapsedThresholdMs;
    const bool appendSnapshotElapsedEnough =
        (m_renderMode == ProgressRenderMode::AppendLines
         || m_renderMode == ProgressRenderMode::AppendTable)
        && m_lastEmitTimer.elapsed() >= kAppendSnapshotThresholdMs;
    const bool elapsedEnough =
        (m_verbose
         && m_lastEmitTimer.elapsed() >= kElapsedThresholdMs)
        || liveTableElapsedEnough
        || appendSnapshotElapsedEnough;

    if (!movedEnough && !focusChanged && !elapsedEnough)
        return;

    m_lastPercent = percent;
    m_lastFocusKey = focusKey;
    m_lastEmitTimer.restart();

    if (m_renderMode == ProgressRenderMode::LiveTable)
    {
        repaintLastSnapshot();
        return;
    }

    if (m_renderMode == ProgressRenderMode::AppendTable)
    {
        const auto lines = renderProgressTable(
            currentTimeSec, snapshot, m_spinnerFrame,
            detectedTerminalColumns());
        for (const auto &line : lines)
            *m_out << line << QLatin1Char('\n');
        *m_out << QLatin1Char('\n');
        m_spinnerFrame = (m_spinnerFrame + 1) % 3;
        m_out->flush();
        return;
    }

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
                               path.executionPathKey);
        }
    }

    m_out->flush();
}

void ProgressReporter::repaintLastSnapshot()
{
    if (m_quiet
        || m_renderMode != ProgressRenderMode::LiveTable
        || !m_hasLastSnapshot)
    {
        return;
    }

    const auto lines = renderProgressTable(
        m_lastSnapshotTimeSec, m_lastSnapshot, m_spinnerFrame,
        detectedTerminalColumns());

    if (m_liveRowsRendered > 0)
    {
        *m_out << QStringLiteral("\033[%1F")
                      .arg(m_liveRowsRendered);
    }

    for (const auto &line : lines)
    {
        *m_out << QStringLiteral("\033[2K") << line
               << QLatin1Char('\n');
    }

    m_liveRowsRendered = lines.size();
    m_liveLineActive = true;
    m_spinnerFrame = (m_spinnerFrame + 1) % 3;
    m_out->flush();
}

void ProgressReporter::emitFinal(double percent)
{
    qCDebug(lcCli) << "ProgressReporter::emitFinal: percent" << percent;
    if (m_quiet) return;
    const QString line = QStringLiteral("  [%1%]  done")
                             .arg(percent, 5, 'f', 1);
    if (m_renderMode == ProgressRenderMode::LiveTable)
    {
        *m_out << line << QLatin1Char('\n');
        m_liveRowsRendered = 0;
        m_liveLineActive = false;
    }
    else if (m_renderMode == ProgressRenderMode::SingleLine)
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
