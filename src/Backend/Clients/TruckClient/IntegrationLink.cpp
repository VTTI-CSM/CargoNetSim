/**
 * @file IntegrationLink.cpp
 * @brief Implements transportation network link
 * @author [Your Name]
 * @date 2025-03-22
 */

#include "IntegrationLink.h"

#include "Backend/Commons/LogCategories.h"

namespace CargoNetSim
{
namespace Backend
{
namespace TruckClient
{

IntegrationLink::IntegrationLink(QObject *parent)
    : BaseObject(parent)
    , m_linkId(0)
    , m_upstreamNodeId(0)
    , m_downstreamNodeId(0)
    , m_length(0.0f)
    , m_freeSpeed(0.0f)
    , m_saturationFlow(0.0f)
    , m_lanes(0.0f)
    , m_speedCoeffVariation(0.0f)
    , m_speedAtCapacity(0.0f)
    , m_jamDensity(0.0f)
    , m_turnProhibition(0)
    , m_prohibitionStart(0)
    , m_prohibitionEnd(0)
    , m_opposingLink1(0)
    , m_opposingLink2(0)
    , m_trafficSignal(0)
    , m_phase1(0)
    , m_phase2(0)
    , m_vehicleClassProhibition(0)
    , m_surveillanceLevel(0)
    , m_description("")
    , m_lengthScale(1.0f)
    , m_speedScale(1.0f)
    , m_saturationFlowScale(1.0f)
    , m_speedAtCapacityScale(1.0f)
    , m_jamDensityScale(1.0f)
{
    qCDebug(lcClientTruck) << "IntegrationLink::IntegrationLink: default constructed";
}

IntegrationLink::IntegrationLink(
    int linkId, int upstreamNodeId, int downstreamNodeId,
    float length, float freeSpeed, float saturationFlow,
    float lanes, float speedCoeffVariation,
    float speedAtCapacity, float jamDensity,
    int turnProhibition, int prohibitionStart,
    int prohibitionEnd, int opposingLink1,
    int opposingLink2, int trafficSignal, int phase1,
    int phase2, int vehicleClassProhibition,
    int surveillanceLevel, const QString &description,
    float lengthScale, float speedScale,
    float saturationFlowScale, float speedAtCapacityScale,
    float jamDensityScale, QObject *parent)
    : BaseObject(parent)
    , m_linkId(linkId)
    , m_upstreamNodeId(upstreamNodeId)
    , m_downstreamNodeId(downstreamNodeId)
    , m_length(length)
    , m_freeSpeed(freeSpeed)
    , m_saturationFlow(saturationFlow)
    , m_lanes(lanes)
    , m_speedCoeffVariation(speedCoeffVariation)
    , m_speedAtCapacity(speedAtCapacity)
    , m_jamDensity(jamDensity)
    , m_turnProhibition(turnProhibition)
    , m_prohibitionStart(prohibitionStart)
    , m_prohibitionEnd(prohibitionEnd)
    , m_opposingLink1(opposingLink1)
    , m_opposingLink2(opposingLink2)
    , m_trafficSignal(trafficSignal)
    , m_phase1(phase1)
    , m_phase2(phase2)
    , m_vehicleClassProhibition(vehicleClassProhibition)
    , m_surveillanceLevel(surveillanceLevel)
    , m_description(description)
    , m_lengthScale(lengthScale)
    , m_speedScale(speedScale)
    , m_saturationFlowScale(saturationFlowScale)
    , m_speedAtCapacityScale(speedAtCapacityScale)
    , m_jamDensityScale(jamDensityScale)
{
    qCDebug(lcClientTruck) << "IntegrationLink::IntegrationLink: linkId=" << linkId
                           << "upstream=" << upstreamNodeId
                           << "downstream=" << downstreamNodeId
                           << "length=" << length
                           << "freeSpeed=" << freeSpeed;
}

IntegrationLink::IntegrationLink(const QJsonObject &json,
                                 QObject           *parent)
    : BaseObject(parent)
{
    qCDebug(lcClientTruck) << "IntegrationLink::IntegrationLink: constructing from JSON";

    m_linkId           = json["link_id"].toInt();
    m_upstreamNodeId   = json["upstream_node_id"].toInt();
    m_downstreamNodeId = json["downstream_node_id"].toInt();
    m_length           = json["length"].toDouble();
    m_freeSpeed        = json["free_speed"].toDouble();
    m_saturationFlow   = json["saturation_flow"].toDouble();
    m_lanes            = json["lanes"].toDouble();
    m_speedCoeffVariation =
        json["speed_coeff_variation"].toDouble();
    m_speedAtCapacity =
        json["speed_at_capacity"].toDouble();
    m_jamDensity       = json["jam_density"].toDouble();
    m_turnProhibition  = json["turn_prohibition"].toInt();
    m_prohibitionStart = json["prohibition_start"].toInt();
    m_prohibitionEnd   = json["prohibition_end"].toInt();
    m_opposingLink1    = json["opposing_link_1"].toInt();
    m_opposingLink2    = json["opposing_link_2"].toInt();
    m_trafficSignal    = json["traffic_signal"].toInt();
    m_phase1           = json["phase_1"].toInt();
    m_phase2           = json["phase_2"].toInt();
    m_vehicleClassProhibition =
        json["vehicle_class_prohibition"].toInt();
    m_surveillanceLevel =
        json["surveillance_level"].toInt();
    m_description = json["description"].toString();
    m_lengthScale = json["length_scale"].toDouble(1.0);
    m_speedScale  = json["speed_scale"].toDouble(1.0);
    m_saturationFlowScale =
        json["saturation_flow_scale"].toDouble(1.0);
    m_speedAtCapacityScale =
        json["speed_at_capacity_scale"].toDouble(1.0);
    m_jamDensityScale =
        json["jam_density_scale"].toDouble(1.0);

    qCDebug(lcClientTruck) << "IntegrationLink::IntegrationLink: from JSON"
                           << "linkId=" << m_linkId
                           << "upstream=" << m_upstreamNodeId
                           << "downstream=" << m_downstreamNodeId
                           << "length=" << m_length;
}

QJsonObject IntegrationLink::toDict() const
{
    qCDebug(lcClientTruck) << "IntegrationLink::toDict: linkId=" << m_linkId;
    QJsonObject dict;
    dict["link_id"]               = m_linkId;
    dict["upstream_node_id"]      = m_upstreamNodeId;
    dict["downstream_node_id"]    = m_downstreamNodeId;
    dict["length"]                = m_length;
    dict["free_speed"]            = m_freeSpeed;
    dict["saturation_flow"]       = m_saturationFlow;
    dict["lanes"]                 = m_lanes;
    dict["speed_coeff_variation"] = m_speedCoeffVariation;
    dict["speed_at_capacity"]     = m_speedAtCapacity;
    dict["jam_density"]           = m_jamDensity;
    dict["turn_prohibition"]      = m_turnProhibition;
    dict["prohibition_start"]     = m_prohibitionStart;
    dict["prohibition_end"]       = m_prohibitionEnd;
    dict["opposing_link_1"]       = m_opposingLink1;
    dict["opposing_link_2"]       = m_opposingLink2;
    dict["traffic_signal"]        = m_trafficSignal;
    dict["phase_1"]               = m_phase1;
    dict["phase_2"]               = m_phase2;
    dict["vehicle_class_prohibition"] =
        m_vehicleClassProhibition;
    dict["surveillance_level"]    = m_surveillanceLevel;
    dict["description"]           = m_description;
    dict["length_scale"]          = m_lengthScale;
    dict["speed_scale"]           = m_speedScale;
    dict["saturation_flow_scale"] = m_saturationFlowScale;
    dict["speed_at_capacity_scale"] =
        m_speedAtCapacityScale;
    dict["jam_density_scale"] = m_jamDensityScale;

    qCDebug(lcClientTruck) << "IntegrationLink::toDict: serialized linkId=" << m_linkId;
    return dict;
}

IntegrationLink *
IntegrationLink::fromDict(const QJsonObject &data,
                          QObject           *parent)
{
    qCDebug(lcClientTruck) << "IntegrationLink::fromDict: linkId=" << data["link_id"].toInt();
    return new IntegrationLink(
        data["link_id"].toInt(),
        data["upstream_node_id"].toInt(),
        data["downstream_node_id"].toInt(),
        data["length"].toDouble(),
        data["free_speed"].toDouble(),
        data["saturation_flow"].toDouble(),
        data["lanes"].toDouble(),
        data["speed_coeff_variation"].toDouble(),
        data["speed_at_capacity"].toDouble(),
        data["jam_density"].toDouble(),
        data["turn_prohibition"].toInt(),
        data["prohibition_start"].toInt(),
        data["prohibition_end"].toInt(),
        data["opposing_link_1"].toInt(),
        data["opposing_link_2"].toInt(),
        data["traffic_signal"].toInt(),
        data["phase_1"].toInt(), data["phase_2"].toInt(),
        data["vehicle_class_prohibition"].toInt(),
        data["surveillance_level"].toInt(),
        data["description"].toString(),
        data["length_scale"].toDouble(1.0),
        data["speed_scale"].toDouble(1.0),
        data["saturation_flow_scale"].toDouble(1.0),
        data["speed_at_capacity_scale"].toDouble(1.0),
        data["jam_density_scale"].toDouble(1.0), parent);
}

void IntegrationLink::setLinkId(int linkId)
{
    if (m_linkId != linkId)
    {
        qCDebug(lcClientTruck) << "IntegrationLink::setLinkId:" << m_linkId << "->" << linkId;
        m_linkId = linkId;
        emit linkChanged();
    }
}

void IntegrationLink::setUpstreamNodeId(int upstreamNodeId)
{
    if (m_upstreamNodeId != upstreamNodeId)
    {
        qCDebug(lcClientTruck) << "IntegrationLink::setUpstreamNodeId: linkId=" << m_linkId
                               << m_upstreamNodeId << "->" << upstreamNodeId;
        m_upstreamNodeId = upstreamNodeId;
        emit linkChanged();
    }
}

void IntegrationLink::setDownstreamNodeId(
    int downstreamNodeId)
{
    if (m_downstreamNodeId != downstreamNodeId)
    {
        qCDebug(lcClientTruck) << "IntegrationLink::setDownstreamNodeId: linkId=" << m_linkId
                               << m_downstreamNodeId << "->" << downstreamNodeId;
        m_downstreamNodeId = downstreamNodeId;
        emit linkChanged();
    }
}

void IntegrationLink::setLength(float length)
{
    if (m_length != length)
    {
        qCDebug(lcClientTruck) << "IntegrationLink::setLength: linkId=" << m_linkId
                               << m_length << "->" << length;
        m_length = length;
        emit linkChanged();
    }
}

void IntegrationLink::setFreeSpeed(float freeSpeed)
{
    if (m_freeSpeed != freeSpeed)
    {
        qCDebug(lcClientTruck) << "IntegrationLink::setFreeSpeed: linkId=" << m_linkId
                               << m_freeSpeed << "->" << freeSpeed;
        m_freeSpeed = freeSpeed;
        emit linkChanged();
    }
}

void IntegrationLink::setSaturationFlow(
    float saturationFlow)
{
    if (m_saturationFlow != saturationFlow)
    {
        qCDebug(lcClientTruck) << "IntegrationLink::setSaturationFlow: linkId=" << m_linkId
                               << m_saturationFlow << "->" << saturationFlow;
        m_saturationFlow = saturationFlow;
        emit linkChanged();
    }
}

void IntegrationLink::setLanes(float lanes)
{
    if (m_lanes != lanes)
    {
        qCDebug(lcClientTruck) << "IntegrationLink::setLanes: linkId=" << m_linkId
                               << m_lanes << "->" << lanes;
        m_lanes = lanes;
        emit linkChanged();
    }
}

void IntegrationLink::setSpeedCoeffVariation(
    float speedCoeffVariation)
{
    if (m_speedCoeffVariation != speedCoeffVariation)
    {
        qCDebug(lcClientTruck) << "IntegrationLink::setSpeedCoeffVariation: linkId=" << m_linkId
                               << m_speedCoeffVariation << "->" << speedCoeffVariation;
        m_speedCoeffVariation = speedCoeffVariation;
        emit linkChanged();
    }
}

void IntegrationLink::setSpeedAtCapacity(
    float speedAtCapacity)
{
    if (m_speedAtCapacity != speedAtCapacity)
    {
        qCDebug(lcClientTruck) << "IntegrationLink::setSpeedAtCapacity: linkId=" << m_linkId
                               << m_speedAtCapacity << "->" << speedAtCapacity;
        m_speedAtCapacity = speedAtCapacity;
        emit linkChanged();
    }
}

void IntegrationLink::setJamDensity(float jamDensity)
{
    if (m_jamDensity != jamDensity)
    {
        qCDebug(lcClientTruck) << "IntegrationLink::setJamDensity: linkId=" << m_linkId
                               << m_jamDensity << "->" << jamDensity;
        m_jamDensity = jamDensity;
        emit linkChanged();
    }
}

void IntegrationLink::setTurnProhibition(
    int turnProhibition)
{
    if (m_turnProhibition != turnProhibition)
    {
        qCDebug(lcClientTruck) << "IntegrationLink::setTurnProhibition: linkId=" << m_linkId
                               << m_turnProhibition << "->" << turnProhibition;
        m_turnProhibition = turnProhibition;
        emit linkChanged();
    }
}

void IntegrationLink::setProhibitionStart(
    int prohibitionStart)
{
    if (m_prohibitionStart != prohibitionStart)
    {
        qCDebug(lcClientTruck) << "IntegrationLink::setProhibitionStart: linkId=" << m_linkId
                               << m_prohibitionStart << "->" << prohibitionStart;
        m_prohibitionStart = prohibitionStart;
        emit linkChanged();
    }
}

void IntegrationLink::setProhibitionEnd(int prohibitionEnd)
{
    if (m_prohibitionEnd != prohibitionEnd)
    {
        qCDebug(lcClientTruck) << "IntegrationLink::setProhibitionEnd: linkId=" << m_linkId
                               << m_prohibitionEnd << "->" << prohibitionEnd;
        m_prohibitionEnd = prohibitionEnd;
        emit linkChanged();
    }
}

void IntegrationLink::setOpposingLink1(int opposingLink1)
{
    if (m_opposingLink1 != opposingLink1)
    {
        qCDebug(lcClientTruck) << "IntegrationLink::setOpposingLink1: linkId=" << m_linkId
                               << m_opposingLink1 << "->" << opposingLink1;
        m_opposingLink1 = opposingLink1;
        emit linkChanged();
    }
}

void IntegrationLink::setOpposingLink2(int opposingLink2)
{
    if (m_opposingLink2 != opposingLink2)
    {
        qCDebug(lcClientTruck) << "IntegrationLink::setOpposingLink2: linkId=" << m_linkId
                               << m_opposingLink2 << "->" << opposingLink2;
        m_opposingLink2 = opposingLink2;
        emit linkChanged();
    }
}

void IntegrationLink::setTrafficSignal(int trafficSignal)
{
    if (m_trafficSignal != trafficSignal)
    {
        qCDebug(lcClientTruck) << "IntegrationLink::setTrafficSignal: linkId=" << m_linkId
                               << m_trafficSignal << "->" << trafficSignal;
        m_trafficSignal = trafficSignal;
        emit linkChanged();
    }
}

void IntegrationLink::setPhase1(int phase1)
{
    if (m_phase1 != phase1)
    {
        qCDebug(lcClientTruck) << "IntegrationLink::setPhase1: linkId=" << m_linkId
                               << m_phase1 << "->" << phase1;
        m_phase1 = phase1;
        emit linkChanged();
    }
}

void IntegrationLink::setPhase2(int phase2)
{
    if (m_phase2 != phase2)
    {
        qCDebug(lcClientTruck) << "IntegrationLink::setPhase2: linkId=" << m_linkId
                               << m_phase2 << "->" << phase2;
        m_phase2 = phase2;
        emit linkChanged();
    }
}

void IntegrationLink::setVehicleClassProhibition(
    int vehicleClassProhibition)
{
    if (m_vehicleClassProhibition
        != vehicleClassProhibition)
    {
        qCDebug(lcClientTruck) << "IntegrationLink::setVehicleClassProhibition: linkId=" << m_linkId
                               << m_vehicleClassProhibition << "->" << vehicleClassProhibition;
        m_vehicleClassProhibition = vehicleClassProhibition;
        emit linkChanged();
    }
}

void IntegrationLink::setSurveillanceLevel(
    int surveillanceLevel)
{
    if (m_surveillanceLevel != surveillanceLevel)
    {
        qCDebug(lcClientTruck) << "IntegrationLink::setSurveillanceLevel: linkId=" << m_linkId
                               << m_surveillanceLevel << "->" << surveillanceLevel;
        m_surveillanceLevel = surveillanceLevel;
        emit linkChanged();
    }
}

void IntegrationLink::setDescription(
    const QString &description)
{
    if (m_description != description)
    {
        qCDebug(lcClientTruck) << "IntegrationLink::setDescription: linkId=" << m_linkId
                               << "description=" << description;
        m_description = description;
        emit linkChanged();
    }
}

void IntegrationLink::setLengthScale(float lengthScale)
{
    if (m_lengthScale != lengthScale)
    {
        qCDebug(lcClientTruck) << "IntegrationLink::setLengthScale: linkId=" << m_linkId
                               << m_lengthScale << "->" << lengthScale;
        m_lengthScale = lengthScale;
        emit linkChanged();
    }
}

void IntegrationLink::setSpeedScale(float speedScale)
{
    if (m_speedScale != speedScale)
    {
        qCDebug(lcClientTruck) << "IntegrationLink::setSpeedScale: linkId=" << m_linkId
                               << m_speedScale << "->" << speedScale;
        m_speedScale = speedScale;
        emit linkChanged();
    }
}

void IntegrationLink::setSaturationFlowScale(
    float saturationFlowScale)
{
    if (m_saturationFlowScale != saturationFlowScale)
    {
        qCDebug(lcClientTruck) << "IntegrationLink::setSaturationFlowScale: linkId=" << m_linkId
                               << m_saturationFlowScale << "->" << saturationFlowScale;
        m_saturationFlowScale = saturationFlowScale;
        emit linkChanged();
    }
}

void IntegrationLink::setSpeedAtCapacityScale(
    float speedAtCapacityScale)
{
    if (m_speedAtCapacityScale != speedAtCapacityScale)
    {
        qCDebug(lcClientTruck) << "IntegrationLink::setSpeedAtCapacityScale: linkId=" << m_linkId
                               << m_speedAtCapacityScale << "->" << speedAtCapacityScale;
        m_speedAtCapacityScale = speedAtCapacityScale;
        emit linkChanged();
    }
}

void IntegrationLink::setJamDensityScale(
    float jamDensityScale)
{
    if (m_jamDensityScale != jamDensityScale)
    {
        qCDebug(lcClientTruck) << "IntegrationLink::setJamDensityScale: linkId=" << m_linkId
                               << m_jamDensityScale << "->" << jamDensityScale;
        m_jamDensityScale = jamDensityScale;
        emit linkChanged();
    }
}

} // namespace TruckClient
} // namespace Backend
} // namespace CargoNetSim
