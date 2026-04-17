#include "SubcommandDispatcher.h"
#include "Backend/Commons/LogCategories.h"

#include <QTextStream>

#include "ExitCodes.h"

namespace CargoNetSim {
namespace Cli {

void SubcommandDispatcher::registerCommand(
    const QString &name, std::shared_ptr<Subcommand> cmd)
{
    qCDebug(lcCli) << "SubcommandDispatcher::registerCommand:" << name;
    m_registry.insert(name, std::move(cmd));
}

int SubcommandDispatcher::run(const QStringList &args)
{
    qCDebug(lcCli) << "SubcommandDispatcher::run: args" << args;
    if (args.isEmpty())
    {
        qCWarning(lcCli) << "SubcommandDispatcher::run: missing subcommand";
        QTextStream(stderr)
            << "cargonetsim-cli: missing subcommand\n"
               "Run `cargonetsim-cli --help` for usage.\n";
        return static_cast<int>(ExitCode::BadArgs);
    }

    const QString name = args.first();

    const auto it = m_registry.constFind(name);
    if (it == m_registry.constEnd())
    {
        qCWarning(lcCli) << "SubcommandDispatcher::run: unknown subcommand" << name;
        QTextStream(stderr)
            << "cargonetsim-cli: unknown subcommand '" << name << "'\n"
               "Run `cargonetsim-cli --help` for usage.\n";
        return static_cast<int>(ExitCode::BadArgs);
    }

    qCInfo(lcCli) << "SubcommandDispatcher::run: dispatching" << name;
    return it.value()->execute(args.mid(1));
}

} // namespace Cli
} // namespace CargoNetSim
