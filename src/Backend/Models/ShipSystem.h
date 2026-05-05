/**
 * @file ship_system.h
 * @brief Defines ship-related classes for the CargoNetSim
 *        system.
 * @author Ahmed Aredah
 * @date 2025-03-19
 * @copyright Copyright (c) 2025
 */

#pragma once

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QString>
#include <QTextStream>
#include <QVector>
#include <QtMath>

#include "Backend/Commons/Units.h"
#include "../Commons/ClientType.h"

namespace CargoNetSim
{
namespace Backend
{

/**
 * @class Ship
 * @brief Represents a maritime vessel in the simulation.
 *
 * This class models a ship with its physical properties,
 * performance characteristics, and path information.
 */
class Ship : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Default constructor.
     * @param parent The parent QObject.
     */
    Ship(QObject *parent = nullptr);

    /**
     * @brief Detailed constructor with all ship parameters.
     * @param shipId Unique identifier for the ship.
     * @param pathCoordinates Vector of coordinate points
     *        defining the ship's path.
     * @param maxSpeed Maximum speed of the ship in knots.
     * @param waterlineLength Length at the waterline in
     *        meters.
     * @param lengthBetweenPerpendiculars Length between
     *        perpendiculars in meters.
     * @param beam Width of the ship in meters.
     * @param draftAtForward Draft at the bow in meters.
     * @param draftAtAft Draft at the stern in meters.
     * @param volumetricDisplacement Volume of water
     *        displaced in cubic meters.
     * @param wettedHullSurface Area of hull below
     *        waterline in square meters.
     * @param areaAboveWaterline Area above waterline in
     *        square meters.
     * @param bulbousBowCenterHeight Height of bulbous bow
     *        center in meters.
     * @param bulbousBowArea Cross-sectional area of
     *        bulbous bow in square meters.
     * @param immersedTransomArea Area of immersed transom
     *        in square meters.
     * @param entranceAngle Angle of entrance at bow in
     *        degrees.
     * @param surfaceRoughness Hull surface roughness in
     *        millimeters.
     * @param buoyancyCenter Longitudinal center of
     *        buoyancy.
     * @param sternShapeParam Stern shape parameter
     *        (integer code).
     * @param midshipSectionCoef Midship section
     *        coefficient.
     * @param waterplaneAreaCoef Waterplane area
     *        coefficient.
     * @param prismaticCoef Prismatic coefficient.
     * @param blockCoef Block coefficient.
     * @param tanksDetails Vector of maps containing tank
     *        details.
     * @param enginesPerPropeller Number of engines per
     *        propeller.
     * @param engineTierII Tier II engine specifications.
     * @param engineTierIII Tier III engine specifications.
     * @param engineTierIICurve Tier II engine performance
     *        curve.
     * @param engineTierIIICurve Tier III engine
     *        performance curve.
     * @param gearboxRatio Gearbox ratio.
     * @param gearboxEfficiency Gearbox efficiency factor.
     * @param shaftEfficiency Shaft efficiency factor.
     * @param propellerCount Number of propellers.
     * @param propellerDiameter Diameter of propeller in
     *        meters.
     * @param propellerPitch Pitch of propeller in meters.
     * @param propellerBladesCount Number of blades per
     *        propeller.
     * @param expandedAreaRatio Expanded area ratio of
     *        propeller.
     * @param stopIfNoEnergy Whether to stop if no energy
     *        remains.
     * @param maxRudderAngle Maximum rudder angle in
     *        degrees.
     * @param vesselWeight Weight of the vessel in tons.
     * @param cargoWeight Weight of cargo in tons.
     * @param appendagesWettedSurfaces Map of appendage
     *        wetted surface areas.
     * @param parent The parent QObject.
     */
    Ship(
        const QString                 &shipId,
        const QVector<QVector<float>> &pathCoordinates,
        float maxSpeed, float waterlineLength,
        float lengthBetweenPerpendiculars, float beam,
        float draftAtForward, float draftAtAft,
        float volumetricDisplacement = -1.0f,
        float wettedHullSurface      = -1.0f,
        float areaAboveWaterline     = 0.0f,
        float bulbousBowCenterHeight = 0.0f,
        float bulbousBowArea         = 0.0f,
        float immersedTransomArea    = 0.0f,
        float entranceAngle          = -1.0f,
        float surfaceRoughness       = 0.0f,
        float buoyancyCenter         = 0.0f,
        int   sternShapeParam        = -1,
        float midshipSectionCoef     = -1.0f,
        float waterplaneAreaCoef     = -1.0f,
        float prismaticCoef          = -1.0f,
        float blockCoef              = -1.0f,
        const QVector<QMap<QString, float>> &tanksDetails =
            QVector<QMap<QString, float>>(),
        int enginesPerPropeller = 1,
        const QVector<QMap<QString, float>> &engineTierII =
            QVector<QMap<QString, float>>(),
        const QVector<QMap<QString, float>> &engineTierIII =
            QVector<QMap<QString, float>>(),
        const QVector<QMap<QString, float>>
            &engineTierIICurve =
                QVector<QMap<QString, float>>(),
        const QVector<QMap<QString, float>>
            &engineTierIIICurve =
                QVector<QMap<QString, float>>(),
        float gearboxRatio         = 0.0f,
        float gearboxEfficiency    = 1.0f,
        float shaftEfficiency      = 1.0f,
        int   propellerCount       = 1,
        float propellerDiameter    = 0.0f,
        float propellerPitch       = 0.0f,
        int   propellerBladesCount = 4,
        float expandedAreaRatio    = 0.0f,
        bool  stopIfNoEnergy       = false,
        float maxRudderAngle       = -1.0f,
        float vesselWeight = 0.0f, float cargoWeight = 0.0f,
        const QMap<int, float> &appendagesWettedSurfaces =
            QMap<int, float>(),
        QObject *parent = nullptr);

    /**
     * @brief Constructor from JSON object.
     * @param json JSON object containing ship parameters.
     * @param parent The parent QObject.
     */
    Ship(const QJsonObject &json,
         QObject           *parent = nullptr);

    /**
     * @brief Copy constructor for Ship class.
     *
     * Creates a new Ship with the same data as the source
     * Ship, but as a separate QObject instance.
     *
     * @param other The Ship to copy from.
     */
    Ship(const Ship &other)
        : QObject(nullptr)
    {
        *this = other; // Use the assignment operator
    }

    /**
     * @brief Copy assignment operator for Ship class.
     *
     * This only copies the data fields of the Ship, not
     * QObject properties like object name, parent/child
     * relationships, or signal/slot connections.
     *
     * @param other The Ship to copy from.
     * @return Reference to this Ship after copying.
     */
    Ship &operator=(const Ship &other)
    {
        if (this != &other)
        { // Check for self-assignment
            // Don't copy QObject properties, only copy the
            // Ship's data fields
            m_shipId          = other.m_shipId;
            m_pathCoordinates = other.m_pathCoordinates;
            m_maxSpeed        = other.m_maxSpeed;
            m_waterlineLength = other.m_waterlineLength;
            m_lengthBetweenPerpendiculars =
                other.m_lengthBetweenPerpendiculars;
            m_beam           = other.m_beam;
            m_draftAtForward = other.m_draftAtForward;
            m_draftAtAft     = other.m_draftAtAft;
            m_volumetricDisplacement =
                other.m_volumetricDisplacement;
            m_wettedHullSurface = other.m_wettedHullSurface;
            m_areaAboveWaterline =
                other.m_areaAboveWaterline;
            m_bulbousBowCenterHeight =
                other.m_bulbousBowCenterHeight;
            m_bulbousBowArea = other.m_bulbousBowArea;
            m_immersedTransomArea =
                other.m_immersedTransomArea;
            m_entranceAngle    = other.m_entranceAngle;
            m_surfaceRoughness = other.m_surfaceRoughness;
            m_buoyancyCenter   = other.m_buoyancyCenter;
            m_sternShapeParam  = other.m_sternShapeParam;
            m_midshipSectionCoef =
                other.m_midshipSectionCoef;
            m_waterplaneAreaCoef =
                other.m_waterplaneAreaCoef;
            m_prismaticCoef = other.m_prismaticCoef;
            m_blockCoef     = other.m_blockCoef;
            m_tanksDetails  = other.m_tanksDetails;
            m_enginesPerPropeller =
                other.m_enginesPerPropeller;
            m_engineTierII      = other.m_engineTierII;
            m_engineTierIII     = other.m_engineTierIII;
            m_engineTierIICurve = other.m_engineTierIICurve;
            m_engineTierIIICurve =
                other.m_engineTierIIICurve;
            m_gearboxRatio      = other.m_gearboxRatio;
            m_gearboxEfficiency = other.m_gearboxEfficiency;
            m_shaftEfficiency   = other.m_shaftEfficiency;
            m_propellerCount    = other.m_propellerCount;
            m_propellerDiameter = other.m_propellerDiameter;
            m_propellerPitch    = other.m_propellerPitch;
            m_propellerBladesCount =
                other.m_propellerBladesCount;
            m_expandedAreaRatio = other.m_expandedAreaRatio;
            m_stopIfNoEnergy    = other.m_stopIfNoEnergy;
            m_maxRudderAngle    = other.m_maxRudderAngle;
            m_vesselWeight      = other.m_vesselWeight;
            m_cargoWeight       = other.m_cargoWeight;
            m_appendagesWettedSurfaces =
                other.m_appendagesWettedSurfaces;

            // Emit signals to notify that ship data has
            // changed
            emit shipChanged();
            emit pathChanged();
            emit propertiesChanged();
        }
        return *this;
    }

    /**
     * @brief Serialize the ship to a JSON object.
     * @return JSON representation of the ship.
     */
    QJsonObject toJson() const;

    /**
     * @brief Serialize the ship to a dictionary-like JSON.
     * @return Dictionary representation of the ship.
     */
    QJsonObject toDict() const;

    /**
     * @brief Create a deep copy of this ship.
     * @return Pointer to a new Ship instance with the same
     *         properties.
     */
    Ship *copy() const;

    // Getters
    /**
     * @brief Get the ship's unique identifier.
     * @return The ship ID.
     */
    QString getUserId() const
    {
        return m_shipId;
    }

    /**
     * @brief Get the ship's path coordinates.
     * @return Vector of coordinate points.
     */
    QVector<QVector<float>> getPathCoordinates() const
    {
        return m_pathCoordinates;
    }

    /**
     * @brief Get the ship's maximum speed.
     * @return Maximum speed in knots.
     */
    float getMaxSpeed() const
    {
        return m_maxSpeed;
    }

    Units::SpeedKnots maxSpeedUnits() const
    {
        return Units::knots(m_maxSpeed);
    }

    /**
     * @brief Get the ship's waterline length.
     * @return Waterline length in meters.
     */
    float getWaterlineLength() const
    {
        return m_waterlineLength;
    }

    Units::LengthMeters waterlineLengthUnits() const
    {
        return Units::meters(m_waterlineLength);
    }

    /**
     * @brief Get the ship's length between perpendiculars.
     * @return Length between perpendiculars in meters.
     */
    float getLengthBetweenPerpendiculars() const
    {
        return m_lengthBetweenPerpendiculars;
    }

    Units::LengthMeters lengthBetweenPerpendicularsUnits() const
    {
        return Units::meters(m_lengthBetweenPerpendiculars);
    }

    /**
     * @brief Get the ship's beam (width).
     * @return Beam in meters.
     */
    float getBeam() const
    {
        return m_beam;
    }

    Units::LengthMeters beamUnits() const
    {
        return Units::meters(m_beam);
    }

    /**
     * @brief Get the ship's draft at the forward.
     * @return Draft at forward in meters.
     */
    float getDraftAtForward() const
    {
        return m_draftAtForward;
    }

    Units::LengthMeters draftAtForwardUnits() const
    {
        return Units::meters(m_draftAtForward);
    }

    /**
     * @brief Get the ship's draft at the aft.
     * @return Draft at aft in meters.
     */
    float getDraftAtAft() const
    {
        return m_draftAtAft;
    }

    Units::LengthMeters draftAtAftUnits() const
    {
        return Units::meters(m_draftAtAft);
    }

    /**
     * @brief Get the ship's volumetric displacement.
     * @return Volumetric displacement in cubic meters.
     */
    float getVolumetricDisplacement() const
    {
        return m_volumetricDisplacement;
    }

    Units::VolumeCubicMeters volumetricDisplacementUnits() const
    {
        return Units::cubicMeters(m_volumetricDisplacement);
    }

    /**
     * @brief Get the ship's wetted hull surface area.
     * @return Wetted hull surface in square meters.
     */
    float getWettedHullSurface() const
    {
        return m_wettedHullSurface;
    }

    Units::AreaSquareMeters wettedHullSurfaceUnits() const
    {
        return Units::squareMeters(m_wettedHullSurface);
    }

    /**
     * @brief Get the ship's area above waterline.
     * @return Area above waterline in square meters.
     */
    float getAreaAboveWaterline() const
    {
        return m_areaAboveWaterline;
    }

    Units::AreaSquareMeters areaAboveWaterlineUnits() const
    {
        return Units::squareMeters(m_areaAboveWaterline);
    }

    /**
     * @brief Get the height of bulbous bow center.
     * @return Bulbous bow center height in meters.
     */
    float getBulbousBowCenterHeight() const
    {
        return m_bulbousBowCenterHeight;
    }

    /**
     * @brief Get the bulbous bow area.
     * @return Bulbous bow area in square meters.
     */
    float getBulbousBowArea() const
    {
        return m_bulbousBowArea;
    }

    /**
     * @brief Get the immersed transom area.
     * @return Immersed transom area in square meters.
     */
    float getImmersedTransomArea() const
    {
        return m_immersedTransomArea;
    }

    /**
     * @brief Get the entrance angle.
     * @return Entrance angle in degrees.
     */
    float getEntranceAngle() const
    {
        return m_entranceAngle;
    }

    /**
     * @brief Get the hull surface roughness.
     * @return Surface roughness in millimeters.
     */
    float getSurfaceRoughness() const
    {
        return m_surfaceRoughness;
    }

    Units::LengthMillimeters surfaceRoughnessUnits() const
    {
        return Units::millimeters(m_surfaceRoughness);
    }

    /**
     * @brief Get the buoyancy center.
     * @return Longitudinal center of buoyancy.
     */
    float getBuoyancyCenter() const
    {
        return m_buoyancyCenter;
    }

    /**
     * @brief Get the stern shape parameter.
     * @return Stern shape parameter code.
     */
    int getSternShapeParam() const
    {
        return m_sternShapeParam;
    }

    /**
     * @brief Get the midship section coefficient.
     * @return Midship section coefficient.
     */
    float getMidshipSectionCoef() const
    {
        return m_midshipSectionCoef;
    }

    /**
     * @brief Get the waterplane area coefficient.
     * @return Waterplane area coefficient.
     */
    float getWaterplaneAreaCoef() const
    {
        return m_waterplaneAreaCoef;
    }

    /**
     * @brief Get the prismatic coefficient.
     * @return Prismatic coefficient.
     */
    float getPrismaticCoef() const
    {
        return m_prismaticCoef;
    }

    /**
     * @brief Get the block coefficient.
     * @return Block coefficient.
     */
    float getBlockCoef() const
    {
        return m_blockCoef;
    }

    /**
     * @brief Get the tank details.
     * @return Vector of maps with tank specifications.
     */
    QVector<QMap<QString, float>> getTanksDetails() const
    {
        return m_tanksDetails;
    }

    /**
     * @brief Get the number of engines per propeller.
     * @return Number of engines per propeller.
     */
    int getEnginesPerPropellerCount() const
    {
        return m_enginesPerPropeller;
    }

    /**
     * @brief Get Tier II engine specifications.
     * @return Vector of Tier II engine specification maps.
     */
    QVector<QMap<QString, float>>
    getEngineTierIISpecs() const
    {
        return m_engineTierII;
    }

    /**
     * @brief Get Tier III engine specifications.
     * @return Vector of Tier III engine specification maps.
     */
    QVector<QMap<QString, float>>
    getEngineTierIIISpecs() const
    {
        return m_engineTierIII;
    }

    /**
     * @brief Get Tier II engine performance curve.
     * @return Vector of Tier II curve point maps.
     */
    QVector<QMap<QString, float>>
    getEngineTierIIPerformanceCurve() const
    {
        return m_engineTierIICurve;
    }

    /**
     * @brief Get Tier III engine performance curve.
     * @return Vector of Tier III curve point maps.
     */
    QVector<QMap<QString, float>>
    getEngineTierIIIPerformanceCurve() const
    {
        return m_engineTierIIICurve;
    }

    /**
     * @brief Get the gearbox ratio.
     * @return Gearbox ratio.
     */
    float getGearboxRatio() const
    {
        return m_gearboxRatio;
    }

    /**
     * @brief Get the gearbox efficiency.
     * @return Gearbox efficiency factor.
     */
    float getGearboxEfficiency() const
    {
        return m_gearboxEfficiency;
    }

    /**
     * @brief Get the shaft efficiency.
     * @return Shaft efficiency factor.
     */
    float getShaftEfficiency() const
    {
        return m_shaftEfficiency;
    }

    /**
     * @brief Get the number of propellers.
     * @return Propeller count.
     */
    int getPropellerCount() const
    {
        return m_propellerCount;
    }

    /**
     * @brief Get the propeller diameter.
     * @return Propeller diameter in meters.
     */
    float getPropellerDiameter() const
    {
        return m_propellerDiameter;
    }

    Units::LengthMeters propellerDiameterUnits() const
    {
        return Units::meters(m_propellerDiameter);
    }

    /**
     * @brief Get the propeller pitch.
     * @return Propeller pitch in meters.
     */
    float getPropellerPitch() const
    {
        return m_propellerPitch;
    }

    Units::LengthMeters propellerPitchUnits() const
    {
        return Units::meters(m_propellerPitch);
    }

    /**
     * @brief Get the number of blades per propeller.
     * @return Propeller blades count.
     */
    int getPropellerBladesCount() const
    {
        return m_propellerBladesCount;
    }

    /**
     * @brief Get the expanded area ratio of propeller.
     * @return Expanded area ratio.
     */
    float getExpandedAreaRatio() const
    {
        return m_expandedAreaRatio;
    }

    /**
     * @brief Get whether the ship stops if no energy.
     * @return True if the ship stops when energy depleted.
     */
    bool shouldStopIfNoEnergy() const
    {
        return m_stopIfNoEnergy;
    }

    /**
     * @brief Get the maximum rudder angle.
     * @return Maximum rudder angle in degrees.
     */
    float getMaxRudderAngle() const
    {
        return m_maxRudderAngle;
    }

    Units::AngleDegrees maxRudderAngleUnits() const
    {
        return Units::degrees(m_maxRudderAngle);
    }

    /**
     * @brief Get the vessel weight.
     * @return Vessel weight in tons.
     */
    float getVesselWeight() const
    {
        return m_vesselWeight;
    }

    Units::MassMetricTons vesselWeightUnits() const
    {
        return Units::metricTons(m_vesselWeight);
    }

    /**
     * @brief Get the cargo weight.
     * @return Cargo weight in tons.
     */
    float getCargoWeight() const
    {
        return m_cargoWeight;
    }

    Units::MassMetricTons cargoWeightUnits() const
    {
        return Units::metricTons(m_cargoWeight);
    }

    /**
     * @brief Get the appendages wetted surfaces.
     * @return Map of appendage ID to wetted surface area.
     */
    QMap<int, float> getAppendagesWettedSurfaces() const
    {
        return m_appendagesWettedSurfaces;
    }

    // Setters
    /**
     * @brief Set the ship's unique identifier.
     * @param shipId The new ship ID.
     */
    void setUserId(const QString &shipId);

    /**
     * @brief Set the ship's path coordinates.
     * @param pathCoordinates New vector of coordinates.
     */
    void setPathCoordinates(
        const QVector<QPointF> &pathCoordinates);

    /**
     * @brief Set the ship's maximum speed.
     * @param maxSpeed New maximum speed in knots.
     */
    void setMaxSpeed(float maxSpeed);

    void setMaxSpeedUnits(Units::SpeedKnots maxSpeed)
    {
        setMaxSpeed(static_cast<float>(maxSpeed.value()));
    }

    /**
     * @brief Set the ship's waterline length.
     * @param waterlineLength New waterline length in
     * meters.
     */
    void setWaterlineLength(float waterlineLength);

    void setWaterlineLengthUnits(
        Units::LengthMeters waterlineLength)
    {
        setWaterlineLength(
            static_cast<float>(waterlineLength.value()));
    }

    /**
     * @brief Set the ship's length between perpendiculars.
     * @param lengthBetweenPerpendiculars New length in
     *        meters.
     */
    void setLengthBetweenPerpendiculars(
        float lengthBetweenPerpendiculars);

    /**
     * @brief Set the ship's beam (width).
     * @param beam New beam in meters.
     */
    void setBeam(float beam);

    void setBeamUnits(Units::LengthMeters beam)
    {
        setBeam(static_cast<float>(beam.value()));
    }

    /**
     * @brief Set the ship's draft at the forward.
     * @param draftAtForward New draft at forward in meters.
     */
    void setDraftAtForward(float draftAtForward);

    void setDraftAtForwardUnits(
        Units::LengthMeters draftAtForward)
    {
        setDraftAtForward(
            static_cast<float>(draftAtForward.value()));
    }

    /**
     * @brief Set the ship's draft at the aft.
     * @param draftAtAft New draft at aft in meters.
     */
    void setDraftAtAft(float draftAtAft);

    void setDraftAtAftUnits(Units::LengthMeters draftAtAft)
    {
        setDraftAtAft(
            static_cast<float>(draftAtAft.value()));
    }

    /**
     * @brief Set the ship's volumetric displacement.
     * @param volumetricDisplacement New displacement in
     *        cubic meters.
     */
    void
    setVolumetricDisplacement(float volumetricDisplacement);

    /**
     * @brief Set the ship's wetted hull surface area.
     * @param wettedHullSurface New wetted hull surface in
     *        square meters.
     */
    void setWettedHullSurface(float wettedHullSurface);

    /**
     * @brief Set the ship's area above waterline.
     * @param areaAboveWaterline New area in square meters.
     */
    void setAreaAboveWaterline(float areaAboveWaterline);

    /**
     * @brief Set the height of bulbous bow center.
     * @param bulbousBowCenterHeight New height in meters.
     */
    void
    setBulbousBowCenterHeight(float bulbousBowCenterHeight);

    /**
     * @brief Set the bulbous bow area.
     * @param bulbousBowArea New area in square meters.
     */
    void setBulbousBowArea(float bulbousBowArea);

    /**
     * @brief Set the immersed transom area.
     * @param immersedTransomArea New area in square meters.
     */
    void setImmersedTransomArea(float immersedTransomArea);

    /**
     * @brief Set the entrance angle.
     * @param entranceAngle New angle in degrees.
     */
    void setEntranceAngle(float entranceAngle);

    /**
     * @brief Set the hull surface roughness.
     * @param surfaceRoughness New roughness in millimeters.
     */
    void setSurfaceRoughness(float surfaceRoughness);

    /**
     * @brief Set the buoyancy center.
     * @param buoyancyCenter New center of buoyancy.
     */
    void setBuoyancyCenter(float buoyancyCenter);

    /**
     * @brief Set the stern shape parameter.
     * @param sternShapeParam New stern shape parameter
     * code.
     */
    void setSternShapeParam(int sternShapeParam);

    /**
     * @brief Set the midship section coefficient.
     * @param midshipSectionCoef New coefficient value.
     */
    void setMidshipSectionCoef(float midshipSectionCoef);

    /**
     * @brief Set the waterplane area coefficient.
     * @param waterplaneAreaCoef New coefficient value.
     */
    void setWaterplaneAreaCoef(float waterplaneAreaCoef);

    /**
     * @brief Set the prismatic coefficient.
     * @param prismaticCoef New coefficient value.
     */
    void setPrismaticCoef(float prismaticCoef);

    /**
     * @brief Set the block coefficient.
     * @param blockCoef New coefficient value.
     */
    void setBlockCoef(float blockCoef);

    /**
     * @brief Set the tank details.
     * @param tanksDetails New vector of tank
     * specifications.
     */
    void setTanksDetails(
        const QVector<QMap<QString, float>> &tanksDetails);

    /**
     * @brief Set the number of engines per propeller.
     * @param enginesPerPropeller New engines count.
     */
    void setEnginesPerPropeller(int enginesPerPropeller);

    /**
     * @brief Set the Tier II engine specifications.
     * @param engineTierII New vector of specifications.
     */
    void setEngineTierII(
        const QVector<QMap<QString, float>> &engineTierII);

    /**
     * @brief Set the Tier III engine specifications.
     * @param engineTierIII New vector of specifications.
     */
    void setEngineTierIII(
        const QVector<QMap<QString, float>> &engineTierIII);

    /**
     * @brief Set the Tier II engine performance curve.
     * @param engineTierIICurve New vector of curve points.
     */
    void
    setEngineTierIICurve(const QVector<QMap<QString, float>>
                             &engineTierIICurve);

    /**
     * @brief Set the Tier III engine performance curve.
     * @param engineTierIIICurve New vector of curve points.
     */
    void setEngineTierIIICurve(
        const QVector<QMap<QString, float>>
            &engineTierIIICurve);

    /**
     * @brief Set the gearbox ratio.
     * @param gearboxRatio New gearbox ratio.
     */
    void setGearboxRatio(float gearboxRatio);

    /**
     * @brief Set the gearbox efficiency.
     * @param gearboxEfficiency New efficiency factor.
     */
    void setGearboxEfficiency(float gearboxEfficiency);

    /**
     * @brief Set the shaft efficiency.
     * @param shaftEfficiency New efficiency factor.
     */
    void setShaftEfficiency(float shaftEfficiency);

    /**
     * @brief Set the number of propellers.
     * @param propellerCount New propeller count.
     */
    void setPropellerCount(int propellerCount);

    /**
     * @brief Set the propeller diameter.
     * @param propellerDiameter New diameter in meters.
     */
    void setPropellerDiameter(float propellerDiameter);

    void setPropellerDiameterUnits(
        Units::LengthMeters propellerDiameter)
    {
        setPropellerDiameter(
            static_cast<float>(propellerDiameter.value()));
    }

    /**
     * @brief Set the propeller pitch.
     * @param propellerPitch New pitch in meters.
     */
    void setPropellerPitch(float propellerPitch);

    void setPropellerPitchUnits(
        Units::LengthMeters propellerPitch)
    {
        setPropellerPitch(
            static_cast<float>(propellerPitch.value()));
    }

    /**
     * @brief Set the number of blades per propeller.
     * @param propellerBladesCount New blades count.
     */
    void setPropellerBladesCount(int propellerBladesCount);

    /**
     * @brief Set the expanded area ratio of propeller.
     * @param expandedAreaRatio New area ratio.
     */
    void setExpandedAreaRatio(float expandedAreaRatio);

    /**
     * @brief Set whether the ship stops if no energy.
     * @param stopIfNoEnergy New stop condition flag.
     */
    void setStopIfNoEnergy(bool stopIfNoEnergy);

    /**
     * @brief Set the maximum rudder angle.
     * @param maxRudderAngle New angle in degrees.
     */
    void setMaxRudderAngle(float maxRudderAngle);

    void setMaxRudderAngleUnits(
        Units::AngleDegrees maxRudderAngle)
    {
        setMaxRudderAngle(
            static_cast<float>(maxRudderAngle.value()));
    }

    /**
     * @brief Set the vessel weight.
     * @param vesselWeight New weight in tons.
     */
    void setVesselWeight(float vesselWeight);

    void setVesselWeightUnits(
        Units::MassMetricTons vesselWeight)
    {
        setVesselWeight(
            static_cast<float>(vesselWeight.value()));
    }

    /**
     * @brief Set the cargo weight.
     * @param cargoWeight New weight in tons.
     */
    void setCargoWeight(float cargoWeight);

    void setCargoWeightUnits(Units::MassMetricTons cargoWeight)
    {
        setCargoWeight(
            static_cast<float>(cargoWeight.value()));
    }

    /**
     * @brief Set the appendages wetted surfaces.
     * @param appendagesWettedSurfaces New map of appendage
     *        areas.
     */
    void setAppendagesWettedSurfaces(
        const QMap<int, float> &appendagesWettedSurfaces);

    /**
     * @brief Create a Ship instance from dictionary data.
     * @param data JSON object containing ship parameters.
     * @param parent The parent QObject.
     * @return Pointer to a new Ship instance.
     */
    static Ship *fromDict(const QJsonObject &data,
                          QObject *parent = nullptr);

signals:
    /**
     * @brief Signal emitted when any ship property changes.
     */
    void shipChanged();

    /**
     * @brief Signal emitted when the ship's path changes.
     */
    void pathChanged();

    /**
     * @brief Signal emitted when ship properties change.
     */
    void propertiesChanged();

private:
    // Add a constant to identify NaN sentinel values
    static constexpr float NAN_SENTINEL_VALUE = -1.0f;

    /** @brief Unique identifier for the ship */
    QString m_shipId;

    /** @brief Vector of coordinate points defining the
     * ship's path */
    QVector<QVector<float>> m_pathCoordinates;

    /** @brief Maximum speed of the ship in knots */
    float m_maxSpeed;

    /** @brief Length at the waterline in meters */
    float m_waterlineLength;

    /** @brief Length between perpendiculars in meters */
    float m_lengthBetweenPerpendiculars;

    /** @brief Width of the ship in meters */
    float m_beam;

    /** @brief Draft at the bow in meters */
    float m_draftAtForward;

    /** @brief Draft at the stern in meters */
    float m_draftAtAft;

    /** @brief Volume of water displaced in cubic meters */
    float m_volumetricDisplacement;

    /** @brief Area of hull below waterline in square meters
     */
    float m_wettedHullSurface;

    /** @brief Area above waterline in square meters */
    float m_areaAboveWaterline;

    /** @brief Height of bulbous bow center in meters */
    float m_bulbousBowCenterHeight;

    /** @brief Cross-sectional area of bulbous bow in square
     * meters */
    float m_bulbousBowArea;

    /** @brief Area of immersed transom in square meters */
    float m_immersedTransomArea;

    /** @brief Angle of entrance at bow in degrees */
    float m_entranceAngle;

    /** @brief Hull surface roughness in millimeters */
    float m_surfaceRoughness;

    /** @brief Longitudinal center of buoyancy */
    float m_buoyancyCenter;

    /** @brief Stern shape parameter (integer code) */
    int m_sternShapeParam;

    /** @brief Midship section coefficient */
    float m_midshipSectionCoef;

    /** @brief Waterplane area coefficient */
    float m_waterplaneAreaCoef;

    /** @brief Prismatic coefficient */
    float m_prismaticCoef;

    /** @brief Block coefficient */
    float m_blockCoef;

    /** @brief Vector of maps containing tank details */
    QVector<QMap<QString, float>> m_tanksDetails;

    /** @brief Number of engines per propeller */
    int m_enginesPerPropeller;

    /** @brief Tier II engine specifications */
    QVector<QMap<QString, float>> m_engineTierII;

    /** @brief Tier III engine specifications */
    QVector<QMap<QString, float>> m_engineTierIII;

    /** @brief Tier II engine performance curve */
    QVector<QMap<QString, float>> m_engineTierIICurve;

    /** @brief Tier III engine performance curve */
    QVector<QMap<QString, float>> m_engineTierIIICurve;

    /** @brief Gearbox ratio */
    float m_gearboxRatio;

    /** @brief Gearbox efficiency factor */
    float m_gearboxEfficiency;

    /** @brief Shaft efficiency factor */
    float m_shaftEfficiency;

    /** @brief Number of propellers */
    int m_propellerCount;

    /** @brief Diameter of propeller in meters */
    float m_propellerDiameter;

    /** @brief Pitch of propeller in meters */
    float m_propellerPitch;

    /** @brief Number of blades per propeller */
    int m_propellerBladesCount;

    /** @brief Expanded area ratio of propeller */
    float m_expandedAreaRatio;

    /** @brief Whether to stop if no energy remains */
    bool m_stopIfNoEnergy;

    /** @brief Maximum rudder angle in degrees */
    float m_maxRudderAngle;

    /** @brief Weight of the vessel in tons */
    float m_vesselWeight;

    /** @brief Weight of cargo in tons */
    float m_cargoWeight;

    /** @brief Map of appendage IDs to wetted surface areas
     */
    QMap<int, float> m_appendagesWettedSurfaces;

    /**
     * @brief Check if a variant contains NaN values.
     * @param value The variant to check.
     * @return True if the variant contains NaN.
     */
    bool containsNaN(const QVariant &value) const;

    /**
     * @brief Helper method to check if a float value should
     * be treated as NaN
     * @param value The float value to check.
     * @return True if the value is NaN.
     */
    bool isNanValue(float value) const;
};

/**
 * @class ShipsReader
 * @brief Utility class for reading ship data from files.
 */
class ShipsReader : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Read ships from a file.
     * @param filePath Path to the ships configuration file.
     * @param parent The parent QObject.
     * @return Vector of Ship pointers loaded from the file.
     */
    static QVector<Ship *>
    readShipsFile(const QString &filePath,
                  QObject       *parent = nullptr);

    // Make Ship class a friend of ShipsReader
    friend class Ship;

protected:
    /**
     * @brief Ordered parameters expected in the ships file.
     *
     * Each pair contains the parameter name and a boolean
     * indicating whether it's required.
     */
    static const QVector<QPair<QString, bool>>
        FILE_ORDERED_PARAMETERS;

    /**
     * @brief Parse ship parameters from a string list.
     * @param parts List of string parts to parse.
     * @return Map of parameter names to values.
     */
    static QMap<QString, QVariant>
    parseShipParameters(const QStringList &parts);

    /**
     * @brief Parse path coordinates from a string.
     * @param pathString String containing path coordinates.
     * @return Vector of coordinate pairs.
     */
    static QVector<QVector<float>>
    parsePath(const QString &pathString);

    /**
     * @brief Parse engine points from a string.
     * @param enginePointsStr String containing engine
     *        points.
     * @return Vector of engine point maps.
     */
    static QVector<QMap<QString, float>>
    parseEnginePoints(const QString &enginePointsStr);

    /**
     * @brief Parse appendages data from a string.
     * @param appendagesStr String containing appendages
     *        data.
     * @return Map of appendage IDs to wetted surface areas.
     */
    static QMap<int, float>
    parseAppendages(const QString &appendagesStr);

    /**
     * @brief Parse tank details from a string.
     * @param tanksStr String containing tank details.
     * @return Vector of tank detail maps.
     */
    static QVector<QMap<QString, float>>
    parseTanksDetails(const QString &tanksStr);

    /**
     * @brief Check if a variant contains N/A values.
     * @param value The variant to check.
     * @return True if the variant contains N/A.
     */
    static bool containsNa(const QVariant &value);
};

} // namespace Backend
} // namespace CargoNetSim

// Register custom types with Qt's meta-object system
Q_DECLARE_METATYPE(CargoNetSim::Backend::Ship)
Q_DECLARE_METATYPE(CargoNetSim::Backend::Ship *)
Q_DECLARE_METATYPE(QVector<CargoNetSim::Backend::Ship>)
Q_DECLARE_METATYPE(QVector<CargoNetSim::Backend::Ship *>)
