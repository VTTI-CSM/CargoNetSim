/**
 * @file IntegrationNodeDataReader.cpp
 * @brief Implements reader for node data from file
 * @author [Your Name]
 * @date 2025-03-22
 */

#include "IntegrationNodeDataReader.h"
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

IntegrationNodeDataReader::IntegrationNodeDataReader(
    QObject *parent)
    : QObject(parent)
{
    qCDebug(lcClientTruck) << "IntegrationNodeDataReader::IntegrationNodeDataReader: constructed";
}

QVector<IntegrationNode *>
IntegrationNodeDataReader::readNodesFile(
    const QString &filename, QObject *parent) const
{
    qCDebug(lcClientTruck) << "IntegrationNodeDataReader::readNodesFile:"
                           << "filename=" << filename;

    try
    {
        QFile file(filename);
        if (!file.open(QIODevice::ReadOnly
                       | QIODevice::Text))
        {
            qCCritical(lcClientTruck) << "IntegrationNodeDataReader::readNodesFile:"
                                      << "cannot open file:" << filename;
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
        qCDebug(lcClientTruck) << "IntegrationNodeDataReader::readNodesFile:"
                               << "read" << lines.size() << "non-empty lines";

        if (lines.isEmpty())
        {
            qCCritical(lcClientTruck) << "IntegrationNodeDataReader::readNodesFile:"
                                      << "file is empty:" << filename;
            throw std::runtime_error("Nodes file is empty");
        }

        // Parse scale information from second line
        QStringList scales = lines[1].split(
            QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (scales.size() < 3)
        {
            qCCritical(lcClientTruck) << "IntegrationNodeDataReader::readNodesFile:"
                                      << "invalid scale line:" << lines[1];
            throw std::runtime_error(
                "Bad nodes file structure: invalid scale "
                "information");
        }

        bool  convOk;
        float scaleX = scales[1].toFloat(&convOk);
        if (!convOk)
        {
            qCCritical(lcClientTruck) << "IntegrationNodeDataReader::readNodesFile:"
                                      << "invalid X scale:" << scales[1];
            throw std::runtime_error(
                "Invalid X scale value");
        }

        float scaleY = scales[2].toFloat(&convOk);
        if (!convOk)
        {
            qCCritical(lcClientTruck) << "IntegrationNodeDataReader::readNodesFile:"
                                      << "invalid Y scale:" << scales[2];
            throw std::runtime_error(
                "Invalid Y scale value");
        }

        qCDebug(lcClientTruck) << "IntegrationNodeDataReader::readNodesFile:"
                               << "scaleX=" << scaleX << "scaleY=" << scaleY;

        // Process node records starting from line 3
        QVector<IntegrationNode *> nodes;
        int skippedLines = 0;
        for (int i = 2; i < lines.size(); i++)
        {
            QStringList values =
                lines[i].split(QRegularExpression("\\s+"),
                               Qt::SkipEmptyParts);
            if (values.size() < 6)
            {
                // Ensure at least the required fields are
                // present
                ++skippedLines;
                continue;
            }

            // Extract description (which might contain
            // spaces)
            QString description;
            if (values.size() > 6)
            {
                description = values.mid(6).join(" ");
            }

            bool ok;
            // Read all values as float/double first
            float nodeIdF = values[0].toFloat(&ok);
            if (!ok)
                continue;
            double xCoord = values[1].toDouble(&ok);
            if (!ok)
                continue;
            double yCoord = values[2].toDouble(&ok);
            if (!ok)
                continue;
            float nodeTypeF = values[3].toFloat(&ok);
            if (!ok)
                continue;
            float macroZoneClusterF =
                values[4].toFloat(&ok);
            if (!ok)
                continue;
            float infoAvailabilityF =
                values[5].toFloat(&ok);
            if (!ok)
                continue;

            // Convert float values to int for the int
            // fields
            int nodeId   = static_cast<int>(nodeIdF);
            int nodeType = static_cast<int>(nodeTypeF);
            int macroZoneCluster =
                static_cast<int>(macroZoneClusterF);
            int infoAvailability =
                static_cast<int>(infoAvailabilityF);

            // Create a node object
            IntegrationNode *node = new IntegrationNode(
                nodeId, xCoord, yCoord, nodeType,
                macroZoneCluster, infoAvailability,
                description, scaleX, scaleY, parent);

            nodes.append(node);
        }

        if (skippedLines > 0)
        {
            qCWarning(lcClientTruck) << "IntegrationNodeDataReader::readNodesFile:"
                                     << "skipped" << skippedLines << "lines with insufficient fields";
        }

        qCInfo(lcClientTruck) << "IntegrationNodeDataReader::readNodesFile:"
                              << "parsed" << nodes.size() << "nodes from" << filename;
        return nodes;
    }
    catch (const std::exception &e)
    {
        // Clean up any created nodes before rethrowing
        qCCritical(lcClientTruck) << "IntegrationNodeDataReader::readNodesFile:"
                                  << "error:" << e.what();
        throw;
    }
}

} // namespace TruckClient
} // namespace Backend
} // namespace CargoNetSim
