#pragma once

#include "Backend/Commons/Units.h"

#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QVariant>

namespace CargoNetSim
{
namespace Backend
{
namespace TrainClient
{

/**
 * @class TrainState
 * @brief Represents the state of a train in the simulation
 *
 * Stores various train metrics and states, providing access
 * and JSON serialization methods. This class encapsulates
 * the dynamic state of a train during simulation.
 */
class TrainState
{
public:
    /**
     * @brief Constructor
     * @param trainData JSON object with train state data
     *
     * Initializes a TrainState object from a JSON object
     * containing train metrics and states.
     */
    explicit TrainState(const QJsonObject &trainData = {});

    /**
     * @brief Get a specific metric value
     * @param metricName Name of the metric to retrieve
     * @return Value of the metric, or null if not found
     *
     * Provides dynamic access to any train state metric.
     */
    QVariant getMetric(const QString &metricName) const;

    /**
     * @brief Convert state to JSON
     * @return JSON object representing the train state
     *
     * Serializes the train state into a JSON format.
     */
    QJsonObject toJson() const;

    // Getters for all member variables
    QMap<QString, double> getTotalFuelConsumed() const;
    double                getCumEnergyStat() const;
    double                getCumulativeDelayTime() const;
    double                getCumulativeMaxDelayTime() const;
    double                getCumulativeStoppedStat() const;
    double                getTripTime() const;
    double                getCurrentAcceleration() const;
    double  getCurrentResistanceForces() const;
    double  getCurrentSpeed() const;
    double  getCurrentTractiveForce() const;
    double  getCurrentUsedTractivePower() const;
    bool    getIsLoaded() const;
    bool    getIsOn() const;
    bool    getIsOutOfEnergy() const;
    bool    getReachedDestination() const;
    double  getTotalEnergyConsumed() const;
    double  getTotalEnergyRegenerated() const;
    double  getTotalCarbonDioxideEmitted() const;
    int     getTotalLength() const;
    double  getTotalMass() const;
    QString getTrainUserId() const;
    double  getTravelledDistance() const;
    int     getContainersCount() const;

    Units::TimeSeconds tripTimeUnits() const
    {
        return Units::seconds(getTripTime());
    }

    Units::LengthMeters travelledDistanceUnits() const
    {
        return Units::meters(getTravelledDistance());
    }

    Units::EnergyKilowattHours totalEnergyConsumedUnits() const
    {
        return Units::kilowattHours(getTotalEnergyConsumed());
    }

    Units::MassKilograms totalCarbonDioxideEmittedUnits() const
    {
        return Units::kilograms(getTotalCarbonDioxideEmitted());
    }

private:
    /// @brief Total fuel consumed by type
    QMap<QString, double> m_totalFuelConsumed;
    /// @brief Cumulative energy statistic
    double m_cumEnergyStat;
    /// @brief Total delay time accumulated
    double m_cumulativeDelayTime;
    /// @brief Maximum delay time accumulated
    double m_cumulativeMaxDelayTime;
    /// @brief Total stopped time statistic
    double m_cumulativeStoppedStat;
    /// @brief Total trip duration
    double m_tripTime;
    /// @brief Current acceleration of the train
    double m_currentAcceleration;
    /// @brief Current resistance forces
    double m_currentResistanceForces;
    /// @brief Current speed of the train
    double m_currentSpeed;
    /// @brief Current tractive force applied
    double m_currentTractiveForce;
    /// @brief Current tractive power used
    double m_currentUsedTractivePower;
    /// @brief Whether the train is loaded
    bool m_isLoaded;
    /// @brief Whether the train is powered on
    bool m_isOn;
    /// @brief Whether the train is out of energy
    bool m_outOfEnergy;
    /// @brief Whether the train reached its destination
    bool m_reachedDestination;
    /// @brief Total energy consumed
    double m_totalEnergyConsumed;
    /// @brief Total energy regenerated
    double m_totalEnergyRegenerated;
    /// @brief Total CO2 emissions
    double m_totalCarbonDioxideEmitted;
    /// @brief Total length of the train
    int m_totalLength;
    /// @brief Total mass of the train
    double m_totalMass;
    /// @brief User-defined train ID
    QString m_trainUserId;
    /// @brief Distance traveled by the train
    double m_travelledDistance;
    /// @brief Number of containers on the train
    int m_containersCount;

    /**
     * @brief Map of all metrics for dynamic access
     *
     * Stores all train state metrics as key-value pairs.
     */
    QMap<QString, QVariant> m_metrics;
};

} // namespace TrainClient
} // namespace Backend
} // namespace CargoNetSim

// Declare metatypes
Q_DECLARE_METATYPE(
    CargoNetSim::Backend::TrainClient::TrainState)
Q_DECLARE_METATYPE(
    CargoNetSim::Backend::TrainClient::TrainState *)
