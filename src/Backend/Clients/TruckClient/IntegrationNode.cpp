/**
 * @file IntegrationNode.cpp
 * @brief Implements transportation network node
 * @author [Your Name]
 * @date 2025-03-22
 */

#include "IntegrationNode.h"

#include "Backend/Commons/LogCategories.h"

namespace CargoNetSim
{
namespace Backend
{
namespace TruckClient
{

IntegrationNode::IntegrationNode(QObject *parent)
    : BaseObject(parent)
    , m_nodeId(0)
    , m_xCoordinate(0.0f)
    , m_yCoordinate(0.0f)
    , m_nodeType(0)
    , m_macroZoneCluster(0)
    , m_informationAvailability(0)
    , m_description("")
    , m_xScale(1.0f)
    , m_yScale(1.0f)
{
    qCDebug(lcClientTruck) << "IntegrationNode::IntegrationNode: default constructed";
}

IntegrationNode::IntegrationNode(
    int nodeId, float xCoordinate, float yCoordinate,
    int nodeType, int macroZoneCluster,
    int informationAvailability, const QString &description,
    float xScale, float yScale, QObject *parent)
    : BaseObject(parent)
    , m_nodeId(nodeId)
    , m_xCoordinate(xCoordinate)
    , m_yCoordinate(yCoordinate)
    , m_nodeType(nodeType)
    , m_macroZoneCluster(macroZoneCluster)
    , m_informationAvailability(informationAvailability)
    , m_description(description)
    , m_xScale(xScale)
    , m_yScale(yScale)
{
    qCDebug(lcClientTruck) << "IntegrationNode::IntegrationNode: nodeId=" << nodeId
                           << "xy=(" << xCoordinate << "," << yCoordinate << ")"
                           << "type=" << nodeType
                           << "macroZone=" << macroZoneCluster;
}

IntegrationNode::IntegrationNode(const QJsonObject &json,
                                 QObject           *parent)
    : BaseObject(parent)
{
    qCDebug(lcClientTruck) << "IntegrationNode::IntegrationNode: constructing from JSON";

    m_nodeId           = json["node_id"].toInt();
    m_xCoordinate      = json["x_coordinate"].toDouble();
    m_yCoordinate      = json["y_coordinate"].toDouble();
    m_nodeType         = json["node_type"].toInt();
    m_macroZoneCluster = json["macro_zone_cluster"].toInt();
    m_informationAvailability =
        json["information_availability"].toInt();
    m_description = json["description"].toString();
    m_xScale      = json["x_scale"].toDouble(1.0);
    m_yScale      = json["y_scale"].toDouble(1.0);

    qCDebug(lcClientTruck) << "IntegrationNode::IntegrationNode: from JSON"
                           << "nodeId=" << m_nodeId
                           << "xy=(" << m_xCoordinate << "," << m_yCoordinate << ")"
                           << "type=" << m_nodeType;
}

QJsonObject IntegrationNode::toDict() const
{
    qCDebug(lcClientTruck) << "IntegrationNode::toDict: nodeId=" << m_nodeId;

    QJsonObject dict;
    dict["node_id"]            = m_nodeId;
    dict["x_coordinate"]       = m_xCoordinate;
    dict["y_coordinate"]       = m_yCoordinate;
    dict["node_type"]          = m_nodeType;
    dict["macro_zone_cluster"] = m_macroZoneCluster;
    dict["information_availability"] =
        m_informationAvailability;
    dict["description"] = m_description;
    dict["x_scale"]     = m_xScale;
    dict["y_scale"]     = m_yScale;
    return dict;
}

IntegrationNode *
IntegrationNode::fromDict(const QJsonObject &data,
                          QObject           *parent)
{
    qCDebug(lcClientTruck) << "IntegrationNode::fromDict: nodeId=" << data["node_id"].toInt();

    return new IntegrationNode(
        data["node_id"].toInt(),
        data["x_coordinate"].toDouble(),
        data["y_coordinate"].toDouble(),
        data["node_type"].toInt(),
        data["macro_zone_cluster"].toInt(),
        data["information_availability"].toInt(),
        data["description"].toString(),
        data["x_scale"].toDouble(1.0),
        data["y_scale"].toDouble(1.0), parent);
}

void IntegrationNode::setNodeId(int nodeId)
{
    if (m_nodeId != nodeId)
    {
        qCDebug(lcClientTruck) << "IntegrationNode::setNodeId:" << m_nodeId << "->" << nodeId;
        m_nodeId = nodeId;
        emit nodeChanged();
    }
}

void IntegrationNode::setXCoordinate(float xCoordinate)
{
    if (m_xCoordinate != xCoordinate)
    {
        qCDebug(lcClientTruck) << "IntegrationNode::setXCoordinate: nodeId=" << m_nodeId
                               << m_xCoordinate << "->" << xCoordinate;
        m_xCoordinate = xCoordinate;
        emit nodeChanged();
    }
}

void IntegrationNode::setYCoordinate(float yCoordinate)
{
    if (m_yCoordinate != yCoordinate)
    {
        qCDebug(lcClientTruck) << "IntegrationNode::setYCoordinate: nodeId=" << m_nodeId
                               << m_yCoordinate << "->" << yCoordinate;
        m_yCoordinate = yCoordinate;
        emit nodeChanged();
    }
}

void IntegrationNode::setNodeType(int nodeType)
{
    if (m_nodeType != nodeType)
    {
        qCDebug(lcClientTruck) << "IntegrationNode::setNodeType: nodeId=" << m_nodeId
                               << m_nodeType << "->" << nodeType;
        m_nodeType = nodeType;
        emit nodeChanged();
    }
}

void IntegrationNode::setMacroZoneCluster(
    int macroZoneCluster)
{
    if (m_macroZoneCluster != macroZoneCluster)
    {
        qCDebug(lcClientTruck) << "IntegrationNode::setMacroZoneCluster: nodeId=" << m_nodeId
                               << m_macroZoneCluster << "->" << macroZoneCluster;
        m_macroZoneCluster = macroZoneCluster;
        emit nodeChanged();
    }
}

void IntegrationNode::setInformationAvailability(
    int informationAvailability)
{
    if (m_informationAvailability
        != informationAvailability)
    {
        qCDebug(lcClientTruck) << "IntegrationNode::setInformationAvailability: nodeId=" << m_nodeId
                               << m_informationAvailability << "->" << informationAvailability;
        m_informationAvailability = informationAvailability;
        emit nodeChanged();
    }
}

void IntegrationNode::setDescription(
    const QString &description)
{
    if (m_description != description)
    {
        qCDebug(lcClientTruck) << "IntegrationNode::setDescription: nodeId=" << m_nodeId
                               << "description=" << description;
        m_description = description;
        emit nodeChanged();
    }
}

void IntegrationNode::setXScale(float xScale)
{
    if (m_xScale != xScale)
    {
        qCDebug(lcClientTruck) << "IntegrationNode::setXScale: nodeId=" << m_nodeId
                               << m_xScale << "->" << xScale;
        m_xScale = xScale;
        emit nodeChanged();
    }
}

void IntegrationNode::setYScale(float yScale)
{
    if (m_yScale != yScale)
    {
        qCDebug(lcClientTruck) << "IntegrationNode::setYScale: nodeId=" << m_nodeId
                               << m_yScale << "->" << yScale;
        m_yScale = yScale;
        emit nodeChanged();
    }
}

} // namespace TruckClient
} // namespace Backend
} // namespace CargoNetSim
