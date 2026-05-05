/**
 * @file TruckNetwork.cpp
 * @brief Implements truck network model
 * @author Ahmed Aredah
 * @date 2025-03-22
 */

#include "TruckNetwork.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QTextStream>

#include "Backend/Commons/LogCategories.h"
#include "Backend/Commons/Units.h"

namespace CargoNetSim
{
namespace Backend
{
namespace TruckClient
{

// Node and Link class forward declarations
class IntegrationNode;
class IntegrationLink;

/**************** SharedIntegrationNetwork ****************/

IntegrationNetwork::IntegrationNetwork(QObject *parent)
    : BaseNetwork(parent)
{
}

IntegrationNetwork::~IntegrationNetwork()
{
    // Clean up resources
    qDeleteAll(m_nodeObjects);
    qDeleteAll(m_linkObjects);
    if (m_graph)
    {
        m_graph->deleteLater();
        m_graph = nullptr;
    }

    m_nodeObjects.clear();
    m_linkObjects.clear();
}

void IntegrationNetwork::initializeNetwork(
    const QVector<IntegrationNode *> &nodes,
    const QVector<IntegrationLink *> &links)
{
    qCInfo(lcClientTruck) << "IntegrationNetwork::initializeNetwork:"
                          << "nodes=" << nodes.size()
                          << "links=" << links.size();
    QMutexLocker locker(&m_mutex);

    // Clean up existing resources
    qDeleteAll(m_nodeObjects);
    qDeleteAll(m_linkObjects);

    m_nodeObjects.clear();
    m_linkObjects.clear();
    if (m_graph)
    {
        m_graph->deleteLater();
        m_graph = nullptr;
    }
    m_graph = new TransportationGraph<int>(); // Reset graph

    // Store nodes and links
    m_nodeObjects = nodes;
    m_linkObjects = links;

    // Take ownership of objects
    for (auto *node : nodes)
    {
        node->setParent(this);

        // Add node to graph with relevant attributes
        QMap<QString, QVariant> attributes;
        attributes["x"]    = node->getXCoordinate();
        attributes["y"]    = node->getYCoordinate();
        attributes["type"] = node->getNodeType();
        m_graph->addNode(node->getNodeId(), attributes);
    }

    // Process links
    for (auto *link : links)
    {
        link->setParent(this);

        // Add edge to graph
        int   fromNode = link->getUpstreamNodeId();
        int   toNode   = link->getDownstreamNodeId();
        float weight = static_cast<float>(
            link->lengthUnits().value());

        QMap<QString, QVariant> attributes;
        attributes["link_id"]    = link->getLinkId();
        attributes["free_speed"] =
            link->freeSpeedUnits().value();
        attributes["lanes"]      = link->getLanes();

        m_graph->addEdge(fromNode, toNode, weight,
                         attributes);
    }

    qCDebug(lcClientTruck) << "IntegrationNetwork::initializeNetwork:"
                          << "graph built with"
                          << m_graph->getNodes().size() << "nodes,"
                          << m_linkObjects.size() << "edges";

    // Emit change signals
    emit networkChanged();
    emit nodesChanged();
    emit linksChanged();
}

bool IntegrationNetwork::nodeExists(int nodeId) const
{
    QMutexLocker locker(&m_mutex);
    return m_graph->hasNode(nodeId);
}

ShortestPathResult
IntegrationNetwork::findShortestPath(int startNodeId,
                                     int endNodeId)
{
    qCDebug(lcClientTruck) << "IntegrationNetwork::findShortestPath:"
                           << "from=" << startNodeId
                           << "to=" << endNodeId;
    QMutexLocker       locker(&m_mutex);
    ShortestPathResult result;

    result.optimizationCriterion =
        "distance"; // Default optimization criterion

    // Find path using transportation graph
    result.pathNodes = m_graph->findShortestPath(
        startNodeId, endNodeId, "distance");

    // If no path found, return empty result (already
    // initialized with infinity values)
    if (result.pathNodes.isEmpty())
    {
        qCWarning(lcClientTruck) << "IntegrationNetwork::findShortestPath:"
                                 << "no path found from" << startNodeId
                                 << "to" << endNodeId;
        return result;
    }

    // Get corresponding links
    result.pathLinks = getPathLinks(result.pathNodes);

    // Calculate path metrics
    result.setTotalLength(
        Units::toMeters(Units::kilometers(
            getPathLengthByLinks(result.pathLinks))));
    result.setMinTravelTime(
        Units::toSeconds(Units::hours(
            m_graph->calculatePathMetric(result.pathNodes,
                                         "time"))));

    return result;
}

QVector<int> IntegrationNetwork::getEndNodes() const
{
    QMutexLocker locker(&m_mutex);
    QVector<int> endNodes;

    // Find nodes with no outgoing edges
    for (int nodeId : m_graph->getNodes())
    {
        if (m_graph->getOutDegree(nodeId) == 0)
        {
            endNodes.append(nodeId);
        }
    }

    return endNodes;
}

QVector<int> IntegrationNetwork::getStartNodes() const
{
    QMutexLocker locker(&m_mutex);
    QVector<int> startNodes;

    // Find nodes with no incoming edges
    for (int nodeId : m_graph->getNodes())
    {
        if (m_graph->getInDegree(nodeId) == 0)
        {
            startNodes.append(nodeId);
        }
    }

    return startNodes;
}

QVector<IntegrationNode *>
IntegrationNetwork::getNodes() const
{
    QMutexLocker locker(&m_mutex);
    return m_nodeObjects;
}

QVector<IntegrationLink *>
IntegrationNetwork::getLinks() const
{
    QMutexLocker locker(&m_mutex);
    return m_linkObjects;
}

const TransportationGraph<int> *
IntegrationNetwork::getGraph() const
{
    return m_graph;
}

IntegrationNode *
IntegrationNetwork::getNode(int nodeId) const
{
    QMutexLocker locker(&m_mutex);

    // Find node by ID
    for (IntegrationNode *node : m_nodeObjects)
    {
        if (node->getNodeId() == nodeId)
        {
            return node;
        }
    }

    qCWarning(lcClientTruck) << "IntegrationNetwork::getNode:"
                             << "node not found:" << nodeId;
    return nullptr;
}

IntegrationLink *
IntegrationNetwork::getLink(int linkId) const
{
    QMutexLocker locker(&m_mutex);

    // Find link by ID
    for (IntegrationLink *link : m_linkObjects)
    {
        if (link->getLinkId() == linkId)
        {
            return link;
        }
    }

    qCWarning(lcClientTruck) << "IntegrationNetwork::getLink:"
                             << "link not found:" << linkId;
    return nullptr;
}

void IntegrationNetwork::setNetworkName(QString networkName)
{
    QMutexLocker locker(&m_mutex);
    m_networkName = networkName;
}

QString IntegrationNetwork::getNetworkName() const
{
    return m_networkName;
}

QList<ShortestPathResult>
IntegrationNetwork::getMultiplePaths(int startNodeId,
                                     int endNodeId,
                                     int maxPaths)
{
    qCDebug(lcClientTruck) << "IntegrationNetwork::getMultiplePaths:"
                           << "from=" << startNodeId
                           << "to=" << endNodeId
                           << "maxPaths=" << maxPaths;
    QMutexLocker              locker(&m_mutex);
    QList<ShortestPathResult> results;

    // Find k-shortest paths
    QList<QVector<int>> paths = m_graph->findKShortestPaths(
        startNodeId, endNodeId, maxPaths);

    // Process each path
    for (const QVector<int> &path : paths)
    {
        if (path.isEmpty())
            continue;

        ShortestPathResult result;
        result.pathNodes = path;

        // Get links for path
        result.pathLinks = getPathLinks(path);

        // Calculate metrics
        result.setTotalLength(
            Units::toMeters(Units::kilometers(
                getPathLengthByLinks(result.pathLinks))));
        result.setMinTravelTime(
            Units::toSeconds(Units::hours(
                m_graph->calculatePathMetric(path, "time"))));
        result.optimizationCriterion =
            "distance"; // Default criterion

        results.append(result);
    }

    qCDebug(lcClientTruck) << "IntegrationNetwork::getMultiplePaths:"
                           << "found" << results.size() << "paths";
    return results;
}

void IntegrationNetwork::setVariable(const QString  &key,
                                     const QVariant &value)
{
    QMutexLocker locker(&m_mutex);
    m_variables[key] = value;
}

QVariant
IntegrationNetwork::getVariable(const QString &key) const
{
    QMutexLocker locker(&m_mutex);
    return m_variables.value(key);
}

QMap<QString, QVariant>
IntegrationNetwork::getVariables() const
{
    QMutexLocker locker(&m_mutex);
    return m_variables;
}

QJsonObject IntegrationNetwork::toJson() const
{
    qCDebug(lcClientTruck) << "IntegrationNetwork::toJson:"
                           << "nodes=" << m_nodeObjects.size()
                           << "links=" << m_linkObjects.size();
    QMutexLocker locker(&m_mutex);
    QJsonObject  result;

    // Add nodes
    QJsonArray nodesArray;
    for (const IntegrationNode *node : m_nodeObjects)
    {
        nodesArray.append(node->toDict());
    }
    result["nodes"] = nodesArray;

    // Add links
    QJsonArray linksArray;
    for (const IntegrationLink *link : m_linkObjects)
    {
        linksArray.append(link->toDict());
    }
    result["links"] = linksArray;

    return result;
}

QVector<int> IntegrationNetwork::getPathLinks(
    const QVector<int> &pathNodes) const
{
    QVector<int> pathLinks;

    // Skip empty paths
    if (pathNodes.size() <= 1)
    {
        return pathLinks;
    }

    // Find links connecting consecutive nodes
    for (int i = 0; i < pathNodes.size() - 1; ++i)
    {
        int fromNode = pathNodes[i];
        int toNode   = pathNodes[i + 1];

        // Get link ID from edge attributes
        QMap<QString, QVariant> edgeAttrs =
            m_graph->getEdgeAttributes(fromNode, toNode);

        if (edgeAttrs.contains("link_id"))
        {
            pathLinks.append(edgeAttrs["link_id"].toInt());
        }
    }

    return pathLinks;
}

double IntegrationNetwork::getPathLengthByLinks(
    const QVector<int> &linkIds) const
{
    double totalLength = 0.0;

    // Sum lengths of all links
    for (int linkId : linkIds)
    {
        // Find link
        for (const IntegrationLink *link : m_linkObjects)
        {
            if (link->getLinkId() == linkId)
            {
                totalLength += link->lengthUnits().value();
                break;
            }
        }
    }

    return totalLength;
}

/********** IntegrationSimulationConfig ***************/

IntegrationSimulationConfig::IntegrationSimulationConfig(
    QObject *parent)
    : QObject(parent)
    , m_simTime(0.0)
{
    // Create network object
    m_network = new IntegrationNetwork(this);
}

IntegrationSimulationConfig::~IntegrationSimulationConfig()
{
    if (m_network)
    {
        m_network->deleteLater();
        m_network = nullptr;
    }
}

bool IntegrationSimulationConfig::initialize(
    const QString &configDir, const QString &title,
    double                        simTime,
    const QMap<QString, QString> &inputFiles,
    const QMap<QString, QString> &outputFiles,
    const QString &inputFolder, const QString &outputFolder,
    const QMap<QString, QVariant> &additionalVariables)
{
    QMutexLocker locker(&m_mutex);

    // Store basic configuration
    m_configDir   = configDir;
    m_title       = title;
    m_simTime     = simTime;
    m_inputFiles  = inputFiles;
    m_outputFiles = outputFiles;

    // Set folders
    m_inputFolder  = inputFolder;
    m_outputFolder = outputFolder;

    // Store additional variables
    for (auto it = additionalVariables.constBegin();
         it != additionalVariables.constEnd(); ++it)
    {
        m_variables[it.key()] = it.value();
    }

    try
    {
        // Read node data using the node reader
        QString nodeFilePath =
            getInputFilePath("node_coordinates");
        IntegrationNodeDataReader  nodeReader;
        QVector<IntegrationNode *> nodes =
            nodeReader.readNodesFile(nodeFilePath);

        if (nodes.isEmpty())
        {
            throw std::runtime_error("No node data found");
        }

        // Read link data using the link reader
        QString linkFilePath =
            getInputFilePath("link_structure");
        IntegrationLinkDataReader  linkReader;
        QVector<IntegrationLink *> links =
            linkReader.readLinksFile(linkFilePath);

        if (links.isEmpty())
        {
            throw std::runtime_error("No link data found");
        }

        // Initialize the network with the nodes and links
        m_network->initializeNetwork(nodes, links);
        m_network->setParent(this);

        emit configChanged();
        return true;
    }
    catch (const std::exception &e)
    {
        qCCritical(lcClientTruck) << "Error initializing configuration:"
                    << e.what();
        return false;
    }
}

IntegrationNetwork *
IntegrationSimulationConfig::getNetwork() const
{
    return m_network;
}

QString IntegrationSimulationConfig::getInputFilePath(
    const QString &key) const
{
    if (!m_inputFiles.contains(key))
    {
        throw std::runtime_error(
            QString("No input file found with key '%1'")
                .arg(key)
                .toStdString());
    }

    return QDir(m_configDir)
        .filePath(QDir(m_inputFolder)
                      .filePath(m_inputFiles[key]));
}

QString IntegrationSimulationConfig::getOutputFilePath(
    const QString &key) const
{
    if (!m_outputFiles.contains(key))
    {
        throw std::runtime_error(
            QString("No output file found with key '%1'")
                .arg(key)
                .toStdString());
    }

    return QDir(m_configDir)
        .filePath(QDir(m_outputFolder)
                      .filePath(m_outputFiles[key]));
}

QJsonObject IntegrationSimulationConfig::toJson() const
{
    QMutexLocker locker(&m_mutex);
    QJsonObject  result;

    // Add basic properties
    result["config_dir"]    = m_configDir;
    result["title"]         = m_title;
    result["sim_time"]      = m_simTime;
    result["input_folder"]  = m_inputFolder;
    result["output_folder"] = m_outputFolder;

    // Add input files
    QJsonObject inputFilesObj;
    for (auto it = m_inputFiles.constBegin();
         it != m_inputFiles.constEnd(); ++it)
    {
        inputFilesObj[it.key()] = it.value();
    }
    result["input_files"] = inputFilesObj;

    // Add output files
    QJsonObject outputFilesObj;
    for (auto it = m_outputFiles.constBegin();
         it != m_outputFiles.constEnd(); ++it)
    {
        outputFilesObj[it.key()] = it.value();
    }
    result["output_files"] = outputFilesObj;

    // Add variables - convert QVariant to appropriate
    // QJsonValue
    QJsonObject varsObj;
    for (auto it = m_variables.constBegin();
         it != m_variables.constEnd(); ++it)
    {
        QVariant var = it.value();
        switch (var.typeId())
        {
        case QMetaType::Bool:
            varsObj[it.key()] = var.toBool();
            break;
        case QMetaType::Int:
        case QMetaType::Long:
        case QMetaType::LongLong:
            varsObj[it.key()] = var.toInt();
            break;
        case QMetaType::UInt:
        case QMetaType::ULong:
        case QMetaType::ULongLong:
            varsObj[it.key()] =
                (int)var.toUInt(); // JSON only supports
                                   // signed integers
            break;
        case QMetaType::Float:
        case QMetaType::Double:
            varsObj[it.key()] = var.toDouble();
            break;
        case QMetaType::QString:
            varsObj[it.key()] = var.toString();
            break;
        case QMetaType::QStringList: {
            QJsonArray array;
            for (const QString &str : var.toStringList())
            {
                array.append(str);
            }
            varsObj[it.key()] = array;
        }
        break;
        case QMetaType::QVariantList: {
            QJsonArray array;
            for (const QVariant &v : var.toList())
            {
                if (v.typeId() == QMetaType::Int)
                    array.append(v.toInt());
                else if (v.typeId() == QMetaType::Double)
                    array.append(v.toDouble());
                else if (v.typeId() == QMetaType::Bool)
                    array.append(v.toBool());
                else
                    array.append(v.toString());
            }
            varsObj[it.key()] = array;
        }
        break;
        case QMetaType::QVariantMap: {
            QJsonObject obj;
            QVariantMap map = var.toMap();
            for (auto mapIt = map.begin();
                 mapIt != map.end(); ++mapIt)
            {
                if (mapIt.value().typeId()
                    == QMetaType::Int)
                    obj[mapIt.key()] =
                        mapIt.value().toInt();
                else if (mapIt.value().typeId()
                         == QMetaType::Double)
                    obj[mapIt.key()] =
                        mapIt.value().toDouble();
                else if (mapIt.value().typeId()
                         == QMetaType::Bool)
                    obj[mapIt.key()] =
                        mapIt.value().toBool();
                else
                    obj[mapIt.key()] =
                        mapIt.value().toString();
            }
            varsObj[it.key()] = obj;
        }
        break;
        default:
            // For other types, convert to string as a
            // fallback
            varsObj[it.key()] = var.toString();
            break;
        }
    }
    result["variables"] = varsObj;

    // Add network
    result["network"] = m_network->toJson();

    return result;
}

/******* IntegrationSimulationConfigReader *******/

IntegrationSimulationConfigReader::
    IntegrationSimulationConfigReader(
        const QString &configFilePath, QObject *parent)
    : QObject(parent)
{
    // Check if the file exists
    if (!QFile::exists(configFilePath))
    {
        qCCritical(lcClientTruck) << "Configuration file does not exist:"
                    << configFilePath;
        return;
    }

    // Read configuration
    m_config = readConfig(configFilePath);
    if (!m_config)
    {
        qCCritical(lcClientTruck)
            << "Failed to read valid configuration from:"
            << configFilePath;
        return;
    }
}

IntegrationSimulationConfigReader::
    ~IntegrationSimulationConfigReader()
{
    // Don't delete m_config, ownership has been transferred
    m_config = nullptr;
}

IntegrationSimulationConfig *
IntegrationSimulationConfigReader::readConfig(
    const QString &configFilePath)
{
    try
    {
        // Get config directory
        QString configDir =
            QFileInfo(configFilePath).dir().absolutePath();

        // Open file
        QFile file(configFilePath);
        if (!file.open(QIODevice::ReadOnly
                       | QIODevice::Text))
        {
            throw std::runtime_error(
                QString("Error opening file: %1")
                    .arg(file.errorString())
                    .toStdString());
        }

        // Read lines
        QTextStream in(&file);
        QStringList lines;
        while (!in.atEnd())
        {
            QString line = in.readLine();
            // // Remove any control characters
            // line.remove(
            //     QRegularExpression("[\\x00-\\x1F\\x7F]"));
            if (!line.isEmpty())
            {
                lines.append(line.trimmed());
            }
        }
        file.close();

        // Validate file
        if (lines.isEmpty())
        {
            throw std::runtime_error(
                "Configuration file is empty");
        }

        // Parse title (line 1)
        QString title = lines[0].trimmed();

        // Parse simulation parameters (line 2)
        QStringList simParams = lines[1].trimmed().split(
            QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (simParams.size() < 5)
        {
            throw std::runtime_error(
                "Invalid simulation parameters");
        }

        double simTime = simParams[0].toDouble();

        // Parse folders
        QString inputFolder =
            lines[2].trimmed().replace("\\", "").replace(
                "/", "");
        inputFolder =
            inputFolder.isEmpty() ? "." : inputFolder;

        QString outputFolder =
            lines[3].trimmed().replace("\\", "").replace(
                "/", "");
        outputFolder =
            outputFolder.isEmpty() ? "." : outputFolder;

        // Parse input files
        QMap<QString, QString> inputFiles;
        inputFiles["node_coordinates"] = lines[4].trimmed();
        inputFiles["link_structure"]   = lines[5].trimmed();
        inputFiles["signal_timing"]    = lines[6].trimmed();
        inputFiles["traffic_demands"]  = lines[7].trimmed();
        inputFiles["incident_descriptions"] =
            lines[8].trimmed();

        // Parse output files
        QMap<QString, QString> outputFiles;
        int                    lineIndex  = 9;
        QStringList            outputKeys = {
            "standard_output",
            "link_flow_microscopic",
            "link_flow_minimum_tree",
            "minimum_path_tree_routing",
            "trip_based_vehicle_probe",
            "second_by_second_vehicle_probe",
            "link_travel_time",
            "minimum_path_tree_output_1",
            "minimum_path_tree_output_2",
            "vehicle_departures",
            "individual_vehicle_path",
            "emission_concentration",
            "summary_output",
            "link_flow_mesoscopic",
            "time_space_output"};

        for (const QString &key : outputKeys)
        {
            if (lineIndex < lines.size())
            {
                QString value =
                    lines[lineIndex++].trimmed();
                outputFiles[key] = value;
            }
        }

        // Store additional variables
        QMap<QString, QVariant> additionalVariables;
        additionalVariables["output_freq_10"] =
            simParams[1].toInt();
        additionalVariables["output_freq_12_14"] =
            simParams[2].toInt();
        additionalVariables["routing_option"] =
            simParams[3].toInt();
        additionalVariables["pause_flag"] =
            simParams[4].toInt();

        // Create and initialize the configuration object
        IntegrationSimulationConfig *config =
            new IntegrationSimulationConfig();

        // Initialize with all parameters
        if (!config->initialize(configDir, title, simTime,
                                inputFiles, outputFiles,
                                inputFolder, outputFolder,
                                additionalVariables))
        {
            delete config;
            throw std::runtime_error(
                "Failed to initialize configuration");
        }

        config->setSourceConfigPath(configFilePath);

        return config;
    }
    catch (const std::exception &e)
    {
        qCCritical(lcClientTruck) << "Error reading configuration file:"
                    << e.what();
        return nullptr;
    }
}

} // namespace TruckClient
} // namespace Backend
} // namespace CargoNetSim
