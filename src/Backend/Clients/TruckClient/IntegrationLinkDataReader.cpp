/**
 * @file IntegrationLinkDataReader.cpp
 * @brief Implements reader for link data from file
 * @author [Your Name]
 * @date 2025-03-22
 */

#include "IntegrationLinkDataReader.h"
#include <QFile>
#include <QRegularExpression>
#include <QTextStream>
#include <stdexcept>

#include "Backend/Commons/LogCategories.h"

namespace CargoNetSim
{
namespace Backend
{
namespace TruckClient
{

IntegrationLinkDataReader::IntegrationLinkDataReader(
    QObject *parent)
    : QObject(parent)
{
    qCDebug(lcClientTruck)
        << "IntegrationLinkDataReader::IntegrationLinkDataReader:"
        << "created";
}

QVector<IntegrationLink *>
IntegrationLinkDataReader::readLinksFile(
    const QString &filename, QObject *parent) const
{
    qCInfo(lcClientTruck)
        << "IntegrationLinkDataReader::readLinksFile:"
        << "file=" << filename;

    try
    {
        QFile file(filename);
        if (!file.open(QIODevice::ReadOnly
                       | QIODevice::Text))
        {
            qCCritical(lcClientTruck)
                << "IntegrationLinkDataReader::readLinksFile:"
                << "file not found or cannot open:"
                << filename;
            throw std::runtime_error(
                QString("Cannot open file: %1")
                    .arg(filename)
                    .toStdString());
        }

        QTextStream stream(&file);
        QStringList lines;

        // Read all lines and filter out control characters
        while (!stream.atEnd())
        {
            QString line = stream.readLine().trimmed();
            // Remove control characters (like 0x1A)
            // line.remove(QRegularExpression("[\\x00-\\x1F\\x7F]"));
            if (!line.isEmpty())
            {
                lines.append(line);
            }
        }

        file.close();

        if (lines.isEmpty())
        {
            qCWarning(lcClientTruck)
                << "IntegrationLinkDataReader::readLinksFile:"
                << "file is empty:" << filename;
            throw std::runtime_error("Links file is empty");
        }

        qCDebug(lcClientTruck)
            << "IntegrationLinkDataReader::readLinksFile:"
            << "read" << lines.size() << "lines from file";

        // Parse scale information from second line
        QStringList scales = lines[1].split(
            QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (scales.size() < 6)
        {
            qCWarning(lcClientTruck)
                << "IntegrationLinkDataReader::readLinksFile:"
                << "invalid scale line, expected >=6 fields,"
                << "got" << scales.size();
            throw std::runtime_error(
                "Bad links file structure: invalid scale "
                "information");
        }

        bool  convOk;
        float lengthScale = scales[1].toFloat(&convOk);
        if (!convOk)
            throw std::runtime_error(
                "Invalid length scale value");

        float speedScale = scales[2].toFloat(&convOk);
        if (!convOk)
            throw std::runtime_error(
                "Invalid speed scale value");

        float saturationFlowScale =
            scales[3].toFloat(&convOk);
        if (!convOk)
            throw std::runtime_error(
                "Invalid saturation flow scale value");

        float speedAtCapacityScale =
            scales[4].toFloat(&convOk);
        if (!convOk)
            throw std::runtime_error(
                "Invalid speed at capacity scale value");

        float jamDensityScale = scales[5].toFloat(&convOk);
        if (!convOk)
            throw std::runtime_error(
                "Invalid jam density scale value");

        // Process link records starting from line 3
        QVector<IntegrationLink *> links;
        for (int i = 2; i < lines.size(); i++)
        {
            QStringList values =
                lines[i].split(QRegularExpression("\\s+"),
                               Qt::SkipEmptyParts);
            if (values.size() < 20)
            {
                // Ensure at least the required fields are
                // present
                continue;
            }

            // Extract description (which might contain
            // spaces)
            QString description;
            if (values.size() > 20)
            {
                description = values.mid(20).join(" ");
            }

            // Parse and validate all fields
            bool ok;
            // Read all as float first
            float linkIdFloat = values[0].toFloat(&ok);
            if (!ok)
                continue;
            float upstreamNodeIdFloat =
                values[1].toFloat(&ok);
            if (!ok)
                continue;
            float downstreamNodeIdFloat =
                values[2].toFloat(&ok);
            if (!ok)
                continue;
            float length = values[3].toFloat(&ok);
            if (!ok)
                continue;
            float freeSpeed = values[4].toFloat(&ok);
            if (!ok)
                continue;
            float saturationFlow = values[5].toFloat(&ok);
            if (!ok)
                continue;
            float lanes = values[6].toFloat(&ok);
            if (!ok)
                continue;
            float speedCoeffVariation =
                values[7].toFloat(&ok);
            if (!ok)
                continue;
            float speedAtCapacity = values[8].toFloat(&ok);
            if (!ok)
                continue;
            float jamDensity = values[9].toFloat(&ok);
            if (!ok)
                continue;
            float turnProhibitionFloat =
                values[10].toFloat(&ok);
            if (!ok)
                continue;
            float prohibitionStartFloat =
                values[11].toFloat(&ok);
            if (!ok)
                continue;
            float prohibitionEndFloat =
                values[12].toFloat(&ok);
            if (!ok)
                continue;
            float opposingLink1Float =
                values[13].toFloat(&ok);
            if (!ok)
                continue;
            float opposingLink2Float =
                values[14].toFloat(&ok);
            if (!ok)
                continue;
            float trafficSignalFloat =
                values[15].toFloat(&ok);
            if (!ok)
                continue;
            float phase1Float = values[16].toFloat(&ok);
            if (!ok)
                continue;
            float phase2Float = values[17].toFloat(&ok);
            if (!ok)
                continue;
            float vehicleClassProhibitionFloat =
                values[18].toFloat(&ok);
            if (!ok)
                continue;
            float surveillanceLevelFloat =
                values[19].toFloat(&ok);
            if (!ok)
                continue;

            // Convert float values to int for the int
            // fields
            int linkId = static_cast<int>(linkIdFloat);
            int upstreamNodeId =
                static_cast<int>(upstreamNodeIdFloat);
            int downstreamNodeId =
                static_cast<int>(downstreamNodeIdFloat);
            int turnProhibition =
                static_cast<int>(turnProhibitionFloat);
            int prohibitionStart =
                static_cast<int>(prohibitionStartFloat);
            int prohibitionEnd =
                static_cast<int>(prohibitionEndFloat);
            int opposingLink1 =
                static_cast<int>(opposingLink1Float);
            int opposingLink2 =
                static_cast<int>(opposingLink2Float);
            int trafficSignal =
                static_cast<int>(trafficSignalFloat);
            int phase1 = static_cast<int>(phase1Float);
            int phase2 = static_cast<int>(phase2Float);
            int vehicleClassProhibition = static_cast<int>(
                vehicleClassProhibitionFloat);
            int surveillanceLevel =
                static_cast<int>(surveillanceLevelFloat);

            // Create link object
            IntegrationLink *link = new IntegrationLink(
                linkId, upstreamNodeId, downstreamNodeId,
                length, freeSpeed, saturationFlow, lanes,
                speedCoeffVariation, speedAtCapacity,
                jamDensity, turnProhibition,
                prohibitionStart, prohibitionEnd,
                opposingLink1, opposingLink2, trafficSignal,
                phase1, phase2, vehicleClassProhibition,
                surveillanceLevel, description, lengthScale,
                speedScale, saturationFlowScale,
                speedAtCapacityScale, jamDensityScale,
                parent);

            links.append(link);
        }

        qCDebug(lcClientTruck)
            << "IntegrationLinkDataReader::readLinksFile:"
            << "parsed" << links.size() << "links from"
            << filename;

        return links;
    }
    catch (const std::exception &e)
    {
        // Log error and rethrow
        qCCritical(lcClientTruck) << "Error reading links file:"
                    << e.what();
        throw;
    }
}

} // namespace TruckClient
} // namespace Backend
} // namespace CargoNetSim
