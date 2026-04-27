#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

#include "Backend/Scenario/TerminalHandoffResolver.h"

using CargoNetSim::Backend::Scenario::TerminalHandoffResolution;
using CargoNetSim::Backend::Scenario::resolveTerminalHandoff;

namespace
{

QJsonObject makeContainer(const QString &containerId,
                          const QString &scenarioTerminalId)
{
    QJsonObject haulerVariables;
    haulerVariables.insert(QStringLiteral("scenario_terminal_id"),
                           scenarioTerminalId);

    QJsonObject customVariables;
    customVariables.insert(QStringLiteral("4"),
                           haulerVariables);

    QJsonObject container;
    container.insert(QStringLiteral("containerID"),
                     containerId);
    container.insert(QStringLiteral("customVariables"),
                     customVariables);
    return container;
}

} // namespace

class TerminalHandoffResolverTest : public QObject
{
    Q_OBJECT

private slots:
    void test_empty_batch_is_no_op()
    {
        const TerminalHandoffResolution resolution =
            resolveTerminalHandoff(QJsonArray{});

        QVERIFY(resolution.isNoOp());
        QCOMPARE(resolution.containerCount, 0);
        QVERIFY(resolution.scenarioTerminalId.isEmpty());
        QVERIFY(resolution.containersJson.isEmpty());
    }

    void test_valid_batch_uses_scenario_terminal_id()
    {
        QJsonArray containers;
        containers.append(
            makeContainer(QStringLiteral("c1"),
                          QStringLiteral("terminal-A")));
        containers.append(
            makeContainer(QStringLiteral("c2"),
                          QStringLiteral("terminal-A")));

        const TerminalHandoffResolution resolution =
            resolveTerminalHandoff(containers);

        QVERIFY(resolution.isReady());
        QCOMPARE(resolution.containerCount, 2);
        QCOMPARE(resolution.scenarioTerminalId,
                 QStringLiteral("terminal-A"));

        const QJsonObject payload =
            QJsonDocument::fromJson(
                resolution.containersJson.toUtf8())
                .object();
        QVERIFY(payload.contains(QStringLiteral("containers")));
        QCOMPARE(payload.value(QStringLiteral("containers"))
                      .toArray()
                      .size(),
                 2);
    }

    void test_missing_scenario_terminal_id_is_error()
    {
        QJsonObject container;
        container.insert(QStringLiteral("containerID"),
                         QStringLiteral("c1"));

        const TerminalHandoffResolution resolution =
            resolveTerminalHandoff(QJsonArray{container});

        QVERIFY(resolution.isError());
        QVERIFY(
            resolution.errorMessage.contains(
                QStringLiteral("missing scenario_terminal_id"),
                Qt::CaseInsensitive));
    }

    void test_mixed_scenario_terminal_ids_are_rejected()
    {
        QJsonArray containers;
        containers.append(
            makeContainer(QStringLiteral("c1"),
                          QStringLiteral("terminal-A")));
        containers.append(
            makeContainer(QStringLiteral("c2"),
                          QStringLiteral("terminal-B")));

        const TerminalHandoffResolution resolution =
            resolveTerminalHandoff(containers);

        QVERIFY(resolution.isError());
        QVERIFY(resolution.errorMessage.contains(
            QStringLiteral("mixes scenario terminal targets"),
            Qt::CaseInsensitive));
    }
};

QTEST_MAIN(TerminalHandoffResolverTest)
#include "TerminalHandoffResolverTest.moc"
