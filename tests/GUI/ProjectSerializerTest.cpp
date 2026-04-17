#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

#include "Backend/Scenario/ScenarioDocument.h"
#include "GUI/Serializers/ProjectSerializer.h"

using namespace CargoNetSim::Backend::Scenario;
using namespace CargoNetSim::GUI;

class ProjectSerializerTest : public QObject
{
    Q_OBJECT
private slots:
    void test_load_minimal_fixture_returns_document()
    {
        const QString path =
            QDir(QCoreApplication::applicationDirPath())
                .filePath("fixtures/scenario/minimal.yml");
        QString err;
        auto doc = ProjectSerializer::loadProject(path, &err);
        QVERIFY2(doc != nullptr, qPrintable(err));
    }

    void test_load_nonexistent_returns_null_with_error()
    {
        QString err;
        auto    doc = ProjectSerializer::loadProject(
            "/definitely/not/a/real/file.yml", &err);
        QVERIFY(doc == nullptr);
        QVERIFY(!err.isEmpty());
    }

    void test_round_trip_through_temporary_file()
    {
        const QString srcPath =
            QDir(QCoreApplication::applicationDirPath())
                .filePath("fixtures/scenario/minimal.yml");

        // Load from the fixture, save to a temp location, load back —
        // the wrapper should support round-tripping as a thin pass-
        // through to ScenarioSerializer.
        QString err;
        auto    doc = ProjectSerializer::loadProject(srcPath, &err);
        QVERIFY2(doc != nullptr, qPrintable(err));

        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString outPath =
            QDir(tempDir.path()).filePath("round_trip.yml");

        QVERIFY(ProjectSerializer::saveProject(*doc, outPath, &err));
        QVERIFY(QFileInfo(outPath).exists());

        auto reloaded = ProjectSerializer::loadProject(outPath, &err);
        QVERIFY2(reloaded != nullptr, qPrintable(err));
    }
};

QTEST_MAIN(ProjectSerializerTest)
#include "ProjectSerializerTest.moc"
