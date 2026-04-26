// cargonetsim-cli entry point (Plan 5 Task 18).
//
// Single source of subcommand registration. Every concrete command
// class lives under tools/cli/Commands/ and each is wired to its
// canonical name (and short aliases, where the help text documents
// them) exactly once here. No business logic in this file — we
// construct QCoreApplication, build the dispatcher, forward the
// argv tail, and return what the dispatcher returns.
//
// --help/-h and --version/-v share a single HelpCommand / VersionCommand
// instance via shared_ptr so both names resolve to the same behavior
// without instantiating twice.

#include <QCoreApplication>
#include <QLoggingCategory>
#include <QString>
#include <QStringList>

#include <memory>

#include "Backend/Bootstrap/BackendBootstrapService.h"
#include "Backend/Commons/LogMessageHandler.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "SubcommandDispatcher.h"

#include "Commands/ConnectionsCommand.h"
#include "Commands/HelpVersion.h"
#include "Commands/PathsCommand.h"
#include "Commands/PreviewCommand.h"
#include "Commands/RunCommand.h"
#include "Commands/ValidateCommand.h"

int main(int argc, char *argv[])
{
    // Configure logging: debug enabled in debug builds only.
    // Override at runtime: QT_LOGGING_RULES="cargonetsim.*=true"
#ifdef QT_DEBUG
    QLoggingCategory::setFilterRules(QStringLiteral(
        "cargonetsim.*.debug=true\n"
        "cargonetsim.*.info=true\n"
        "cargonetsim.*.warning=true\n"
        "cargonetsim.*.critical=true\n"));
#else
    QLoggingCategory::setFilterRules(QStringLiteral(
        "cargonetsim.*.debug=false\n"
        "cargonetsim.*.info=true\n"
        "cargonetsim.*.warning=true\n"
        "cargonetsim.*.critical=true\n"));
#endif

    // stderr-only handler for CLI (no GUI bridge).
    CargoNetSim::Backend::installCargoNetSimLogHandler(
        nullptr);

    QCoreApplication app(argc, argv);

    // Tier 1 (Option E): stack-allocate the controller so C++ scope
    // rules destroy it at main() return, AFTER exec()/dispatch has
    // returned but BEFORE QCoreApplication's destructor. The CLI has
    // no widgets so the GUI-before-controller ordering concern does
    // not apply - we just need deterministic cleanup. The CLI does
    // not initialize the backend until a command that needs it
    // (e.g., RunCommand) calls initialize()/startAll().
    CargoNetSim::CargoNetSimController controller(/*logger=*/nullptr);

    // CLI subcommands that do not boot the full backend can still
    // rely on shared metatype registration.
    const CargoNetSim::Backend::BackendBootstrapService
        backendBootstrap;
    const auto bootstrapResult = backendBootstrap.registerOnly();
    if (!bootstrapResult.succeeded())
    {
        qFatal("cargonetsim-cli: %s",
               qPrintable(bootstrapResult.message.isEmpty()
                              ? QStringLiteral(
                                    "backend metatype registration failed")
                              : bootstrapResult.message));
    }

    using namespace CargoNetSim::Cli;

    SubcommandDispatcher d;

    d.registerCommand(QStringLiteral("run"),
                      std::make_shared<RunCommand>());
    d.registerCommand(QStringLiteral("validate"),
                      std::make_shared<ValidateCommand>());
    d.registerCommand(QStringLiteral("preview"),
                      std::make_shared<PreviewCommand>());
    d.registerCommand(QStringLiteral("paths"),
                      std::make_shared<PathsCommand>());
    d.registerCommand(QStringLiteral("connections"),
                      std::make_shared<ConnectionsCommand>());

    // Share one instance between the long + short aliases so a future
    // HelpCommand that caches state (e.g. precomputed help text) would
    // only do the work once regardless of which name the user types.
    auto help    = std::make_shared<HelpCommand>();
    auto version = std::make_shared<VersionCommand>();
    d.registerCommand(QStringLiteral("--help"),    help);
    d.registerCommand(QStringLiteral("-h"),        help);
    d.registerCommand(QStringLiteral("--version"), version);
    d.registerCommand(QStringLiteral("-v"),        version);

    QStringList args;
    args.reserve(argc - 1);
    for (int i = 1; i < argc; ++i)
        args << QString::fromLocal8Bit(argv[i]);

    return d.run(args);
}
