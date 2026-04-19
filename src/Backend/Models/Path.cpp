#include "Path.h"
#include "Backend/Commons/LogCategories.h"
#include <stdexcept>

namespace CargoNetSim
{
namespace Backend
{

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
           const QList<Terminal *>    &terminals,
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
    if (json.contains("total_path_cost")
        && json["total_path_cost"].isDouble())
    {
        m_totalPathCost =
            json["total_path_cost"].toDouble();
    }

    if (json.contains("total_edge_costs")
        && json["total_edge_costs"].isDouble())
    {
        m_totalEdgeCosts =
            json["total_edge_costs"].toDouble();
    }

    if (json.contains("total_terminal_costs")
        && json["total_terminal_costs"].isDouble())
    {
        m_totalTerminalCosts =
            json["total_terminal_costs"].toDouble();
    }

    // Extract terminals in path
    if (json.contains("terminals_in_path")
        && json["terminals_in_path"].isArray())
    {
        QJsonArray terminalsArray =
            json["terminals_in_path"].toArray();

        for (const QJsonValue &terminalValue :
             terminalsArray)
        {
            if (terminalValue.isObject())
            {
                QJsonObject terminalObj =
                    terminalValue.toObject();
                if (terminalDB.contains(
                        terminalObj["terminal"].toString()))
                {
                    m_terminalsInPath.append(
                        terminalDB.value(
                            terminalObj["terminal"]
                                .toString(),
                            nullptr));
                }
            }
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
    qCDebug(lcModel) << "Path::fromJson: path_id="
                     << json.value("path_id").toInt();
    return new Path(json, terminalDB, parent);
}

QJsonObject Path::toJson() const
{
    qCDebug(lcModel) << "Path::toJson: path_id=" << m_pathId
                     << "segments=" << m_segments.size();
    QJsonObject json;
    json["path_id"]              = m_pathId;
    json["total_path_cost"]      = m_totalPathCost;
    json["total_edge_costs"]     = m_totalEdgeCosts;
    json["total_terminal_costs"] = m_totalTerminalCosts;

    // Add terminals
    QJsonArray terminalsArray;
    for (const Terminal *terminal : m_terminalsInPath)
    {
        terminalsArray.append(terminal->toJson());
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

double Path::totalActualLength() const
{ return sumSegments(m_segments, [](auto &s){ return s.actualDistance(); }); }

double Path::totalActualTravelTime() const
{ return sumSegments(m_segments, [](auto &s){ return s.actualTravelTime(); }); }

double Path::totalActualEnergyConsumption() const
{ return sumSegments(m_segments, [](auto &s){ return s.actualEnergyConsumption(); }); }

double Path::totalActualCarbonEmissions() const
{ return sumSegments(m_segments, [](auto &s){ return s.actualCarbonEmissions(); }); }

double Path::totalActualRisk() const
{ return sumSegments(m_segments, [](auto &s){ return s.actualRisk(); }); }

} // namespace Backend
} // namespace CargoNetSim
