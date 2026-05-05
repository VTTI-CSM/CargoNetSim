#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QtTest/QtTest>

namespace
{

QString cliRootPath()
{
    return QDir(QCoreApplication::applicationDirPath())
        .absoluteFilePath(QStringLiteral("../../src/CLI"));
}

QSet<QString> allowedBackendIncludes()
{
    return {
        QStringLiteral("Backend/Application/AvailabilityService.h"),
        QStringLiteral("Backend/Application/PreparedPathService.h"),
        QStringLiteral("Backend/Application/ScenarioLoadService.h"),
        QStringLiteral("Backend/Application/ScenarioPreviewService.h"),
        QStringLiteral("Backend/Application/SimulationRunService.h"),
        QStringLiteral("Backend/Bootstrap/BackendBootstrapService.h"),
        QStringLiteral("Backend/CliApi/ResultsApi.h"),
        QStringLiteral("Backend/CliApi/PathsApi.h"),
        QStringLiteral("Backend/CliApi/ScenarioDocumentApi.h"),
        QStringLiteral("Backend/CliApi/ValidationApi.h"),
        QStringLiteral("Backend/Commons/LogCategories.h"),
        QStringLiteral("Backend/Commons/LogMessageHandler.h"),
        QStringLiteral("Backend/Commons/TransportationMode.h"),
        QStringLiteral("Backend/Controllers/CargoNetSimController.h"),
        QStringLiteral("Backend/Scenario/ScenarioRuntime.h"),
    };
}

QStringList scanViolations()
{
    const QSet<QString> allowlist = allowedBackendIncludes();
    const QRegularExpression includePattern(
        QString::fromLatin1("^\\s*#include\\s+\"(Backend/[^\"]+)\""));

    QStringList violations;
    QDirIterator it(cliRootPath(),
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

class CliBackendBoundaryTest : public QObject
{
    Q_OBJECT

private slots:
    void test_cli_backend_includes_stay_within_allowlist()
    {
        const QString root = cliRootPath();
        QVERIFY2(QFileInfo::exists(root),
                 qPrintable(QStringLiteral("CLI root not found: %1")
                                .arg(root)));

        const QStringList violations = scanViolations();
        QVERIFY2(violations.isEmpty(),
                 qPrintable(QStringLiteral(
                     "CLI backend boundary violations:\n%1")
                                .arg(violations.join(QLatin1Char('\n')))));
    }
};

QTEST_MAIN(CliBackendBoundaryTest)

#include "CliBackendBoundaryTest.moc"
