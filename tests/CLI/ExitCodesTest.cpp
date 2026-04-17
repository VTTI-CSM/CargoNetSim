#include <QTest>

#include "CLI/ExitCodes.h"

// Lock-test for the CLI exit-code contract (Plan 5 spec §8.5).
//
// These integer values are published through `cargonetsim-cli --help`
// (Task 11) and consumed by CI pipelines. Any accidental re-numbering
// would silently break downstream consumers; this test fails loudly.
// Single test, single source of truth, no behavioural assertions.
class ExitCodesTest : public QObject
{
    Q_OBJECT

private slots:
    void test_contract_values_stable()
    {
        using E = CargoNetSim::Cli::ExitCode;
        QCOMPARE(static_cast<int>(E::Success),            0);
        QCOMPARE(static_cast<int>(E::RunFailed),          1);
        QCOMPARE(static_cast<int>(E::ValidationFailed),   2);
        QCOMPARE(static_cast<int>(E::ConnectTimeout),     3);
        QCOMPARE(static_cast<int>(E::NoRunningSim),       4);
        QCOMPARE(static_cast<int>(E::ServerDisconnected), 5);
        QCOMPARE(static_cast<int>(E::BadArgs),            64);
    }
};

QTEST_MAIN(ExitCodesTest)
#include "ExitCodesTest.moc"
