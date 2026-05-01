#include "PathSegment.h"
#include "Backend/Commons/LogCategories.h"
#include <stdexcept>

#include "Backend/Scenario/PropertyKeys.h"

namespace CargoNetSim
{
namespace Backend
{

namespace PK = Scenario::PropertyKeys;

namespace {

double subValue(const QJsonObject &attrs,
                const QString &sub, const QString &key)
{
    if (!attrs.contains(sub)) return 0.0;
    const auto obj = attrs.value(sub).toObject();
    return obj.value(key).toDouble(0.0);
}

PathSegment::SegmentMetricSnapshot metricSnapshot(
    const QJsonObject &attrs, const QString &subObject)
{
    PathSegment::SegmentMetricSnapshot out;
    if (!attrs.contains(subObject)
        || !attrs.value(subObject).isObject())
    {
        return out;
    }

    out.available = true;
    out.travelTime = subValue(attrs, subObject, PK::Segment::TravelTime);
    out.distance = subValue(attrs, subObject, PK::Segment::Distance);
    out.carbonEmissions =
        subValue(attrs, subObject, PK::Segment::CarbonEmissions);
    out.energyConsumption =
        subValue(attrs, subObject, PK::Segment::EnergyConsumption);
    out.risk = subValue(attrs, subObject, PK::Segment::Risk);
    return out;
}

PathSegment::SegmentCostSnapshot costSnapshot(
    const QJsonObject &attrs, const QString &subObject)
{
    PathSegment::SegmentCostSnapshot out;
    if (!attrs.contains(subObject)
        || !attrs.value(subObject).isObject())
    {
        return out;
    }

    out.available = true;
    out.travelTime = subValue(attrs, subObject, PK::Segment::TravelTime);
    out.distance = subValue(attrs, subObject, PK::Segment::Distance);
    out.carbonEmissions =
        subValue(attrs, subObject, PK::Segment::CarbonEmissions);
    out.energyConsumption =
        subValue(attrs, subObject, PK::Segment::EnergyConsumption);
    out.risk = subValue(attrs, subObject, PK::Segment::Risk);
    out.directCost = subValue(attrs, subObject, PK::Segment::Cost);
    return out;
}

QJsonObject scenarioSegmentAttributes(QJsonObject attributes)
{
    // PathSegment is scenario-definition data; runtime actuals live in
    // ScenarioExecutionResult and must not persist in segment attributes.
    attributes.remove(QStringLiteral("actual_values"));
    attributes.remove(QStringLiteral("actual_cost"));
    return attributes;
}

} // namespace

// PathSegment constructor
PathSegment::PathSegment(
    const QString &pathSegmentId, const QString &start,
    const QString                          &end,
    TransportationTypes::TransportationMode mode,
    const QJsonObject &attributes, QObject *parent)
    : QObject(parent)
    , m_pathSegmentId(pathSegmentId)
    , m_start(start)
    , m_end(end)
    , m_mode(mode)
    , m_attributes(scenarioSegmentAttributes(attributes))
{
    // Validate input parameters for construction
    if (pathSegmentId.isEmpty() || start.isEmpty()
        || end.isEmpty())
    {
        // Throw exception if any required field is missing
        throw std::invalid_argument(
            "Path segment parameters cannot be empty");
    }
    // No additional logging here; handled by caller if
    // needed
}

PathSegment::PathSegment(const QJsonObject &json,
                         QObject           *parent)
    : QObject(parent)
    , m_pathSegmentId("")
{
    // Extract fields from JSON
    if (json.contains("from") && json["from"].isString())
    {
        m_start = json["from"].toString();
    }
    else
    {
        throw std::invalid_argument(
            "Missing or invalid 'from' field in "
            "PathSegment JSON");
    }

    if (json.contains("to") && json["to"].isString())
    {
        m_end = json["to"].toString();
    }
    else
    {
        throw std::invalid_argument(
            "Missing or invalid 'to' field in PathSegment "
            "JSON");
    }

    if (json.contains("mode") && json["mode"].isDouble())
    {
        m_mode = static_cast<
            TransportationTypes::TransportationMode>(
            json["mode"].toInt());
    }
    else
    {
        throw std::invalid_argument(
            "Missing or invalid 'mode' field in "
            "PathSegment JSON");
    }

    // Construct an ID from endpoints and mode
    m_pathSegmentId =
        QString("%1_%2_%3")
            .arg(m_start, m_end,
                 QString::number(static_cast<int>(m_mode)));

    // Extract optional attributes
    if (json.contains("attributes")
        && json["attributes"].isObject())
    {
        m_attributes = scenarioSegmentAttributes(
            json["attributes"].toObject());
    }

    if (json.contains("sequence_index")
        && json["sequence_index"].isDouble())
    {
        m_sequenceIndex = json["sequence_index"].toInt();
    }

    if (json.contains("ranking_cost_contribution")
        && json["ranking_cost_contribution"].isDouble())
    {
        m_rankingCostContribution =
            json["ranking_cost_contribution"].toDouble();
    }

    if (json.contains("weighted_edge_cost")
        && json["weighted_edge_cost"].isDouble())
    {
        m_weightedEdgeCost =
            json["weighted_edge_cost"].toDouble();
    }

    if (json.contains("weighted_terminal_cost_embedded_in_segment")
        && json["weighted_terminal_cost_embedded_in_segment"].isDouble())
    {
        m_weightedTerminalCostEmbeddedInSegment =
            json["weighted_terminal_cost_embedded_in_segment"]
                .toDouble();
    }

    // Extract weight if available
    if (json.contains("weight")
        && json["weight"].isDouble())
    {
        m_attributes["weight"] = json["weight"].toDouble();
    }

    // Normalize TerminalSim's wire shape once at parse time. Downstream
    // code reads the canonical `estimated` key; GUI/report compatibility
    // still keeps `estimated_cost` in the raw attributes bag.
    if (m_attributes.contains("estimated_values")
        && m_attributes["estimated_values"].isObject())
    {
        m_attributes[PK::Segment::Estimated] =
            m_attributes["estimated_values"].toObject();
    }
}

// Convert PathSegment to JSON
QJsonObject PathSegment::toJson() const
{
    qCDebug(lcModel) << "PathSegment::toJson:" << m_pathSegmentId;
    // Initialize JSON object for serialization
    QJsonObject json;

    // Set mandatory fields required by server
    json["route_id"]       = m_pathSegmentId;
    json["start_terminal"] = m_start;
    json["end_terminal"]   = m_end;
    json["mode"] = TransportationTypes::toInt(m_mode);

    // Include attributes only if not empty
    if (!m_attributes.isEmpty())
    {
        json["attributes"] = m_attributes;
    }

    // Return constructed JSON object
    return json;
}

PathSegment *PathSegment::fromJson(const QJsonObject &json,
                                   QObject *parent)
{
    qCDebug(lcModel) << "PathSegment::fromJson:"
                     << json.value("from").toString()
                     << "->" << json.value("to").toString();
    return new PathSegment(json, parent);
}

PathSegment *PathSegment::clone(QObject *parent) const
{
    auto *segment = new PathSegment(
        m_pathSegmentId, m_start, m_end, m_mode, m_attributes,
        parent);
    segment->m_sequenceIndex = m_sequenceIndex;
    segment->m_rankingCostContribution =
        m_rankingCostContribution;
    segment->m_weightedEdgeCost = m_weightedEdgeCost;
    segment->m_weightedTerminalCostEmbeddedInSegment =
        m_weightedTerminalCostEmbeddedInSegment;
    return segment;
}

void PathSegment::setAttributes(
    const QJsonObject &attributes)
{
    qCDebug(lcModel) << "PathSegment::setAttributes:"
                     << m_pathSegmentId
                     << "keys=" << attributes.keys().size();
    // Set the attributes of the path segment
    m_attributes = scenarioSegmentAttributes(attributes);
}

double PathSegment::estimatedDistance() const
{ return subValue(m_attributes, PK::Segment::Estimated, PK::Segment::Distance); }

double PathSegment::estimatedTravelTime() const
{ return subValue(m_attributes, PK::Segment::Estimated, PK::Segment::TravelTime); }

PathSegment::SegmentMetricSnapshot PathSegment::estimatedValues() const
{
    return metricSnapshot(m_attributes, PK::Segment::Estimated);
}

PathSegment::SegmentCostSnapshot PathSegment::estimatedCosts() const
{
    return costSnapshot(m_attributes, PK::Segment::EstimatedCost);
}

void PathSegment::setEstimatedDistanceAndTravelTime(
    double distanceMeters, double travelTimeSeconds)
{
    QJsonObject est = m_attributes.value(PK::Segment::Estimated).toObject();
    est[PK::Segment::Distance]   = distanceMeters;
    est[PK::Segment::TravelTime] = travelTimeSeconds;
    m_attributes[PK::Segment::Estimated] = est;
}

void PathSegment::setEstimatedPhysicalMetrics(
    double energyKWh, double carbonTonnes, double risk)
{
    QJsonObject est = m_attributes.value(PK::Segment::Estimated).toObject();
    est[PK::Segment::EnergyConsumption] = energyKWh;
    est[PK::Segment::CarbonEmissions]   = carbonTonnes;
    est[PK::Segment::Risk]              = risk;
    m_attributes[PK::Segment::Estimated] = est;
}

double PathSegment::estimatedEnergyConsumption() const
{ return subValue(m_attributes, PK::Segment::Estimated, PK::Segment::EnergyConsumption); }

double PathSegment::estimatedCarbonEmissions() const
{ return subValue(m_attributes, PK::Segment::Estimated, PK::Segment::CarbonEmissions); }

double PathSegment::estimatedRisk() const
{ return subValue(m_attributes, PK::Segment::Estimated, PK::Segment::Risk); }

} // namespace Backend
} // namespace CargoNetSim
