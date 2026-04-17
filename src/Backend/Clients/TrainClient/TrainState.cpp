#include "TrainState.h"
#include <QDebug>

#include "Backend/Commons/LogCategories.h"

namespace CargoNetSim
{
namespace Backend
{
namespace TrainClient
{

TrainState::TrainState(const QJsonObject &trainData)
    : m_cumEnergyStat(
          trainData.value("cumEnergyStat").toDouble(0.0))
    , m_cumulativeDelayTime(
          trainData.value("cumulativeDelayTimeStat")
              .toDouble(0.0))
    , m_cumulativeMaxDelayTime(
          trainData.value("cumulativeMaxDelayTimeStat")
              .toDouble(0.0))
    , m_cumulativeStoppedStat(
          trainData.value("cumulativeStoppedStat")
              .toDouble(0.0))
    , m_tripTime(trainData.value("tripTime").toDouble(0.0))
    , m_currentAcceleration(
          trainData.value("currentAcceleration")
              .toDouble(0.0))
    , m_currentResistanceForces(
          trainData.value("currentResistanceForces")
              .toDouble(0.0))
    , m_currentSpeed(
          trainData.value("currentSpeed").toDouble(0.0))
    , m_currentTractiveForce(
          trainData.value("currentTractiveForce")
              .toDouble(0.0))
    , m_currentUsedTractivePower(
          trainData.value("currentUsedTractivePower")
              .toDouble(0.0))
    , m_isLoaded(trainData.value("isLoaded").toBool(false))
    , m_isOn(trainData.value("isOn").toBool(false))
    , m_outOfEnergy(
          trainData.value("outOfEnergy").toBool(false))
    , m_reachedDestination(
          trainData.value("reachedDestination")
              .toBool(false))
    , m_totalEnergyConsumed(
          trainData.value("totalEnergyConsumed")
              .toDouble(0.0))
    , m_totalEnergyRegenerated(
          trainData.value("totalEnergyRegenerated")
              .toDouble(0.0))
    , m_totalCarbonDioxideEmitted(
          trainData.value("totalCarbonDioxideEmitted")
              .toDouble(0.0))
    , m_totalLength(trainData.value("totalLength").toInt(0))
    , m_totalMass(
          trainData.value("totalMass").toDouble(0.0))
    , m_trainUserId(trainData.value("trainUserID")
                        .toString("Unknown"))
    , m_travelledDistance(
          trainData.value("travelledDistance")
              .toDouble(0.0))
    , m_containersCount(
          trainData.value("containersCount").toInt(0))
{
    qCDebug(lcClientTrain)
        << "TrainState::TrainState:"
        << "trainId=" << m_trainUserId
        << "speed=" << m_currentSpeed
        << "loaded=" << m_isLoaded;

    // Extract fuel consumption data from JSON
    QJsonObject fuelObj =
        trainData.value("totalFuelConsumed").toObject();

    // Iterate over fuel types and their values
    for (auto it = fuelObj.begin(); it != fuelObj.end();
         ++it)
    {
        // Store each fuel type and its consumption
        m_totalFuelConsumed[it.key()] =
            it.value().toDouble(0.0);
    }

    // Populate the metrics map for dynamic access
    m_metrics["totalFuelConsumed"] =
        QVariant::fromValue(m_totalFuelConsumed);
    m_metrics["cumEnergyStat"] = m_cumEnergyStat;
    m_metrics["cumulativeDelayTime"] =
        m_cumulativeDelayTime;
    m_metrics["cumulativeMaxDelayTime"] =
        m_cumulativeMaxDelayTime;
    m_metrics["cumulativeStoppedStat"] =
        m_cumulativeStoppedStat;
    m_metrics["tripTime"] = m_tripTime;
    m_metrics["currentAcceleration"] =
        m_currentAcceleration;
    m_metrics["currentResistanceForces"] =
        m_currentResistanceForces;
    m_metrics["currentSpeed"] = m_currentSpeed;
    m_metrics["currentTractiveForce"] =
        m_currentTractiveForce;
    m_metrics["currentUsedTractivePower"] =
        m_currentUsedTractivePower;
    m_metrics["isLoaded"]           = m_isLoaded;
    m_metrics["isOn"]               = m_isOn;
    m_metrics["outOfEnergy"]        = m_outOfEnergy;
    m_metrics["reachedDestination"] = m_reachedDestination;
    m_metrics["totalEnergyConsumed"] =
        m_totalEnergyConsumed;
    m_metrics["totalEnergyRegenerated"] =
        m_totalEnergyRegenerated;
    m_metrics["totalCarbonDioxideEmitted"] =
        m_totalCarbonDioxideEmitted;
    m_metrics["totalLength"]       = m_totalLength;
    m_metrics["totalMass"]         = m_totalMass;
    m_metrics["trainUserId"]       = m_trainUserId;
    m_metrics["travelledDistance"] = m_travelledDistance;
    m_metrics["containersCount"]   = m_containersCount;
}

QVariant
TrainState::getMetric(const QString &metricName) const
{
    qCDebug(lcClientTrain)
        << "TrainState::getMetric:"
        << "trainId=" << m_trainUserId
        << "metric=" << metricName;

    // Return the value of the requested metric
    if (!m_metrics.contains(metricName))
    {
        qCWarning(lcClientTrain) << "Unknown metric requested:"
                   << metricName;
        return QVariant();
    }
    return m_metrics.value(metricName);
}

QJsonObject TrainState::toJson() const
{
    qCDebug(lcClientTrain)
        << "TrainState::toJson:"
        << "trainId=" << m_trainUserId
        << "speed=" << m_currentSpeed
        << "distance=" << m_travelledDistance
        << "reachedDest=" << m_reachedDestination;

    // Create a JSON object to hold the train state
    QJsonObject obj;

    // Convert fuel map to JSON object
    QJsonObject fuelObj;
    for (auto it = m_totalFuelConsumed.constBegin();
         it != m_totalFuelConsumed.constEnd(); ++it)
    {
        fuelObj[it.key()] = it.value();
    }
    obj["totalFuelConsumed"] = fuelObj;

    // Add scalar metrics to JSON
    obj["cumEnergyStat"]           = m_cumEnergyStat;
    obj["cumulativeDelayTimeStat"] = m_cumulativeDelayTime;
    obj["cumulativeMaxDelayTimeStat"] =
        m_cumulativeMaxDelayTime;
    obj["cumulativeStoppedStat"] = m_cumulativeStoppedStat;
    obj["tripTime"]              = m_tripTime;
    obj["currentAcceleration"]   = m_currentAcceleration;
    obj["currentResistanceForces"] =
        m_currentResistanceForces;
    obj["currentSpeed"]         = m_currentSpeed;
    obj["currentTractiveForce"] = m_currentTractiveForce;
    obj["currentUsedTractivePower"] =
        m_currentUsedTractivePower;

    // Add boolean states to JSON
    obj["isLoaded"]           = m_isLoaded;
    obj["isOn"]               = m_isOn;
    obj["outOfEnergy"]        = m_outOfEnergy;
    obj["reachedDestination"] = m_reachedDestination;

    // Add remaining scalar metrics
    obj["totalEnergyConsumed"] = m_totalEnergyConsumed;
    obj["totalEnergyRegenerated"] =
        m_totalEnergyRegenerated;
    obj["totalCarbonDioxideEmitted"] =
        m_totalCarbonDioxideEmitted;
    obj["totalLength"]       = m_totalLength;
    obj["totalMass"]         = m_totalMass;
    obj["trainUserID"]       = m_trainUserId;
    obj["travelledDistance"] = m_travelledDistance;
    obj["containersCount"]   = m_containersCount;

    // Return the completed JSON object
    return obj;
}

// Implement all getters
QMap<QString, double>
TrainState::getTotalFuelConsumed() const
{
    return m_totalFuelConsumed;
}
double TrainState::getCumEnergyStat() const
{
    return m_cumEnergyStat;
}
double TrainState::getCumulativeDelayTime() const
{
    return m_cumulativeDelayTime;
}
double TrainState::getCumulativeMaxDelayTime() const
{
    return m_cumulativeMaxDelayTime;
}
double TrainState::getCumulativeStoppedStat() const
{
    return m_cumulativeStoppedStat;
}
double TrainState::getTripTime() const
{
    return m_tripTime;
}
double TrainState::getCurrentAcceleration() const
{
    return m_currentAcceleration;
}
double TrainState::getCurrentResistanceForces() const
{
    return m_currentResistanceForces;
}
double TrainState::getCurrentSpeed() const
{
    return m_currentSpeed;
}
double TrainState::getCurrentTractiveForce() const
{
    return m_currentTractiveForce;
}
double TrainState::getCurrentUsedTractivePower() const
{
    return m_currentUsedTractivePower;
}
bool TrainState::getIsLoaded() const
{
    return m_isLoaded;
}
bool TrainState::getIsOn() const
{
    return m_isOn;
}
bool TrainState::getIsOutOfEnergy() const
{
    return m_outOfEnergy;
}
bool TrainState::getReachedDestination() const
{
    return m_reachedDestination;
}
double TrainState::getTotalEnergyConsumed() const
{
    return m_totalEnergyConsumed;
}
double TrainState::getTotalEnergyRegenerated() const
{
    return m_totalEnergyRegenerated;
}
double TrainState::getTotalCarbonDioxideEmitted() const
{
    return m_totalCarbonDioxideEmitted;
}
int TrainState::getTotalLength() const
{
    return m_totalLength;
}
double TrainState::getTotalMass() const
{
    return m_totalMass;
}
QString TrainState::getTrainUserId() const
{
    return m_trainUserId;
}
double TrainState::getTravelledDistance() const
{
    return m_travelledDistance;
}
int TrainState::getContainersCount() const
{
    return m_containersCount;
}

} // namespace TrainClient
} // namespace Backend
} // namespace CargoNetSim
