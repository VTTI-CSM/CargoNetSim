#include "HelpVersion.h"

#include <QFile>
#include <QIODevice>

#include <cstdio>

#include "Backend/Commons/LogCategories.h"
#include "Version.h"
#include "CLI/Commands/CommandOutput.h"
#include "CLI/ExitCodes.h"

/// `Q_INIT_RESOURCE` expands to `extern int qInitResources_xxx();`
/// + a call. Both must be at **global namespace scope** because the
/// resource-compiler-generated symbol lives there. Putting the
/// wrapper inside ANY namespace (including an anonymous one) makes
/// the linker look for `<ns>::qInitResources_cli_resources()` which
/// does not exist. `static inline` gives file-local linkage without
/// changing the enclosing scope. Idempotent on repeat calls; cheap.
static inline void initCliResources()
{
    Q_INIT_RESOURCE(cli_resources);
}

namespace CargoNetSim {
namespace Cli {

HelpCommand::HelpCommand(QIODevice *out)
    : m_out(out)
{
}

int HelpCommand::execute(const QStringList &)
{
    qCDebug(lcCli) << "HelpCommand::execute()";
    // The cli_resources.qrc lives in the cargonetsim-cli-lib STATIC
    // library. Qt resources in static libraries get stripped by the
    // linker unless an explicit reference forces the init function
    // to be retained — `initCliResources()` (file-scope helper above)
    // does that. Idempotent; cheap on subsequent calls.
    initCliResources();

    // Single source of truth for the help text: the embedded resource
    // built from tools/cli/docs/help.txt via AUTORCC.
    QFile resource(QStringLiteral(":/cli/help.txt"));
    if (!resource.open(QIODevice::ReadOnly | QIODevice::Text))
        return static_cast<int>(ExitCode::BadArgs);

    streamToOr(m_out, stdout, resource.readAll());
    return static_cast<int>(ExitCode::Success);
}

VersionCommand::VersionCommand(QIODevice *out)
    : m_out(out)
{
}

int VersionCommand::execute(const QStringList &)
{
    qCDebug(lcCli) << "VersionCommand::execute()";
    // CARGONETSIM_VERSION is the string macro emitted by CMake into
    // Version.h (configured from src/Version.h.in). Single source of
    // truth for the version exposed to consumers.
    const QByteArray line =
        QByteArray("cargonetsim-cli ") + CARGONETSIM_VERSION + "\n";
    streamToOr(m_out, stdout, line);
    return static_cast<int>(ExitCode::Success);
}

} // namespace Cli
} // namespace CargoNetSim
