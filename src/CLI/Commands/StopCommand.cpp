#include "StopCommand.h"

#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <cstdio>

#include "Backend/Commons/LogCategories.h"
#include "CLI/Commands/CommandOutput.h"
#include "CLI/ExitCodes.h"
#include "CLI/Ipc/ControlChannel.h"
#include "CLI/Ipc/RuntimeStateFile.h"

namespace CargoNetSim {
namespace Cli {

namespace {

/// Acknowledgement window for `{"op":"stop"}`. Longer than `status`
/// because graceful simulation shutdown may take several seconds.
constexpr int kStopTimeoutMs = 10000;

/// Human-readable "pid1, pid2, pid3" render of a scan result. Used
/// in the "multiple running simulations" and "pid not found"
/// diagnostics so the user knows what to pass next.
QString pidList(const QList<RuntimeStateEntry> &entries)
{
    QStringList ids;
    ids.reserve(entries.size());
    for (const auto &e : entries)
        ids << QString::number(e.pid);
    return ids.join(QStringLiteral(", "));
}

} // namespace

StopCommand::StopCommand(QIODevice *errSink)
    : m_err(errSink)
{
}

int StopCommand::execute(const QStringList &args)
{
    qCInfo(lcCli) << "StopCommand::execute: entry, args =" << args;

    const auto entries = RuntimeStateFile::scan();
    qCDebug(lcCli) << "StopCommand::execute: scan found"
                   << entries.size() << "state file(s)";
    if (entries.isEmpty())
    {
        qCInfo(lcCli) << "StopCommand::execute: no running simulation";
        streamToOr(m_err, stderr,
                   QStringLiteral("stop: no running simulation\n"));
        return static_cast<int>(ExitCode::NoRunningSim);
    }

    // ---- Resolve target PID --------------------------------------------
    qint64    targetPid = -1;
    const int pidFlag   = args.indexOf(QStringLiteral("--pid"));

    if (pidFlag >= 0)
    {
        if (pidFlag + 1 >= args.size())
        {
            qCWarning(lcCli) << "StopCommand::execute: --pid flag missing value";
            streamToOr(m_err, stderr,
                       QStringLiteral("stop: --pid requires a value\n"));
            return static_cast<int>(ExitCode::BadArgs);
        }
        bool ok = false;
        targetPid = args[pidFlag + 1].toLongLong(&ok);
        if (!ok || targetPid <= 0)
        {
            qCWarning(lcCli) << "StopCommand::execute: invalid --pid value"
                             << args[pidFlag + 1];
            streamToOr(m_err, stderr,
                       QStringLiteral(
                           "stop: --pid must be a positive integer (got '%1')\n")
                           .arg(args[pidFlag + 1]));
            return static_cast<int>(ExitCode::BadArgs);
        }
    }
    else if (entries.size() == 1)
    {
        targetPid = entries.first().pid;
    }
    else
    {
        qCWarning(lcCli) << "StopCommand::execute: multiple simulations running,"
                         << "user must specify --pid";
        streamToOr(m_err, stderr,
                   QStringLiteral(
                       "stop: multiple running simulations; "
                       "specify --pid N\n"
                       "Running PIDs: %1\n")
                       .arg(pidList(entries)));
        return static_cast<int>(ExitCode::BadArgs);
    }

    qCInfo(lcCli) << "StopCommand::execute: target pid =" << targetPid;

    // ---- Locate the entry + send stop ---------------------------------
    for (const auto &e : entries)
    {
        if (e.pid != targetPid) continue;

        qCDebug(lcCli) << "StopCommand::execute: sending stop to socket"
                       << e.socketName << "(timeout =" << kStopTimeoutMs << "ms)";
        const auto reply = ControlChannelClient::send(
            e.socketName,
            QJsonObject{{QStringLiteral("op"), QStringLiteral("stop")}},
            kStopTimeoutMs);

        if (reply && reply->value(QStringLiteral("ok")).toBool())
        {
            qCInfo(lcCli) << "StopCommand::execute: pid" << targetPid
                          << "stopped successfully";
            return static_cast<int>(ExitCode::Success);
        }

        // Reply present but `ok != true` -> server refused; no reply
        // -> timeout / unreachable socket. Report the variant that
        // applied and fall through to NoRunningSim so shell scripts
        // treat the simulation as "not stopped".
        if (reply)
        {
            qCWarning(lcCli) << "StopCommand::execute: pid" << targetPid
                             << "refused shutdown";
            streamToOr(m_err, stderr,
                       QStringLiteral("stop: pid %1 refused shutdown "
                                      "(reply: %2)\n")
                           .arg(targetPid)
                           .arg(QString::fromUtf8(
                               QJsonDocument(*reply)
                                   .toJson(QJsonDocument::Compact))));
        }
        else
        {
            qCWarning(lcCli) << "StopCommand::execute: pid" << targetPid
                             << "did not respond within" << kStopTimeoutMs << "ms";
            streamToOr(m_err, stderr,
                       QStringLiteral(
                           "stop: pid %1 did not respond within %2 ms\n")
                           .arg(targetPid)
                           .arg(kStopTimeoutMs));
        }
        return static_cast<int>(ExitCode::NoRunningSim);
    }

    // --pid was supplied but did not match any discovered entry.
    qCWarning(lcCli) << "StopCommand::execute: pid" << targetPid
                     << "not found among running simulations";
    streamToOr(m_err, stderr,
               QStringLiteral(
                   "stop: pid %1 not found among running simulations\n"
                   "Running PIDs: %2\n")
                   .arg(targetPid)
                   .arg(pidList(entries)));
    return static_cast<int>(ExitCode::NoRunningSim);
}

} // namespace Cli
} // namespace CargoNetSim
