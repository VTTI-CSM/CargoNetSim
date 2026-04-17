#include "RuntimeStateFile.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QSaveFile>
#include <QStandardPaths>

#include "Backend/Commons/LogCategories.h"

namespace CargoNetSim {
namespace Cli {

namespace {

/// Stable filename suffix per state file: every consumer (scan glob,
/// per-pid path lookup) reads from this constant.
constexpr auto kFilenamePrefix = "cargonetsim-cli-";
constexpr auto kFilenameSuffix = ".state";

/// Build the absolute path for @p pid's state file. Uses
/// `QDir::filePath()` so a missing or extra trailing slash on the
/// runtime dir is normalised correctly on every platform.
QString pathForPid(qint64 pid)
{
    return QDir(RuntimeStateFile::runtimeStateDir())
        .filePath(QStringLiteral("%1%2%3")
                      .arg(QLatin1String(kFilenamePrefix))
                      .arg(pid)
                      .arg(QLatin1String(kFilenameSuffix)));
}

/// Reverse of `pathForPid` for the scan glob: the glob `*` matches the
/// PID portion. Centralises the prefix/suffix in one place so a future
/// rename touches only the constants above.
QString scanGlob()
{
    return QStringLiteral("%1*%2")
        .arg(QLatin1String(kFilenamePrefix))
        .arg(QLatin1String(kFilenameSuffix));
}

} // namespace

QString RuntimeStateFile::runtimeStateDir()
{
    const QString override_ =
        QProcessEnvironment::systemEnvironment().value(
            QStringLiteral("CARGONETSIM_CLI_RUNTIME_DIR"));
    if (!override_.isEmpty())
        return override_;
    // Default: <TempLocation>/cargonetsim-cli — use QDir::filePath
    // rather than string concat so the result is well-formed even if
    // QStandardPaths returns a path with a trailing separator.
    return QDir(QStandardPaths::writableLocation(
                    QStandardPaths::TempLocation))
        .filePath(QStringLiteral("cargonetsim-cli"));
}

bool RuntimeStateFile::write(const RuntimeStateEntry &e, QString *err)
{
    qCDebug(lcCli) << "RuntimeStateFile::write: pid =" << e.pid
                   << ", socket =" << e.socketName
                   << ", scenario =" << e.scenarioPath;
    QDir().mkpath(runtimeStateDir());

    QJsonObject obj;
    obj[QStringLiteral("pid")]           = e.pid;
    obj[QStringLiteral("socket_name")]   = e.socketName;
    obj[QStringLiteral("scenario_path")] = e.scenarioPath;
    obj[QStringLiteral("started_at")]    =
        e.startedAt.toString(Qt::ISODate);

    // Binary mode (no QIODevice::Text) for the same reason
    // CsvResultsWriter omits it: avoid any accidental Windows
    // \n → \r\n translation. JSON parsers tolerate either, but byte
    // stability across platforms is the cheaper invariant.
    const QString filePath = pathForPid(e.pid);
    qCDebug(lcCli) << "RuntimeStateFile::write: path =" << filePath;
    QSaveFile f(filePath);
    if (!f.open(QIODevice::WriteOnly))
    {
        qCWarning(lcCli) << "RuntimeStateFile::write: open failed —"
                         << f.errorString();
        if (err) *err = f.errorString();
        return false;
    }
    f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    if (!f.commit())
    {
        qCWarning(lcCli) << "RuntimeStateFile::write: commit failed —"
                         << f.errorString();
        if (err) *err = f.errorString();
        return false;
    }
    qCDebug(lcCli) << "RuntimeStateFile::write: state file written successfully";
    return true;
}

bool RuntimeStateFile::removeForPid(qint64 pid)
{
    qCDebug(lcCli) << "RuntimeStateFile::removeForPid: pid =" << pid;
    const bool ok = QFile::remove(pathForPid(pid));
    if (!ok)
        qCDebug(lcCli) << "RuntimeStateFile::removeForPid: file not found or remove failed for pid" << pid;
    return ok;
}

QList<RuntimeStateEntry> RuntimeStateFile::scan()
{
    qCDebug(lcCli) << "RuntimeStateFile::scan: scanning" << runtimeStateDir();
    QList<RuntimeStateEntry> out;
    QDir dir(runtimeStateDir());
    if (!dir.exists())
    {
        qCDebug(lcCli) << "RuntimeStateFile::scan: directory does not exist, returning empty";
        return out;
    }

    const auto files = dir.entryList({scanGlob()}, QDir::Files);
    for (const auto &name : files)
    {
        QFile f(dir.filePath(name));
        if (!f.open(QIODevice::ReadOnly)) continue;
        const auto doc = QJsonDocument::fromJson(f.readAll());
        if (!doc.isObject()) continue;
        const auto obj = doc.object();

        RuntimeStateEntry e;
        e.pid          = obj.value(QStringLiteral("pid")).toInteger();
        e.socketName   =
            obj.value(QStringLiteral("socket_name")).toString();
        e.scenarioPath =
            obj.value(QStringLiteral("scenario_path")).toString();
        e.startedAt    = QDateTime::fromString(
            obj.value(QStringLiteral("started_at")).toString(),
            Qt::ISODate);

        // Defensive filter: pid+socketName are the load-bearing fields
        // that downstream consumers (status/stop) need. Anything
        // missing them is treated as corruption and silently skipped.
        if (e.pid > 0 && !e.socketName.isEmpty())
            out.append(e);
    }
    qCDebug(lcCli) << "RuntimeStateFile::scan: found" << out.size()
                   << "valid state file(s) out of" << files.size() << "on disk";
    return out;
}

} // namespace Cli
} // namespace CargoNetSim
