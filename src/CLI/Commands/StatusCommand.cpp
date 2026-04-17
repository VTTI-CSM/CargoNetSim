#include "StatusCommand.h"

#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <cstdio>

#include "Backend/Commons/LogCategories.h"
#include "CLI/Commands/CommandOutput.h"
#include "CLI/ExitCodes.h"
#include "CLI/Ipc/ControlChannel.h"
#include "CLI/Ipc/RuntimeStateFile.h"

namespace CargoNetSim {
namespace Cli {

namespace {

/// Per-stage wait for the `{"op":"status"}` probe. Keep it snug so
/// `status` does not appear to hang when one of many state files
/// is stale.
constexpr int kStatusProbeTimeoutMs = 500;

/// Single-line per entry + one indented line per field. Label column
/// is left-justified in 14 cols; the trailing padding aligns values
/// so users reading multiple entries see a clean column. The exact
/// padding width is part of the published format (tests pin the
/// `"progress:     16.7%"` substring).
QString formatHumanLine(const QString &label, const QString &value)
{
    return QStringLiteral("  %1%2\n").arg(label, -14).arg(value);
}

/// Render one state-file entry + its status response as a
/// human-readable block.
QString formatHumanEntry(const RuntimeStateEntry &entry,
                         const QJsonObject       &reply)
{
    QString buf = QStringLiteral("[%1] %2 (started %3)\n")
                      .arg(entry.pid)
                      .arg(entry.scenarioPath)
                      .arg(entry.startedAt.toString(Qt::ISODate));

    buf += formatHumanLine(
        QStringLiteral("state:"),
        reply.value(QStringLiteral("state")).toString());
    buf += formatHumanLine(
        QStringLiteral("current_time:"),
        QString::number(
            reply.value(QStringLiteral("current_time")).toDouble(), 'f', 1));
    buf += formatHumanLine(
        QStringLiteral("end_time:"),
        QString::number(
            reply.value(QStringLiteral("end_time")).toDouble(), 'f', 1));
    buf += formatHumanLine(
        QStringLiteral("progress:"),
        QString::number(
            reply.value(QStringLiteral("progress")).toDouble(), 'f', 1)
            + QStringLiteral("%"));
    return buf;
}

/// Merge a state-file entry with the server's response into one
/// JSON object. The state-file fields (pid, scenario_path,
/// started_at) carry the "who am I" metadata that the server cannot
/// know about itself; the server's reply carries the live runtime
/// fields. If a key collides the reply wins — it is the authority
/// for runtime state.
QJsonObject mergeEntryAndReply(const RuntimeStateEntry &entry,
                               const QJsonObject       &reply)
{
    QJsonObject merged = reply;
    merged[QStringLiteral("pid")]           = static_cast<qint64>(entry.pid);
    merged[QStringLiteral("scenario_path")] = entry.scenarioPath;
    merged[QStringLiteral("started_at")]    =
        entry.startedAt.toString(Qt::ISODate);
    return merged;
}

} // namespace

StatusCommand::StatusCommand(QIODevice *outSink)
    : m_out(outSink)
{
}

int StatusCommand::execute(const QStringList &args)
{
    qCDebug(lcCli) << "StatusCommand::execute: entry, args =" << args;

    const bool jsonMode = args.contains(QStringLiteral("--json"));
    qCDebug(lcCli) << "StatusCommand::execute: jsonMode =" << jsonMode;

    const auto entries = RuntimeStateFile::scan();
    qCDebug(lcCli) << "StatusCommand::execute: scan found"
                   << entries.size() << "state file(s)";
    if (entries.isEmpty())
    {
        qCInfo(lcCli) << "StatusCommand::execute: no running simulation found";
        streamToOr(m_out, stderr,
                   QStringLiteral("status: no running simulation\n"));
        return static_cast<int>(ExitCode::NoRunningSim);
    }

    // Aggregate every successful probe. Unresponsive entries are
    // silently skipped — their existence is implied by the state
    // files but their absence from the output tells the user "these
    // sockets are stale / dead". A future refactor could add a
    // `--include-stale` switch; for now, keep the output focused on
    // live simulations.
    QJsonArray jsonArr;
    QString    humanBuffer;
    int        responded = 0;

    for (const auto &entry : entries)
    {
        qCDebug(lcCli) << "StatusCommand::execute: probing pid =" << entry.pid
                       << ", socket =" << entry.socketName;
        const auto reply = ControlChannelClient::send(
            entry.socketName,
            QJsonObject{{QStringLiteral("op"), QStringLiteral("status")}},
            kStatusProbeTimeoutMs);
        if (!reply)
        {
            qCDebug(lcCli) << "StatusCommand::execute: pid" << entry.pid
                           << "did not respond (stale)";
            continue;  // stale / unresponsive — skip
        }

        ++responded;
        qCDebug(lcCli) << "StatusCommand::execute: pid" << entry.pid
                       << "responded, state ="
                       << reply->value(QStringLiteral("state")).toString();
        if (jsonMode)
            jsonArr.append(mergeEntryAndReply(entry, *reply));
        else
            humanBuffer += formatHumanEntry(entry, *reply);
    }

    if (responded == 0)
    {
        // Scan found entries but all were stale / unreachable.
        qCInfo(lcCli) << "StatusCommand::execute: all" << entries.size()
                      << "state files were stale";
        streamToOr(m_out, stderr,
                   QStringLiteral("status: no running simulation "
                                  "(all state files stale)\n"));
        return static_cast<int>(ExitCode::NoRunningSim);
    }

    if (jsonMode)
    {
        streamToOr(m_out, stdout,
                   QJsonDocument(jsonArr).toJson(QJsonDocument::Indented));
    }
    else
    {
        streamToOr(m_out, stdout, humanBuffer);
    }

    qCInfo(lcCli) << "StatusCommand::execute:" << responded
                  << "live simulation(s) reported";
    return static_cast<int>(ExitCode::Success);
}

} // namespace Cli
} // namespace CargoNetSim
