#include "TrainNetwork.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Commons/Units.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QPair>
#include <QQueue>
#include <QSet>
#include <QString>
#include <QTextStream>
#include <QVector>
#include <algorithm>
#include <limits>

namespace CargoNetSim
{
namespace Backend
{
namespace TrainClient
{

// NeTrainSimNode Implementation
NeTrainSimNode::NeTrainSimNode(QObject *parent)
    : BaseObject(parent)
    , m_simulatorId(0)
    , m_userId(0)
    , m_x(0.0f)
    , m_y(0.0f)
    , m_xScale(1.0f)
    , m_yScale(1.0f)
    , m_isTerminal(false)
    , m_dwellTime(0.0f)
{
}

NeTrainSimNode::NeTrainSimNode(
    int simulatorId, int userId, float x, float y,
    const QString &description, float xScale, float yScale,
    bool isTerminal, float dwellTime, QObject *parent)
    : BaseObject(parent)
    , m_simulatorId(simulatorId)
    , m_userId(userId)
    , m_x(x)
    , m_y(y)
    , m_description(description)
    , m_xScale(xScale)
    , m_yScale(yScale)
    , m_isTerminal(isTerminal)
    , m_dwellTime(dwellTime)
{
}

NeTrainSimNode::NeTrainSimNode(const QJsonObject &json,
                               QObject           *parent)
    : BaseObject(parent)
{
    m_simulatorId = json["simulator_id"].toInt();
    m_userId      = json["user_id"].toInt();
    m_x           = json["x"].toDouble();
    m_y           = json["y"].toDouble();
    m_description = json["description"].toString();
    m_xScale      = json["x_scale"].toDouble();
    m_yScale      = json["y_scale"].toDouble();
    m_isTerminal  = json["is_terminal"].toBool();
    m_dwellTime   = json["dwell_time"].toDouble();
}

QJsonObject NeTrainSimNode::toDict() const
{
    QJsonObject dict;
    dict["simulator_id"] = m_simulatorId;
    dict["user_id"]      = m_userId;
    dict["x"]            = m_x;
    dict["y"]            = m_y;
    dict["description"]  = m_description;
    dict["x_scale"]      = m_xScale;
    dict["y_scale"]      = m_yScale;
    dict["is_terminal"]  = m_isTerminal;
    dict["dwell_time"]   = m_dwellTime;
    return dict;
}

NeTrainSimNode *
NeTrainSimNode::fromDict(const QJsonObject &data,
                         QObject           *parent)
{
    return new NeTrainSimNode(
        data["simulator_id"].toInt(),
        data["user_id"].toInt(), data["x"].toDouble(),
        data["y"].toDouble(),
        data["description"].toString(),
        data["x_scale"].toDouble(),
        data["y_scale"].toDouble(),
        data["is_terminal"].toBool(),
        data["dwell_time"].toDouble(), parent);
}

void NeTrainSimNode::setSimulatorId(int simulatorId)
{
    if (m_simulatorId != simulatorId)
    {
        m_simulatorId = simulatorId;
        emit nodeChanged();
    }
}

void NeTrainSimNode::setUserId(int userId)
{
    if (m_userId != userId)
    {
        m_userId = userId;
        emit nodeChanged();
    }
}

void NeTrainSimNode::setX(float x)
{
    if (m_x != x)
    {
        m_x = x;
        emit nodeChanged();
    }
}

void NeTrainSimNode::setY(float y)
{
    if (m_y != y)
    {
        m_y = y;
        emit nodeChanged();
    }
}

void NeTrainSimNode::setDescription(
    const QString &description)
{
    if (m_description != description)
    {
        m_description = description;
        emit nodeChanged();
    }
}

void NeTrainSimNode::setXScale(float xScale)
{
    if (m_xScale != xScale)
    {
        m_xScale = xScale;
        emit nodeChanged();
    }
}

void NeTrainSimNode::setYScale(float yScale)
{
    if (m_yScale != yScale)
    {
        m_yScale = yScale;
        emit nodeChanged();
    }
}

void NeTrainSimNode::setIsTerminal(bool isTerminal)
{
    if (m_isTerminal != isTerminal)
    {
        m_isTerminal = isTerminal;
        emit nodeChanged();
    }
}

void NeTrainSimNode::setDwellTime(float dwellTime)
{
    if (m_dwellTime != dwellTime)
    {
        m_dwellTime = dwellTime;
        emit nodeChanged();
    }
}

// NeTrainSimLink Implementation
NeTrainSimLink::NeTrainSimLink(QObject *parent)
    : BaseObject(parent)
    , m_simulatorId(0)
    , m_userId(0)
    , m_fromNode(nullptr)
    , m_toNode(nullptr)
    , m_length(0.0f)
    , m_maxSpeed(0.0f)
    , m_signalId(0)
    , m_grade(0.0f)
    , m_curvature(0.0f)
    , m_numDirections(1)
    , m_speedVariationFactor(0.0f)
    , m_hasCatenary(false)
    , m_lengthScale(1.0f)
    , m_speedScale(1.0f)
{
}

NeTrainSimLink::NeTrainSimLink(
    int simulatorId, int userId, NeTrainSimNode *fromNode,
    NeTrainSimNode *toNode, float length, float maxSpeed,
    int signalId, const QString &signalsAtNodes,
    float grade, float curvature, int numDirections,
    float speedVariationFactor, bool hasCatenary,
    const QString &region, float lengthScale,
    float speedScale, QObject *parent)
    : BaseObject(parent)
    , m_simulatorId(simulatorId)
    , m_userId(userId)
    , m_fromNode(fromNode)
    , m_toNode(toNode)
    , m_length(length)
    , m_maxSpeed(maxSpeed)
    , m_signalId(signalId)
    , m_signalsAtNodes(signalsAtNodes)
    , m_grade(grade)
    , m_curvature(curvature)
    , m_numDirections(numDirections)
    , m_speedVariationFactor(speedVariationFactor)
    , m_hasCatenary(hasCatenary)
    , m_region(region)
    , m_lengthScale(lengthScale)
    , m_speedScale(speedScale)
{
    // Connect to node changes
    if (m_fromNode)
    {
        connect(m_fromNode, &NeTrainSimNode::nodeChanged,
                this, &NeTrainSimLink::linkChanged);
    }

    if (m_toNode)
    {
        connect(m_toNode, &NeTrainSimNode::nodeChanged,
                this, &NeTrainSimLink::linkChanged);
    }
}

NeTrainSimLink::NeTrainSimLink(const QJsonObject &json,
                               QObject           *parent)
    : BaseObject(parent)
{
    m_simulatorId = json["simulator_id"].toInt();
    m_userId      = json["user_id"].toInt();

    // Parse from_node and to_node
    QJsonObject fromNodeObj = json["from_node"].toObject();
    QJsonObject toNodeObj   = json["to_node"].toObject();

    m_fromNode = new NeTrainSimNode(fromNodeObj, this);
    m_toNode   = new NeTrainSimNode(toNodeObj, this);

    m_length         = json["length"].toDouble();
    m_maxSpeed       = json["max_speed"].toDouble();
    m_signalId       = json["signal_id"].toInt();
    m_signalsAtNodes = json["signals_at_nodes"].toString();
    m_grade          = json["grade"].toDouble();
    m_curvature      = json["curvature"].toDouble();
    m_numDirections  = json["num_directions"].toInt();
    m_speedVariationFactor =
        json["speed_variation_factor"].toDouble();
    m_hasCatenary = json["has_catenary"].toBool();
    m_region      = json["region"].toString();
    m_lengthScale = json["length_scale"].toDouble();
    m_speedScale  = json["speed_scale"].toDouble();

    // Connect to node changes
    connect(m_fromNode, &NeTrainSimNode::nodeChanged, this,
            &NeTrainSimLink::linkChanged);
    connect(m_toNode, &NeTrainSimNode::nodeChanged, this,
            &NeTrainSimLink::linkChanged);
}

QJsonObject NeTrainSimLink::toDict() const
{
    QJsonObject dict;
    dict["simulator_id"] = m_simulatorId;
    dict["user_id"]      = m_userId;

    // Convert nodes to JSON
    dict["from_node"] = m_fromNode->toDict();
    dict["to_node"]   = m_toNode->toDict();

    dict["length"]                 = m_length;
    dict["max_speed"]              = m_maxSpeed;
    dict["signal_id"]              = m_signalId;
    dict["signals_at_nodes"]       = m_signalsAtNodes;
    dict["grade"]                  = m_grade;
    dict["curvature"]              = m_curvature;
    dict["num_directions"]         = m_numDirections;
    dict["speed_variation_factor"] = m_speedVariationFactor;
    dict["has_catenary"]           = m_hasCatenary;
    dict["region"]                 = m_region;
    dict["length_scale"]           = m_lengthScale;
    dict["speed_scale"]            = m_speedScale;

    return dict;
}

NeTrainSimLink *
NeTrainSimLink::fromDict(const QJsonObject &data,
                         QObject           *parent)
{
    // Create from_node and to_node objects
    NeTrainSimNode *fromNode = NeTrainSimNode::fromDict(
        data["from_node"].toObject(), parent);
    NeTrainSimNode *toNode = NeTrainSimNode::fromDict(
        data["to_node"].toObject(), parent);

    NeTrainSimLink *link = new NeTrainSimLink(
        data["simulator_id"].toInt(),
        data["user_id"].toInt(), fromNode, toNode,
        data["length"].toDouble(),
        data["max_speed"].toDouble(),
        data["signal_id"].toInt(),
        data["signals_at_nodes"].toString(),
        data["grade"].toDouble(),
        data["curvature"].toDouble(),
        data["num_directions"].toInt(),
        data["speed_variation_factor"].toDouble(),
        data["has_catenary"].toBool(),
        data["region"].toString(),
        data["length_scale"].toDouble(),
        data["speed_scale"].toDouble(), parent);

    // Set parent for nodes to ensure proper memory
    // management
    fromNode->setParent(link);
    toNode->setParent(link);

    return link;
}

void NeTrainSimLink::setSimulatorId(int simulatorId)
{
    if (m_simulatorId != simulatorId)
    {
        m_simulatorId = simulatorId;
        emit linkChanged();
    }
}

void NeTrainSimLink::setUserId(int userId)
{
    if (m_userId != userId)
    {
        m_userId = userId;
        emit linkChanged();
    }
}

void NeTrainSimLink::setFromNode(NeTrainSimNode *fromNode)
{
    if (m_fromNode)
    {
        disconnect(m_fromNode, &NeTrainSimNode::nodeChanged,
                   this, &NeTrainSimLink::linkChanged);
    }

    m_fromNode = fromNode;

    if (m_fromNode)
    {
        connect(m_fromNode, &NeTrainSimNode::nodeChanged,
                this, &NeTrainSimLink::linkChanged);
    }

    emit linkChanged();
}

void NeTrainSimLink::setToNode(NeTrainSimNode *toNode)
{
    if (m_toNode)
    {
        disconnect(m_toNode, &NeTrainSimNode::nodeChanged,
                   this, &NeTrainSimLink::linkChanged);
    }

    m_toNode = toNode;

    if (m_toNode)
    {
        connect(m_toNode, &NeTrainSimNode::nodeChanged,
                this, &NeTrainSimLink::linkChanged);
    }

    emit linkChanged();
}

void NeTrainSimLink::setLength(float length)
{
    if (m_length != length)
    {
        m_length = length;
        emit linkChanged();
    }
}

void NeTrainSimLink::setMaxSpeed(float maxSpeed)
{
    if (m_maxSpeed != maxSpeed)
    {
        m_maxSpeed = maxSpeed;
        emit linkChanged();
    }
}

void NeTrainSimLink::setSignalId(int signalId)
{
    if (m_signalId != signalId)
    {
        m_signalId = signalId;
        emit linkChanged();
    }
}

void NeTrainSimLink::setSignalsAtNodes(
    const QString &signalsAtNodes)
{
    if (m_signalsAtNodes != signalsAtNodes)
    {
        m_signalsAtNodes = signalsAtNodes;
        emit linkChanged();
    }
}

void NeTrainSimLink::setGrade(float grade)
{
    if (m_grade != grade)
    {
        m_grade = grade;
        emit linkChanged();
    }
}

void NeTrainSimLink::setCurvature(float curvature)
{
    if (m_curvature != curvature)
    {
        m_curvature = curvature;
        emit linkChanged();
    }
}

void NeTrainSimLink::setNumDirections(int numDirections)
{
    if (m_numDirections != numDirections)
    {
        m_numDirections = numDirections;
        emit linkChanged();
    }
}

void NeTrainSimLink::setSpeedVariationFactor(
    float speedVariationFactor)
{
    if (m_speedVariationFactor != speedVariationFactor)
    {
        m_speedVariationFactor = speedVariationFactor;
        emit linkChanged();
    }
}

void NeTrainSimLink::setHasCatenary(bool hasCatenary)
{
    if (m_hasCatenary != hasCatenary)
    {
        m_hasCatenary = hasCatenary;
        emit linkChanged();
    }
}

void NeTrainSimLink::setRegion(const QString &region)
{
    if (m_region != region)
    {
        m_region = region;
        emit linkChanged();
    }
}

void NeTrainSimLink::setLengthScale(float lengthScale)
{
    if (m_lengthScale != lengthScale)
    {
        m_lengthScale = lengthScale;
        emit linkChanged();
    }
}

void NeTrainSimLink::setSpeedScale(float speedScale)
{
    if (m_speedScale != speedScale)
    {
        m_speedScale = speedScale;
        emit linkChanged();
    }
}

// NeTrainSimNodeDataReader Implementation
QVector<QMap<QString, QString>>
NeTrainSimNodeDataReader::readNodesFile(
    const QString &filename)
{
    QVector<QMap<QString, QString>> records;

    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        throw std::runtime_error(
            QString("Error reading nodes "
                    "file: %1")
                .arg(file.errorString())
                .toStdString());
    }

    QTextStream in(&file);
    QStringList lines;

    while (!in.atEnd())
    {
        lines.append(in.readLine());
    }

    file.close();

    if (lines.isEmpty())
    {
        throw std::runtime_error("Nodes file is empty");
    }

    // Parse scale values from second line
    QStringList scales = lines[1].trimmed().split('\t');
    if (scales.size() < 3)
    {
        throw std::runtime_error(
            "Bad nodes file structure");
    }

    QString n      = scales[0];
    QString scaleX = scales[1];
    QString scaleY = scales[2];

    // Parse node records
    for (int i = 2; i < lines.size(); ++i)
    {
        QStringList values = lines[i].trimmed().split('\t');

        // Ensure we have enough values
        if (values.size() < 5)
        {
            continue; // Skip malformed records
        }

        // Add description if missing
        if (values.size() < 6)
        {
            values.append("ND");
        }

        QMap<QString, QString> record;
        record["UserID"]            = values[0];
        record["x"]                 = values[1];
        record["y"]                 = values[2];
        record["IsTerminal"]        = values[3];
        record["TerminalDwellTime"] = values[4];
        record["Desc"]              = values[5];
        record["XScale"]            = scaleX;
        record["YScale"]            = scaleY;

        records.append(record);
    }

    return records;
}

// NeTrainSimLinkDataReader Implementation
QVector<QMap<QString, QString>>
NeTrainSimLinkDataReader::readLinksFile(
    const QString &filename)
{
    QVector<QMap<QString, QString>> records;

    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        throw std::runtime_error(
            QString("Error reading links "
                    "file: %1")
                .arg(file.errorString())
                .toStdString());
    }

    QTextStream in(&file);
    QStringList lines;

    while (!in.atEnd())
    {
        lines.append(in.readLine());
    }

    file.close();

    if (lines.isEmpty())
    {
        throw std::runtime_error("Links file is empty");
    }

    // Parse scale values from second line
    QStringList scales = lines[1].trimmed().split('\t');
    if (scales.size() < 3)
    {
        throw std::runtime_error(
            "Bad links file structure");
    }

    QString n           = scales[0];
    QString lengthScale = scales[1];
    QString speedScale  = scales[2];

    // Parse link records
    for (int i = 2; i < lines.size(); ++i)
    {
        QStringList values = lines[i].trimmed().split('\t');

        // Ensure we have minimum required values
        if (values.size() < 11)
        {
            continue; // Skip malformed records
        }

        QMap<QString, QString> record;
        record["UserID"]           = values[0];
        record["FromNodeID"]       = values[1];
        record["ToNodeID"]         = values[2];
        record["Length"]           = values[3];
        record["FreeFlowSpeed"]    = values[4];
        record["SignalNo"]         = values[5];
        record["DirectionalGrade"] = values[6];
        record["Curvature"]        = values[7];
        record["Directions"]       = values[8];
        record["SpeedVariation"]   = values[9];
        record["HasCatenary"]      = values[10];

        // Add optional fields if available
        if (values.size() > 11)
        {
            record["SignalsAtNodes"] = values[11];
        }
        else
        {
            record["SignalsAtNodes"] = "";
        }

        if (values.size() > 12)
        {
            record["Region"] = values[12];
        }
        else
        {
            record["Region"] = "ND Region";
        }

        record["LengthScale"]        = lengthScale;
        record["FreeFlowSpeedScale"] = speedScale;

        records.append(record);
    }

    return records;
}

///////////////////////////////////////////////////////////////////////////////

// NeTrainSimNetworkBase Implementation

NeTrainSimNetwork::NeTrainSimNetwork(QObject *parent)
    : BaseNetwork(parent)
    , m_graph(new DirectedGraph<int>(this))
{
    // Connect graph signals to network signals
    connect(m_graph, &DirectedGraph<int>::graphChanged,
            this, &NeTrainSimNetwork::networkChanged);
}

NeTrainSimNetwork::~NeTrainSimNetwork()
{
    // Clean up node and link objects (if not already
    // parented properly)
    qDeleteAll(m_nodes);
    qDeleteAll(m_links);

    m_nodes.clear();
    m_links.clear();
}

void NeTrainSimNetwork::setVariable(const QString  &key,
                                    const QVariant &value)
{
    QMutexLocker locker(&m_mutex);
    m_variables[key] = value;
}

QVariant
NeTrainSimNetwork::getVariable(const QString &key) const
{
    QMutexLocker locker(&m_mutex);
    return m_variables.value(key);
}

QMap<QString, QVariant>
NeTrainSimNetwork::getVariables() const
{
    QMutexLocker locker(&m_mutex);
    return m_variables;
}

void NeTrainSimNetwork::loadNetwork(
    const QString &nodesFile, const QString &linksFile)
{
    qCDebug(lcRail) << "[RailLoad] loadNetwork start"
             << "nodes=" << nodesFile
             << "links=" << linksFile;
    QMutexLocker locker(&m_mutex);

    // Clean up existing objects
    qDeleteAll(m_nodes);
    qDeleteAll(m_links);

    m_nodes.clear();
    m_links.clear();

    // Clear the graph
    m_graph->clear();

    try
    {
        // Read nodes
        qCDebug(lcRail) << "[RailLoad] readNodesFile begin";
        QVector<QMap<QString, QString>> nodeRecords =
            NeTrainSimNodeDataReader::readNodesFile(
                nodesFile);
        qCDebug(lcRail) << "[RailLoad] readNodesFile ok, records="
                 << nodeRecords.size();
        m_nodes = generateNodes(nodeRecords);
        qCDebug(lcRail) << "[RailLoad] generateNodes ok, nodes="
                 << m_nodes.size();

        // Read links
        qCDebug(lcRail) << "[RailLoad] readLinksFile begin";
        QVector<QMap<QString, QString>> linkRecords =
            NeTrainSimLinkDataReader::readLinksFile(
                linksFile);
        qCDebug(lcRail) << "[RailLoad] readLinksFile ok, records="
                 << linkRecords.size();
        m_links = generateLinks(linkRecords);
        qCDebug(lcRail) << "[RailLoad] generateLinks ok, links="
                 << m_links.size();

        // Build graph representation
        qCDebug(lcRail) << "[RailLoad] buildGraph begin";
        buildGraph();
        qCDebug(lcRail) << "[RailLoad] buildGraph ok";

        qCDebug(lcRail) << "[RailLoad] emit signals";
        emit networkChanged();
        emit nodesChanged();
        emit linksChanged();
        qCDebug(lcRail) << "[RailLoad] loadNetwork done";
    }
    catch (const std::exception &e)
    {
        qCCritical(lcRail)
            << "[RailLoad] Error loading network:"
            << e.what();
        throw;
    }
}

QJsonArray NeTrainSimNetwork::getNodesAsJson() const
{
    QMutexLocker         locker(&m_mutex);
    QJsonArray           nodeJsons;

    for (NeTrainSimNode *node : m_nodes)
    {
        nodeJsons.append(node->toDict());
    }

    return nodeJsons;
}

QJsonArray NeTrainSimNetwork::getLinksAsJson() const
{
    QMutexLocker         locker(&m_mutex);
    QJsonArray           linkJsons;

    for (NeTrainSimLink *link : m_links)
    {
        linkJsons.append(link->toDict());
    }

    return linkJsons;
}

QVector<NeTrainSimNode *>
NeTrainSimNetwork::getNodes() const
{
    QMutexLocker locker(&m_mutex);
    return m_nodes;
}

QVector<NeTrainSimLink *>
NeTrainSimNetwork::getLinks() const
{
    QMutexLocker locker(&m_mutex);
    return m_links;
}

const NeTrainSimNode *
NeTrainSimNetwork::getNodeByID(int id) const
{
    for (auto node : m_nodes)
    {
        if (node->getUserId() == id)
        {
            return node;
        }
    }

    return nullptr;
}

void NeTrainSimNetwork::setNetworkName(QString networkName)
{
    QMutexLocker locker(&m_mutex);
    m_networkName = networkName;
}

QString NeTrainSimNetwork::getNetworkName() const
{
    QMutexLocker locker(&m_mutex);
    return m_networkName;
}

NeTrainSimNode *
NeTrainSimNetwork::getNodeByUserId(int userId) const
{
    for (NeTrainSimNode *node : m_nodes)
    {
        if (node->getUserId() == userId)
        {
            return node;
        }
    }
    return nullptr;
}

QVector<NeTrainSimNode *> NeTrainSimNetwork::generateNodes(
    const QVector<QMap<QString, QString>> &nodeRecords)
{
    QVector<NeTrainSimNode *> nodes;

    for (int i = 0; i < nodeRecords.size(); ++i)
    {
        const QMap<QString, QString> &record =
            nodeRecords[i];

        bool isTerminal =
            record["IsTerminal"].toLower() == "true"
            || record["IsTerminal"] == "1";

        NeTrainSimNode *node = new NeTrainSimNode(
            i, // simulator_id
            record["UserID"].toInt(), record["x"].toFloat(),
            record["y"].toFloat(), record["Desc"],
            record["XScale"].toFloat(),
            record["YScale"].toFloat(), isTerminal,
            record["TerminalDwellTime"].toFloat(), this);

        nodes.append(node);
    }

    return nodes;
}

QVector<NeTrainSimLink *> NeTrainSimNetwork::generateLinks(
    const QVector<QMap<QString, QString>> &linkRecords)
{
    QVector<NeTrainSimLink *> links;

    for (int i = 0; i < linkRecords.size(); ++i)
    {
        const QMap<QString, QString> &record =
            linkRecords[i];

        NeTrainSimNode *fromNode =
            getNodeByUserId(record["FromNodeID"].toInt());
        NeTrainSimNode *toNode =
            getNodeByUserId(record["ToNodeID"].toInt());

        if (!fromNode || !toNode)
        {
            qCCritical(lcRail)
                << "[RailLoad] missing node(s) for"
                        << " linkId=" << record["UserID"]
                        << " fromId=" << record["FromNodeID"]
                        << " toId=" << record["ToNodeID"]
                        << " fromResolved=" << (fromNode != nullptr)
                        << " toResolved=" << (toNode != nullptr);
            throw std::runtime_error(
                QString("Could not find nodes for link %1")
                    .arg(record["UserID"])
                    .toStdString());
        }

        bool hasCatenary =
            record["HasCatenary"].toLower() == "true"
            || record["HasCatenary"] == "1";

        NeTrainSimLink *link = new NeTrainSimLink(
            i, // simulator_id
            record["UserID"].toInt(), fromNode, toNode,
            record["Length"].toFloat(),
            record["FreeFlowSpeed"].toFloat(),
            record["SignalNo"].toInt(),
            record["SignalsAtNodes"],
            record["DirectionalGrade"].toFloat(),
            record["Curvature"].toFloat(),
            record["Directions"].toInt(),
            record["SpeedVariation"].toFloat(), hasCatenary,
            record["Region"],
            record["LengthScale"].toFloat(),
            record["FreeFlowSpeedScale"].toFloat(), this);

        links.append(link);
    }

    return links;
}

void NeTrainSimNetwork::buildGraph()
{
    m_graph->clear();

    // Add nodes to the graph
    for (NeTrainSimNode *node : m_nodes)
    {
        int userId = node->getUserId();

        // Create attributes map for the node
        QMap<QString, QVariant> attributes;
        attributes["simulator_id"] = node->getSimulatorId();
        attributes["x"]            = node->getX();
        attributes["y"]            = node->getY();
        attributes["description"]  = node->getDescription();
        attributes["is_terminal"]  = node->isTerminal();
        attributes["dwell_time"] =
            node->dwellTimeUnits().value();
        attributes["x_scale"]      = node->getXScale();
        attributes["y_scale"]      = node->getYScale();

        // Add node to the graph
        m_graph->addNode(userId, attributes);
    }

    // Add edges to the graph
    for (NeTrainSimLink *link : m_links)
    {
        int   fromNodeId = link->getFromNode()->getUserId();
        int   toNodeId   = link->getToNode()->getUserId();
        float length = static_cast<float>(
            link->lengthUnits().value());
        int   numDirections = link->getNumDirections();

        // Create attributes map for the edge
        QMap<QString, QVariant> attributes;
        attributes["simulator_id"] = link->getSimulatorId();
        attributes["user_id"]      = link->getUserId();
        attributes["max_speed"] =
            link->maxSpeedUnits().value();
        attributes["signal_id"]    = link->getSignalId();
        attributes["signals_at_nodes"] =
            link->getSignalsAtNodes();
        attributes["grade"]     = link->getGrade();
        attributes["curvature"] = link->getCurvature();
        attributes["speed_variation_factor"] =
            link->getSpeedVariationFactor();
        attributes["has_catenary"] = link->hasCatenary();
        attributes["region"]       = link->getRegion();
        attributes["length_scale"] = link->getLengthScale();
        attributes["speed_scale"]  = link->getSpeedScale();

        // Add forward direction edge
        m_graph->addEdge(fromNodeId, toNodeId, length,
                         attributes);

        // Add reverse direction if bidirectional
        if (numDirections == 2)
        {
            m_graph->addEdge(toNodeId, fromNodeId, length,
                             attributes);
        }
    }
}

QPair<QVector<int>, QVector<float>>
NeTrainSimNetwork::getPathLinks(
    const QVector<int> &path) const
{
    QVector<int>   linkIds;
    QVector<float> distances;

    // For each consecutive pair of nodes in the path
    for (int i = 0; i < path.size() - 1; ++i)
    {
        int  fromNodeId = path[i];
        int  toNodeId   = path[i + 1];
        bool found      = false;

        // Find the link connecting these nodes
        for (NeTrainSimLink *link : m_links)
        {
            int linkFromNodeId =
                link->getFromNode()->getUserId();
            int linkToNodeId =
                link->getToNode()->getUserId();

            if (linkFromNodeId == fromNodeId
                && linkToNodeId == toNodeId)
            {
                linkIds.append(link->getUserId());
                distances.append(static_cast<float>(
                    link->lengthUnits().value()));
                found = true;
                break;
            }

            // Check reverse direction if link is
            // bidirectional
            if (link->getNumDirections() == 2
                && linkFromNodeId == toNodeId
                && linkToNodeId == fromNodeId)
            {
                linkIds.append(link->getUserId());
                distances.append(static_cast<float>(
                    link->lengthUnits().value()));
                found = true;
                break;
            }
        }

        if (!found)
        {
            qCWarning(lcRail)
                << "Could not find link between nodes"
                << fromNodeId << "and" << toNodeId;
        }
    }

    return qMakePair(linkIds, distances);
}

ShortestPathResult NeTrainSimNetwork::findShortestPath(
    int startNodeId, int endNodeId,
    const QString &optimizeFor)
{
    QMutexLocker       locker(&m_mutex);
    ShortestPathResult result;

    if (optimizeFor != "distance" && optimizeFor != "time")
    {
        throw std::invalid_argument(
            "optimize_for must be either "
            "'distance' or 'time'");
    }

    result.optimizationCriterion = optimizeFor;

    // Find shortest path using the directed graph
    result.pathNodes = m_graph->findShortestPath(
        startNodeId, endNodeId, optimizeFor);

    // If no path found, return empty result (already
    // initialized with infinity values)
    if (result.pathNodes.isEmpty())
    {
        return result;
    }

    // Get link IDs along the path
    QPair<QVector<int>, QVector<float>> pathLinksInfo =
        getPathLinks(result.pathNodes);
    result.pathLinks             = pathLinksInfo.first;
    QVector<float> linkDistances = pathLinksInfo.second;

    // Calculate total distance and travel time in canonical
    // SI units that match the upstream NeTrainSim contract:
    // length in meters, speed in meters/second, time in seconds.
    double totalLengthMeters    = 0.0;
    double minTravelTimeSeconds = 0.0;

    for (int i = 0; i < result.pathLinks.size(); ++i)
    {
        int   linkId   = result.pathLinks[i];
        const double distanceMeters =
            static_cast<double>(linkDistances[i]);
        totalLengthMeters += distanceMeters;

        // Rebuild per-link travel time from the same SI
        // edge contract the graph uses when optimizing for
        // "time".
        for (NeTrainSimLink *link : m_links)
        {
            if (link->getUserId() == linkId)
            {
                const double safeMaxSpeedMetersPerSecond =
                    std::max(link->maxSpeedUnits().value(),
                             0.01);
                minTravelTimeSeconds +=
                    distanceMeters / safeMaxSpeedMetersPerSecond;
                break;
            }
        }
    }

    result.setTotalLength(Units::meters(totalLengthMeters));
    result.setMinTravelTime(Units::seconds(minTravelTimeSeconds));

    return result;
}

QJsonObject NeTrainSimNetwork::nodesToJson() const
{
    QMutexLocker locker(&m_mutex);

    if (m_nodes.isEmpty())
    {
        QJsonObject result;
        result["scales"] =
            QJsonObject{{"x", "1.0"}, {"y", "1.0"}};
        result["nodes"] = QJsonArray();
        return result;
    }

    NeTrainSimNode *firstNode = m_nodes.first();
    QJsonObject     scales{
            {"x", QString::number(firstNode->getXScale())},
            {"y", QString::number(firstNode->getYScale())}};

    QJsonArray nodesArray;
    for (NeTrainSimNode *node : m_nodes)
    {
        QJsonObject nodeData{
            {"userID", node->getUserId()},
            {"x", node->getX()},
            {"y", node->getY()},
            {"description", node->getDescription()},
            {"isTerminal", node->isTerminal()},
            {"terminalDwellTime",
             node->dwellTimeUnits().value()}};
        nodesArray.append(nodeData);
    }

    QJsonObject result;
    result["scales"] = scales;
    result["nodes"]  = nodesArray;

    return result;
}

QJsonObject NeTrainSimNetwork::linksToJson() const
{
    QMutexLocker locker(&m_mutex);

    if (m_links.isEmpty())
    {
        QJsonObject result;
        result["scales"] = QJsonObject{{"length", "1.0"},
                                       {"speed", "1.0"}};
        result["links"]  = QJsonArray();
        return result;
    }

    NeTrainSimLink *firstLink = m_links.first();
    QJsonObject     scales{
            {"length",
             QString::number(firstLink->getLengthScale())},
            {"speed",
             QString::number(firstLink->getSpeedScale())}};

    QJsonArray linksArray;
    for (NeTrainSimLink *link : m_links)
    {
        QJsonObject linkData{
            {"userID", link->getUserId()},
            {"fromNodeID",
             link->getFromNode()->getUserId()},
            {"toNodeID", link->getToNode()->getUserId()},
            {"length", link->lengthUnits().value()},
            {"maxSpeed", link->maxSpeedUnits().value()},
            {"trafficSignalID", link->getSignalId()},
            {"grade", link->getGrade()},
            {"curvature", link->getCurvature()},
            {"numberOfDirections",
             link->getNumDirections()},
            {"speedVariationFactor",
             link->getSpeedVariationFactor()},
            {"isCatenaryAvailable", link->hasCatenary()},
            {"signalsAtNodes", link->getSignalsAtNodes()},
            {"region", link->getRegion()}};
        linksArray.append(linkData);
    }

    QJsonObject result;
    result["scales"] = scales;
    result["links"]  = linksArray;

    return result;
}

void NeTrainSimNetwork::setNodesAndLinksFromJson(
    const QVector<QJsonObject> &nodes,
    const QVector<QJsonObject> &links)
{
    QMutexLocker locker(&m_mutex);

    // Clean up existing objects
    qDeleteAll(m_nodes);
    qDeleteAll(m_links);

    m_nodes.clear();
    m_links.clear();
    // Create node objects
    for (const QJsonObject &nodeJson : nodes)
    {
        NeTrainSimNode *node =
            NeTrainSimNode::fromDict(nodeJson, this);
        m_nodes.append(node);
    }

    // Create link objects
    for (const QJsonObject &linkJson : links)
    {
        // We need to find the actual node objects for
        // fromNode and toNode
        int fromNodeId = linkJson["from_node"]
                             .toObject()["user_id"]
                             .toInt();
        int toNodeId = linkJson["to_node"]
                           .toObject()["user_id"]
                           .toInt();

        NeTrainSimNode *fromNode =
            getNodeByUserId(fromNodeId);
        NeTrainSimNode *toNode = getNodeByUserId(toNodeId);

        if (fromNode && toNode)
        {
            NeTrainSimLink *link = new NeTrainSimLink(
                linkJson["simulator_id"].toInt(),
                linkJson["user_id"].toInt(), fromNode,
                toNode, linkJson["length"].toDouble(),
                linkJson["max_speed"].toDouble(),
                linkJson["signal_id"].toInt(),
                linkJson["signals_at_nodes"].toString(),
                linkJson["grade"].toDouble(),
                linkJson["curvature"].toDouble(),
                linkJson["num_directions"].toInt(),
                linkJson["speed_variation_factor"]
                    .toDouble(),
                linkJson["has_catenary"].toBool(),
                linkJson["region"].toString(),
                linkJson["length_scale"].toDouble(),
                linkJson["speed_scale"].toDouble(), this);

            m_links.append(link);
        }
        else
        {
            qCWarning(lcRail) << "Could not find nodes for link"
                       << linkJson["user_id"].toInt();
        }
    }

    // Rebuild the graph
    initializeGraph();

    emit networkChanged();
    emit nodesChanged();
    emit linksChanged();
}

void NeTrainSimNetwork::initializeGraph()
{
    buildGraph();
}

} // namespace TrainClient
} // namespace Backend
} // namespace CargoNetSim
