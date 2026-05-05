#pragma once

#include "Backend/Commons/Units.h"

#include <QByteArray>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QThread>
#include <QVariant>
#include <QWaitCondition>

namespace CargoNetSim
{
namespace Backend
{
namespace ShipClient
{

/**
 * @brief Represents the state of a ship in the simulation
 */
class ShipState
{
public:
    /**
     * @brief Constructor from JSON data
     * @param shipData Ship state data as JSON
     */
    explicit ShipState(const QJsonObject &shipData);

    /**
     * @brief Get value of a specific metric
     * @param metricName Name of the metric
     * @return Value as QVariant, invalid if not found
     */
    QVariant getMetric(const QString &metricName) const;

    /**
     * @brief Get all metrics as a map
     * @return Map of all metrics
     */
    QVariantMap info() const;

    /**
     * @brief Convert to JSON
     * @return JSON representation of ship state
     */
    QJsonObject toJson() const;

    // Accessor methods - getters for all variables
    QString getShipId() const;
    double  getTravelledDistance() const;
    double  getCurrentAcceleration() const;
    double  getPreviousAcceleration() const;
    double  getCurrentSpeed() const;
    double  getPreviousSpeed() const;
    double  getTotalThrust() const;
    double  getTotalResistance() const;
    double  getVesselWeight() const;
    double  getCargoWeight() const;
    bool    getIsOn() const;
    bool    getIsOutOfEnergy() const;
    bool    getIsLoaded() const;
    bool    getReachedDestination() const;
    double  getTripTime() const;
    int     getContainersCount() const;
    QString getClosestPort() const;
    double  getEnergyConsumption() const;
    const QMap<QString, double> &getFuelConsumption() const;
    double                       getCarbonEmissions() const;
    const QList<QVariantMap>    &getEnergySources() const;
    double                       getLatitude() const;
    double                       getLongitude() const;
    const QList<double>         &getPosition() const;
    double                       getWaterDepth() const;
    double                       getSalinity() const;
    double                       getTemperature() const;
    double                       getWaveHeight() const;
    double                       getWaveLength() const;
    double getWaveAngularFrequency() const;

    Units::LengthMeters travelledDistanceUnits() const
    {
        return Units::meters(getTravelledDistance());
    }

    Units::TimeSeconds tripTimeUnits() const
    {
        return Units::seconds(getTripTime());
    }

    Units::EnergyKilowattHours energyConsumptionUnits() const
    {
        return Units::kilowattHours(getEnergyConsumption());
    }

    Units::MassKilograms carbonEmissionsUnits() const
    {
        return Units::kilograms(getCarbonEmissions());
    }

private:
    QString m_shipId;
    double  m_travelledDistance;
    double  m_currentAcceleration;
    double  m_previousAcceleration;
    double  m_currentSpeed;
    double  m_previousSpeed;
    double  m_totalThrust;
    double  m_totalResistance;
    double  m_vesselWeight;
    double  m_cargoWeight;
    bool    m_isOn;
    bool    m_outOfEnergy;
    bool    m_loaded;
    bool    m_reachedDestination;
    double  m_tripTime;
    int     m_containersCount;
    QString m_closestPort;

    // Energy and fuel consumption
    double                m_energyConsumption;
    QMap<QString, double> m_fuelConsumption;
    double                m_carbonDioxideEmitted;

    // Energy sources
    QList<QVariantMap> m_energySources;

    // Position
    double        m_latitude;
    double        m_longitude;
    QList<double> m_position;

    // Environmental conditions
    double m_waterDepth;
    double m_salinity;
    double m_temperature;
    double m_waveHeight;
    double m_waveLength;
    double m_waveAngularFrequency;
};

} // namespace ShipClient
} // namespace Backend
} // namespace CargoNetSim

// Declare metatypes
Q_DECLARE_METATYPE(
    CargoNetSim::Backend::ShipClient::ShipState)
Q_DECLARE_METATYPE(
    CargoNetSim::Backend::ShipClient::ShipState *)
