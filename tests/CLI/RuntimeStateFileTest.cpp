#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

#include <memory>

#include "CLI/Ipc/RuntimeStateFile.h"

// Contract lock-tests for RuntimeStateFile (Plan 5 Task 10).
//
// Each test runs against a fresh QTemporaryDir whose path is exposed
// to RuntimeStateFile via the CARGONETSIM_CLI_RUNTIME_DIR env
// override. init() / cleanup() slots set and unset the env so tests
// stay isolated from each other and from any leftover state on disk.

class RuntimeStateFileTest : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;

    static constexpr auto kEnvName = "CARGONETSIM_CLI_RUNTIME_DIR";

private slots:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
        qputenv(kEnvName, m_dir->path().toLocal8Bit());
    }

    void cleanup()
    {
        qunsetenv(kEnvName);
        m_dir.reset();
    }

    void test_write_then_scan_round_trips_entry_with_all_fields()
    {
        using namespace CargoNetSim;

        Cli::RuntimeStateEntry e;
        e.pid          = 1234;
        e.socketName   = QStringLiteral("cargonetsim-cli-1234");
        e.scenarioPath = QStringLiteral("/tmp/x.yml");
        e.startedAt    = QDateTime(QDate(2026, 4, 14),
                                   QTime(12, 0, 0), QTimeZone::UTC);

        QString err;
        QVERIFY2(Cli::RuntimeStateFile::write(e, &err), qPrintable(err));

        const auto entries = Cli::RuntimeStateFile::scan();
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries[0].pid,          e.pid);
        QCOMPARE(entries[0].socketName,   e.socketName);
        QCOMPARE(entries[0].scenarioPath, e.scenarioPath);
        // QDateTime ISO round-trip is to-the-second; compare at that
        // precision.
        QCOMPARE(entries[0].startedAt.toString(Qt::ISODate),
                 e.startedAt.toString(Qt::ISODate));
    }

    void test_remove_for_pid_deletes_file_and_subsequent_scan_is_empty()
    {
        using namespace CargoNetSim;

        Cli::RuntimeStateEntry e;
        e.pid        = 4321;
        e.socketName = QStringLiteral("cargonetsim-cli-4321");
        QVERIFY(Cli::RuntimeStateFile::write(e, nullptr));
        QCOMPARE(Cli::RuntimeStateFile::scan().size(), 1);

        QVERIFY(Cli::RuntimeStateFile::removeForPid(e.pid));
        QVERIFY(Cli::RuntimeStateFile::scan().isEmpty());
    }

    void test_scan_returns_all_entries_for_multiple_pids()
    {
        using namespace CargoNetSim;

        for (qint64 pid : {100LL, 200LL, 300LL})
        {
            Cli::RuntimeStateEntry e;
            e.pid        = pid;
            e.socketName = QStringLiteral("cargonetsim-cli-%1").arg(pid);
            QVERIFY(Cli::RuntimeStateFile::write(e, nullptr));
        }

        const auto entries = Cli::RuntimeStateFile::scan();
        QCOMPARE(entries.size(), 3);

        // entryList returns alphabetical/lexicographic order; build
        // a set so the assertion is order-independent.
        QSet<qint64> seenPids;
        for (const auto &e : entries) seenPids.insert(e.pid);
        QCOMPARE(seenPids,
                 (QSet<qint64>{100, 200, 300}));
    }

    void test_runtime_dir_honours_env_override()
    {
        using namespace CargoNetSim;
        // init() set the env to m_dir->path() — confirm runtimeStateDir
        // returns the same path. This locks the env-override contract
        // that tests and the future Task 17 RunCommand depend on.
        QCOMPARE(Cli::RuntimeStateFile::runtimeStateDir(),
                 m_dir->path());
    }

    void test_scan_over_nonexistent_directory_returns_empty_not_error()
    {
        using namespace CargoNetSim;
        // Point at a path that definitely does not exist; scan() must
        // return an empty list silently rather than crashing or
        // erroring out.
        const QString ghost = m_dir->path() + QStringLiteral("/no-such-subdir");
        qputenv(kEnvName, ghost.toLocal8Bit());
        QVERIFY(Cli::RuntimeStateFile::scan().isEmpty());
    }

    void test_scan_silently_skips_malformed_state_file()
    {
        using namespace CargoNetSim;

        // Drop a junk file matching the scan glob into the runtime
        // dir. scan() must skip it without aborting.
        const QString junkPath = QDir(m_dir->path())
            .filePath(QStringLiteral("cargonetsim-cli-bogus.state"));
        {
            QFile junk(junkPath);
            QVERIFY(junk.open(QIODevice::WriteOnly));
            junk.write("not json at all");
        }
        // Also write one valid entry so we can confirm scan returns
        // exactly the good one (and didn't bail out before reaching it).
        Cli::RuntimeStateEntry good;
        good.pid        = 555;
        good.socketName = QStringLiteral("cargonetsim-cli-555");
        QVERIFY(Cli::RuntimeStateFile::write(good, nullptr));

        const auto entries = Cli::RuntimeStateFile::scan();
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries[0].pid, qint64(555));
    }
};

QTEST_MAIN(RuntimeStateFileTest)
#include "RuntimeStateFileTest.moc"
