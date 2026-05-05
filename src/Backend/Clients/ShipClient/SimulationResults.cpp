#include "ShipSimulationClient.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QThread>

// Placeholder includes
#include "Backend/Commons/LogCategories.h"
#include "Backend/Models/ShipSystem.h"
// #include "TerminalGraphServer.h"
// #include "SimulatorTimeServer.h"
// #include "ProgressBarManager.h"
// #include "ApplicationLogger.h"

namespace CargoNetSim
{
namespace Backend
{
namespace ShipClient
{

SimulationResults::SimulationResults()
    : m_summaryData(QList<QPair<QString, QString>>())
    , m_trajectoryFileData()
    , m_trajectoryFileName()
    , m_summaryFileName()
{
    qCDebug(lcClientShip) << "SimulationResults::SimulationResults: default constructed";
}

SimulationResults::SimulationResults(
    const QList<QPair<QString, QString>> &summaryData,
    const QByteArray &trajectoryFileData,
    const QString    &trajectoryFileName,
    const QString    &summaryFileName)
    : m_summaryData(summaryData)
    , m_trajectoryFileData(trajectoryFileData)
    , m_trajectoryFileName(trajectoryFileName)
    , m_summaryFileName(summaryFileName)
{
    qCDebug(lcClientShip) << "SimulationResults::SimulationResults:"
                          << "summaryPairs=" << summaryData.size()
                          << "trajectoryDataSize=" << trajectoryFileData.size()
                          << "trajectoryFile=" << trajectoryFileName
                          << "summaryFile=" << summaryFileName;
}

SimulationResults
SimulationResults::fromJson(const QJsonObject &jsonObj)
{
    qCDebug(lcClientShip) << "SimulationResults::fromJson: parsing JSON object"
                          << "keys=" << jsonObj.keys();

    QList<QPair<QString, QString>> summaryData;

    // Parse summary data from JSON
    QJsonArray summaryArray =
        jsonObj.value("summaryData").toArray();
    qCDebug(lcClientShip) << "SimulationResults::fromJson: summaryArray size=" << summaryArray.size();

    for (const QJsonValue &pairValue : summaryArray)
    {
        QJsonObject pairObj = pairValue.toObject();
        for (auto it = pairObj.constBegin();
             it != pairObj.constEnd(); ++it)
        {
            QString key   = it.key().trimmed();
            QString value = it.value().toString().trimmed();

            if (!key.isEmpty())
            {
                summaryData.append(qMakePair(key, value));
            }
        }
    }

    // Get trajectory file data
    QByteArray trajectoryFileData;
    if (jsonObj.value("trajectoryFileDataIncluded")
            .toBool())
    {
        QString base64Data =
            jsonObj.value("trajectoryFileData").toString();
        trajectoryFileData =
            QByteArray::fromBase64(base64Data.toLatin1());
        qCDebug(lcClientShip) << "SimulationResults::fromJson: decoded trajectory data"
                              << "size=" << trajectoryFileData.size();
    }
    else
    {
        qCDebug(lcClientShip) << "SimulationResults::fromJson: no trajectory data included";
    }

    qCInfo(lcClientShip) << "SimulationResults::fromJson: parsed"
                         << summaryData.size() << "summary pairs";

    return SimulationResults(
        summaryData, trajectoryFileData,
        jsonObj.value("trajectoryFileName").toString(),
        jsonObj.value("summaryFileName").toString());
}

QString SimulationResults::getTrajectoryFileName() const
{
    QString result = QFileInfo(m_trajectoryFileName).fileName();
    qCDebug(lcClientShip) << "SimulationResults::getTrajectoryFileName:" << result;
    return result;
}

QString SimulationResults::getSummaryFileName() const
{
    QString result = QFileInfo(m_summaryFileName).fileName();
    qCDebug(lcClientShip) << "SimulationResults::getSummaryFileName:" << result;
    return result;
}

SimulationSummaryData SimulationResults::summaryData() const
{
    qCDebug(lcClientShip) << "SimulationResults::summaryData: returning summary data";
    return m_summaryData;
}

QByteArray SimulationResults::trajectoryFileData() const
{
    qCDebug(lcClientShip) << "SimulationResults::trajectoryFileData: size=" << m_trajectoryFileData.size();
    return m_trajectoryFileData;
}

QString SimulationResults::trajectoryFileName() const
{
    qCDebug(lcClientShip) << "SimulationResults::trajectoryFileName:" << m_trajectoryFileName;
    return m_trajectoryFileName;
}

QString SimulationResults::summaryFileName() const
{
    qCDebug(lcClientShip) << "SimulationResults::summaryFileName:" << m_summaryFileName;
    return m_summaryFileName;
}

} // namespace ShipClient
} // namespace Backend
} // namespace CargoNetSim
