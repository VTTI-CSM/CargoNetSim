#pragma once
/**
 * @file PathSegment.h
 * @brief Defines the PathSegment class for path segments
 * @author Ahmed Aredah
 * @date March 21, 2025
 *
 * This file declares the PathSegment class, representing a
 * segment of a transportation path between two terminals in
 * the CargoNetSim simulation framework.
 *
 * @note Part of the CargoNetSim::Backend namespace.
 * @warning Instances should be managed by Path or caller.
 */
#include "Backend/Commons/Units.h"
#include "Backend/Commons/TransportationMode.h"
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVariantMap>
namespace CargoNetSim
{
namespace Backend
{
/**
 * @class PathSegment
 * @brief Represents a single segment in a transportation
 * path
 *
 * This class encapsulates details of a route between two
 * terminals, including its identifier, endpoints, mode, and
 * attributes, used for defining static routes or dynamic
 * path segments in the simulation.
 *
 * @note Inherits QObject for signal-slot functionality.
 */
class PathSegment : public QObject
{
    Q_OBJECT
public:
    struct SegmentMetricSnapshot
    {
        bool   available = false;
        double travelTime = 0.0;
        double distance = 0.0;
        double carbonEmissions = 0.0;
        double energyConsumption = 0.0;
        double risk = 0.0;

        Units::TimeSeconds travelTimeUnits() const
        {
            return Units::seconds(travelTime);
        }

        Units::LengthMeters distanceUnits() const
        {
            return Units::meters(distance);
        }

        Units::MassMetricTons carbonEmissionsUnits() const
        {
            return Units::metricTons(carbonEmissions);
        }

        Units::EnergyKilowattHours energyConsumptionUnits() const
        {
            return Units::kilowattHours(energyConsumption);
        }

        Units::Scalar riskUnits() const
        {
            return Units::scalar(risk);
        }
    };

    struct SegmentCostSnapshot
    {
        bool   available = false;
        double travelTime = 0.0;
        double distance = 0.0;
        double carbonEmissions = 0.0;
        double energyConsumption = 0.0;
        double risk = 0.0;
        double directCost = 0.0;

        double total() const
        {
            return travelTime + distance + carbonEmissions
                   + energyConsumption + risk + directCost;
        }
    };

    /**
     * @brief Constructs a PathSegment instance
     * @param pathSegmentId Unique identifier for the
     * segment
     * @param start Starting terminal identifier
     * @param end Ending terminal identifier
     * @param mode Transportation mode as integer
     * @param attributes Optional attributes as JSON object
     * @param parent Parent QObject, defaults to nullptr
     * @throws std::invalid_argument If required fields are
     * empty
     *
     * Creates a PathSegment with specified properties.
     */
    explicit PathSegment(
        const QString &pathSegmentId, const QString &start,
        const QString                          &end,
        TransportationTypes::TransportationMode mode,
        const QJsonObject &attributes = QJsonObject(),
        QObject           *parent     = nullptr);

    /**
     * @brief Constructs a PathSegment from a JSON object
     * @param json JSON object containing segment data
     * @param parent Parent QObject, defaults to nullptr
     * @throws std::invalid_argument If required fields are
     * missing
     *
     * Creates a PathSegment by parsing a JSON object from
     * the server.
     */
    explicit PathSegment(const QJsonObject &json,
                         QObject *parent = nullptr);

    /**
     * @brief Creates a PathSegment from a JSON object
     * @param json JSON object containing segment data
     * @param parent Parent QObject, defaults to nullptr
     * @return New PathSegment instance
     * @throws std::invalid_argument If required fields are
     * missing
     *
     * Static factory method to create a PathSegment from
     * JSON.
     */
    static PathSegment *fromJson(const QJsonObject &json,
                                 QObject *parent = nullptr);

    /**
     * @brief Creates a deep copy of the segment.
     * @param parent Parent QObject for the clone
     * @return Newly allocated copy owned by the caller
     */
    PathSegment *clone(QObject *parent = nullptr) const;

    /**
     * @brief Retrieves the path segment identifier
     * @return Unique identifier as QString
     *
     * Returns the unique ID assigned to this segment.
     */
    QString getPathSegmentId() const
    {
        return m_pathSegmentId;
    }
    /**
     * @brief Retrieves the starting terminal
     * @return Starting terminal identifier as QString
     *
     * Returns the ID of the segment's starting terminal.
     */
    QString getStart() const
    {
        return m_start;
    }
    /**
     * @brief Retrieves the ending terminal
     * @return Ending terminal identifier as QString
     *
     * Returns the ID of the segment's ending terminal.
     */
    QString getEnd() const
    {
        return m_end;
    }
    /**
     * @brief Retrieves the transportation mode
     * @return Mode as integer (enum value)
     *
     * Returns the transportation mode used in this segment.
     */
    TransportationTypes::TransportationMode getMode() const
    {
        return m_mode;
    }
    /**
     * @brief Retrieves the segment attributes
     * @return Attributes as QJsonObject
     *
     * Returns additional properties of the segment.
     */
    QJsonObject getAttributes() const
    {
        return m_attributes;
    }

    int sequenceIndex() const
    {
        return m_sequenceIndex;
    }

    double rankingCostContribution() const
    {
        return m_rankingCostContribution;
    }

    double weightedEdgeCost() const
    {
        return m_weightedEdgeCost;
    }

    double weightedTerminalCostEmbeddedInSegment() const
    {
        return m_weightedTerminalCostEmbeddedInSegment;
    }

    /**
     * @brief Sets the segment attributes
     * @param attributes New attributes as QJsonObject
     *
     * Updates the segment's properties.
     */
    void setAttributes(const QJsonObject &attributes);

    // ---- Typed accessors over estimated sub-objects inside `attributes`.
    //      Key names match what the path preparation pipeline writes:
    //        travelTime, distance, carbonEmissions, energyConsumption,
    //        risk, cost.
    //      `fuel` is NOT tracked per segment; callers that need a fuel
    //      display derive it from the calculator's per-vehicle formula.

    // Pre-simulation (populator writes these — only distance + time
    // are produced by per-mode shortest-path routing).
    double estimatedDistance()   const;  // metres
    double estimatedTravelTime() const;  // seconds
    Units::LengthMeters estimatedDistanceUnits() const
    {
        return Units::meters(estimatedDistance());
    }
    Units::TimeSeconds estimatedTravelTimeUnits() const
    {
        return Units::seconds(estimatedTravelTime());
    }
    SegmentMetricSnapshot estimatedValues() const;

    SegmentCostSnapshot estimatedCosts() const;

    /// Populator-side setter. Replaces or creates the `estimated`
    /// sub-object; leaves any pre-existing non-estimated keys
    /// (`weight`, etc.) untouched.
    void   setEstimatedDistanceAndTravelTime(double distanceMeters,
                                             double travelTimeSeconds);
    void setEstimatedDistanceAndTravelTime(
        Units::LengthMeters distance,
        Units::TimeSeconds  travelTime)
    {
        setEstimatedDistanceAndTravelTime(distance.value(),
                                          travelTime.value());
    }

    /// Physics-side setter. Merges energyKWh, carbonTonnes, and risk into
    /// the existing "estimated" sub-object without disturbing distance or
    /// travelTime written by setEstimatedDistanceAndTravelTime().
    /// Safe to call before or after the geometry setter.
    void setEstimatedPhysicalMetrics(double energyKWh,
                                      double carbonTonnes,
                                      double risk);
    void setEstimatedPhysicalMetrics(
        Units::EnergyKilowattHours energy,
        Units::MassMetricTons      carbon,
        Units::Scalar              riskValue)
    {
        setEstimatedPhysicalMetrics(energy.value(),
                                    carbon.value(),
                                    riskValue.value());
    }

    double estimatedEnergyConsumption() const;  // kWh
    double estimatedCarbonEmissions()   const;  // tonnes CO₂
    double estimatedRisk()              const;  // dimensionless
    Units::EnergyKilowattHours estimatedEnergyConsumptionUnits() const
    {
        return Units::kilowattHours(
            estimatedEnergyConsumption());
    }
    Units::MassMetricTons estimatedCarbonEmissionsUnits() const
    {
        return Units::metricTons(
            estimatedCarbonEmissions());
    }
    Units::Scalar estimatedRiskUnits() const
    {
        return Units::scalar(estimatedRisk());
    }

    /**
     * @brief Converts the segment to JSON format
     * @return QJsonObject representing the segment
     *
     * Serializes the segment for server communication.
     */
    QJsonObject toJson() const;

private:
    /**
     * @brief Unique identifier for the path segment
     */
    QString m_pathSegmentId;
    /**
     * @brief Identifier of the starting terminal
     */
    QString m_start;
    /**
     * @brief Identifier of the ending terminal
     */
    QString m_end;
    /**
     * @brief Transportation mode for the segment
     */
    TransportationTypes::TransportationMode m_mode;
    int                                     m_sequenceIndex = 0;
    double                                  m_rankingCostContribution = 0.0;
    double                                  m_weightedEdgeCost = 0.0;
    double m_weightedTerminalCostEmbeddedInSegment = 0.0;
    /**
     * @brief Additional attributes of the segment
     */
    QJsonObject m_attributes;
};
} // namespace Backend
} // namespace CargoNetSim
