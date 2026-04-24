#include <QApplication>
#include <QCheckBox>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QTableWidget>
#include <QTest>

#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Models/Path.h"
#include "Backend/Scenario/PathMetrics.h"
#include "Backend/Scenario/PreparedPathStatus.h"
#include "Backend/Scenario/ScenarioDocument.h"
#include "GUI/Widgets/ShortestPathTable.h"

using CargoNetSim::Backend::Path;
using CargoNetSim::Backend::Scenario::PathMetrics;
using CargoNetSim::Backend::Scenario::PreparedPathEligibility;
using CargoNetSim::Backend::Scenario::ScenarioDocument;
using CargoNetSim::GUI::ShortestPathsTable;

namespace
{

Path *makePath(int pathId, const QString &pathUid,
               const QString &from, const QString &to,
               double totalCost)
{
    const QJsonObject segment{
        {QStringLiteral("from"), from},
        {QStringLiteral("to"), to},
        {QStringLiteral("mode"), 1},
        {QStringLiteral("sequence_index"), 0},
        {QStringLiteral("ranking_cost_contribution"), totalCost},
        {QStringLiteral("weighted_edge_cost"), totalCost}
    };

    const QJsonArray terminals{
        QJsonObject{
            {QStringLiteral("terminal"), from},
            {QStringLiteral("display_name"), from},
            {QStringLiteral("canonical_name"), from},
            {QStringLiteral("sequence_index"), 0}
        },
        QJsonObject{
            {QStringLiteral("terminal"), to},
            {QStringLiteral("display_name"), to},
            {QStringLiteral("canonical_name"), to},
            {QStringLiteral("sequence_index"), 1}
        }
    };

    const QJsonObject pathJson{
        {QStringLiteral("path_id"), pathId},
        {QStringLiteral("path_uid"), pathUid},
        {QStringLiteral("rank"), 0},
        {QStringLiteral("ranking_cost"), totalCost},
        {QStringLiteral("start_terminal"), from},
        {QStringLiteral("end_terminal"), to},
        {QStringLiteral("requested_mode"), 0},
        {QStringLiteral("requested_top_n"), 2},
        {QStringLiteral("skip_same_mode_terminal_delays_and_costs"), true},
        {QStringLiteral("effective_container_count"), 0},
        {QStringLiteral("total_path_cost"), totalCost},
        {QStringLiteral("total_edge_costs"), totalCost},
        {QStringLiteral("total_terminal_costs"), 0.0},
        {QStringLiteral("terminals_in_path"), terminals},
        {QStringLiteral("segments"), QJsonArray{segment}}
    };

    return Path::fromJson(pathJson, {});
}

PathMetrics predictedMetric(double distanceKm, int containers)
{
    PathMetrics m;
    m.valid = true;
    m.distanceKm = distanceKm;
    m.travelTimeHours = distanceKm / 10.0;
    m.containerCount = containers;
    m.vehiclesNeeded = containers > 0 ? 1 : 0;
    m.energyPerVehicle = distanceKm * 2.0;
    m.carbonPerVehicle = distanceKm * 0.5;
    m.riskPerVehicle = distanceKm * 0.01;
    return m;
}

QCheckBox *rowCheckbox(ShortestPathsTable &table, int row)
{
    auto *tableWidget = table.findChild<QTableWidget *>();
    if (!tableWidget)
        return nullptr;

    auto *checkboxWidget = tableWidget->cellWidget(row, 0);
    if (!checkboxWidget || !checkboxWidget->layout())
        return nullptr;

    return qobject_cast<QCheckBox *>(
        checkboxWidget->layout()->itemAt(0)->widget());
}

} // namespace

class ShortestPathTableIdentityTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (!CargoNetSim::CargoNetSimController::instance())
        {
            new CargoNetSim::CargoNetSimController(
                nullptr, QCoreApplication::instance());
        }
    }

    void test_duplicate_display_path_ids_keep_distinct_identity()
    {
        ShortestPathsTable table;

        const QString firstKey = QStringLiteral("uid-O1-D1-r0");
        const QString secondKey = QStringLiteral("uid-O2-D2-r0");

        QHash<QString, PathMetrics> predicted;
        predicted.insert(firstKey, predictedMetric(10.0, 3));
        predicted.insert(secondKey, predictedMetric(25.0, 7));

        table.addPaths(
            {makePath(1, firstKey, QStringLiteral("O1"),
                      QStringLiteral("D1"), 10.0),
             makePath(1, secondKey, QStringLiteral("O2"),
                      QStringLiteral("D2"), 25.0)},
            predicted);

        QCOMPARE(table.pathsSize(), 2);
        QVERIFY(table.getDataByPathKey(firstKey) != nullptr);
        QVERIFY(table.getDataByPathKey(secondKey) != nullptr);

        auto *tableWidget = table.findChild<QTableWidget *>();
        QVERIFY(tableWidget != nullptr);
        QCOMPARE(tableWidget->rowCount(), 2);

        tableWidget->selectRow(1);
        QCoreApplication::processEvents();
        QCOMPARE(table.getSelectedPathKey(), secondKey);

        auto *firstCheckbox = rowCheckbox(table, 0);
        QVERIFY(firstCheckbox != nullptr);
        firstCheckbox->setChecked(true);
        QCoreApplication::processEvents();
        QCOMPARE(table.getCheckedPathKeys(),
                 QVector<QString>{firstKey});

        table.updateSimulationCosts(firstKey, 111.0, 70.0, 41.0);

        ScenarioDocument doc;
        const auto snapshots = table.buildComparisonSnapshots(doc);
        QCOMPARE(snapshots.size(), 2);

        QHash<QString, QJsonObject> byKey;
        for (const auto &snapshot : snapshots)
        {
            byKey.insert(
                snapshot.value(QStringLiteral("canonical_path_key"))
                    .toString(),
                snapshot);
        }

        QCOMPARE(byKey.size(), 2);
        QCOMPARE(byKey.value(firstKey)
                     .value(QStringLiteral("predicted_metrics"))
                     .toObject()
                     .value(QStringLiteral("distance_km"))
                     .toDouble(),
                 10.0);
        QCOMPARE(byKey.value(secondKey)
                     .value(QStringLiteral("predicted_metrics"))
                     .toObject()
                     .value(QStringLiteral("distance_km"))
                     .toDouble(),
                 25.0);
        QCOMPARE(byKey.value(firstKey)
                     .value(QStringLiteral("simulation"))
                     .toObject()
                     .value(QStringLiteral("total_cost"))
                     .toDouble(),
                 111.0);
        QCOMPARE(byKey.value(secondKey)
                     .value(QStringLiteral("simulation"))
                     .toObject()
                     .value(QStringLiteral("state"))
                     .toString(),
                 QStringLiteral("not_simulated"));
    }

    void test_backend_eligibility_disables_blocked_rows()
    {
        ShortestPathsTable table;

        const QString pathKey = QStringLiteral("uid-O1-D1-r0");
        table.addPaths({makePath(1, pathKey, QStringLiteral("O1"),
                                 QStringLiteral("D1"), 10.0)},
                       {{pathKey, predictedMetric(10.0, 3)}});

        PreparedPathEligibility blocked;
        blocked.selectable = false;
        blocked.simulatable = false;
        blocked.blockingReason =
            QStringLiteral("NeTrainSim consumer is unavailable");

        table.setPathEligibility({{pathKey, blocked}});

        auto *checkbox = rowCheckbox(table, 0);
        QVERIFY(checkbox != nullptr);
        QVERIFY(!checkbox->isEnabled());

        auto *tableWidget = table.findChild<QTableWidget *>();
        QVERIFY(tableWidget != nullptr);
        QVERIFY(tableWidget->item(0, 1)->toolTip().contains(
            QStringLiteral("Blocked")));
        QVERIFY(table.getCheckedPathKeys().isEmpty());
    }
};

QTEST_MAIN(ShortestPathTableIdentityTest)
#include "ShortestPathTableIdentityTest.moc"
