/**
 * @file SimulationResults.cpp
 * @brief Implements truck simulation results
 * @author Ahmed Aredah
 * @date March 21, 2025
 */

#include "SimulationResults.h"
#include <QFileInfo>
#include <QJsonArray>

#include "Backend/Commons/LogCategories.h"

namespace CargoNetSim
{
namespace Backend
{
namespace TruckClient
{

SimulationResults::SimulationResults(QObject *parent)
    : QObject(parent)
    , m_summaryData(QList<QPair<QString, QString>>())
    , m_trajectoryFileData()
    , m_trajectoryFileName()
    , m_summaryFileName()
{
    qCDebug(lcClientTruck) << "SimulationResults::SimulationResults: default constructed";
}

SimulationResults::SimulationResults(
    const QList<QPair<QString, QString>> &summaryData,
    const QByteArray                     &trajectoryData,
    const QString &trajectoryFileName,
    const QString &summaryFileName, QObject *parent)
    : QObject(parent)
    , m_summaryData(summaryData)
    , m_trajectoryFileData(trajectoryData)
    , m_trajectoryFileName(trajectoryFileName)
    , m_summaryFileName(summaryFileName)
{
    qCDebug(lcClientTruck) << "SimulationResults::SimulationResults:"
                           << "summaryPairs=" << summaryData.size()
                           << "trajectoryDataSize=" << trajectoryData.size()
                           << "trajectoryFile=" << trajectoryFileName
                           << "summaryFile=" << summaryFileName;
}

SimulationResults *
SimulationResults::fromJson(const QJsonObject &jsonObj,
                            QObject           *parent)
{
    qCDebug(lcClientTruck) << "SimulationResults::fromJson: parsing JSON object"
                           << "keys=" << jsonObj.keys();

    QList<QPair<QString, QString>> summaryData;
    QJsonArray                     summaryArray =
        jsonObj["summaryData"].toArray();
    qCDebug(lcClientTruck) << "SimulationResults::fromJson: summaryArray size=" << summaryArray.size();

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

    QByteArray trajectoryData;
    if (jsonObj["trajectoryFileDataIncluded"].toBool())
    {
        QString base64Data =
            jsonObj["trajectoryFileData"].toString();
        trajectoryData =
            QByteArray::fromBase64(base64Data.toLatin1());
        qCDebug(lcClientTruck) << "SimulationResults::fromJson: decoded trajectory data"
                               << "size=" << trajectoryData.size();
    }
    else
    {
        qCDebug(lcClientTruck) << "SimulationResults::fromJson: no trajectory data included";
    }

    qCInfo(lcClientTruck) << "SimulationResults::fromJson: parsed"
                          << summaryData.size() << "summary pairs";

    return new SimulationResults(
        summaryData, trajectoryData,
        jsonObj["trajectoryFileName"].toString(),
        jsonObj["summaryFileName"].toString(), parent);
}

QString SimulationResults::getTrajectoryFileName() const
{
    QString result = QFileInfo(m_trajectoryFileName).fileName();
    qCDebug(lcClientTruck) << "SimulationResults::getTrajectoryFileName:" << result;
    return result;
}

QString SimulationResults::getSummaryFileName() const
{
    QString result = QFileInfo(m_summaryFileName).fileName();
    qCDebug(lcClientTruck) << "SimulationResults::getSummaryFileName:" << result;
    return result;
}

const SimulationSummaryData &
SimulationResults::summaryData() const
{
    qCDebug(lcClientTruck) << "SimulationResults::summaryData: returning summary data";
    return m_summaryData;
}

QByteArray SimulationResults::trajectoryFileData() const
{
    qCDebug(lcClientTruck) << "SimulationResults::trajectoryFileData: size=" << m_trajectoryFileData.size();
    return m_trajectoryFileData;
}

QString SimulationResults::trajectoryFileName() const
{
    qCDebug(lcClientTruck) << "SimulationResults::trajectoryFileName:" << m_trajectoryFileName;
    return m_trajectoryFileName;
}

QString SimulationResults::summaryFileName() const
{
    qCDebug(lcClientTruck) << "SimulationResults::summaryFileName:" << m_summaryFileName;
    return m_summaryFileName;
}

} // namespace TruckClient
} // namespace Backend
} // namespace CargoNetSim
