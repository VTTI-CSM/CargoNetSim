#include <QTest>

#include "CLI/ExitCodes.h"
#include "CLI/Subcommand.h"
#include "CLI/SubcommandDispatcher.h"

// Test fake that records every invocation — a minimal Subcommand
// implementation sufficient to observe the dispatcher's routing
// behaviour without involving any real command.
namespace {
class RecordingCommand : public CargoNetSim::Cli::Subcommand
{
public:
    int         calls = 0;
    QStringList lastArgs;

    int execute(const QStringList &args) override
    {
        ++calls;
        lastArgs = args;
        return 0;
    }
};
} // namespace

class SubcommandDispatcherTest : public QObject
{
    Q_OBJECT

private slots:
    void test_dispatch_routes_by_name_and_forwards_tail_args()
    {
        using namespace CargoNetSim::Cli;
        SubcommandDispatcher d;
        auto fake = std::make_shared<RecordingCommand>();
        d.registerCommand(QStringLiteral("do-thing"), fake);

        const int rc = d.run(
            {QStringLiteral("do-thing"),
             QStringLiteral("--opt"),
             QStringLiteral("value")});

        QCOMPARE(rc, 0);
        QCOMPARE(fake->calls, 1);
        QCOMPARE(fake->lastArgs,
                 QStringList({QStringLiteral("--opt"),
                              QStringLiteral("value")}));
    }

    void test_dispatch_unknown_command_returns_bad_args()
    {
        using namespace CargoNetSim::Cli;
        SubcommandDispatcher d;
        QCOMPARE(d.run({QStringLiteral("nope")}),
                 static_cast<int>(ExitCode::BadArgs));
    }

    void test_dispatch_empty_argv_returns_bad_args()
    {
        using namespace CargoNetSim::Cli;
        SubcommandDispatcher d;
        QCOMPARE(d.run({}), static_cast<int>(ExitCode::BadArgs));
    }

    void test_dispatch_re_registration_replaces_previous_binding()
    {
        using namespace CargoNetSim::Cli;
        SubcommandDispatcher d;
        auto first  = std::make_shared<RecordingCommand>();
        auto second = std::make_shared<RecordingCommand>();
        d.registerCommand(QStringLiteral("cmd"), first);
        d.registerCommand(QStringLiteral("cmd"), second);

        d.run({QStringLiteral("cmd")});

        QCOMPARE(first->calls,  0);
        QCOMPARE(second->calls, 1);
    }
};

QTEST_MAIN(SubcommandDispatcherTest)
#include "SubcommandDispatcherTest.moc"
