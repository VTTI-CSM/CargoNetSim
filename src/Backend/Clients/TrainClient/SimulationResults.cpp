#include "SimulationResults.h"
#include <QJsonArray>

#include "Backend/Commons/LogCategories.h"

/**
 * @file SimulationResults.cpp
 * @brief Implementation of SimulationResults class
 * @author Ahmed Aredah
 * @date March 20, 2025
 *
 * Implements the SimulationResults class, managing
 * simulation results with extensive comments for auditing
 * and review.
 */

namespace CargoNetSim
{
namespace Backend
{
namespace TrainClient
{

SimulationResults::SimulationResults(
    const QVector<QPair<QString, QString>> &summaryData,
    const QByteArray &trajectoryFileData,
    const QString    &trajectoryFileName,
    const QString    &summaryFileName)
    : m_summaryData(summaryData)
    , // Init summary data
    m_trajectoryFileData(trajectoryFileData)
    , // Init file data
    m_trajectoryFileName(trajectoryFileName)
    ,                                  // Init file name
    m_summaryFileName(summaryFileName) // Init summary name
{
    qCDebug(lcClientTrain) << "SimulationResults::SimulationResults:"
                           << "summaryPairs=" << summaryData.size()
                           << "trajectoryDataSize=" << trajectoryFileData.size()
                           << "trajectoryFile=" << trajectoryFileName
                           << "summaryFile=" << summaryFileName;
}

SimulationResults
SimulationResults::fromJson(const QJsonObject &jsonObj)
{
    qCDebug(lcClientTrain) << "SimulationResults::fromJson: parsing JSON object"
                           << "keys=" << jsonObj.keys();

    // Vector to store summary data as text-value pairs
    QVector<QPair<QString, QString>> summaryData;

    // Extract summary data array from JSON object
    QJsonArray summaryArray =
        jsonObj["summaryData"].toArray();
    qCDebug(lcClientTrain) << "SimulationResults::fromJson: summaryArray size=" << summaryArray.size();

    // Iterate over each element in the summary array
    for (const QJsonValue &pairVal : summaryArray)
    {
        // Convert value to object (expected format:
        // key-value pairs)
        QJsonObject pairObj = pairVal.toObject();

        // Process each key-value pair in the object
        for (auto it = pairObj.begin(); it != pairObj.end();
             ++it)
        {
            // Clean the key by removing whitespace
            QString key = it.key().trimmed();

            // Skip if key is empty to avoid invalid entries
            if (!key.isEmpty())
            {
                // Get value, trim if it exists, empty
                // string if not
                QString value =
                    it.value().toString().trimmed();

                // Add cleaned key-value pair to summary
                // data
                summaryData.append(qMakePair(key, value));
            }
        }
    }

    // Initialize trajectory file data as empty by default
    QByteArray trajectoryFileData;

    // Check if trajectory data is included in JSON
    if (jsonObj["trajectoryFileDataIncluded"].toBool())
    {
        // Get base64-encoded string from JSON
        QString base64Data =
            jsonObj["trajectoryFileData"].toString();

        // Decode base64 string to raw byte data
        trajectoryFileData =
            QByteArray::fromBase64(base64Data.toUtf8());
        qCDebug(lcClientTrain) << "SimulationResults::fromJson: decoded trajectory data"
                               << "size=" << trajectoryFileData.size();
    }
    else
    {
        qCDebug(lcClientTrain) << "SimulationResults::fromJson: no trajectory data included";
    }

    // Extract trajectory file name from JSON, default to
    // empty
    QString trajectoryFileName =
        jsonObj["trajectoryFileName"].toString();

    // Extract summary file name from JSON, default to empty
    QString summaryFileName =
        jsonObj["summaryFileName"].toString();

    qCInfo(lcClientTrain) << "SimulationResults::fromJson: parsed"
                          << summaryData.size() << "summary pairs";

    // Return a new instance with parsed data
    return SimulationResults(
        summaryData,        // Parsed summary data
        trajectoryFileData, // Decoded file data
        trajectoryFileName, // Trajectory file name
        summaryFileName);   // Summary file name
}

SimulationSummaryData SimulationResults::summaryData() const
{
    qCDebug(lcClientTrain) << "SimulationResults::summaryData: returning summary data";
    return m_summaryData;
}

QByteArray SimulationResults::trajectoryFileData() const
{
    qCDebug(lcClientTrain) << "SimulationResults::trajectoryFileData: size=" << m_trajectoryFileData.size();
    return m_trajectoryFileData;
}

QString SimulationResults::trajectoryFileName() const
{
    qCDebug(lcClientTrain) << "SimulationResults::trajectoryFileName:" << m_trajectoryFileName;
    return m_trajectoryFileName;
}

QString SimulationResults::summaryFileName() const
{
    qCDebug(lcClientTrain) << "SimulationResults::summaryFileName:" << m_summaryFileName;
    return m_summaryFileName;
}

QString SimulationResults::getTrajectoryFileName() const
{
    // Split the file name by '/' to separate path
    // components
    QStringList parts = m_trajectoryFileName.split("/");
    QString result = parts.isEmpty() ? "" : parts.last();
    qCDebug(lcClientTrain) << "SimulationResults::getTrajectoryFileName:" << result;
    return result;
}

QString SimulationResults::getSummaryFileName() const
{
    // Split the file name by '/' to separate path
    // components
    QStringList parts = m_summaryFileName.split("/");
    QString result = parts.isEmpty() ? "" : parts.last();
    qCDebug(lcClientTrain) << "SimulationResults::getSummaryFileName:" << result;
    return result;
}

} // namespace TrainClient
} // namespace Backend
} // namespace CargoNetSim
