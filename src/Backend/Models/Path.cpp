#include "Path.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Scenario/PropertyKeys.h"
#include <stdexcept>

namespace CargoNetSim
{
namespace Backend
{

namespace PK = Scenario::PropertyKeys;

namespace {
// Single sum-across-segments helper — every total* delegates to it
// so we never duplicate the segment-iteration loop.
template <typename Getter>
double sumSegments(const QList<PathSegment *> &segs, Getter g)
{
    double total = 0.0;
    for (const auto *s : segs)
        if (s) total += g(*s);
    return total;
}
} // namespace

// Path constructor
Path::Path(int id, double totalCost, double edgeCost,
           double                      termCost,
           const QList<PathTerminal>  &terminals,
           const QList<PathSegment *> &segments,
           QObject                    *parent)
    : QObject(parent)
    , m_pathId(id)
    , m_totalPathCost(totalCost)
    , m_totalEdgeCosts(edgeCost)
    , m_totalTerminalCosts(termCost)
    , m_terminalsInPath(terminals)
    , m_segments(segments)
{
    // Validate input parameters
    if (id <= 0)
    {
        // Ensure path ID is positive
        throw std::invalid_argument(
            "Path ID must be positive");
    }
    if (totalCost < 0 || edgeCost < 0 || termCost < 0)
    {
        // Ensure costs are non-negative
        throw std::invalid_argument(
            "Costs must be non-negative");
    }
    if (segments.isEmpty())
    {
        // Ensure at least one segment exists
        throw std::invalid_argument(
            "Path must have segments");
    }

    // Verify terminal list consistency with segments
    int expectedTerminals = segments.size() + 1;
    if (terminals.size() != expectedTerminals)
    {
        qCWarning(lcModel) << "Terminal count" << terminals.size()
                   << "does not match expected"
                   << expectedTerminals;
    }

    // No ownership transfer; segments managed by this
    // instance
}

Path::Path(const QJsonObject              &json,
           const QMap<QString, Terminal *> terminalDB,
           QObject                        *parent)
    : QObject(parent)
    , m_pathId(0)
    , m_totalPathCost(0.0)
    , m_totalEdgeCosts(0.0)
    , m_totalTerminalCosts(0.0)
{
    // Extract path ID
    if (json.contains("path_id")
        && json["path_id"].isDouble())
    {
        m_pathId = json["path_id"].toInt();
    }
    else
    {
        throw std::invalid_argument(
            "Missing or invalid 'path_id' field in Path "
            "JSON");
    }

    // Extract costs
    m_pathUid = json.value("path_uid").toString();
    m_rank = json.value("rank").toInt(0);
    m_rankingCost =
        json.value("ranking_cost")
            .toDouble(json.value("total_path_cost").toDouble(0.0));

    const QJsonObject discoveryContext =
        json.value("discovery_context").toObject();
    m_originId =
        discoveryContext.value("start_terminal")
            .toString(json.value("start_terminal").toString());
    m_destinationId =
        discoveryContext.value("end_terminal")
            .toString(json.value("end_terminal").toString());
    m_requestedMode =
        discoveryContext.value("requested_mode")
            .toInt(json.value("requested_mode").toInt(0));
    m_requestedTopN =
        discoveryContext.value("requested_top_n")
            .toInt(json.value("requested_top_n").toInt(0));
    m_skipSameModeTerminalDelaysAndCosts =
        discoveryContext
            .value("skip_same_mode_terminal_delays_and_costs")
            .toBool(json.value("skip_same_mode_terminal_delays_and_costs")
                        .toBool(true));
    m_effectiveContainerCount =
        qMax(0, json.value("effective_container_count").toInt(0));

    if (json.contains("total_path_cost")
        && json["total_path_cost"].isDouble())
    {
        m_totalPathCost =
            json["total_path_cost"].toDouble();
    }

    if (json.contains("weighted_edge_cost_total")
        && json["weighted_edge_cost_total"].isDouble())
    {
        m_totalEdgeCosts =
            json["weighted_edge_cost_total"].toDouble();
    }
    else if (json.contains("total_edge_costs")
        && json["total_edge_costs"].isDouble())
    {
        m_totalEdgeCosts =
            json["total_edge_costs"].toDouble();
    }

    if (json.contains("weighted_terminal_cost_total")
        && json["weighted_terminal_cost_total"].isDouble())
    {
        m_totalTerminalCosts =
            json["weighted_terminal_cost_total"].toDouble();
    }
    else if (json.contains("total_terminal_costs")
        && json["total_terminal_costs"].isDouble())
    {
        m_totalTerminalCosts =
            json["total_terminal_costs"].toDouble();
    }

    m_weightedTerminalDelayTotal =
        json.value("weighted_terminal_delay_total").toDouble(0.0);
    m_weightedTerminalDirectCostTotal =
        json.value("weighted_terminal_direct_cost_total")
            .toDouble(0.0);
    m_rawTerminalDelayTotal =
        json.value("raw_terminal_delay_total").toDouble(0.0);
    m_rawTerminalCostTotal =
        json.value("raw_terminal_cost_total").toDouble(0.0);
    if (json.contains("cost_breakdown")
        && json["cost_breakdown"].isObject())
    {
        m_costBreakdown = json["cost_breakdown"].toObject();
    }

    // Extract terminals in path
    //
    // Snapshot id + displayName + canonicalName at parse time
    // so we never hold a pointer into the client's live
    // `m_terminalStatus` cache. That cache mutates on every
    // terminal event (add / remove / serverReset) which
    // previously left every surviving Path with dangling
    // `Terminal *` and produced the SIGSEGV in
    // ShortestPathsTable::createPathRow.
    if (json.contains("terminals_in_path")
        && json["terminals_in_path"].isArray())
    {
        const QJsonArray terminalsArray =
            json["terminals_in_path"].toArray();

        for (const QJsonValue &terminalValue :
             terminalsArray)
        {
            if (!terminalValue.isObject()) continue;

            const QJsonObject terminalObj =
                terminalValue.toObject();
            const QString id =
                terminalObj["terminal"].toString();
            if (id.isEmpty()) continue;

            PathTerminal snap;
            snap.id = id;
            snap.sequenceIndex =
                terminalObj.value("sequence_index")
                    .toInt(m_terminalsInPath.size());
            snap.handlingTime =
                terminalObj.value("handling_time").toDouble(0.0);
            snap.rawCost =
                terminalObj.value("cost").toDouble(0.0);
            snap.costsSkipped =
                terminalObj.value("costs_skipped").toBool(false);
            snap.weightedTerminalDelayContribution =
                terminalObj
                    .value("weighted_terminal_delay_contribution")
                    .toDouble(0.0);
            snap.weightedTerminalCostContribution =
                terminalObj
                    .value("weighted_terminal_cost_contribution")
                    .toDouble(0.0);
            snap.weightedTerminalTotalContribution =
                terminalObj
                    .value("weighted_terminal_total_contribution")
                    .toDouble(0.0);
            snap.skipReason =
                terminalObj.value("skip_reason").toString();
            snap.displayName =
                terminalObj.value("display_name").toString();
            snap.canonicalName =
                terminalObj.value("canonical_name").toString();
            const Terminal *live =
                terminalDB.value(id, nullptr);
            if (live && snap.displayName.isEmpty())
            {
                snap.displayName   = live->getDisplayName();
            }
            if (live && snap.canonicalName.isEmpty())
            {
                snap.canonicalName = live->getCanonicalName();
            }
            if (snap.displayName.isEmpty())
            {
                snap.displayName   = id;
            }
            if (snap.canonicalName.isEmpty())
            {
                snap.canonicalName = id;
            }
            m_terminalsInPath.append(snap);
        }
    }

    // Extract segments
    if (json.contains("segments")
        && json["segments"].isArray())
    {
        QJsonArray segmentsArray =
            json["segments"].toArray();
        for (const QJsonValue &segmentValue : segmentsArray)
        {
            if (segmentValue.isObject())
            {
                QJsonObject segmentObj =
                    segmentValue.toObject();
                PathSegment *segment =
                    PathSegment::fromJson(segmentObj, this);
                m_segments.append(segment);
            }
        }
    }
    else
    {
        throw std::invalid_argument(
            "Missing or invalid 'segments' field in Path "
            "JSON");
    }
}

// Path destructor
Path::~Path()
{
    // Iterate over all segments in the path
    for (PathSegment *segment : m_segments)
    {
        // Delete each segment to free memory
        delete segment;
        // Nullify pointer for safety (not required but
        // explicit)
        segment = nullptr;
    }
    // Clear the list to ensure no dangling pointers
    m_segments.clear();
}

void Path::setTotalPathCost(double cost)
{
    // Validate that the cost is non-negative
    if (cost < 0)
    {
        throw std::invalid_argument(
            "Total path cost must be non-negative");
    }

    // Update the total path cost
    m_totalPathCost = cost;
}

void Path::setTotalEdgeCosts(double cost)
{
    // Validate that the cost is non-negative
    if (cost < 0)
    {
        throw std::invalid_argument(
            "Edge costs must be non-negative");
    }

    // Update the total edge costs
    m_totalEdgeCosts = cost;
}

void Path::setTotalTerminalCosts(double cost)
{
    // Validate that the cost is non-negative
    if (cost < 0)
    {
        throw std::invalid_argument(
            "Terminal costs must be non-negative");
    }

    // Update the total terminal costs
    m_totalTerminalCosts = cost;
}

Path *
Path::fromJson(const QJsonObject              &json,
               const QMap<QString, Terminal *> terminalDB,
               QObject                        *parent)
{
    return new Path(json, terminalDB, parent);
}

Path *Path::clone(QObject *parent) const
{
    QList<PathSegment *> segments;
    segments.reserve(m_segments.size());
    for (const auto *segment : m_segments)
        segments.append(segment ? segment->clone() : nullptr);

    auto *path = new Path(
        m_pathId, m_totalPathCost, m_totalEdgeCosts,
        m_totalTerminalCosts, m_terminalsInPath, segments, parent);
    path->m_pathUid = m_pathUid;
    path->m_originId = m_originId;
    path->m_destinationId = m_destinationId;
    path->m_rank = m_rank;
    path->m_requestedMode = m_requestedMode;
    path->m_requestedTopN = m_requestedTopN;
    path->m_skipSameModeTerminalDelaysAndCosts =
        m_skipSameModeTerminalDelaysAndCosts;
    path->m_rankingCost = m_rankingCost;
    path->m_effectiveContainerCount = m_effectiveContainerCount;
    path->m_weightedTerminalDelayTotal =
        m_weightedTerminalDelayTotal;
    path->m_weightedTerminalDirectCostTotal =
        m_weightedTerminalDirectCostTotal;
    path->m_rawTerminalDelayTotal = m_rawTerminalDelayTotal;
    path->m_rawTerminalCostTotal = m_rawTerminalCostTotal;
    path->m_costBreakdown = m_costBreakdown;
    return path;
}

QJsonObject Path::toJson() const
{
    qCDebug(lcModel) << "Path::toJson: path_id=" << m_pathId
                     << "segments=" << m_segments.size();
    QJsonObject json;
    json["path_id"]              = m_pathId;
    if (!m_pathUid.isEmpty()) json["path_uid"] = m_pathUid;
    json["rank"] = m_rank;
    json["ranking_cost"] = m_rankingCost;
    json["start_terminal"] = m_originId;
    json["end_terminal"] = m_destinationId;
    json["requested_mode"] = m_requestedMode;
    json["requested_top_n"] = m_requestedTopN;
    json["skip_same_mode_terminal_delays_and_costs"] =
        m_skipSameModeTerminalDelaysAndCosts;
    json["effective_container_count"] = m_effectiveContainerCount;
    json["total_path_cost"]      = m_totalPathCost;
    json["total_edge_costs"]     = m_totalEdgeCosts;
    json["total_terminal_costs"] = m_totalTerminalCosts;
    json["weighted_edge_cost_total"] = m_totalEdgeCosts;
    json["weighted_terminal_cost_total"] = m_totalTerminalCosts;
    json["weighted_terminal_delay_total"] =
        m_weightedTerminalDelayTotal;
    json["weighted_terminal_direct_cost_total"] =
        m_weightedTerminalDirectCostTotal;
    json["raw_terminal_delay_total"] = m_rawTerminalDelayTotal;
    json["raw_terminal_cost_total"] = m_rawTerminalCostTotal;
    if (!m_costBreakdown.isEmpty())
        json["cost_breakdown"] = m_costBreakdown;

    // Add terminals — mirror the wire format the server uses
    // for `terminals_in_path` entries (object with a `terminal`
    // id field). Display / canonical names are derived on the
    // consumer side from the live Terminal, not serialised here.
    QJsonArray terminalsArray;
    for (const PathTerminal &terminal : m_terminalsInPath)
    {
        QJsonObject obj;
        obj["terminal"] = terminal.id;
        obj["terminal_id"] = terminal.id;
        obj["sequence_index"] = terminal.sequenceIndex;
        obj["handling_time"] = terminal.handlingTime;
        obj["cost"] = terminal.rawCost;
        obj["costs_skipped"] = terminal.costsSkipped;
        obj["weighted_terminal_delay_contribution"] =
            terminal.weightedTerminalDelayContribution;
        obj["weighted_terminal_cost_contribution"] =
            terminal.weightedTerminalCostContribution;
        obj["weighted_terminal_total_contribution"] =
            terminal.weightedTerminalTotalContribution;
        if (!terminal.skipReason.isEmpty())
            obj["skip_reason"] = terminal.skipReason;
        terminalsArray.append(obj);
    }
    json["terminals_in_path"] = terminalsArray;

    // Add segments
    QJsonArray segmentsArray;
    for (const PathSegment *segment : m_segments)
    {
        segmentsArray.append(segment->toJson());
    }
    json["segments"] = segmentsArray;

    return json;
}

QString Path::canonicalPathKey() const
{
    if (!m_pathUid.isEmpty())
        return m_pathUid;

    const QString origin =
        !m_originId.isEmpty() ? m_originId : getStartTerminal();
    const QString destination =
        !m_destinationId.isEmpty() ? m_destinationId
                                   : getEndTerminal();
    return QStringLiteral("odr|%1|%2|%3")
        .arg(origin, destination, QString::number(m_rank));
}

QString Path::getStartTerminal() const
{
    if (m_segments.isEmpty())
    {
        throw std::runtime_error("Path has no segments");
    }
    return m_segments.first()->getStart();
}

QString Path::getEndTerminal() const
{
    if (m_segments.isEmpty())
    {
        throw std::runtime_error("Path has no segments");
    }
    return m_segments.last()->getEnd();
}

double Path::totalEstimatedLength() const
{ return sumSegments(m_segments, [](auto &s){ return s.estimatedDistance(); }); }

double Path::totalEstimatedTravelTime() const
{ return sumSegments(m_segments, [](auto &s){ return s.estimatedTravelTime(); }); }

double Path::totalEstimatedEnergyConsumption() const
{ return sumSegments(m_segments, [](auto &s){ return s.estimatedEnergyConsumption(); }); }

double Path::totalEstimatedCarbonEmissions() const
{ return sumSegments(m_segments, [](auto &s){ return s.estimatedCarbonEmissions(); }); }

double Path::totalEstimatedRisk() const
{ return sumSegments(m_segments, [](auto &s){ return s.estimatedRisk(); }); }

} // namespace Backend
} // namespace CargoNetSim
