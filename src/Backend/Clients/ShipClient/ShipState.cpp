#include "ShipState.h"
#include <QDebug>
#include "Backend/Commons/LogCategories.h"

namespace CargoNetSim
{
namespace Backend
{
namespace ShipClient
{

ShipState::ShipState(const QJsonObject &shipData)
    : m_shipId(shipData.value("shipID").toString("Unknown"))
    // Note: logging after member init list, see constructor body
    , m_travelledDistance(
          shipData.value("travelledDistance").toDouble(0.0))
    , m_currentAcceleration(
          shipData.value("currentAcceleration")
              .toDouble(0.0))
    , m_previousAcceleration(
          shipData.value("previousAcceleration")
              .toDouble(0.0))
    , m_currentSpeed(
          shipData.value("currentSpeed").toDouble(0.0))
    , m_previousSpeed(
          shipData.value("previousSpeed").toDouble(0.0))
    , m_totalThrust(
          shipData.value("totalThrust").toDouble(0.0))
    , m_totalResistance(
          shipData.value("totalResistance").toDouble(0.0))
    , m_vesselWeight(
          shipData.value("vesselWeight").toDouble(0.0))
    , m_cargoWeight(
          shipData.value("cargoWeight").toDouble(0.0))
    , m_isOn(shipData.value("isOn").toBool(false))
    , m_outOfEnergy(
          shipData.value("outOfEnergy").toBool(false))
    , m_loaded(shipData.value("loaded").toBool(false))
    , m_reachedDestination(
          shipData.value("reachedDestination")
              .toBool(false))
    , m_tripTime(shipData.value("tripTime").toDouble(0.0))
    , m_containersCount(
          shipData.value("containersCount").toInt(0))
    , m_closestPort(
          shipData.value("closestPort").toString("Unknown"))
    , m_energyConsumption(0.0)
    , m_carbonDioxideEmitted(0.0)
    , m_latitude(0.0)
    , m_longitude(0.0)
    , m_waterDepth(0.0)
    , m_salinity(0.0)
    , m_temperature(0.0)
    , m_waveHeight(0.0)
    , m_waveLength(0.0)
    , m_waveAngularFrequency(0.0)
{
    qCDebug(lcClientShip) << "ShipState::ShipState:"
                          << "id=" << m_shipId
                          << "speed=" << m_currentSpeed
                          << "containers=" << m_containersCount;

    // Parse consumption data
    QJsonObject consumption =
        shipData.value("consumption").toObject();
    if (!consumption.isEmpty())
    {
        m_energyConsumption =
            consumption.value("energyConsumption")
                .toDouble(0.0);
        m_carbonDioxideEmitted =
            consumption.value("carbonDioxideEmitted")
                .toDouble(0.0);

        // Parse fuel consumption
        QJsonArray fuelConsumption =
            consumption.value("fuelConsumption").toArray();
        for (const QJsonValue &fuelEntry : fuelConsumption)
        {
            QJsonObject fuelObj  = fuelEntry.toObject();
            QString     fuelType = fuelObj.value("fuelType")
                                   .toString("Unknown");
            double consumedVolume =
                fuelObj.value("consumedVolumeLiters")
                    .toDouble(0.0);
            m_fuelConsumption[fuelType] = consumedVolume;
        }
    }

    // Parse energy sources
    QJsonArray energySources =
        shipData.value("energySources").toArray();
    for (const QJsonValue &sourceValue : energySources)
    {
        QJsonObject source = sourceValue.toObject();
        QVariantMap sourceMap;
        sourceMap["capacity"] =
            source.value("capacity").toDouble(0.0);
        sourceMap["fuelType"] =
            source.value("fuelType").toString("Unknown");
        sourceMap["energyConsumed"] =
            source.value("energyConsumed").toDouble(0.0);
        sourceMap["weight"] =
            source.value("weight").toDouble(0.0);
        m_energySources.append(sourceMap);
    }

    // Parse position data
    QJsonObject position =
        shipData.value("position").toObject();
    if (!position.isEmpty())
    {
        m_latitude =
            position.value("latitude").toDouble(0.0);
        m_longitude =
            position.value("longitude").toDouble(0.0);

        QJsonArray posArray =
            position.value("position").toArray();
        for (const QJsonValue &pos : posArray)
        {
            m_position.append(pos.toDouble(0.0));
        }
    }

    // Parse environmental data
    QJsonObject env =
        shipData.value("environment").toObject();
    if (!env.isEmpty())
    {
        m_waterDepth =
            env.value("waterDepth").toDouble(0.0);
        m_salinity = env.value("salinity").toDouble(0.0);
        m_temperature =
            env.value("temperature").toDouble(0.0);
        m_waveHeight =
            env.value("waveHeight").toDouble(0.0);
        m_waveLength =
            env.value("waveLength").toDouble(0.0);
        m_waveAngularFrequency =
            env.value("waveAngularFrequency").toDouble(0.0);
    }
}

QVariant
ShipState::getMetric(const QString &metricName) const
{
    qCDebug(lcClientShip) << "getMetric: metric=" << metricName
                          << "shipId=" << m_shipId;
    if (metricName == "shipId")
        return m_shipId;
    if (metricName == "travelledDistance")
        return m_travelledDistance;
    if (metricName == "currentAcceleration")
        return m_currentAcceleration;
    if (metricName == "previousAcceleration")
        return m_previousAcceleration;
    if (metricName == "currentSpeed")
        return m_currentSpeed;
    if (metricName == "previousSpeed")
        return m_previousSpeed;
    if (metricName == "totalThrust")
        return m_totalThrust;
    if (metricName == "totalResistance")
        return m_totalResistance;
    if (metricName == "vesselWeight")
        return m_vesselWeight;
    if (metricName == "cargoWeight")
        return m_cargoWeight;
    if (metricName == "isOn")
        return m_isOn;
    if (metricName == "outOfEnergy")
        return m_outOfEnergy;
    if (metricName == "loaded")
        return m_loaded;
    if (metricName == "reachedDestination")
        return m_reachedDestination;
    if (metricName == "tripTime")
        return m_tripTime;
    if (metricName == "containersCount")
        return m_containersCount;
    if (metricName == "closestPort")
        return m_closestPort;
    if (metricName == "energyConsumption")
        return m_energyConsumption;
    if (metricName == "carbonDioxideEmitted")
        return m_carbonDioxideEmitted;
    if (metricName == "latitude")
        return m_latitude;
    if (metricName == "longitude")
        return m_longitude;
    if (metricName == "waterDepth")
        return m_waterDepth;
    if (metricName == "salinity")
        return m_salinity;
    if (metricName == "temperature")
        return m_temperature;
    if (metricName == "waveHeight")
        return m_waveHeight;
    if (metricName == "waveLength")
        return m_waveLength;
    if (metricName == "waveAngularFrequency")
        return m_waveAngularFrequency;

    // For complex types, we could add special handling here

    // If we don't have a direct match
    qCWarning(lcClientShip) << "Unknown metric requested:" << metricName;
    return QVariant();
}

QVariantMap ShipState::info() const
{
    qCDebug(lcClientShip) << "info: shipId=" << m_shipId;
    QVariantMap info;
    info["shipId"]               = m_shipId;
    info["travelledDistance"]    = m_travelledDistance;
    info["currentAcceleration"]  = m_currentAcceleration;
    info["previousAcceleration"] = m_previousAcceleration;
    info["currentSpeed"]         = m_currentSpeed;
    info["previousSpeed"]        = m_previousSpeed;
    info["totalThrust"]          = m_totalThrust;
    info["totalResistance"]      = m_totalResistance;
    info["vesselWeight"]         = m_vesselWeight;
    info["cargoWeight"]          = m_cargoWeight;
    info["isOn"]                 = m_isOn;
    info["outOfEnergy"]          = m_outOfEnergy;
    info["loaded"]               = m_loaded;
    info["reachedDestination"]   = m_reachedDestination;
    info["tripTime"]             = m_tripTime;
    info["containersCount"]      = m_containersCount;
    info["closestPort"]          = m_closestPort;
    info["energyConsumption"]    = m_energyConsumption;
    info["carbonDioxideEmitted"] = m_carbonDioxideEmitted;

    // Add fuel consumption
    QVariantMap fuelMap;
    for (auto it = m_fuelConsumption.constBegin();
         it != m_fuelConsumption.constEnd(); ++it)
    {
        fuelMap[it.key()] = it.value();
    }
    info["fuelConsumption"] = fuelMap;

    info["energySources"] =
        QVariant::fromValue(m_energySources);

    // Position information
    QVariantMap positionMap;
    positionMap["latitude"]  = m_latitude;
    positionMap["longitude"] = m_longitude;
    positionMap["position"] =
        QVariant::fromValue(m_position);
    info["position"] = positionMap;

    // Environmental information
    QVariantMap envMap;
    envMap["waterDepth"]           = m_waterDepth;
    envMap["salinity"]             = m_salinity;
    envMap["temperature"]          = m_temperature;
    envMap["waveHeight"]           = m_waveHeight;
    envMap["waveLength"]           = m_waveLength;
    envMap["waveAngularFrequency"] = m_waveAngularFrequency;
    info["environment"]            = envMap;

    return info;
}

QJsonObject ShipState::toJson() const
{
    qCDebug(lcClientShip) << "ShipState::toJson:" << m_shipId;
    QJsonObject json;
    json["shipID"]               = m_shipId;
    json["travelledDistance"]    = m_travelledDistance;
    json["currentAcceleration"]  = m_currentAcceleration;
    json["previousAcceleration"] = m_previousAcceleration;
    json["currentSpeed"]         = m_currentSpeed;
    json["previousSpeed"]        = m_previousSpeed;
    json["totalThrust"]          = m_totalThrust;
    json["totalResistance"]      = m_totalResistance;
    json["vesselWeight"]         = m_vesselWeight;
    json["cargoWeight"]          = m_cargoWeight;
    json["isOn"]                 = m_isOn;
    json["outOfEnergy"]          = m_outOfEnergy;
    json["loaded"]               = m_loaded;
    json["reachedDestination"]   = m_reachedDestination;
    json["tripTime"]             = m_tripTime;
    json["containersCount"]      = m_containersCount;
    json["closestPort"]          = m_closestPort;

    // Consumption
    QJsonObject consumption;
    consumption["energyConsumption"] = m_energyConsumption;
    consumption["carbonDioxideEmitted"] =
        m_carbonDioxideEmitted;

    QJsonArray fuelConsumption;
    for (auto it = m_fuelConsumption.constBegin();
         it != m_fuelConsumption.constEnd(); ++it)
    {
        QJsonObject fuelObj;
        fuelObj["fuelType"]             = it.key();
        fuelObj["consumedVolumeLiters"] = it.value();
        fuelConsumption.append(fuelObj);
    }
    consumption["fuelConsumption"] = fuelConsumption;
    json["consumption"]            = consumption;

    // Energy sources
    QJsonArray energySources;
    for (const QVariantMap &source : m_energySources)
    {
        QJsonObject sourceObj;
        sourceObj["capacity"] =
            source["capacity"].toDouble();
        sourceObj["fuelType"] =
            source["fuelType"].toString();
        sourceObj["energyConsumed"] =
            source["energyConsumed"].toDouble();
        sourceObj["weight"] = source["weight"].toDouble();
        energySources.append(sourceObj);
    }
    json["energySources"] = energySources;

    // Position
    QJsonObject position;
    position["latitude"]  = m_latitude;
    position["longitude"] = m_longitude;

    QJsonArray positionArray;
    for (double pos : m_position)
    {
        positionArray.append(pos);
    }
    position["position"] = positionArray;
    json["position"]     = position;

    // Environment
    QJsonObject environment;
    environment["waterDepth"]  = m_waterDepth;
    environment["salinity"]    = m_salinity;
    environment["temperature"] = m_temperature;
    environment["waveHeight"]  = m_waveHeight;
    environment["waveLength"]  = m_waveLength;
    environment["waveAngularFrequency"] =
        m_waveAngularFrequency;
    json["environment"] = environment;

    return json;
}

// Implementation of all getter methods
QString ShipState::getShipId() const
{
    return m_shipId;
}
double ShipState::getTravelledDistance() const
{
    return m_travelledDistance;
}
double ShipState::getCurrentAcceleration() const
{
    return m_currentAcceleration;
}
double ShipState::getPreviousAcceleration() const
{
    return m_previousAcceleration;
}
double ShipState::getCurrentSpeed() const
{
    return m_currentSpeed;
}
double ShipState::getPreviousSpeed() const
{
    return m_previousSpeed;
}
double ShipState::getTotalThrust() const
{
    return m_totalThrust;
}
double ShipState::getTotalResistance() const
{
    return m_totalResistance;
}
double ShipState::getVesselWeight() const
{
    return m_vesselWeight;
}
double ShipState::getCargoWeight() const
{
    return m_cargoWeight;
}
bool ShipState::getIsOn() const
{
    return m_isOn;
}
bool ShipState::getIsOutOfEnergy() const
{
    return m_outOfEnergy;
}
bool ShipState::getIsLoaded() const
{
    return m_loaded;
}
bool ShipState::getReachedDestination() const
{
    return m_reachedDestination;
}
double ShipState::getTripTime() const
{
    return m_tripTime;
}
int ShipState::getContainersCount() const
{
    return m_containersCount;
}
QString ShipState::getClosestPort() const
{
    return m_closestPort;
}
double ShipState::getEnergyConsumption() const
{
    return m_energyConsumption;
}
const QMap<QString, double> &
ShipState::getFuelConsumption() const
{
    return m_fuelConsumption;
}
double ShipState::getCarbonEmissions() const
{
    return m_carbonDioxideEmitted;
}
const QList<QVariantMap> &
ShipState::getEnergySources() const
{
    return m_energySources;
}
double ShipState::getLatitude() const
{
    return m_latitude;
}
double ShipState::getLongitude() const
{
    return m_longitude;
}
const QList<double> &ShipState::getPosition() const
{
    return m_position;
}
double ShipState::getWaterDepth() const
{
    return m_waterDepth;
}
double ShipState::getSalinity() const
{
    return m_salinity;
}
double ShipState::getTemperature() const
{
    return m_temperature;
}
double ShipState::getWaveHeight() const
{
    return m_waveHeight;
}
double ShipState::getWaveLength() const
{
    return m_waveLength;
}
double ShipState::getWaveAngularFrequency() const
{
    return m_waveAngularFrequency;
}

} // namespace ShipClient
} // namespace Backend
} // namespace CargoNetSim
