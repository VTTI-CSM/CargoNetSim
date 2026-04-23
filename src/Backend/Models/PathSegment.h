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

    // ---- Plan 8.2: typed accessors over the estimated/actual
    //      sub-objects inside `attributes`. Key names match what
    //      `SegmentCostMath` already writes (verified 2026-04-15):
    //        travelTime, distance, carbonEmissions, energyConsumption,
    //        risk, cost.
    //      Adopting those names avoids renaming at the write boundary
    //      and keeps one canonical key set end-to-end. `fuel` is NOT
    //      tracked per segment — SegmentCostMath folds fuel into
    //      energyConsumption; callers that need a fuel display
    //      derive it from the calculator's per-vehicle formula.

    // Pre-simulation (populator writes these — only distance + time
    // are produced by per-mode shortest-path routing).
    double estimatedDistance()   const;  // metres
    double estimatedTravelTime() const;  // seconds
    SegmentMetricSnapshot estimatedValues() const;

    // Post-simulation (SegmentCostMath writes these in km/hours/tons/kWh;
    // typed accessors below convert distance→metres and time→seconds so
    // callers see one unit convention across estimated and actual phases.
    // Dialogs that read the raw sub-object directly still see km/hours.)
    double actualDistance()          const;  // metres (stored km × 1000)
    double actualTravelTime()        const;  // seconds (stored hours × 3600)
    double actualEnergyConsumption() const;  // kWh (direct)
    double actualCarbonEmissions()   const;  // tonnes CO2 (direct)
    double actualRisk()              const;  // risk-factor units (direct)
    SegmentMetricSnapshot actualValues() const;
    SegmentCostSnapshot estimatedCosts() const;
    SegmentCostSnapshot actualCosts() const;

    /// Populator-side setter. Replaces or creates the `estimated`
    /// sub-object; leaves `actual` and any pre-existing keys
    /// (`weight`, etc.) untouched.
    void   setEstimatedDistanceAndTravelTime(double distanceMeters,
                                             double travelTimeSeconds);

    /// Physics-side setter. Merges energyKWh, carbonTonnes, and risk into
    /// the existing "estimated" sub-object without disturbing distance or
    /// travelTime written by setEstimatedDistanceAndTravelTime().
    /// Safe to call before or after the geometry setter.
    void setEstimatedPhysicalMetrics(double energyKWh,
                                      double carbonTonnes,
                                      double risk);

    double estimatedEnergyConsumption() const;  // kWh
    double estimatedCarbonEmissions()   const;  // tonnes CO₂
    double estimatedRisk()              const;  // dimensionless

    /// SegmentCostMath-side setter. Merges each key into the existing
    /// actual sub-object — keys already present are overwritten, keys
    /// NOT in @p values are left untouched (useful for phased writes).
    /// Call clearActual() before this if a full replace is desired.
    /// Canonical keys: `travelTime`, `distance`, `carbonEmissions`,
    /// `energyConsumption`, `risk`. (`cost` is aggregate-monetary and
    /// lives on PathData's totalSimulationCost fields, not on segments.)
    void   setActualValues(const QVariantMap &values);

    /// Remove the entire actual sub-object. Useful before a simulation
    /// re-run so stale keys from the prior run don't persist.
    void   clearActual();

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
