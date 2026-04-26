/**
 * @file Train.h
 * @brief Train simulation classes for CargoNetSim
 * @author Ahmed Aredah
 * @date 2025-03-19
 * @copyright Copyright (c) 2025
 */

#pragma once

#include "Backend/Commons/Units.h"

#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QRegularExpression>
#include <QString>
#include <QTextStream>
#include <QVector>

namespace CargoNetSim
{
namespace Backend
{

/**
 * @class Locomotive
 * @brief Represents a locomotive in a train simulation
 *
 * The Locomotive class models the characteristics and
 * properties of a locomotive, including its physical
 * dimensions, power capabilities, and performance
 * attributes.
 *
 * This class must be registered with Qt's meta-object
 * system using
 * Q_DECLARE_METATYPE(CargoNetSim::Backend::Locomotive)
 * after class definition.
 */
class Locomotive : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Default constructor
     * @param parent The parent QObject
     */
    Locomotive(QObject *parent = nullptr);

    /**
     * @brief Parameterized constructor
     * @param power Engine power in kilowatts
     * @param transmissionEff Transmission efficiency
     * (0.0-1.0)
     * @param length Locomotive length in meters
     * @param airDragCoeff Air drag coefficient
     * @param frontalArea Frontal area in square meters
     * @param grossWeight Total weight in tonnes
     * @param noOfAxles Number of axles
     * @param locoType Type identifier for the locomotive
     * @param count Number of identical locomotives
     * @param parent The parent QObject
     */
    Locomotive(float power, float transmissionEff,
               float length, float airDragCoeff,
               float frontalArea, float grossWeight,
               int noOfAxles, int locoType, int count,
               QObject *parent = nullptr);

    /**
     * @brief Constructor from JSON data
     * @param json JSON object containing locomotive data
     * @param parent The parent QObject
     */
    Locomotive(const QJsonObject &json,
               QObject           *parent = nullptr);

    /**
     * @brief Converts the locomotive to a JSON object
     * @return QJsonObject representation of the locomotive
     */
    QJsonObject toJson() const;

    /**
     * @brief Creates a deep copy of the locomotive
     * @return Pointer to the new locomotive copy
     */
    Locomotive *copy() const;

    /**
     * @brief Gets the engine power
     * @return Power in kilowatts
     */
    float getPower() const
    {
        return m_power;
    }

    /**
     * @brief Gets the transmission efficiency
     * @return Efficiency as a ratio (0.0-1.0)
     */
    float getTransmissionEff() const
    {
        return m_transmissionEff;
    }

    /**
     * @brief Gets the locomotive length
     * @return Length in meters
     */
    float getLength() const
    {
        return m_length;
    }

    Units::LengthMeters lengthUnits() const
    {
        return Units::meters(m_length);
    }

    /**
     * @brief Gets the air drag coefficient
     * @return Air drag coefficient
     */
    float getAirDragCoeff() const
    {
        return m_airDragCoeff;
    }

    /**
     * @brief Gets the frontal area
     * @return Frontal area in square meters
     */
    float getFrontalArea() const
    {
        return m_frontalArea;
    }

    /**
     * @brief Gets the gross weight
     * @return Weight in tonnes
     */
    float getGrossWeight() const
    {
        return m_grossWeight;
    }

    Units::MassMetricTons grossWeightUnits() const
    {
        return Units::metricTons(m_grossWeight);
    }

    /**
     * @brief Gets the number of axles
     * @return Number of axles
     */
    int getNoOfAxles() const
    {
        return m_noOfAxles;
    }

    /**
     * @brief Gets the locomotive type
     * @return Type identifier
     */
    int getLocoType() const
    {
        return m_locoType;
    }

    /**
     * @brief Gets the Count of identical locomotives
     * @return Count value
     */
    int getCount() const
    {
        return m_count;
    }

    /**
     * @brief Sets the engine power
     * @param power Power in kilowatts
     */
    void setPower(float power);

    /**
     * @brief Sets the transmission efficiency
     * @param transmissionEff Efficiency as a ratio
     * (0.0-1.0)
     */
    void setTransmissionEff(float transmissionEff);

    /**
     * @brief Sets the locomotive length
     * @param length Length in meters
     */
    void setLength(float length);

    void setLengthUnits(Units::LengthMeters length)
    {
        setLength(static_cast<float>(length.value()));
    }

    /**
     * @brief Sets the air drag coefficient
     * @param airDragCoeff Air drag coefficient
     */
    void setAirDragCoeff(float airDragCoeff);

    /**
     * @brief Sets the frontal area
     * @param frontalArea Frontal area in square meters
     */
    void setFrontalArea(float frontalArea);

    /**
     * @brief Sets the gross weight
     * @param grossWeight Weight in tonnes
     */
    void setGrossWeight(float grossWeight);

    void setGrossWeightUnits(Units::MassMetricTons grossWeight)
    {
        setGrossWeight(
            static_cast<float>(grossWeight.value()));
    }

    /**
     * @brief Sets the number of axles
     * @param noOfAxles Number of axles
     */
    void setNoOfAxles(int noOfAxles);

    /**
     * @brief Sets the locomotive type
     * @param locoType Type identifier
     */
    void setLocoType(int locoType);

    /**
     * @brief Sets the count of identical locomotives
     * @param count Count value
     */
    void setCount(int count);

signals:
    /**
     * @brief Signal emitted when any locomotive property
     * changes
     */
    void locomotiveChanged();

private:
    float m_power;           ///< Engine power in kilowatts
    float m_transmissionEff; ///< Transmission efficiency
                             ///< (0.0-1.0)
    float m_length;       ///< Locomotive length in meters
    float m_airDragCoeff; ///< Air drag coefficient
    float m_frontalArea;  ///< Frontal area in square meters
    float m_grossWeight;  ///< Total weight in tonnes
    int   m_noOfAxles;    ///< Number of axles
    int m_locoType; ///< Type identifier for the locomotive
    int m_count;    ///< Number of identical locomotives
};

/**
 * @class Car
 * @brief Represents a railway car in a train simulation
 *
 * The Car class models the characteristics and properties
 * of a railway car, including its physical dimensions,
 * weight characteristics, and type.
 *
 * This class must be registered with Qt's meta-object
 * system using
 * Q_DECLARE_METATYPE(CargoNetSim::Backend::Car) after class
 * definition.
 */
class Car : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Default constructor
     * @param parent The parent QObject
     */
    Car(QObject *parent = nullptr);

    /**
     * @brief Parameterized constructor
     * @param length Car length in meters
     * @param airDragCoeff Air drag coefficient
     * @param frontalArea Frontal area in square meters
     * @param tareWeight Empty weight in tonnes
     * @param grossWeight Total weight in tonnes (with
     * cargo)
     * @param noOfAxles Number of axles
     * @param carType Type identifier for the car
     * @param count Number of identical cars
     * @param parent The parent QObject
     */
    Car(float length, float airDragCoeff, float frontalArea,
        float tareWeight, float grossWeight, int noOfAxles,
        int carType, int count, QObject *parent = nullptr);

    /**
     * @brief Constructor from JSON data
     * @param json JSON object containing car data
     * @param parent The parent QObject
     */
    Car(const QJsonObject &json, QObject *parent = nullptr);

    /**
     * @brief Converts the car to a JSON object
     * @return QJsonObject representation of the car
     */
    QJsonObject toJson() const;

    /**
     * @brief Creates a deep copy of the car
     * @return Pointer to the new car copy
     */
    Car *copy() const;

    /**
     * @brief Gets the car length
     * @return Length in meters
     */
    float getLength() const
    {
        return m_length;
    }

    Units::LengthMeters lengthUnits() const
    {
        return Units::meters(m_length);
    }

    /**
     * @brief Gets the air drag coefficient
     * @return Air drag coefficient
     */
    float getAirDragCoeff() const
    {
        return m_airDragCoeff;
    }

    /**
     * @brief Gets the frontal area
     * @return Frontal area in square meters
     */
    float getFrontalArea() const
    {
        return m_frontalArea;
    }

    /**
     * @brief Gets the tare (empty) weight
     * @return Tare weight in tonnes
     */
    float getTareWeight() const
    {
        return m_tareWeight;
    }

    Units::MassMetricTons tareWeightUnits() const
    {
        return Units::metricTons(m_tareWeight);
    }

    /**
     * @brief Gets the gross weight (with cargo)
     * @return Gross weight in tonnes
     */
    float getGrossWeight() const
    {
        return m_grossWeight;
    }

    Units::MassMetricTons grossWeightUnits() const
    {
        return Units::metricTons(m_grossWeight);
    }

    /**
     * @brief Gets the number of axles
     * @return Number of axles
     */
    int getNoOfAxles() const
    {
        return m_noOfAxles;
    }

    /**
     * @brief Gets the car type
     * @return Type identifier
     */
    int getCarType() const
    {
        return m_carType;
    }

    /**
     * @brief Gets the getCount of identical cars
     * @return Count value
     */
    int getCount() const
    {
        return m_count;
    }

    /**
     * @brief Sets the car length
     * @param length Length in meters
     */
    void setLength(float length);

    void setLengthUnits(Units::LengthMeters length)
    {
        setLength(static_cast<float>(length.value()));
    }

    /**
     * @brief Sets the air drag coefficient
     * @param airDragCoeff Air drag coefficient
     */
    void setAirDragCoeff(float airDragCoeff);

    /**
     * @brief Sets the frontal area
     * @param frontalArea Frontal area in square meters
     */
    void setFrontalArea(float frontalArea);

    /**
     * @brief Sets the tare (empty) weight
     * @param tareWeight Weight in tonnes
     */
    void setTareWeight(float tareWeight);

    void setTareWeightUnits(Units::MassMetricTons tareWeight)
    {
        setTareWeight(
            static_cast<float>(tareWeight.value()));
    }

    /**
     * @brief Sets the gross weight (with cargo)
     * @param grossWeight Weight in tonnes
     */
    void setGrossWeight(float grossWeight);

    void setGrossWeightUnits(Units::MassMetricTons grossWeight)
    {
        setGrossWeight(
            static_cast<float>(grossWeight.value()));
    }

    /**
     * @brief Sets the number of axles
     * @param noOfAxles Number of axles
     */
    void setNoOfAxles(int noOfAxles);

    /**
     * @brief Sets the car type
     * @param carType Type identifier
     */
    void setCarType(int carType);

    /**
     * @brief Sets the count of identical cars
     * @param count Count value
     */
    void setCount(int count);

signals:
    /**
     * @brief Signal emitted when any car property changes
     */
    void carChanged();

private:
    float m_length;       ///< Car length in meters
    float m_airDragCoeff; ///< Air drag coefficient
    float m_frontalArea;  ///< Frontal area in square meters
    float m_tareWeight;   ///< Empty weight in tonnes
    float m_grossWeight;  ///< Total weight with cargo in
                          ///< tonnes
    int m_noOfAxles;      ///< Number of axles
    int m_carType;        ///< Type identifier for the car
    int m_count;          ///< Number of identical cars
};

/**
 * @class Train
 * @brief Represents a complete train in a simulation
 *
 * The Train class combines locomotives and cars into a
 * complete train with route information and physical
 * properties.
 *
 * This class must be registered with Qt's meta-object
 * system using
 * Q_DECLARE_METATYPE(CargoNetSim::Backend::Train) after
 * class definition.
 */
class Train : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Default constructor
     * @param parent The parent QObject
     */
    Train(QObject *parent = nullptr);

    /**
     * @brief Parameterized constructor
     * @param userId User identifier string
     * @param trainPathOnNodeIds Path nodes the train
     * traverses
     * @param loadTime Loading time in hours
     * @param frictionCoef Coefficient of friction
     * @param locomotives Vector of locomotive objects
     * @param cars Vector of car objects
     * @param optimize Flag for optimization calculations
     * @param parent The parent QObject
     */
    Train(const QString      &userId,
          const QVector<int> &trainPathOnNodeIds,
          float loadTime, float frictionCoef,
          const QVector<Locomotive *> &locomotives,
          const QVector<Car *> &cars, bool optimize = false,
          QObject *parent = nullptr);

    /**
     * @brief Constructor from JSON data
     * @param json JSON object containing train data
     * @param parent The parent QObject
     */
    Train(const QJsonObject &json,
          QObject           *parent = nullptr);

    /**
     * @brief Destructor
     *
     * Properly cleans up all locomotive and car objects.
     */
    ~Train();

    /**
     * @brief Converts the train to a JSON object
     * @return QJsonObject representation of the train
     */
    QJsonObject toJson() const;

    /**
     * @brief Creates a deep copy of the train
     * @return Pointer to the new train copy
     */
    Train *copy() const;

    /**
     * @brief Gets the user identifier
     * @return User ID string
     */
    QString getUserId() const
    {
        return m_userId;
    }

    /**
     * @brief Gets the path node IDs
     * @return Vector of node IDs representing the path
     */
    QVector<int> getTrainPathOnNodeIds() const
    {
        return m_trainPathOnNodeIds;
    }

    /**
     * @brief Gets the loading time
     * @return Loading time in hours
     */
    float getLoadTime() const
    {
        return m_loadTime;
    }

    Units::TimeHours loadTimeUnits() const
    {
        return Units::hours(m_loadTime);
    }

    /**
     * @brief Gets the friction coefficient
     * @return Coefficient of friction
     */
    float getFrictionCoef() const
    {
        return m_frictionCoef;
    }

    /**
     * @brief Gets the locomotives
     * @return Vector of locomotive objects
     */
    QVector<Locomotive *> getLocomotives() const
    {
        return m_locomotives;
    }

    /**
     * @brief Gets the cars
     * @return Vector of car objects
     */
    QVector<Car *> getCars() const
    {
        return m_cars;
    }

    /**
     * @brief Gets the optimization flag
     * @return True if optimization is enabled
     */
    bool isOptimizing() const
    {
        return m_optimize;
    }

    /**
     * @brief Sets the user identifier
     * @param userId User ID string
     */
    void setUserId(const QString &userId);

    /**
     * @brief Sets the path node IDs
     * @param trainPathOnNodeIds Vector of node IDs
     */
    void setTrainPathOnNodeIds(
        const QVector<int> &trainPathOnNodeIds);

    /**
     * @brief Sets the loading time
     * @param loadTime Loading time in hours
     */
    void setLoadTime(float loadTime);

    void setLoadTimeUnits(Units::TimeHours loadTime)
    {
        setLoadTime(static_cast<float>(loadTime.value()));
    }

    /**
     * @brief Sets the friction coefficient
     * @param frictionCoef Coefficient of friction
     */
    void setFrictionCoef(float frictionCoef);

    /**
     * @brief Sets the locomotives
     * @param locomotives Vector of locomotive objects
     */
    void setLocomotives(
        const QVector<Locomotive *> &locomotives);

    /**
     * @brief Sets the cars
     * @param cars Vector of car objects
     */
    void setCars(const QVector<Car *> &cars);

    /**
     * @brief Sets the optimization flag
     * @param optimize True to enable optimization
     */
    void setOptimize(bool optimize);

signals:
    /**
     * @brief Signal emitted when any train property changes
     */
    void trainChanged();

    /**
     * @brief Signal emitted when locomotives are modified
     */
    void locomotivesChanged();

    /**
     * @brief Signal emitted when cars are modified
     */
    void carsChanged();

private:
    QString      m_userId; ///< User identifier string
    QVector<int> m_trainPathOnNodeIds; ///< Path node IDs
    float        m_loadTime; ///< Loading time in hours
    float m_frictionCoef;    ///< Coefficient of friction
    QVector<Locomotive *>
                   m_locomotives; ///< Vector of locomotives
    QVector<Car *> m_cars;        ///< Vector of cars
    bool           m_optimize;    ///< Flag for optimization
};

/**
 * @class TrainsReader
 * @brief Utility class for reading train data from files
 *
 * The TrainsReader provides static methods to parse and
 * load train data from text files into Train objects.
 *
 * This class must be registered with Qt's meta-object
 * system using
 * Q_DECLARE_METATYPE(CargoNetSim::Backend::TrainsReader)
 * after class definition.
 */
class TrainsReader : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Reads trains from a file
     * @param filePath Path to the trains data file
     * @param parent Parent QObject for created train
     * objects
     * @return Vector of train objects
     */
    static QVector<Train *>
    readTrainsFile(const QString &filePath,
                   QObject       *parent = nullptr);

private:
    /**
     * @brief Parses locomotive data from a string
     * @param locomotivesStr String containing locomotive
     * data
     * @param parent Parent QObject for created objects
     * @return Vector of locomotive objects
     */
    static QVector<Locomotive *>
    parseLocomotives(const QString &locomotivesStr,
                     QObject       *parent);

    /**
     * @brief Parses car data from a string
     * @param carsStr String containing car data
     * @param parent Parent QObject for created objects
     * @return Vector of car objects
     */
    static QVector<Car *> parseCars(const QString &carsStr,
                                    QObject       *parent);

    /**
     * @brief Splits a string into a vector of integers
     * @param string String containing integer values
     * @return Vector of parsed integers
     */
    static QVector<int>
    splitStringToIntList(const QString &string);
};

} // namespace Backend
} // namespace CargoNetSim

// Register custom types with Qt's meta-object system
Q_DECLARE_METATYPE(CargoNetSim::Backend::Locomotive)
Q_DECLARE_METATYPE(CargoNetSim::Backend::Locomotive *)
Q_DECLARE_METATYPE(CargoNetSim::Backend::Car)
Q_DECLARE_METATYPE(CargoNetSim::Backend::Car *)
Q_DECLARE_METATYPE(CargoNetSim::Backend::Train)
Q_DECLARE_METATYPE(CargoNetSim::Backend::Train *)
Q_DECLARE_METATYPE(CargoNetSim::Backend::TrainsReader)
Q_DECLARE_METATYPE(CargoNetSim::Backend::TrainsReader *)
