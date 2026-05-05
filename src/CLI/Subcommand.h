#pragma once

#include <QStringList>

namespace CargoNetSim {
namespace Cli {

/**
 * @brief Abstract base for every CLI subcommand (Plan 5 Task 3).
 *
 * Stateless contract. `execute(args)` returns an exit code (castable
 * from `ExitCode`). The command owns no per-invocation state; every
 * input arrives via @p args, every output goes to stdout/stderr or a
 * file the command writes itself.
 *
 * The return type is a plain `int` (not `ExitCode`) so the abstract
 * boundary does not force every consumer of this header to include
 * `ExitCodes.h`. Concrete commands include it when they name specific
 * exit values; casts are explicit at the call site.
 *
 * Ownership: the dispatcher holds each command as `std::shared_ptr<>`
 * so tests can inject fakes through the same API as production code.
 */
class Subcommand
{
public:
    virtual ~Subcommand() = default;

    /// @param args argv with the program name and subcommand name both
    ///        already stripped — i.e. the arguments the subcommand is
    ///        supposed to consume. Never null (may be empty).
    /// @return exit code. Cast from `CargoNetSim::Cli::ExitCode`.
    virtual int execute(const QStringList &args) = 0;
};

} // namespace Cli
} // namespace CargoNetSim
