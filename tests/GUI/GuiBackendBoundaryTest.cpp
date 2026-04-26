#include <QDirIterator>
#include <QFile>
#include <QCoreApplication>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QtTest/QtTest>

namespace
{

QString guiRootPath()
{
    return QDir(QCoreApplication::applicationDirPath())
        .absoluteFilePath(QStringLiteral("../../src/GUI"));
}

QSet<QString> allowedBackendIncludes()
{
    return {
        QStringLiteral("Backend/Application/AvailabilityService.h"),
        QStringLiteral("Backend/Application/NetworkManagementService.h"),
        QStringLiteral("Backend/Application/NetworkViewService.h"),
        QStringLiteral("Backend/Application/PathPresentationService.h"),
        QStringLiteral("Backend/Application/PreparedPathService.h"),
        QStringLiteral("Backend/Application/RouteAuthoringService.h"),
        QStringLiteral("Backend/Application/ScenarioEditService.h"),
        QStringLiteral("Backend/Application/ScenarioLoadService.h"),
        QStringLiteral("Backend/Application/ScenarioPersistenceService.h"),
        QStringLiteral("Backend/Application/SimulationRunService.h"),
        QStringLiteral("Backend/Commons/GeoDistance.h"),
        QStringLiteral("Backend/Commons/LogCategories.h"),
        QStringLiteral("Backend/Commons/LoggerInterface.h"),
        QStringLiteral("Backend/Commons/ShortestPathResult.h"),
        QStringLiteral("Backend/Commons/TransportationMode.h"),
        QStringLiteral("Backend/Commons/Units.h"),
        QStringLiteral("Backend/Controllers/CargoNetSimController.h"),
        QStringLiteral("Backend/GuiApi/ScenarioContractsApi.h"),
        QStringLiteral("Backend/GuiApi/ScenarioDocumentApi.h"),
        QStringLiteral("Backend/Scenario/ScenarioRuntime.h"),
    };
}

QStringList scanViolations()
{
    const QSet<QString> allowlist = allowedBackendIncludes();
    const QRegularExpression includePattern(
        QString::fromLatin1("^\\s*#include\\s+\"(Backend/[^\"]+)\""));

    QStringList violations;
    QDirIterator it(guiRootPath(),
                    QStringList() << QStringLiteral("*.h")
                                  << QStringLiteral("*.cpp"),
                    QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        const QString filePath = it.next();
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            violations.append(
                QStringLiteral("%1: unable to read file").arg(filePath));
            continue;
        }

        int lineNumber = 0;
        while (!file.atEnd())
        {
            const QString line =
                QString::fromUtf8(file.readLine());
            ++lineNumber;
            const auto match = includePattern.match(line);
            if (!match.hasMatch())
                continue;

            const QString includePath = match.captured(1);
            if (allowlist.contains(includePath))
                continue;

            violations.append(QStringLiteral("%1:%2: %3")
                                  .arg(filePath)
                                  .arg(lineNumber)
                                  .arg(includePath));
        }
    }

    violations.sort();
    return violations;
}

} // namespace

class GuiBackendBoundaryTest : public QObject
{
    Q_OBJECT

private slots:
    void test_gui_backend_includes_stay_within_allowlist()
    {
        const QString root = guiRootPath();
        QVERIFY2(QFileInfo::exists(root),
                 qPrintable(QStringLiteral("GUI root not found: %1").arg(root)));

        const QStringList violations = scanViolations();
        QVERIFY2(violations.isEmpty(),
                 qPrintable(QStringLiteral(
                     "GUI backend boundary violations:\n%1")
                                .arg(violations.join(QLatin1Char('\n')))));
    }
};

QTEST_MAIN(GuiBackendBoundaryTest)

#include "GuiBackendBoundaryTest.moc"
