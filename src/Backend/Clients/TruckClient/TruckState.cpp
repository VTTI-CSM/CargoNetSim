/**
 * @file TruckState.cpp
 * @brief Implements truck state
 * @author Ahmed Aredah
 * @date March 21, 2025
 */

#include "TruckState.h"
#include "Backend/Commons/LogCategories.h"

namespace CargoNetSim
{
namespace Backend
{
namespace TruckClient
{

TruckState::TruckState(const QString &networkName,
                       int tripId, const QString &originId,
                       const QString &destinationId,
                       QObject       *parent)
    : QObject(parent)
    , m_networkName(networkName)
    , m_tripId(tripId)
    , m_originId(originId)
    , m_destinationId(destinationId)
{
    qCDebug(lcClientTruck)
        << "TruckState::TruckState:"
        << "network=" << networkName
        << "tripId=" << tripId
        << "origin=" << originId
        << "dest=" << destinationId;
}

TruckState::TruckState(const QJsonObject &jsonData,
                       QObject           *parent)
    : QObject(parent)
{
    qCDebug(lcClientTruck)
        << "TruckState::TruckState:"
        << "constructing from JSON";
    updateFromJson(jsonData);
}

QVariant
TruckState::getMetric(const QString &metricName) const
{
    if (metricName == "networkName")
        return m_networkName;
    if (metricName == "tripId")
        return QString::number(m_tripId);
    if (metricName == "originId")
        return m_originId;
    if (metricName == "destinationId")
        return m_destinationId;
    if (metricName == "linkId")
        return m_linkId;
    if (metricName == "distance")
        return m_distance;
    if (metricName == "speed")
        return m_speed;
    if (metricName == "fuelConsumption")
        return m_fuelConsumption;
    if (metricName == "travelTime")
        return m_travelTime;
    if (metricName == "isCompleted")
        return m_isCompleted;
    return QVariant();
}

QVariantMap TruckState::info() const
{
    QVariantMap map;
    map["networkName"]     = m_networkName;
    map["tripId"]          = QString::number(m_tripId);
    map["originId"]        = m_originId;
    map["destinationId"]   = m_destinationId;
    map["linkId"]          = m_linkId;
    map["distance"]        = m_distance;
    map["speed"]           = m_speed;
    map["fuelConsumption"] = m_fuelConsumption;
    map["travelTime"]      = m_travelTime;
    map["isCompleted"]     = m_isCompleted;
    return map;
}

QJsonObject TruckState::toJson() const
{
    QJsonObject json;
    json["networkName"]     = m_networkName;
    json["tripId"]          = QString::number(m_tripId);
    json["originId"]        = m_originId;
    json["destinationId"]   = m_destinationId;
    json["linkId"]          = m_linkId;
    json["distance"]        = m_distance;
    json["speed"]           = m_speed;
    json["fuelConsumption"] = m_fuelConsumption;
    json["travelTime"]      = m_travelTime;
    json["isCompleted"]     = m_isCompleted;
    return json;
}

void TruckState::updateFromJson(const QJsonObject &jsonData)
{
    qCDebug(lcClientTruck)
        << "TruckState::updateFromJson:"
        << "tripId=" << jsonData["tripId"].toString()
        << "network="
        << jsonData["networkName"].toString();

    m_networkName =
        jsonData["networkName"].toString(m_networkName);
    m_tripId   = jsonData["tripId"].toString().toInt();
    m_originId = jsonData["origin"].toString(m_originId);
    m_destinationId =
        jsonData["destination"].toString(m_destinationId);
    m_distance = jsonData["distance"].toDouble(m_distance);
    m_fuelConsumption =
        jsonData["fuelConsumption"].toDouble(
            m_fuelConsumption);
    m_travelTime =
        jsonData["travelTime"].toDouble(m_travelTime);
    m_isCompleted = true;

    qCDebug(lcClientTruck)
        << "TruckState::updateFromJson:"
        << "completed, distance=" << m_distance
        << "fuel=" << m_fuelConsumption
        << "travelTime=" << m_travelTime;

    emit tripEnded();
}

void TruckState::updateInfoFromJson(
    const QJsonObject &jsonData)
{
    qCDebug(lcClientTruck)
        << "TruckState::updateInfoFromJson:"
        << "tripId=" << jsonData["tripId"].toString()
        << "linkId=" << jsonData["linkId"].toString();

    m_networkName =
        jsonData["networkName"].toString(m_networkName);
    m_tripId   = jsonData["tripId"].toString().toInt();
    m_linkId   = jsonData["linkId"].toString(m_linkId);
    m_speed    = jsonData["speed"].toDouble(m_speed);
    m_distance = jsonData["distance"].toDouble(m_distance);
    m_isCompleted = false;

    qCDebug(lcClientTruck)
        << "TruckState::updateInfoFromJson:"
        << "speed=" << m_speed
        << "distance=" << m_distance;
}

} // namespace TruckClient
} // namespace Backend
} // namespace CargoNetSim
