#pragma once

/**
 * @file Path.h
 * @brief Defines the Path class for complete paths
 * @author Ahmed Aredah
 * @date March 21, 2025
 *
 * This file declares the Path class, representing a
 * complete transportation path with multiple segments in
 * the CargoNetSim simulation framework.
 *
 * @note Part of the CargoNetSim::Backend namespace.
 * @warning Manages PathSegment pointers; ensure proper
 * deletion.
 */

#include "PathSegment.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QtGlobal>

#include "Terminal.h"

namespace CargoNetSim
{
namespace Backend
{

/**
 * @struct PathTerminal
 * @brief Immutable snapshot of a terminal as referenced by a
 *        Path.
 *
 * A Path's terminal list used to hold raw `Terminal *` aliases
 * into `TerminalSimulationClient::m_terminalStatus`. That cache
 * is rewritten on every `terminalAdded` / `terminalsAdded` /
 * `terminalRemoved` / `serverReset` event (old `Terminal`
 * objects get `delete`d and replaced), so every pre-existing
 * Path silently held dangling pointers.
 *
 * Storing a value snapshot here decouples Path lifetime from
 * the client's cache. Consumers in the GUI only ever need the
 * display / canonical names; anything else belongs on the
 * live Terminal object, not in a captured path result.
 */
struct PathTerminal
{
    QString id;             ///< Server-side terminal name/id.
    QString displayName;    ///< Human-readable name for UI.
    QString canonicalName;  ///< First of Terminal::getNames().
    int     sequenceIndex = 0;
    double  handlingTime = 0.0;
    double  rawCost = 0.0;
    bool    costsSkipped = false;
    double  weightedTerminalDelayContribution = 0.0;
    double  weightedTerminalCostContribution = 0.0;
    double  weightedTerminalTotalContribution = 0.0;
    QString skipReason;
};

/**
 * @class Path
 * @brief Represents a complete transportation path
 *
 * This class encapsulates a full path, including its ID,
 * costs, terminals, and segments, used for path-finding
 * results in the simulation.
 *
 * @note Owns PathSegment pointers and deletes them on
 * destruction.
 */
class Path : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief Constructs a Path instance
     * @param id Unique path identifier
     * @param totalCost Total cost of the path
     * @param edgeCost Sum of edge costs
     * @param termCost Sum of terminal costs
     * @param terminals List of terminal details
     * @param segments List of PathSegment pointers
     * @param parent Parent QObject, defaults to nullptr
     *
     * Creates a Path with specified properties and
     * segments.
     */
    explicit Path(int id, double totalCost, double edgeCost,
                  double                      termCost,
                  const QList<PathTerminal>  &terminals,
                  const QList<PathSegment *> &segments,
                  QObject *parent = nullptr);

    /**
     * @brief Constructs a Path from a JSON object
     * @param json JSON object containing path data
     * @param terminalDB Map of terminal IDs to Terminal
     * pointer
     * @param parent Parent QObject, defaults to nullptr
     * @throws std::invalid_argument If required fields are
     * missing
     *
     * Creates a Path by parsing a JSON object from the
     * server.
     */
    explicit Path(
        const QJsonObject              &json,
        const QMap<QString, Terminal *> terminalDB,
        QObject                        *parent = nullptr);

    /**
     * @brief Creates a Path from a JSON object
     * @param json JSON object containing path data
     * @param terminalDB Map of terminal IDs to Terminal
     * pointers
     * @param parent Parent QObject, defaults to nullptr
     * @return New Path instance
     * @throws std::invalid_argument If required fields are
     * missing
     *
     * Static factory method to create a Path from JSON.
     */
    static Path *
    fromJson(const QJsonObject              &json,
             const QMap<QString, Terminal *> terminalDB,
             QObject *parent = nullptr);

    /**
     * @brief Creates a deep copy of the path and its segments.
     * @param parent Parent QObject for the clone
     * @return Newly allocated copy owned by the caller
     */
    Path *clone(QObject *parent = nullptr) const;

    /**
     * @brief Destroys the Path, freeing segments
     *
     * Deletes all owned PathSegment pointers.
     */
    ~Path();

    /**
     * @brief Retrieves the path identifier
     * @return Unique path ID as integer
     *
     * Returns the ID assigned to this path.
     */
    int getPathId() const
    {
        return m_pathId;
    }

    QString getPathUid() const
    {
        return m_pathUid;
    }

    QString getOriginId() const
    {
        return m_originId;
    }

    QString getDestinationId() const
    {
        return m_destinationId;
    }

    int getRank() const
    {
        return m_rank;
    }

    int getRequestedMode() const
    {
        return m_requestedMode;
    }

    int getRequestedTopN() const
    {
        return m_requestedTopN;
    }

    bool skipSameModeTerminalDelaysAndCosts() const
    {
        return m_skipSameModeTerminalDelaysAndCosts;
    }

    double getRankingCost() const
    {
        return m_rankingCost;
    }

    void setRankingCost(double cost)
    {
        m_rankingCost = qMax(0.0, cost);
    }

    int getEffectiveContainerCount() const
    {
        return m_effectiveContainerCount;
    }

    void setEffectiveContainerCount(int count)
    {
        m_effectiveContainerCount = qMax(0, count);
    }

    double getWeightedTerminalDelayTotal() const
    {
        return m_weightedTerminalDelayTotal;
    }

    double getWeightedTerminalDirectCostTotal() const
    {
        return m_weightedTerminalDirectCostTotal;
    }

    double getRawTerminalDelayTotal() const
    {
        return m_rawTerminalDelayTotal;
    }

    double getRawTerminalCostTotal() const
    {
        return m_rawTerminalCostTotal;
    }

    QJsonObject getCostBreakdown() const
    {
        return m_costBreakdown;
    }

    void setCostBreakdown(const QJsonObject &breakdown)
    {
        m_costBreakdown = breakdown;
    }

    QString canonicalPathKey() const;

    /**
     * @brief Retrieves the total path cost
     * @return Total cost as double
     *
     * Returns the sum of edge and terminal costs.
     */
    double getTotalPathCost() const
    {
        return m_totalPathCost;
    }

    /**
     * @brief Sets the total path cost
     * @param cost New total cost to set
     * @throws std::invalid_argument If cost is negative
     *
     * Updates the total path cost with validation.
     */
    void setTotalPathCost(double cost);

    /**
     * @brief Retrieves the total edge costs
     * @return Sum of edge costs as double
     *
     * Returns the cumulative cost of path segments.
     */
    double getTotalEdgeCosts() const
    {
        return m_totalEdgeCosts;
    }

    /**
     * @brief Sets the total edge costs
     * @param cost New edge cost to set
     * @throws std::invalid_argument If cost is negative
     *
     * Updates the total edge costs with validation.
     */
    void setTotalEdgeCosts(double cost);

    /**
     * @brief Retrieves the total terminal costs
     * @return Sum of terminal costs as double
     *
     * Returns the cumulative cost at terminals.
     */
    double getTotalTerminalCosts() const
    {
        return m_totalTerminalCosts;
    }

    /**
     * @brief Sets the total terminal costs
     * @param cost New terminal cost to set
     * @throws std::invalid_argument If cost is negative
     *
     * Updates the total terminal costs with validation.
     */
    void setTotalTerminalCosts(double cost);

    /**
     * @brief Retrieves terminals in the path
     * @return List of terminal value snapshots
     *
     * Returns snapshots (id + display / canonical name) of the
     * terminals this path traverses. These are captured at
     * parse time so they stay valid for the lifetime of the
     * Path regardless of what happens to the live Terminal
     * cache on the simulation client.
     */
    const QList<PathTerminal> &getTerminalsInPath() const
    {
        return m_terminalsInPath;
    }

    /**
     * @brief Retrieves path segments
     * @return List of PathSegment pointers
     *
     * Returns the segments composing this path.
     */
    QList<PathSegment *> getSegments() const
    {
        return m_segments;
    }

    /**
     * @brief Retrieves the starting terminal of the path
     * @return Starting terminal identifier as QString
     * @throws std::runtime_error If path has no segments
     *      * Returns the ID of the first terminal in the
     * path.
     */
    QString getStartTerminal() const;

    /**
     * @brief Retrieves the ending terminal of the path
     * @return Ending terminal identifier as QString
     * @throws std::runtime_error If path has no segments
     *      * Returns the ID of the last terminal in the
     * path.
     */
    QString getEndTerminal() const;

    /**
     * @brief Converts the path to JSON format
     * @return QJsonObject representing the path
     *
     * Serializes the path for server communication.
     */
    QJsonObject toJson() const;

    /// Plan 8.2: sum-across-segments accessors over the
    /// estimated/actual sub-objects on each PathSegment.
    ///
    /// Unit invariant: all accessors return SI units (metres, seconds,
    /// kWh, tonnes, risk-factor). The PathSegment typed accessors
    /// handle any internal unit conversion (SegmentCostMath stores
    /// distance in km and time in hours; the accessors multiply back).
    /// Do not introduce a third writer without matching this convention.

    double totalEstimatedLength()     const;   // metres
    double totalEstimatedTravelTime() const;   // seconds
    double totalEstimatedEnergyConsumption() const;  // kWh
    double totalEstimatedCarbonEmissions()   const;  // tonnes CO₂
    double totalEstimatedRisk()              const;  // dimensionless

private:
    /**
     * @brief Unique identifier for the path
     */
    int m_pathId;
    QString m_pathUid;
    QString m_originId;
    QString m_destinationId;
    int     m_rank = 0;
    int     m_requestedMode = 0;
    int     m_requestedTopN = 0;
    bool    m_skipSameModeTerminalDelaysAndCosts = true;
    double  m_rankingCost = 0.0;
    int     m_effectiveContainerCount = 0;

    /**
     * @brief Total cost of the path
     */
    double m_totalPathCost;

    /**
     * @brief Total cost of path edges
     */
    double m_totalEdgeCosts;

    /**
     * @brief Total cost at terminals
     */
    double m_totalTerminalCosts;
    double m_weightedTerminalDelayTotal = 0.0;
    double m_weightedTerminalDirectCostTotal = 0.0;
    double m_rawTerminalDelayTotal = 0.0;
    double m_rawTerminalCostTotal = 0.0;
    QJsonObject m_costBreakdown;

    /**
     * @brief Value-snapshot list of terminals traversed by
     *        this path. Decoupled from the client's live
     *        `Terminal *` cache on purpose — see PathTerminal.
     */
    QList<PathTerminal> m_terminalsInPath;

    /**
     * @brief List of segments composing the path
     */
    QList<PathSegment *> m_segments;
};

} // namespace Backend
} // namespace CargoNetSim
