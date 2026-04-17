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
    , m_attributes(attributes)
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
        m_attributes = json["attributes"].toObject();
    }

    // Extract weight if available
    if (json.contains("weight")
        && json["weight"].isDouble())
    {
        m_attributes["weight"] = json["weight"].toDouble();
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

void PathSegment::setAttributes(
    const QJsonObject &attributes)
{
    qCDebug(lcModel) << "PathSegment::setAttributes:"
                     << m_pathSegmentId
                     << "keys=" << attributes.keys().size();
    // Set the attributes of the path segment
    m_attributes = attributes;
}

double PathSegment::estimatedDistance() const
{ return subValue(m_attributes, PK::Segment::Estimated, PK::Segment::Distance); }

double PathSegment::estimatedTravelTime() const
{ return subValue(m_attributes, PK::Segment::Estimated, PK::Segment::TravelTime); }

double PathSegment::actualDistance() const
{
    // SegmentCostMath stores distance in km (ship/train clients report
    // m; the cost-math layer converts at aggregation). Multiply back
    // to SI so the typed accessor matches the estimated side's metres.
    return subValue(m_attributes, PK::Segment::ActualValues, PK::Segment::Distance) * 1000.0;
}

double PathSegment::actualTravelTime() const
{
    // Stored in hours (see shipSegmentCost / trainSegmentCost).
    // Return seconds for SI consistency with estimatedTravelTime().
    return subValue(m_attributes, PK::Segment::ActualValues, PK::Segment::TravelTime) * 3600.0;
}

double PathSegment::actualEnergyConsumption() const
{ return subValue(m_attributes, PK::Segment::ActualValues, PK::Segment::EnergyConsumption); }

double PathSegment::actualCarbonEmissions() const
{ return subValue(m_attributes, PK::Segment::ActualValues, PK::Segment::CarbonEmissions); }

double PathSegment::actualRisk() const
{ return subValue(m_attributes, PK::Segment::ActualValues, PK::Segment::Risk); }

void PathSegment::setEstimatedDistanceAndTravelTime(
    double distanceMeters, double travelTimeSeconds)
{
    QJsonObject est;
    est[PK::Segment::Distance]   = distanceMeters;
    est[PK::Segment::TravelTime] = travelTimeSeconds;
    m_attributes[PK::Segment::Estimated] = est;
}

void PathSegment::setActualValues(const QVariantMap &values)
{
    QJsonObject act = m_attributes.value(PK::Segment::ActualValues).toObject();
    for (auto it = values.constBegin(); it != values.constEnd(); ++it)
    {
        const QVariant &v = it.value();
        if (v.canConvert<double>())
            act[it.key()] = v.toDouble();
    }
    m_attributes[PK::Segment::ActualValues] = act;
}

void PathSegment::clearActual()
{
    m_attributes.remove(PK::Segment::ActualValues);
}

} // namespace Backend
} // namespace CargoNetSim
