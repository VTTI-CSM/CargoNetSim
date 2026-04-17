// ship_system.cpp
#include "ShipSystem.h"
#include "Backend/Commons/LogCategories.h"
#include <QtCore/qpoint.h>

namespace CargoNetSim
{
namespace Backend
{

// Define the ordered parameters for file parsing
const QVector<QPair<QString, bool>>
    ShipsReader::FILE_ORDERED_PARAMETERS = {
        {"ID", false},
        {"Path", false},
        {"MaxSpeed", false},
        {"WaterlineLength", false},
        {"LengthBetweenPerpendiculars", false},
        {"Beam", false},
        {"DraftAtForward", false},
        {"DraftAtAft", false},
        {"VolumetricDisplacement", true},
        {"WettedHullSurface", true},
        {"ShipAndCargoAreaAboveWaterline", false},
        {"BulbousBowTransverseAreaCenterHeight", false},
        {"BulbousBowTransverseArea", false},
        {"ImmersedTransomArea", false},
        {"HalfWaterlineEntranceAngle", true},
        {"SurfaceRoughness", false},
        {"LongitudinalBuoyancyCenter", false},
        {"SternShapeParam", false},
        {"MidshipSectionCoef", true},
        {"WaterplaneAreaCoef", true},
        {"PrismaticCoef", true},
        {"BlockCoef", true},
        {"TanksDetails", false},
        {"EnginesCountPerPropeller", false},
        {"EngineTierIIPropertiesPoints", false},
        {"EngineTierIIIPropertiesPoints", true},
        {"EngineTierIICurve", true},
        {"EngineTierIIICurve", true},
        {"GearboxRatio", false},
        {"GearboxEfficiency", false},
        {"ShaftEfficiency", false},
        {"PropellerCount", false},
        {"PropellerDiameter", false},
        {"PropellerPitch", false},
        {"PropellerBladesCount", false},
        {"PropellerExpandedAreaRatio", false},
        {"StopIfNoEnergy", true},
        {"MaxRudderAngle", true},
        {"VesselWeight", false},
        {"CargoWeight", false},
        {"AppendagesWettedSurfaces", true}};

// Ship Implementation
Ship::Ship(QObject *parent)
    : QObject(parent)
    , m_maxSpeed(0.0f)
    , m_waterlineLength(0.0f)
    , m_lengthBetweenPerpendiculars(0.0f)
    , m_beam(0.0f)
    , m_draftAtForward(0.0f)
    , m_draftAtAft(0.0f)
    , m_volumetricDisplacement(-1.0f)
    , m_wettedHullSurface(-1.0f)
    , m_areaAboveWaterline(0.0f)
    , m_bulbousBowCenterHeight(0.0f)
    , m_bulbousBowArea(0.0f)
    , m_immersedTransomArea(0.0f)
    , m_entranceAngle(-1.0f)
    , m_surfaceRoughness(0.0f)
    , m_buoyancyCenter(0.0f)
    , m_sternShapeParam(-1)
    , m_midshipSectionCoef(-1.0f)
    , m_waterplaneAreaCoef(-1.0f)
    , m_prismaticCoef(-1.0f)
    , m_blockCoef(-1.0f)
    , m_enginesPerPropeller(1)
    , m_gearboxRatio(0.0f)
    , m_gearboxEfficiency(1.0f)
    , m_shaftEfficiency(1.0f)
    , m_propellerCount(1)
    , m_propellerDiameter(0.0f)
    , m_propellerPitch(0.0f)
    , m_propellerBladesCount(4)
    , m_expandedAreaRatio(0.0f)
    , m_stopIfNoEnergy(false)
    , m_maxRudderAngle(-1.0f)
    , m_vesselWeight(0.0f)
    , m_cargoWeight(0.0f)
{
}

Ship::Ship(
    const QString                 &shipId,
    const QVector<QVector<float>> &pathCoordinates,
    float maxSpeed, float waterlineLength,
    float lengthBetweenPerpendiculars, float beam,
    float draftAtForward, float draftAtAft,
    float volumetricDisplacement, float wettedHullSurface,
    float areaAboveWaterline, float bulbousBowCenterHeight,
    float bulbousBowArea, float immersedTransomArea,
    float entranceAngle, float surfaceRoughness,
    float buoyancyCenter, int sternShapeParam,
    float midshipSectionCoef, float waterplaneAreaCoef,
    float prismaticCoef, float blockCoef,
    const QVector<QMap<QString, float>> &tanksDetails,
    int enginesPerPropeller,
    const QVector<QMap<QString, float>> &engineTierII,
    const QVector<QMap<QString, float>> &engineTierIII,
    const QVector<QMap<QString, float>> &engineTierIICurve,
    const QVector<QMap<QString, float>> &engineTierIIICurve,
    float gearboxRatio, float gearboxEfficiency,
    float shaftEfficiency, int propellerCount,
    float propellerDiameter, float propellerPitch,
    int propellerBladesCount, float expandedAreaRatio,
    bool stopIfNoEnergy, float maxRudderAngle,
    float vesselWeight, float cargoWeight,
    const QMap<int, float> &appendagesWettedSurfaces,
    QObject                *parent)
    : QObject(parent)
    , m_shipId(shipId)
    , m_pathCoordinates(pathCoordinates)
    , m_maxSpeed(maxSpeed)
    , m_waterlineLength(waterlineLength)
    , m_lengthBetweenPerpendiculars(
          lengthBetweenPerpendiculars)
    , m_beam(beam)
    , m_draftAtForward(draftAtForward)
    , m_draftAtAft(draftAtAft)
    , m_volumetricDisplacement(volumetricDisplacement)
    , m_wettedHullSurface(wettedHullSurface)
    , m_areaAboveWaterline(areaAboveWaterline)
    , m_bulbousBowCenterHeight(bulbousBowCenterHeight)
    , m_bulbousBowArea(bulbousBowArea)
    , m_immersedTransomArea(immersedTransomArea)
    , m_entranceAngle(entranceAngle)
    , m_surfaceRoughness(surfaceRoughness)
    , m_buoyancyCenter(buoyancyCenter)
    , m_sternShapeParam(sternShapeParam)
    , m_midshipSectionCoef(midshipSectionCoef)
    , m_waterplaneAreaCoef(waterplaneAreaCoef)
    , m_prismaticCoef(prismaticCoef)
    , m_blockCoef(blockCoef)
    , m_tanksDetails(tanksDetails)
    , m_enginesPerPropeller(enginesPerPropeller)
    , m_engineTierII(engineTierII)
    , m_engineTierIII(engineTierIII)
    , m_engineTierIICurve(engineTierIICurve)
    , m_engineTierIIICurve(engineTierIIICurve)
    , m_gearboxRatio(gearboxRatio)
    , m_gearboxEfficiency(gearboxEfficiency)
    , m_shaftEfficiency(shaftEfficiency)
    , m_propellerCount(propellerCount)
    , m_propellerDiameter(propellerDiameter)
    , m_propellerPitch(propellerPitch)
    , m_propellerBladesCount(propellerBladesCount)
    , m_expandedAreaRatio(expandedAreaRatio)
    , m_stopIfNoEnergy(stopIfNoEnergy)
    , m_maxRudderAngle(maxRudderAngle)
    , m_vesselWeight(vesselWeight)
    , m_cargoWeight(cargoWeight)
    , m_appendagesWettedSurfaces(appendagesWettedSurfaces)
{
}

Ship::Ship(const QJsonObject &json, QObject *parent)
    : QObject(parent)
{
    qCDebug(lcModel) << "Ship::Ship(json): deserializing from JSON";
    // Helper function for consistent NaN handling during
    // parsing
    auto parseFloat = [](const QJsonValue &value,
                         float             defaultValue =
                             0.0f) -> float {
        if (value.isNull())
        {
            return NAN_SENTINEL_VALUE;
        }

        if (value.isString())
        {
            QString strVal = value.toString().toLower();
            if (strVal.contains("nan")
                || strVal.contains("na"))
            {
                return NAN_SENTINEL_VALUE;
            }

            bool  ok;
            float floatVal = strVal.toFloat(&ok);
            return ok ? floatVal : NAN_SENTINEL_VALUE;
        }

        return value.toDouble(defaultValue);
    };

    // Parse basic string parameters
    m_shipId = json["ID"].toString();

    // Parse numeric parameters
    m_maxSpeed        = parseFloat(json["MaxSpeed"]);
    m_waterlineLength = parseFloat(json["WaterlineLength"]);
    m_lengthBetweenPerpendiculars =
        parseFloat(json["LengthBetweenPerpendiculars"]);
    m_beam           = parseFloat(json["Beam"]);
    m_draftAtForward = parseFloat(json["DraftAtForward"]);
    m_draftAtAft     = parseFloat(json["DraftAtAft"]);
    m_volumetricDisplacement =
        parseFloat(json["VolumetricDisplacement"]);
    m_wettedHullSurface =
        parseFloat(json["WettedHullSurface"]);
    m_areaAboveWaterline = parseFloat(
        json["ShipAndCargoAreaAboveWaterline"], 0.0f);
    m_bulbousBowCenterHeight = parseFloat(
        json["BulbousBowTransverseAreaCenterHeight"], 0.0f);
    m_bulbousBowArea =
        parseFloat(json["BulbousBowTransverseArea"], 0.0f);
    m_immersedTransomArea =
        json["ImmersedTransomArea"].toDouble(0.0);
    m_entranceAngle =
        parseFloat(json["HalfWaterlineEntranceAngle"]);
    m_surfaceRoughness =
        parseFloat(json["SurfaceRoughness"], 0.0f);
    m_buoyancyCenter = parseFloat(
        json["LongitudinalBuoyancyCenter"], 0.0f);

    if (json.contains("SternShapeParam")
        && !json["SternShapeParam"].isNull())
    {
        bool ok;
        m_sternShapeParam =
            json["SternShapeParam"].toString().toInt(&ok);
        if (!ok)
        {
            m_sternShapeParam = -1;
        }
    }
    else
    {
        m_sternShapeParam = -1;
    }

    m_midshipSectionCoef =
        parseFloat(json["MidshipSectionCoef"]);
    m_waterplaneAreaCoef =
        parseFloat(json["WaterplaneAreaCoef"]);
    m_prismaticCoef = parseFloat(json["PrismaticCoef"]);
    m_blockCoef     = parseFloat(json["BlockCoef"]);

    // Parse engine and propeller parameters
    m_enginesPerPropeller =
        json["EnginesCountPerPropeller"].toInt(1);
    m_gearboxRatio = json["GearboxRatio"].toDouble(0.0);
    m_gearboxEfficiency =
        json["GearboxEfficiency"].toDouble(1.0);
    m_shaftEfficiency =
        json["ShaftEfficiency"].toDouble(1.0);
    m_propellerCount = json["PropellerCount"].toInt(1);
    m_propellerDiameter =
        json["PropellerDiameter"].toDouble(0.0);
    m_propellerPitch = json["PropellerPitch"].toDouble(0.0);
    m_propellerBladesCount =
        json["PropellerBladesCount"].toInt(4);
    m_expandedAreaRatio =
        json["PropellerExpandedAreaRatio"].toDouble(0.0);

    if (json.contains("StopIfNoEnergy")
        && !json["StopIfNoEnergy"].isNull())
    {
        m_stopIfNoEnergy = json["StopIfNoEnergy"].toBool();
    }
    else
    {
        m_stopIfNoEnergy = false;
    }

    if (json.contains("MaxRudderAngle")
        && !json["MaxRudderAngle"].isNull())
    {
        m_maxRudderAngle =
            json["MaxRudderAngle"].toDouble();
    }
    else
    {
        m_maxRudderAngle = -1.0f;
    }

    m_vesselWeight = json["VesselWeight"].toDouble(0.0);
    m_cargoWeight  = json["CargoWeight"].toDouble(0.0);

    // Parse complex parameters
    // 1. Path
    QString pathStr   = json["Path"].toString();
    m_pathCoordinates = ShipsReader::parsePath(pathStr);

    // 2. TanksDetails
    QString tanksStr = json["TanksDetails"].toString();
    m_tanksDetails =
        ShipsReader::parseTanksDetails(tanksStr);

    // 3. EngineTierIIPropertiesPoints
    QString engineTier2Str =
        json["EngineTierIIPropertiesPoints"].toString();
    m_engineTierII =
        ShipsReader::parseEnginePoints(engineTier2Str);

    // 4. EngineTierIIIPropertiesPoints (optional)
    if (json.contains("EngineTierIIIPropertiesPoints")
        && !json["EngineTierIIIPropertiesPoints"].isNull())
    {
        QString engineTier3Str =
            json["EngineTierIIIPropertiesPoints"]
                .toString();
        if (engineTier3Str.toLower().contains("nan")
            || engineTier3Str.toLower().contains("na"))
        {
            m_engineTierIII.clear();
        }
        else
        {
            m_engineTierIII =
                ShipsReader::parseEnginePoints(
                    engineTier3Str);
        }
    }
    else
    {
        m_engineTierIII.clear();
    }

    // 5. EngineTierIICurve (optional)
    if (json.contains("EngineTierIICurve")
        && !json["EngineTierIICurve"].isNull())
    {
        QString engineTier2CurveStr =
            json["EngineTierIICurve"].toString();
        if (engineTier2CurveStr.toLower().contains("nan")
            || engineTier2CurveStr.toLower().contains("na"))
        {
            m_engineTierIICurve.clear();
        }
        else
        {
            m_engineTierIICurve =
                ShipsReader::parseEnginePoints(
                    engineTier2CurveStr);
        }
    }
    else
    {
        m_engineTierIICurve.clear();
    }

    // 6. EngineTierIIICurve (optional)
    if (json.contains("EngineTierIIICurve")
        && !json["EngineTierIIICurve"].isNull())
    {
        QString engineTier3CurveStr =
            json["EngineTierIIICurve"].toString();
        if (engineTier3CurveStr.toLower().contains("nan")
            || engineTier3CurveStr.toLower().contains("na"))
        {
            m_engineTierIIICurve.clear();
        }
        else
        {
            m_engineTierIIICurve =
                ShipsReader::parseEnginePoints(
                    engineTier3CurveStr);
        }
    }
    else
    {
        m_engineTierIIICurve.clear();
    }

    // 7. AppendagesWettedSurfaces (optional)
    if (json.contains("AppendagesWettedSurfaces")
        && !json["AppendagesWettedSurfaces"].isNull())
    {
        QVariant appendagesVar =
            json["AppendagesWettedSurfaces"].toVariant();
        if (appendagesVar.toString().toLower().contains(
                "na")
            || appendagesVar.toString().toLower().contains(
                "nan"))
        {
            m_appendagesWettedSurfaces.clear();
        }
        else
        {
            QString appendagesStr =
                json["AppendagesWettedSurfaces"].toString();
            m_appendagesWettedSurfaces =
                ShipsReader::parseAppendages(appendagesStr);
        }
    }
    else
    {
        m_appendagesWettedSurfaces.clear();
    }
}

// Helper function to check if value contains NaN
bool Ship::containsNaN(const QVariant &value) const
{
    if (value.isNull())
    {
        return true;
    }

    // Handle float values
    if (value.typeId() == QMetaType::Float
        || value.typeId() == QMetaType::Double)
    {
        return isNanValue(value.toFloat());
    }

    // Handle string values
    if (value.typeId() == QMetaType::QString)
    {
        QString strVal = value.toString().toLower();
        return strVal.contains("nan")
               || strVal.contains("na");
    }

    // Check lists
    if (value.typeId() == QMetaType::QVariantList)
    {
        QVariantList list = value.toList();
        for (const auto &item : list)
        {
            if (containsNaN(item))
            {
                return true;
            }
        }
    }

    return false;
}

bool Ship::isNanValue(float value) const
{
    return qIsNaN(value) || value == NAN_SENTINEL_VALUE;
}

QJsonObject Ship::toJson() const
{
    qCDebug(lcModel) << "Ship::toJson:" << m_shipId;
    // Helper function to handle float values that might be
    // NaN
    auto serializeFloat =
        [this](float value) -> QJsonValue {
        if (isNanValue(value))
        {
            return QString("nan");
        }
        return value;
    };

    // Helper function to format path
    auto formatPath =
        [](const QVector<QVector<float>> &path) -> QString {
        if (path.isEmpty())
        {
            return QString();
        }

        QStringList pointList;
        for (const auto &point : path)
        {
            if (point.size() >= 2)
            {
                pointList.append(
                    QString("%1,%2").arg(point[0]).arg(
                        point[1]));
            }
        }

        return pointList.join(';');
    };

    // Helper function to format tanks details
    auto formatTanksDetails =
        [](const QVector<QMap<QString, float>> &tanks)
        -> QString {
        if (tanks.isEmpty())
        {
            return QString();
        }

        QStringList tanksList;
        for (const auto &tank : tanks)
        {
            if (tank.contains("FuelType")
                && tank.contains("MaxCapacity")
                && tank.contains(
                    "TankInitialCapacityPercentage")
                && tank.contains("TankDepthOfDischage"))
            {
                tanksList.append(
                    QString("%1,%2,%3,%4")
                        .arg(tank["FuelType"])
                        .arg(tank["MaxCapacity"])
                        .arg(tank["TankInitialCapacityPerce"
                                  "ntage"])
                        .arg(tank["TankDepthOfDischage"]));
            }
        }

        return tanksList.join(';');
    };

    // Helper function to format engine points
    auto formatEnginePoints =
        [this](const QVector<QMap<QString, float>> &points)
        -> QString {
        if (points.isEmpty())
        {
            return "nan";
        }

        QVariant pointsVar = QVariant::fromValue(points);
        if (containsNaN(pointsVar))
        {
            return "nan";
        }

        QStringList pointsList;
        for (const auto &point : points)
        {
            if (point.contains("Power")
                && point.contains("RPM")
                && point.contains("Efficiency"))
            {
                pointsList.append(
                    QString("%1,%2,%3")
                        .arg(point["Power"])
                        .arg(point["RPM"])
                        .arg(point["Efficiency"]));
            }
        }

        return pointsList.join(';');
    };

    // Helper function to safely convert to string
    auto safeConvertToString =
        [this](const QVariant &value) -> QVariant {
        if (value.isNull()
            || (value.typeId() == QMetaType::Double
                && qIsNaN(value.toDouble())))
        {
            return value;
        }

        return value.toString();
    };

    QJsonObject json;

    // Add all ship attributes to JSON
    json["ID"]              = m_shipId;
    json["Path"]            = formatPath(m_pathCoordinates);
    json["MaxSpeed"]        = serializeFloat(m_maxSpeed);
    json["WaterlineLength"] =
        serializeFloat(m_waterlineLength);
    json["LengthBetweenPerpendiculars"] =
        serializeFloat(m_lengthBetweenPerpendiculars);
    json["Beam"] = serializeFloat(m_beam);
    json["DraftAtForward"] =
        serializeFloat(m_draftAtForward);
    json["DraftAtAft"] = serializeFloat(m_draftAtAft);
    json["VolumetricDisplacement"] =
        serializeFloat(m_volumetricDisplacement);
    json["WettedHullSurface"] =
        serializeFloat(m_wettedHullSurface);
    json["ShipAndCargoAreaAboveWaterline"] =
        serializeFloat(m_areaAboveWaterline);
    json["BulbousBowTransverseAreaCenterHeight"] =
        serializeFloat(m_bulbousBowCenterHeight);
    json["BulbousBowTransverseArea"] =
        serializeFloat(m_bulbousBowArea);
    json["ImmersedTransomArea"] =
        serializeFloat(m_immersedTransomArea);
    json["HalfWaterlineEntranceAngle"] =
        serializeFloat(m_entranceAngle);
    json["SurfaceRoughness"] =
        serializeFloat(m_surfaceRoughness);
    json["LongitudinalBuoyancyCenter"] =
        serializeFloat(m_buoyancyCenter);
    json["SternShapeParam"] =
        (m_sternShapeParam == -1)
            ? QJsonValue(QString("nan"))
            : QJsonValue(m_sternShapeParam);
    json["MidshipSectionCoef"] =
        serializeFloat(m_midshipSectionCoef);
    json["WaterplaneAreaCoef"] =
        serializeFloat(m_waterplaneAreaCoef);
    json["PrismaticCoef"] = serializeFloat(m_prismaticCoef);
    json["BlockCoef"]     = serializeFloat(m_blockCoef);
    json["TanksDetails"] =
        formatTanksDetails(m_tanksDetails);
    json["EnginesCountPerPropeller"] =
        m_enginesPerPropeller;
    json["EngineTierIIPropertiesPoints"] =
        formatEnginePoints(m_engineTierII);
    json["EngineTierIIIPropertiesPoints"] =
        formatEnginePoints(m_engineTierIII);
    json["EngineTierIICurve"] =
        formatEnginePoints(m_engineTierIICurve);
    json["EngineTierIIICurve"] =
        formatEnginePoints(m_engineTierIIICurve);
    json["GearboxRatio"]         = m_gearboxRatio;
    json["GearboxEfficiency"]    = m_gearboxEfficiency;
    json["ShaftEfficiency"]      = m_shaftEfficiency;
    json["PropellerCount"]       = m_propellerCount;
    json["PropellerDiameter"]    = m_propellerDiameter;
    json["PropellerPitch"]       = m_propellerPitch;
    json["PropellerBladesCount"] = m_propellerBladesCount;
    json["PropellerExpandedAreaRatio"] =
        m_expandedAreaRatio;
    json["StopIfNoEnergy"] = m_stopIfNoEnergy;
    json["MaxRudderAngle"] = m_maxRudderAngle;
    json["VesselWeight"]   = m_vesselWeight;
    json["CargoWeight"]    = m_cargoWeight;

    // Format appendages wetted surfaces
    if (m_appendagesWettedSurfaces.isEmpty())
    {
        json["AppendagesWettedSurfaces"] = "nan";
    }
    else
    {
        QStringList appendagesList;
        for (auto it =
                 m_appendagesWettedSurfaces.constBegin();
             it != m_appendagesWettedSurfaces.constEnd();
             ++it)
        {
            appendagesList.append(
                QString("%1,%2").arg(it.key()).arg(
                    it.value()));
        }
        json["AppendagesWettedSurfaces"] =
            appendagesList.join(';');
    }

    qCDebug(lcModel) << "ship json: " << json;

    return json;
}

QJsonObject Ship::toDict() const
{
    QJsonObject dict;

    // Convert path coordinates to JSON array
    QJsonArray pathArray;
    for (const auto &point : m_pathCoordinates)
    {
        QJsonArray pointArray;
        for (float value : point)
        {
            pointArray.append(value);
        }
        pathArray.append(pointArray);
    }

    // Convert tanks details to JSON array
    QJsonArray tanksArray;
    for (const auto &tank : m_tanksDetails)
    {
        QJsonObject tankObj;
        for (auto it = tank.constBegin();
             it != tank.constEnd(); ++it)
        {
            tankObj[it.key()] = it.value();
        }
        tanksArray.append(tankObj);
    }

    // Convert engine tier II to JSON array
    QJsonArray engineTier2Array;
    for (const auto &point : m_engineTierII)
    {
        QJsonObject pointObj;
        for (auto it = point.constBegin();
             it != point.constEnd(); ++it)
        {
            pointObj[it.key()] = it.value();
        }
        engineTier2Array.append(pointObj);
    }

    // Convert engine tier III to JSON array
    QJsonArray engineTier3Array;
    for (const auto &point : m_engineTierIII)
    {
        QJsonObject pointObj;
        for (auto it = point.constBegin();
             it != point.constEnd(); ++it)
        {
            pointObj[it.key()] = it.value();
        }
        engineTier3Array.append(pointObj);
    }

    // Convert engine tier II curve to JSON array
    QJsonArray engineTier2CurveArray;
    for (const auto &point : m_engineTierIICurve)
    {
        QJsonObject pointObj;
        for (auto it = point.constBegin();
             it != point.constEnd(); ++it)
        {
            pointObj[it.key()] = it.value();
        }
        engineTier2CurveArray.append(pointObj);
    }

    // Convert engine tier III curve to JSON array
    QJsonArray engineTier3CurveArray;
    for (const auto &point : m_engineTierIIICurve)
    {
        QJsonObject pointObj;
        for (auto it = point.constBegin();
             it != point.constEnd(); ++it)
        {
            pointObj[it.key()] = it.value();
        }
        engineTier3CurveArray.append(pointObj);
    }

    // Convert appendages wetted surfaces to JSON object
    QJsonObject appendagesObj;
    for (auto it = m_appendagesWettedSurfaces.constBegin();
         it != m_appendagesWettedSurfaces.constEnd(); ++it)
    {
        appendagesObj[QString::number(it.key())] =
            it.value();
    }

    // Add all attributes to the dictionary with
    // Python-style naming
    dict["ship_id"]          = m_shipId;
    dict["path_coordinates"] = pathArray;
    dict["max_speed"]        = m_maxSpeed;
    dict["waterline_length"] = m_waterlineLength;
    dict["length_between_perpendiculars"] =
        m_lengthBetweenPerpendiculars;
    dict["beam"]             = m_beam;
    dict["draft_at_forward"] = m_draftAtForward;
    dict["draft_at_aft"]     = m_draftAtAft;
    dict["volumetric_displacement"] =
        m_volumetricDisplacement;
    dict["wetted_hull_surface"]  = m_wettedHullSurface;
    dict["area_above_waterline"] = m_areaAboveWaterline;
    dict["bulbous_bow_center_height"] =
        m_bulbousBowCenterHeight;
    dict["bulbous_bow_area"]       = m_bulbousBowArea;
    dict["immersed_transom_area"]  = m_immersedTransomArea;
    dict["entrance_angle"]         = m_entranceAngle;
    dict["surface_roughness"]      = m_surfaceRoughness;
    dict["buoyancy_center"]        = m_buoyancyCenter;
    dict["stern_shape_param"]      = m_sternShapeParam;
    dict["midship_section_coef"]   = m_midshipSectionCoef;
    dict["waterplane_area_coef"]   = m_waterplaneAreaCoef;
    dict["prismatic_coef"]         = m_prismaticCoef;
    dict["block_coef"]             = m_blockCoef;
    dict["tanks_details"]          = tanksArray;
    dict["engines_per_propeller"]  = m_enginesPerPropeller;
    dict["engine_tier_ii"]         = engineTier2Array;
    dict["engine_tier_iii"]        = engineTier3Array;
    dict["engine_tier_ii_curve"]   = engineTier2CurveArray;
    dict["engine_tier_iii_curve"]  = engineTier3CurveArray;
    dict["gearbox_ratio"]          = m_gearboxRatio;
    dict["gearbox_efficiency"]     = m_gearboxEfficiency;
    dict["shaft_efficiency"]       = m_shaftEfficiency;
    dict["propeller_count"]        = m_propellerCount;
    dict["propeller_diameter"]     = m_propellerDiameter;
    dict["propeller_pitch"]        = m_propellerPitch;
    dict["propeller_blades_count"] = m_propellerBladesCount;
    dict["expanded_area_ratio"]    = m_expandedAreaRatio;
    dict["stop_if_no_energy"]      = m_stopIfNoEnergy;
    dict["max_rudder_angle"]       = m_maxRudderAngle;
    dict["vessel_weight"]          = m_vesselWeight;
    dict["cargo_weight"]           = m_cargoWeight;
    dict["appendages_wetted_surfaces"] = appendagesObj;

    return dict;
}

Ship *Ship::fromDict(const QJsonObject &data,
                     QObject           *parent)
{
    qCDebug(lcModel) << "Ship::fromDict:"
                     << data.value("ship_id").toString();
    // Parse path coordinates from JSON array
    QVector<QVector<float>> pathCoordinates;
    QJsonArray              pathArray =
        data["path_coordinates"].toArray();
    for (const QJsonValue &pointValue : pathArray)
    {
        QVector<float> point;
        QJsonArray     pointArray = pointValue.toArray();
        for (const QJsonValue &coordValue : pointArray)
        {
            point.append(coordValue.toDouble());
        }
        pathCoordinates.append(point);
    }

    // Parse tanks details from JSON array
    QVector<QMap<QString, float>> tanksDetails;
    QJsonArray tanksArray = data["tanks_details"].toArray();
    for (const QJsonValue &tankValue : tanksArray)
    {
        QMap<QString, float> tank;
        QJsonObject          tankObj = tankValue.toObject();
        for (auto it = tankObj.constBegin();
             it != tankObj.constEnd(); ++it)
        {
            tank[it.key()] = it.value().toDouble();
        }
        tanksDetails.append(tank);
    }

    // Parse engine tier II from JSON array
    QVector<QMap<QString, float>> engineTierII;
    QJsonArray                    engineTier2Array =
        data["engine_tier_ii"].toArray();
    for (const QJsonValue &pointValue : engineTier2Array)
    {
        QMap<QString, float> point;
        QJsonObject pointObj = pointValue.toObject();
        for (auto it = pointObj.constBegin();
             it != pointObj.constEnd(); ++it)
        {
            point[it.key()] = it.value().toDouble();
        }
        engineTierII.append(point);
    }

    // Parse engine tier III from JSON array (optional)
    QVector<QMap<QString, float>> engineTierIII;
    if (data.contains("engine_tier_iii")
        && !data["engine_tier_iii"].isNull())
    {
        QJsonArray engineTier3Array =
            data["engine_tier_iii"].toArray();
        for (const QJsonValue &pointValue :
             engineTier3Array)
        {
            QMap<QString, float> point;
            QJsonObject pointObj = pointValue.toObject();
            for (auto it = pointObj.constBegin();
                 it != pointObj.constEnd(); ++it)
            {
                point[it.key()] = it.value().toDouble();
            }
            engineTierIII.append(point);
        }
    }

    // Parse engine tier II curve from JSON array (optional)
    QVector<QMap<QString, float>> engineTierIICurve;
    if (data.contains("engine_tier_ii_curve")
        && !data["engine_tier_ii_curve"].isNull())
    {
        QJsonArray engineTier2CurveArray =
            data["engine_tier_ii_curve"].toArray();
        for (const QJsonValue &pointValue :
             engineTier2CurveArray)
        {
            QMap<QString, float> point;
            QJsonObject pointObj = pointValue.toObject();
            for (auto it = pointObj.constBegin();
                 it != pointObj.constEnd(); ++it)
            {
                point[it.key()] = it.value().toDouble();
            }
            engineTierIICurve.append(point);
        }
    }

    // Parse engine tier III curve from JSON array
    // (optional)
    QVector<QMap<QString, float>> engineTierIIICurve;
    if (data.contains("engine_tier_iii_curve")
        && !data["engine_tier_iii_curve"].isNull())
    {
        QJsonArray engineTier3CurveArray =
            data["engine_tier_iii_curve"].toArray();
        for (const QJsonValue &pointValue :
             engineTier3CurveArray)
        {
            QMap<QString, float> point;
            QJsonObject pointObj = pointValue.toObject();
            for (auto it = pointObj.constBegin();
                 it != pointObj.constEnd(); ++it)
            {
                point[it.key()] = it.value().toDouble();
            }
            engineTierIIICurve.append(point);
        }
    }

    // Parse appendages wetted surfaces from JSON object
    // (optional)
    QMap<int, float> appendagesWettedSurfaces;
    if (data.contains("appendages_wetted_surfaces")
        && !data["appendages_wetted_surfaces"].isNull())
    {
        QJsonObject appendagesObj =
            data["appendages_wetted_surfaces"].toObject();
        for (auto it = appendagesObj.constBegin();
             it != appendagesObj.constEnd(); ++it)
        {
            appendagesWettedSurfaces[it.key().toInt()] =
                it.value().toDouble();
        }
    }

    // Create and return Ship object
    return new Ship(
        data["ship_id"].toString(), pathCoordinates,
        data["max_speed"].toDouble(),
        data["waterline_length"].toDouble(),
        data["length_between_perpendiculars"].toDouble(),
        data["beam"].toDouble(),
        data["draft_at_forward"].toDouble(),
        data["draft_at_aft"].toDouble(),
        data.value("volumetric_displacement")
            .toDouble(-1.0),
        data.value("wetted_hull_surface").toDouble(-1.0),
        data.value("area_above_waterline").toDouble(0.0),
        data.value("bulbous_bow_center_height")
            .toDouble(0.0),
        data.value("bulbous_bow_area").toDouble(0.0),
        data.value("immersed_transom_area").toDouble(0.0),
        data.value("entrance_angle").toDouble(-1.0),
        data.value("surface_roughness").toDouble(0.0),
        data.value("buoyancy_center").toDouble(0.0),
        data.value("stern_shape_param").toInt(-1),
        data.value("midship_section_coef").toDouble(-1.0),
        data.value("waterplane_area_coef").toDouble(-1.0),
        data.value("prismatic_coef").toDouble(-1.0),
        data.value("block_coef").toDouble(-1.0),
        tanksDetails,
        data.value("engines_per_propeller").toInt(1),
        engineTierII, engineTierIII, engineTierIICurve,
        engineTierIIICurve,
        data.value("gearbox_ratio").toDouble(0.0),
        data.value("gearbox_efficiency").toDouble(1.0),
        data.value("shaft_efficiency").toDouble(1.0),
        data.value("propeller_count").toInt(1),
        data.value("propeller_diameter").toDouble(0.0),
        data.value("propeller_pitch").toDouble(0.0),
        data.value("propeller_blades_count").toInt(4),
        data.value("expanded_area_ratio").toDouble(0.0),
        data.value("stop_if_no_energy").toBool(false),
        data.value("max_rudder_angle").toDouble(-1.0),
        data.value("vessel_weight").toDouble(0.0),
        data.value("cargo_weight").toDouble(0.0),
        appendagesWettedSurfaces, parent);
}

Ship *Ship::copy() const
{
    // Create deep copies of all complex data structures

    // Copy path coordinates
    QVector<QVector<float>> pathCopy;
    for (const auto &point : m_pathCoordinates)
    {
        pathCopy.append(QVector<float>(point));
    }

    // Copy tanks details
    QVector<QMap<QString, float>> tanksCopy;
    for (const auto &tank : m_tanksDetails)
    {
        tanksCopy.append(QMap<QString, float>(tank));
    }

    // Copy engine tier II
    QVector<QMap<QString, float>> engineTier2Copy;
    for (const auto &point : m_engineTierII)
    {
        engineTier2Copy.append(QMap<QString, float>(point));
    }

    // Copy engine tier III
    QVector<QMap<QString, float>> engineTier3Copy;
    for (const auto &point : m_engineTierIII)
    {
        engineTier3Copy.append(QMap<QString, float>(point));
    }

    // Copy engine tier II curve
    QVector<QMap<QString, float>> engineTier2CurveCopy;
    for (const auto &point : m_engineTierIICurve)
    {
        engineTier2CurveCopy.append(
            QMap<QString, float>(point));
    }

    // Copy engine tier III curve
    QVector<QMap<QString, float>> engineTier3CurveCopy;
    for (const auto &point : m_engineTierIIICurve)
    {
        engineTier3CurveCopy.append(
            QMap<QString, float>(point));
    }

    // Copy appendages wetted surfaces
    QMap<int, float> appendagesCopy(
        m_appendagesWettedSurfaces);

    // Create and return a new Ship with the copied data
    return new Ship(
        m_shipId, pathCopy, m_maxSpeed, m_waterlineLength,
        m_lengthBetweenPerpendiculars, m_beam,
        m_draftAtForward, m_draftAtAft,
        m_volumetricDisplacement, m_wettedHullSurface,
        m_areaAboveWaterline, m_bulbousBowCenterHeight,
        m_bulbousBowArea, m_immersedTransomArea,
        m_entranceAngle, m_surfaceRoughness,
        m_buoyancyCenter, m_sternShapeParam,
        m_midshipSectionCoef, m_waterplaneAreaCoef,
        m_prismaticCoef, m_blockCoef, tanksCopy,
        m_enginesPerPropeller, engineTier2Copy,
        engineTier3Copy, engineTier2CurveCopy,
        engineTier3CurveCopy, m_gearboxRatio,
        m_gearboxEfficiency, m_shaftEfficiency,
        m_propellerCount, m_propellerDiameter,
        m_propellerPitch, m_propellerBladesCount,
        m_expandedAreaRatio, m_stopIfNoEnergy,
        m_maxRudderAngle, m_vesselWeight, m_cargoWeight,
        appendagesCopy);
}

// Setters implementation with signals
void Ship::setUserId(const QString &shipId)
{
    if (m_shipId != shipId)
    {
        m_shipId = shipId;
        emit shipChanged();
    }
}

void Ship::setPathCoordinates(
    const QVector<QPointF> &pathCoordinates)
{
    // Convert QVector<QPointF> to our internal
    // QVector<QVector<float>> format
    QVector<QVector<float>> convertedPath;
    for (const QPointF &point : pathCoordinates)
    {
        QVector<float> pointVector;
        pointVector.append(point.x());
        pointVector.append(point.y());
        convertedPath.append(pointVector);
    }

    if (m_pathCoordinates != convertedPath)
    {
        m_pathCoordinates = convertedPath;
        emit pathChanged();
        emit shipChanged();
    }
}

void Ship::setMaxSpeed(float maxSpeed)
{
    if (!qFuzzyCompare(m_maxSpeed, maxSpeed))
    {
        m_maxSpeed = maxSpeed;
        emit propertiesChanged();
        emit shipChanged();
    }
}

void Ship::setWaterlineLength(float waterlineLength)
{
    if (!qFuzzyCompare(m_waterlineLength, waterlineLength))
    {
        m_waterlineLength = waterlineLength;
        emit propertiesChanged();
        emit shipChanged();
    }
}

void Ship::setLengthBetweenPerpendiculars(
    float lengthBetweenPerpendiculars)
{
    if (!qFuzzyCompare(m_lengthBetweenPerpendiculars,
                       lengthBetweenPerpendiculars))
    {
        m_lengthBetweenPerpendiculars =
            lengthBetweenPerpendiculars;
        emit propertiesChanged();
        emit shipChanged();
    }
}

void Ship::setBeam(float beam)
{
    if (!qFuzzyCompare(m_beam, beam))
    {
        m_beam = beam;
        emit propertiesChanged();
        emit shipChanged();
    }
}

void Ship::setDraftAtForward(float draftAtForward)
{
    if (!qFuzzyCompare(m_draftAtForward, draftAtForward))
    {
        m_draftAtForward = draftAtForward;
        emit propertiesChanged();
        emit shipChanged();
    }
}

void Ship::setDraftAtAft(float draftAtAft)
{
    if (!qFuzzyCompare(m_draftAtAft, draftAtAft))
    {
        m_draftAtAft = draftAtAft;
        emit propertiesChanged();
        emit shipChanged();
    }
}

void Ship::setVolumetricDisplacement(
    float volumetricDisplacement)
{
    if (!qFuzzyCompare(m_volumetricDisplacement,
                       volumetricDisplacement))
    {
        m_volumetricDisplacement = volumetricDisplacement;
        emit propertiesChanged();
        emit shipChanged();
    }
}

void Ship::setWettedHullSurface(float wettedHullSurface)
{
    if (!qFuzzyCompare(m_wettedHullSurface,
                       wettedHullSurface))
    {
        m_wettedHullSurface = wettedHullSurface;
        emit propertiesChanged();
        emit shipChanged();
    }
}

void Ship::setAreaAboveWaterline(float areaAboveWaterline)
{
    if (!qFuzzyCompare(m_areaAboveWaterline,
                       areaAboveWaterline))
    {
        m_areaAboveWaterline = areaAboveWaterline;
        emit propertiesChanged();
        emit shipChanged();
    }
}

void Ship::setBulbousBowCenterHeight(
    float bulbousBowCenterHeight)
{
    if (!qFuzzyCompare(m_bulbousBowCenterHeight,
                       bulbousBowCenterHeight))
    {
        m_bulbousBowCenterHeight = bulbousBowCenterHeight;
        emit propertiesChanged();
        emit shipChanged();
    }
}

void Ship::setBulbousBowArea(float bulbousBowArea)
{
    if (!qFuzzyCompare(m_bulbousBowArea, bulbousBowArea))
    {
        m_bulbousBowArea = bulbousBowArea;
        emit propertiesChanged();
        emit shipChanged();
    }
}

void Ship::setImmersedTransomArea(float immersedTransomArea)
{
    if (!qFuzzyCompare(m_immersedTransomArea,
                       immersedTransomArea))
    {
        m_immersedTransomArea = immersedTransomArea;
        emit propertiesChanged();
        emit shipChanged();
    }
}

void Ship::setEntranceAngle(float entranceAngle)
{
    if (!qFuzzyCompare(m_entranceAngle, entranceAngle))
    {
        m_entranceAngle = entranceAngle;
        emit propertiesChanged();
        emit shipChanged();
    }
}

void Ship::setSurfaceRoughness(float surfaceRoughness)
{
    if (!qFuzzyCompare(m_surfaceRoughness,
                       surfaceRoughness))
    {
        m_surfaceRoughness = surfaceRoughness;
        emit propertiesChanged();
        emit shipChanged();
    }
}

void Ship::setBuoyancyCenter(float buoyancyCenter)
{
    if (!qFuzzyCompare(m_buoyancyCenter, buoyancyCenter))
    {
        m_buoyancyCenter = buoyancyCenter;
        emit propertiesChanged();
        emit shipChanged();
    }
}

void Ship::setSternShapeParam(int sternShapeParam)
{
    if (m_sternShapeParam != sternShapeParam)
    {
        m_sternShapeParam = sternShapeParam;
        emit propertiesChanged();
        emit shipChanged();
    }
}

void Ship::setMidshipSectionCoef(float midshipSectionCoef)
{
    if (!qFuzzyCompare(m_midshipSectionCoef,
                       midshipSectionCoef))
    {
        m_midshipSectionCoef = midshipSectionCoef;
        emit propertiesChanged();
        emit shipChanged();
    }
}

void Ship::setWaterplaneAreaCoef(float waterplaneAreaCoef)
{
    if (!qFuzzyCompare(m_waterplaneAreaCoef,
                       waterplaneAreaCoef))
    {
        m_waterplaneAreaCoef = waterplaneAreaCoef;
        emit propertiesChanged();
        emit shipChanged();
    }
}

void Ship::setPrismaticCoef(float prismaticCoef)
{
    if (!qFuzzyCompare(m_prismaticCoef, prismaticCoef))
    {
        m_prismaticCoef = prismaticCoef;
        emit propertiesChanged();
        emit shipChanged();
    }
}

void Ship::setBlockCoef(float blockCoef)
{
    if (!qFuzzyCompare(m_blockCoef, blockCoef))
    {
        m_blockCoef = blockCoef;
        emit propertiesChanged();
        emit shipChanged();
    }
}

void Ship::setTanksDetails(
    const QVector<QMap<QString, float>> &tanksDetails)
{
    if (m_tanksDetails != tanksDetails)
    {
        m_tanksDetails = tanksDetails;
        emit propertiesChanged();
        emit shipChanged();
    }
}

void Ship::setEnginesPerPropeller(int enginesPerPropeller)
{
    if (m_enginesPerPropeller != enginesPerPropeller)
    {
        m_enginesPerPropeller = enginesPerPropeller;
        emit propertiesChanged();
        emit shipChanged();
    }
}

void Ship::setEngineTierII(
    const QVector<QMap<QString, float>> &engineTierII)
{
    if (m_engineTierII != engineTierII)
    {
        m_engineTierII = engineTierII;
        emit propertiesChanged();
        emit shipChanged();
    }
}

void Ship::setEngineTierIII(
    const QVector<QMap<QString, float>> &engineTierIII)
{
    if (m_engineTierIII != engineTierIII)
    {
        m_engineTierIII = engineTierIII;
        emit propertiesChanged();
        emit shipChanged();
    }
}

void Ship::setEngineTierIICurve(
    const QVector<QMap<QString, float>> &engineTierIICurve)
{
    if (m_engineTierIICurve != engineTierIICurve)
    {
        m_engineTierIICurve = engineTierIICurve;
        emit propertiesChanged();
        emit shipChanged();
    }
}

void Ship::setEngineTierIIICurve(
    const QVector<QMap<QString, float>> &engineTierIIICurve)
{
    if (m_engineTierIIICurve != engineTierIIICurve)
    {
        m_engineTierIIICurve = engineTierIIICurve;
        emit propertiesChanged();
        emit shipChanged();
    }
}

void Ship::setGearboxRatio(float gearboxRatio)
{
    if (!qFuzzyCompare(m_gearboxRatio, gearboxRatio))
    {
        m_gearboxRatio = gearboxRatio;
        emit propertiesChanged();
        emit shipChanged();
    }
}

void Ship::setGearboxEfficiency(float gearboxEfficiency)
{
    if (!qFuzzyCompare(m_gearboxEfficiency,
                       gearboxEfficiency))
    {
        m_gearboxEfficiency = gearboxEfficiency;
        emit propertiesChanged();
        emit shipChanged();
    }
}

void Ship::setShaftEfficiency(float shaftEfficiency)
{
    if (!qFuzzyCompare(m_shaftEfficiency, shaftEfficiency))
    {
        m_shaftEfficiency = shaftEfficiency;
        emit propertiesChanged();
        emit shipChanged();
    }
}

void Ship::setPropellerCount(int propellerCount)
{
    if (m_propellerCount != propellerCount)
    {
        m_propellerCount = propellerCount;
        emit propertiesChanged();
        emit shipChanged();
    }
}

void Ship::setPropellerDiameter(float propellerDiameter)
{
    if (!qFuzzyCompare(m_propellerDiameter,
                       propellerDiameter))
    {
        m_propellerDiameter = propellerDiameter;
        emit propertiesChanged();
        emit shipChanged();
    }
}

void Ship::setPropellerPitch(float propellerPitch)
{
    if (!qFuzzyCompare(m_propellerPitch, propellerPitch))
    {
        m_propellerPitch = propellerPitch;
        emit propertiesChanged();
        emit shipChanged();
    }
}

void Ship::setPropellerBladesCount(int propellerBladesCount)
{
    if (m_propellerBladesCount != propellerBladesCount)
    {
        m_propellerBladesCount = propellerBladesCount;
        emit propertiesChanged();
        emit shipChanged();
    }
}

void Ship::setExpandedAreaRatio(float expandedAreaRatio)
{
    if (!qFuzzyCompare(m_expandedAreaRatio,
                       expandedAreaRatio))
    {
        m_expandedAreaRatio = expandedAreaRatio;
        emit propertiesChanged();
        emit shipChanged();
    }
}

void Ship::setStopIfNoEnergy(bool stopIfNoEnergy)
{
    if (m_stopIfNoEnergy != stopIfNoEnergy)
    {
        m_stopIfNoEnergy = stopIfNoEnergy;
        emit propertiesChanged();
        emit shipChanged();
    }
}

void Ship::setMaxRudderAngle(float maxRudderAngle)
{
    if (!qFuzzyCompare(m_maxRudderAngle, maxRudderAngle))
    {
        m_maxRudderAngle = maxRudderAngle;
        emit propertiesChanged();
        emit shipChanged();
    }
}

void Ship::setVesselWeight(float vesselWeight)
{
    if (!qFuzzyCompare(m_vesselWeight, vesselWeight))
    {
        m_vesselWeight = vesselWeight;
        emit propertiesChanged();
        emit shipChanged();
    }
}

void Ship::setCargoWeight(float cargoWeight)
{
    if (!qFuzzyCompare(m_cargoWeight, cargoWeight))
    {
        m_cargoWeight = cargoWeight;
        emit propertiesChanged();
        emit shipChanged();
    }
}

void Ship::setAppendagesWettedSurfaces(
    const QMap<int, float> &appendagesWettedSurfaces)
{
    if (m_appendagesWettedSurfaces
        != appendagesWettedSurfaces)
    {
        m_appendagesWettedSurfaces =
            appendagesWettedSurfaces;
        emit propertiesChanged();
        emit shipChanged();
    }
}

// ShipsReader Implementation
QVector<Ship *>
ShipsReader::readShipsFile(const QString &filePath,
                           QObject       *parent)
{
    qCInfo(lcModel) << "ShipsReader::readShipsFile:" << filePath;
    auto getNanOrValue =
        [](const QMap<QString, QVariant> &params,
           const QString &key, float defaultVal = -1.0f) {
            if (!params.contains(key)
                || params[key].isNull())
            {
                return defaultVal;
            }
            return params[key].toFloat();
        };

    auto getNanOrValueInt =
        [](const QMap<QString, QVariant> &params,
           const QString &key, int defaultVal = -1) {
            if (!params.contains(key)
                || params[key].isNull())
            {
                return defaultVal;
            }
            return params[key].toInt();
        };

    auto getNanOrValueBool =
        [](const QMap<QString, QVariant> &params,
           const QString &key, bool defaultVal = false) {
            if (!params.contains(key)
                || params[key].isNull())
            {
                return defaultVal;
            }
            return params[key].toBool();
        };

    QVector<Ship *> ships;

    if (!QFile::exists(filePath))
    {
        qCWarning(lcModel) << "ShipsReader::readShipsFile:"
                           << "file does not exist:" << filePath;
        return ships;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qCCritical(lcModel) << "ShipsReader::readShipsFile:"
                            << "could not open file:" << filePath;
        return ships;
    }

    QTextStream in(&file);
    while (!in.atEnd())
    {
        QString line = in.readLine();

        // Remove comments and leading/trailing whitespace
        int commentPos = line.indexOf('#');
        if (commentPos >= 0)
        {
            line = line.left(commentPos);
        }
        line = line.trimmed();

        if (line.isEmpty())
        {
            continue; // Skip empty or comment-only lines
        }

        // Parse the line into parameters
        QStringList parts =
            line.split('\t'); // tab-separated values

        if (parts.size() < FILE_ORDERED_PARAMETERS.size())
        {
            // ApplicationLogger::log_error(
            //     QString("Incomplete data in line:
            //     %1").arg(line), ClientType::ShipClient
            // );
            continue;
        }

        try
        {
            // Map the parts to the Ship class parameters
            QMap<QString, QVariant> shipParams =
                parseShipParameters(parts);

            // Create a Ship instance with getNanOrValue for
            // all optional parameters
            Ship *ship = new Ship(
                shipParams["ID"].toString(),
                shipParams["Path"]
                    .value<QVector<QVector<float>>>(),
                shipParams["MaxSpeed"].toFloat(),
                shipParams["WaterlineLength"].toFloat(),
                shipParams["LengthBetweenPerpendiculars"]
                    .toFloat(),
                shipParams["Beam"].toFloat(),
                shipParams["DraftAtForward"].toFloat(),
                shipParams["DraftAtAft"].toFloat(),
                getNanOrValue(
                    shipParams,
                    "VolumetricDisplacement"), // Optional
                getNanOrValue(
                    shipParams,
                    "WettedHullSurface"), // Optional
                shipParams["ShipAndCargoAreaAboveWaterline"]
                    .toFloat(),
                shipParams
                    ["BulbousBowTransverseAreaCenterHeight"]
                        .toFloat(),
                shipParams["BulbousBowTransverseArea"]
                    .toFloat(),
                shipParams["ImmersedTransomArea"].toFloat(),
                getNanOrValue(shipParams,
                              "HalfWaterlineEntranceAngl"
                              "e"), // Optional
                shipParams["SurfaceRoughness"].toFloat(),
                shipParams["LongitudinalBuoyancyCenter"]
                    .toFloat(),
                shipParams["SternShapeParam"].toInt(),
                getNanOrValue(
                    shipParams,
                    "MidshipSectionCoef"), // Optional
                getNanOrValue(
                    shipParams,
                    "WaterplaneAreaCoef"), // Optional
                getNanOrValue(shipParams,
                              "PrismaticCoef"), // Optional
                getNanOrValue(shipParams,
                              "BlockCoef"), // Optional
                shipParams["TanksDetails"]
                    .value<QVector<QMap<QString, float>>>(),
                shipParams["EnginesCountPerPropeller"]
                    .toInt(),
                shipParams["EngineTierIIPropertiesPoints"]
                    .value<QVector<QMap<QString, float>>>(),
                // Optional complex parameters
                shipParams.contains(
                    "EngineTierIIIPropertiesPoints")
                        && !shipParams["EngineTierIIIProper"
                                       "tiesPoints"]
                                .isNull()
                    ? shipParams
                          ["EngineTierIIIPropertiesPoints"]
                              .value<QVector<
                                  QMap<QString, float>>>()
                    : QVector<QMap<QString, float>>(),
                shipParams.contains("EngineTierIICurve")
                        && !shipParams["EngineTierIICurve"]
                                .isNull()
                    ? shipParams["EngineTierIICurve"]
                          .value<QVector<
                              QMap<QString, float>>>()
                    : QVector<QMap<QString, float>>(),
                shipParams.contains("EngineTierIIICurve")
                        && !shipParams["EngineTierIIICurve"]
                                .isNull()
                    ? shipParams["EngineTierIIICurve"]
                          .value<QVector<
                              QMap<QString, float>>>()
                    : QVector<QMap<QString, float>>(),
                shipParams["GearboxRatio"].toFloat(),
                shipParams["GearboxEfficiency"].toFloat(),
                shipParams["ShaftEfficiency"].toFloat(),
                shipParams["PropellerCount"].toInt(),
                shipParams["PropellerDiameter"].toFloat(),
                shipParams["PropellerPitch"].toFloat(),
                shipParams["PropellerBladesCount"].toInt(),
                shipParams["PropellerExpandedAreaRatio"]
                    .toFloat(),
                getNanOrValueBool(shipParams,
                                  "StopIfNoEnergy",
                                  false), // Optional
                getNanOrValue(shipParams,
                              "MaxRudderAngle"), // Optional
                shipParams["VesselWeight"].toFloat(),
                shipParams["CargoWeight"].toFloat(),
                shipParams.contains(
                    "AppendagesWettedSurfaces")
                        && !shipParams
                                ["AppendagesWettedSurfaces"]
                                    .isNull()
                    ? shipParams["AppendagesWettedSurfaces"]
                          .value<QMap<int, float>>()
                    : QMap<int, float>(), // Optional
                parent);

            ships.append(ship);
        }
        catch (const std::exception &e)
        {
            qCWarning(lcModel) << "ShipsReader::readShipsFile:"
                               << "error parsing ship:" << e.what();
            continue;
        }
    }

    file.close();
    qCDebug(lcModel) << "ShipsReader::readShipsFile:"
                     << "parsed" << ships.size() << "ships";
    return ships;
}

QMap<QString, QVariant>
ShipsReader::parseShipParameters(const QStringList &parts)
{
    QMap<QString, QVariant> paramMapping;

    for (int i = 0; i < FILE_ORDERED_PARAMETERS.size(); ++i)
    {
        const auto &paramInfo  = FILE_ORDERED_PARAMETERS[i];
        QString     paramName  = paramInfo.first;
        bool        isOptional = paramInfo.second;

        // Check if the parameter has a value in the parts
        // list
        if (i < parts.size())
        {
            QString paramValue = parts[i];

            // Handle optional parameters that might be
            // empty
            if (paramValue.isEmpty() && isOptional)
            {
                paramMapping[paramName] = QVariant();
            }
            else
            {
                // Parse the parameter based on its type
                if (paramName == "Path")
                {
                    paramMapping[paramName] =
                        QVariant::fromValue(
                            parsePath(paramValue));
                }
                else if (paramName
                         == "EngineTierIIPropertiesPoint"
                            "s")
                {
                    paramMapping[paramName] =
                        QVariant::fromValue(
                            parseEnginePoints(paramValue));
                }
                else if (paramName
                         == "EngineTierIIIPropertiesPoint"
                            "s")
                {
                    QString cleanValue =
                        paramValue.trimmed().toLower();
                    if (cleanValue.contains("na"))
                    {
                        paramMapping[paramName] =
                            QVariant::fromValue(
                                QVector<QMap<QString,
                                             float>>());
                    }
                    else
                    {
                        paramMapping[paramName] =
                            QVariant::fromValue(
                                parseEnginePoints(
                                    paramValue));
                    }
                }
                else if (paramName == "EngineTierIICurve"
                         || paramName
                                == "EngineTierIIICurve")
                {
                    QString cleanValue =
                        paramValue.trimmed().toLower();
                    if (cleanValue.contains("na"))
                    {
                        paramMapping[paramName] =
                            QVariant::fromValue(
                                QVector<QMap<QString,
                                             float>>());
                    }
                    else
                    {
                        paramMapping[paramName] =
                            QVariant::fromValue(
                                parseEnginePoints(
                                    paramValue));
                    }
                }
                else if (paramName
                         == "AppendagesWettedSurfaces")
                {
                    QString cleanValue =
                        paramValue.trimmed().toLower();
                    if (cleanValue.contains("na"))
                    {
                        paramMapping[paramName] =
                            QVariant::fromValue(
                                QMap<int, float>());
                    }
                    else
                    {
                        paramMapping[paramName] =
                            QVariant::fromValue(
                                parseAppendages(
                                    paramValue));
                    }
                }
                else if (paramName == "StopIfNoEnergy")
                {
                    paramMapping[paramName] =
                        QVariant(paramValue.toInt() != 0);
                }
                else if (paramName == "TanksDetails")
                {
                    paramMapping[paramName] =
                        QVariant::fromValue(
                            parseTanksDetails(paramValue));
                }
                else
                {
                    QString cleanValue =
                        paramValue.trimmed()
                            .toLower()
                            .replace(
                                "−",
                                "-"); // Replace Unicode
                                      // minus with ASCII
                                      // minus
                    // Check if paramValue contains "na"
                    // (anywhere in the string)
                    if (cleanValue.contains("na"))
                    {
                        paramMapping[paramName] =
                            QVariant();
                    }
                    else
                    {
                        bool  ok;
                        float numValue =
                            cleanValue.toFloat(&ok);
                        if (!ok)
                        {
                            throw std::runtime_error(
                                QString("Invalid numeric "
                                        "value for %1: %2")
                                    .arg(paramName)
                                    .arg(paramValue)
                                    .toStdString());
                        }
                        paramMapping[paramName] =
                            QVariant(numValue);
                    }
                }
            }
        }
        else
        {
            if (!isOptional)
            {
                throw std::runtime_error(
                    QString(
                        "Missing required parameter: %1")
                        .arg(paramName)
                        .toStdString());
            }
            paramMapping[paramName] =
                QVariant(); // Set to null for optional
                            // parameters without a value
        }
    }

    return paramMapping;
}

QVector<QVector<float>>
ShipsReader::parsePath(const QString &pathString)
{
    QVector<QVector<float>> pathPoints;

    if (pathString.isEmpty()
        || pathString.toLower().contains("na"))
    {
        return pathPoints;
    }

    QStringList pairs = pathString.split(';');
    for (const QString &pair : pairs)
    {
        QStringList keyValue = pair.split(',');
        if (keyValue.size() != 2)
        {
            throw std::runtime_error(
                QString("Malformed coordinate pair: %1")
                    .arg(pair)
                    .toStdString());
        }

        bool  okLon, okLat;
        float lon = keyValue[0].trimmed().toFloat(&okLon);
        float lat = keyValue[1].trimmed().toFloat(&okLat);

        if (!okLon || !okLat)
        {
            throw std::runtime_error(
                QString("Invalid coordinate values: %1")
                    .arg(pair)
                    .toStdString());
        }

        if (qAbs(lon) > 180.0f || qAbs(lat) > 90.0f)
        {
            throw std::runtime_error(
                QString("Invalid WGS84 coordinates: %1")
                    .arg(pair)
                    .toStdString());
        }

        QVector<float> point;
        point.append(lon);
        point.append(lat);
        pathPoints.append(point);
    }

    return pathPoints;
}

QVector<QMap<QString, float>>
ShipsReader::parseEnginePoints(
    const QString &enginePointsStr)
{
    QVector<QMap<QString, float>> engineProperties;

    if (enginePointsStr.toLower().contains("na"))
    {
        return engineProperties;
    }

    QStringList pointsData = enginePointsStr.split(';');
    for (const QString &pointData : pointsData)
    {
        QStringList values = pointData.split(',');
        if (values.size() != 3)
        {
            throw std::runtime_error(
                QString("Malformed Engine Property: "
                        "%1\nEngine Power-RPM-Efficiency "
                        "Mapping must "
                        "have 3 values representing Brake "
                        "Power, "
                        "RPM, Efficiency!")
                    .arg(pointData)
                    .toStdString());
        }

        bool  okPower, okRPM, okEff;
        float power = values[0].trimmed().toFloat(&okPower);
        float rpm   = values[1].trimmed().toFloat(&okRPM);
        float efficiency =
            values[2].trimmed().toFloat(&okEff);

        if (!okPower || !okRPM || !okEff)
        {
            throw std::runtime_error(
                QString("Invalid engine point values: %1")
                    .arg(pointData)
                    .toStdString());
        }

        QMap<QString, float> point;
        point["Power"]      = power;
        point["RPM"]        = rpm;
        point["Efficiency"] = efficiency;
        engineProperties.append(point);
    }

    return engineProperties;
}

QMap<int, float>
ShipsReader::parseAppendages(const QString &appendagesStr)
{
    QMap<int, float> appendages;

    if (appendagesStr.isEmpty()
        || appendagesStr.toLower().contains("na"))
    {
        return appendages;
    }

    QStringList pairs = appendagesStr.split(';');
    for (const QString &pair : pairs)
    {
        QStringList keyValue = pair.split(',');
        if (keyValue.size() == 2)
        {
            bool okType, okSurface;
            int  appendageType =
                keyValue[0].trimmed().toInt(&okType);
            float wettedSurface =
                keyValue[1].trimmed().toFloat(&okSurface);

            if (!okType || !okSurface)
            {
                throw std::runtime_error(
                    QString("Invalid appendage values: %1")
                        .arg(pair)
                        .toStdString());
            }

            appendages[appendageType] = wettedSurface;
        }
        else
        {
            throw std::runtime_error(
                QString("Malformed appendage entry: %1")
                    .arg(pair)
                    .toStdString());
        }
    }

    return appendages;
}

QVector<QMap<QString, float>>
ShipsReader::parseTanksDetails(const QString &tanksStr)
{
    QVector<QMap<QString, float>> tanks;

    if (tanksStr.isEmpty())
    {
        return tanks;
    }

    QStringList tanksList = tanksStr.split(';');
    for (const QString &tankStr : tanksList)
    {
        QStringList values = tankStr.split(',');
        if (values.size() != 4)
        {
            throw std::runtime_error(
                QString("Malformed tank details: %1")
                    .arg(tankStr)
                    .toStdString());
        }

        bool  ok1, ok2, ok3, ok4;
        float fuelType         = values[0].toFloat(&ok1);
        float maxCapacity      = values[1].toFloat(&ok2);
        float initialCapacity  = values[2].toFloat(&ok3);
        float depthOfDischarge = values[3].toFloat(&ok4);

        if (!ok1 || !ok2 || !ok3 || !ok4)
        {
            throw std::runtime_error(
                QString("Invalid tank detail values: %1")
                    .arg(tankStr)
                    .toStdString());
        }

        QMap<QString, float> tank;
        tank["FuelType"]    = fuelType;
        tank["MaxCapacity"] = maxCapacity;
        tank["TankInitialCapacityPercentage"] =
            initialCapacity;
        tank["TankDepthOfDischage"] = depthOfDischarge;
        tanks.append(tank);
    }

    return tanks;
}

bool ShipsReader::containsNa(const QVariant &value)
{
    if (value.typeId() == QMetaType::QString)
    {
        return value.toString().toLower().contains("na");
    }

    if (value.typeId() == QMetaType::QVariantList)
    {
        const QVariantList list = value.toList();
        for (const QVariant &item : list)
        {
            if (containsNa(item))
            {
                return true;
            }
        }
    }

    return false;
}

} // namespace Backend
} // namespace CargoNetSim
