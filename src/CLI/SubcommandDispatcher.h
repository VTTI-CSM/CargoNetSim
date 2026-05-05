#pragma once

#include <QMap>
#include <QString>
#include <QStringList>
#include <memory>

#include "Subcommand.h"

namespace CargoNetSim {
namespace Cli {

/**
 * @brief Maps subcommand names to concrete `Subcommand` implementations
 *        and routes an argv-style string list to the matching command.
 *
 * Plan 5 Task 3. Single responsibility: name-based dispatch. No parsing
 * beyond splitting `args[0]` (subcommand name) from `args[1..]`
 * (forwarded arguments). Help and version handling are themselves
 * subcommands registered under `--help`, `-h`, `--version`, `-v` by the
 * main wire-up (Task 18), not special cases inside this class.
 *
 * The registry owns its commands via `std::shared_ptr<Subcommand>` so
 * the same instance can be bound to multiple names (e.g. `--help` and
 * `-h` share one `HelpCommand`). Shared ownership also makes test
 * fakes trivial to inject.
 *
 * `ExitCodes.h` is intentionally NOT included from this header — the
 * public API returns `int`, and consumers that name specific exit
 * values should include the contract explicitly.
 */
class SubcommandDispatcher
{
public:
    /**
     * @brief Register @p cmd under @p name. Re-registering an existing
     *        name replaces the previous binding (idempotent for
     *        setup-then-override patterns used by tests).
     */
    void registerCommand(const QString              &name,
                         std::shared_ptr<Subcommand> cmd);

    /**
     * @brief Run the given argv (excluding the program name).
     *
     * `args[0]` must be the subcommand name; `args[1..]` are forwarded
     * to the command's `execute()`. Returns `ExitCode::BadArgs` on
     * empty argv or unknown name; otherwise returns whatever the
     * command returns.
     */
    int run(const QStringList &args);

private:
    QMap<QString, std::shared_ptr<Subcommand>> m_registry;
};

} // namespace Cli
} // namespace CargoNetSim
