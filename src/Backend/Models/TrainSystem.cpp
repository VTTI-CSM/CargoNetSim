#include "TrainSystem.h"
#include "Backend/Commons/LogCategories.h"

namespace CargoNetSim
{
namespace Backend
{
// Locomotive Implementation
Locomotive::Locomotive(QObject *parent)
    : QObject(parent)
    , m_power(0.0f)
    , m_transmissionEff(0.0f)
    , m_length(0.0f)
    , m_airDragCoeff(0.0f)
    , m_frontalArea(0.0f)
    , m_grossWeight(0.0f)
    , m_noOfAxles(0)
    , m_locoType(0)
    , m_count(0)
{
}

Locomotive::Locomotive(float power, float transmissionEff,
                       float length, float airDragCoeff,
                       float frontalArea, float grossWeight,
                       int noOfAxles, int locoType,
                       int count, QObject *parent)
    : QObject(parent)
    , m_power(power)
    , m_transmissionEff(transmissionEff)
    , m_length(length)
    , m_airDragCoeff(airDragCoeff)
    , m_frontalArea(frontalArea)
    , m_grossWeight(grossWeight)
    , m_noOfAxles(noOfAxles)
    , m_locoType(locoType)
    , m_count(count)
{
}

Locomotive::Locomotive(const QJsonObject &json,
                       QObject           *parent)
    : QObject(parent)
{
    m_count           = json["Count"].toInt();
    m_power           = json["Power"].toDouble();
    m_transmissionEff = json["TransmissionEff"].toDouble();
    m_length          = json["Length"].toDouble();
    m_airDragCoeff    = json["AirDragCoeff"].toDouble();
    m_frontalArea     = json["FrontalArea"].toDouble();
    m_grossWeight     = json["GrossWeight"].toDouble();
    m_noOfAxles       = json["NoOfAxles"].toInt();
    m_locoType        = json["Type"].toInt();
}

QJsonObject Locomotive::toJson() const
{
    QJsonObject json;
    json["Count"]           = m_count;
    json["Power"]           = m_power;
    json["TransmissionEff"] = m_transmissionEff;
    json["Length"]          = m_length;
    json["AirDragCoeff"]    = m_airDragCoeff;
    json["FrontalArea"]     = m_frontalArea;
    json["GrossWeight"]     = m_grossWeight;
    json["NoOfAxles"]       = m_noOfAxles;
    json["Type"]            = m_locoType;
    return json;
}

Locomotive *Locomotive::copy() const
{
    return new Locomotive(m_power, m_transmissionEff,
                          m_length, m_airDragCoeff,
                          m_frontalArea, m_grossWeight,
                          m_noOfAxles, m_locoType, m_count);
}

void Locomotive::setPower(float power)
{
    if (m_power != power)
    {
        m_power = power;
        emit locomotiveChanged();
    }
}

void Locomotive::setTransmissionEff(float transmissionEff)
{
    if (m_transmissionEff != transmissionEff)
    {
        m_transmissionEff = transmissionEff;
        emit locomotiveChanged();
    }
}

void Locomotive::setLength(float length)
{
    if (m_length != length)
    {
        m_length = length;
        emit locomotiveChanged();
    }
}

void Locomotive::setAirDragCoeff(float airDragCoeff)
{
    if (m_airDragCoeff != airDragCoeff)
    {
        m_airDragCoeff = airDragCoeff;
        emit locomotiveChanged();
    }
}

void Locomotive::setFrontalArea(float frontalArea)
{
    if (m_frontalArea != frontalArea)
    {
        m_frontalArea = frontalArea;
        emit locomotiveChanged();
    }
}

void Locomotive::setGrossWeight(float grossWeight)
{
    if (m_grossWeight != grossWeight)
    {
        m_grossWeight = grossWeight;
        emit locomotiveChanged();
    }
}

void Locomotive::setNoOfAxles(int noOfAxles)
{
    if (m_noOfAxles != noOfAxles)
    {
        m_noOfAxles = noOfAxles;
        emit locomotiveChanged();
    }
}

void Locomotive::setLocoType(int locoType)
{
    if (m_locoType != locoType)
    {
        m_locoType = locoType;
        emit locomotiveChanged();
    }
}

void Locomotive::setCount(int count)
{
    if (m_count != count)
    {
        m_count = count;
        emit locomotiveChanged();
    }
}

// Car Implementation
Car::Car(QObject *parent)
    : QObject(parent)
    , m_length(0.0f)
    , m_airDragCoeff(0.0f)
    , m_frontalArea(0.0f)
    , m_tareWeight(0.0f)
    , m_grossWeight(0.0f)
    , m_noOfAxles(0)
    , m_carType(0)
    , m_count(0)
{
}

Car::Car(float length, float airDragCoeff,
         float frontalArea, float tareWeight,
         float grossWeight, int noOfAxles, int carType,
         int count, QObject *parent)
    : QObject(parent)
    , m_length(length)
    , m_airDragCoeff(airDragCoeff)
    , m_frontalArea(frontalArea)
    , m_tareWeight(tareWeight)
    , m_grossWeight(grossWeight)
    , m_noOfAxles(noOfAxles)
    , m_carType(carType)
    , m_count(count)
{
}

Car::Car(const QJsonObject &json, QObject *parent)
    : QObject(parent)
{
    m_count        = json["Count"].toInt();
    m_length       = json["Length"].toDouble();
    m_airDragCoeff = json["AirDragCoeff"].toDouble();
    m_frontalArea  = json["FrontalArea"].toDouble();
    m_tareWeight   = json["TareWeight"].toDouble();
    m_grossWeight  = json["GrossWeight"].toDouble();
    m_noOfAxles    = json["NoOfAxles"].toInt();
    m_carType      = json["Type"].toInt();
}

QJsonObject Car::toJson() const
{
    QJsonObject json;
    json["Count"]        = m_count;
    json["Length"]       = m_length;
    json["AirDragCoeff"] = m_airDragCoeff;
    json["FrontalArea"]  = m_frontalArea;
    json["TareWeight"]   = m_tareWeight;
    json["GrossWeight"]  = m_grossWeight;
    json["NoOfAxles"]    = m_noOfAxles;
    json["Type"]         = m_carType;
    return json;
}

Car *Car::copy() const
{
    return new Car(m_length, m_airDragCoeff, m_frontalArea,
                   m_tareWeight, m_grossWeight, m_noOfAxles,
                   m_carType, m_count);
}

void Car::setLength(float length)
{
    if (m_length != length)
    {
        m_length = length;
        emit carChanged();
    }
}

void Car::setAirDragCoeff(float airDragCoeff)
{
    if (m_airDragCoeff != airDragCoeff)
    {
        m_airDragCoeff = airDragCoeff;
        emit carChanged();
    }
}

void Car::setFrontalArea(float frontalArea)
{
    if (m_frontalArea != frontalArea)
    {
        m_frontalArea = frontalArea;
        emit carChanged();
    }
}

void Car::setTareWeight(float tareWeight)
{
    if (m_tareWeight != tareWeight)
    {
        m_tareWeight = tareWeight;
        emit carChanged();
    }
}

void Car::setGrossWeight(float grossWeight)
{
    if (m_grossWeight != grossWeight)
    {
        m_grossWeight = grossWeight;
        emit carChanged();
    }
}

void Car::setNoOfAxles(int noOfAxles)
{
    if (m_noOfAxles != noOfAxles)
    {
        m_noOfAxles = noOfAxles;
        emit carChanged();
    }
}

void Car::setCarType(int carType)
{
    if (m_carType != carType)
    {
        m_carType = carType;
        emit carChanged();
    }
}

void Car::setCount(int count)
{
    if (m_count != count)
    {
        m_count = count;
        emit carChanged();
    }
}

// Train Implementation
Train::Train(QObject *parent)
    : QObject(parent)
    , m_loadTime(0.0f)
    , m_frictionCoef(0.0f)
    , m_optimize(false)
{
}

Train::Train(const QString      &userId,
             const QVector<int> &trainPathOnNodeIds,
             float loadTime, float frictionCoef,
             const QVector<Locomotive *> &locomotives,
             const QVector<Car *> &cars, bool optimize,
             QObject *parent)
    : QObject(parent)
    , m_userId(userId)
    , m_trainPathOnNodeIds(trainPathOnNodeIds)
    , m_loadTime(loadTime)
    , m_frictionCoef(frictionCoef)
    , m_locomotives(locomotives)
    , m_cars(cars)
    , m_optimize(optimize)
{
    // Reparent the locomotives and cars
    for (auto locomotive : m_locomotives)
    {
        locomotive->setParent(this);
        connect(locomotive, &Locomotive::locomotiveChanged,
                this, &Train::trainChanged);
    }

    for (auto car : m_cars)
    {
        car->setParent(this);
        connect(car, &Car::carChanged, this,
                &Train::trainChanged);
    }
}

Train::Train(const QJsonObject &json, QObject *parent)
    : QObject(parent)
{
    qCDebug(lcModel) << "Train::Train(json): deserializing from JSON";
    m_userId       = json["UserID"].toString();
    m_loadTime     = json["LoadTime"].toDouble();
    m_frictionCoef = json["FrictionCoef"].toDouble();
    m_optimize     = json["Optimize"].toBool();

    // Parse train path
    QJsonArray pathArray =
        json["TrainPathOnNodeIDs"].toArray();
    m_trainPathOnNodeIds.clear();
    for (int i = 0; i < pathArray.size(); ++i)
    {
        m_trainPathOnNodeIds.append(pathArray[i].toInt());
    }

    // Parse locomotives
    QJsonArray locoArray = json["Locomotives"].toArray();
    m_locomotives.clear();
    for (int i = 0; i < locoArray.size(); ++i)
    {
        Locomotive *locomotive =
            new Locomotive(locoArray[i].toObject(), this);
        m_locomotives.append(locomotive);
        connect(locomotive, &Locomotive::locomotiveChanged,
                this, &Train::trainChanged);
    }

    // Parse cars
    QJsonArray carsArray = json["Cars"].toArray();
    m_cars.clear();
    for (int i = 0; i < carsArray.size(); ++i)
    {
        Car *car = new Car(carsArray[i].toObject(), this);
        m_cars.append(car);
        connect(car, &Car::carChanged, this,
                &Train::trainChanged);
    }
}

Train::~Train()
{
    // Child QObjects will be automatically deleted
}

QJsonObject Train::toJson() const
{
    qCDebug(lcModel) << "Train::toJson:" << m_userId;
    QJsonObject json;
    json["UserID"]       = m_userId;
    json["LoadTime"]     = m_loadTime;
    json["FrictionCoef"] = m_frictionCoef;
    json["Optimize"]     = m_optimize;

    // Convert train path to JSON array
    QJsonArray pathArray;
    for (int nodeId : m_trainPathOnNodeIds)
    {
        pathArray.append(nodeId);
    }
    json["TrainPathOnNodeIDs"] = pathArray;

    // Convert locomotives to JSON array
    QJsonArray locoArray;
    for (const Locomotive *locomotive : m_locomotives)
    {
        locoArray.append(locomotive->toJson());
    }
    json["Locomotives"] = locoArray;

    // Convert cars to JSON array
    QJsonArray carsArray;
    for (const Car *car : m_cars)
    {
        carsArray.append(car->toJson());
    }
    json["Cars"] = carsArray;

    return json;
}

Train *Train::copy() const
{
    QVector<Locomotive *> copiedLocomotives;
    for (const Locomotive *locomotive : m_locomotives)
    {
        copiedLocomotives.append(locomotive->copy());
    }

    QVector<Car *> copiedCars;
    for (const Car *car : m_cars)
    {
        copiedCars.append(car->copy());
    }

    return new Train(m_userId, m_trainPathOnNodeIds,
                     m_loadTime, m_frictionCoef,
                     copiedLocomotives, copiedCars,
                     m_optimize);
}

void Train::setUserId(const QString &userId)
{
    if (m_userId != userId)
    {
        m_userId = userId;
        emit trainChanged();
    }
}

void Train::setTrainPathOnNodeIds(
    const QVector<int> &trainPathOnNodeIds)
{
    if (m_trainPathOnNodeIds != trainPathOnNodeIds)
    {
        m_trainPathOnNodeIds = trainPathOnNodeIds;
        emit trainChanged();
    }
}

void Train::setLoadTime(float loadTime)
{
    if (m_loadTime != loadTime)
    {
        m_loadTime = loadTime;
        emit trainChanged();
    }
}

void Train::setFrictionCoef(float frictionCoef)
{
    if (m_frictionCoef != frictionCoef)
    {
        m_frictionCoef = frictionCoef;
        emit trainChanged();
    }
}

void Train::setLocomotives(
    const QVector<Locomotive *> &locomotives)
{
    // Disconnect old signals
    for (auto locomotive : m_locomotives)
    {
        disconnect(locomotive,
                   &Locomotive::locomotiveChanged, this,
                   &Train::trainChanged);
    }

    // Delete old locomotives
    qDeleteAll(m_locomotives);
    m_locomotives.clear();

    // Add new locomotives
    m_locomotives = locomotives;

    // Reparent and connect new locomotives
    for (auto locomotive : m_locomotives)
    {
        locomotive->setParent(this);
        connect(locomotive, &Locomotive::locomotiveChanged,
                this, &Train::trainChanged);
    }

    emit locomotivesChanged();
    emit trainChanged();
}

void Train::setCars(const QVector<Car *> &cars)
{
    // Disconnect old signals
    for (auto car : m_cars)
    {
        disconnect(car, &Car::carChanged, this,
                   &Train::trainChanged);
    }

    // Delete old cars
    qDeleteAll(m_cars);
    m_cars.clear();

    // Add new cars
    m_cars = cars;

    // Reparent and connect new cars
    for (auto car : m_cars)
    {
        car->setParent(this);
        connect(car, &Car::carChanged, this,
                &Train::trainChanged);
    }

    emit carsChanged();
    emit trainChanged();
}

void Train::setOptimize(bool optimize)
{
    if (m_optimize != optimize)
    {
        m_optimize = optimize;
        emit trainChanged();
    }
}

// TrainsReader Implementation
QVector<Train *>
TrainsReader::readTrainsFile(const QString &filePath,
                             QObject       *parent)
{
    qCInfo(lcModel) << "TrainsReader::readTrainsFile:" << filePath;
    QVector<Train *> trains;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qCCritical(lcModel) << "Error: Trains file" << filePath
                    << "does not exist.";
        return trains;
    }

    QTextStream in(&file);
    QStringList lines;

    while (!in.atEnd())
    {
        lines.append(in.readLine());
    }

    file.close();

    // Check if file is empty
    if (lines.isEmpty())
    {
        qCCritical(lcModel) << "Error: Trains file" << filePath
                    << "is empty!";
        return trains;
    }

    // Skip first two lines and start parsing trains
    for (int i = 2; i < lines.size(); ++i)
    {
        QString line = lines[i].trimmed();

        if (line.isEmpty())
        {
            continue; // Skip empty lines
        }

        // Split the line by tab
        QStringList lv =
            line.split(QRegularExpression("\\t+"));

        if (lv.size() != 6)
        {
            qCCritical(lcModel) << "Error: Trains file has a wrong "
                           "structure.";
            qDeleteAll(trains);
            return QVector<Train *>();
        }

        // Parse train path, locomotives, and cars
        QVector<int> trainPathOnNodeIds =
            splitStringToIntList(lv[1]);
        QVector<Locomotive *> locomotives =
            parseLocomotives(lv[4], parent);
        QVector<Car *> cars = parseCars(lv[5], parent);

        // Create Train object
        Train *train =
            new Train(lv[0], trainPathOnNodeIds,
                      lv[2].toFloat(), lv[3].toFloat(),
                      locomotives, cars, false, parent);

        trains.append(train);
    }

    qCDebug(lcModel) << "TrainsReader::readTrainsFile:"
                     << "parsed" << trains.size() << "trains";
    return trains;
}

QVector<Locomotive *> TrainsReader::parseLocomotives(
    const QString &locomotivesStr, QObject *parent)
{
    QVector<Locomotive *> locomotives;
    QStringList locList = locomotivesStr.split(";");

    for (const QString &loc : locList)
    {
        QStringList fields = loc.split(",");
        if (fields.size() != 9)
        {
            qCCritical(lcModel) << "Wrong Locomotive structure!";
            qDeleteAll(locomotives);
            return QVector<Locomotive *>();
        }

        // Create Locomotive object
        Locomotive *locomotive = new Locomotive(
            fields[1].toFloat(), fields[2].toFloat(),
            fields[6].toFloat(), fields[4].toFloat(),
            fields[5].toFloat(), fields[7].toFloat(),
            fields[3].toInt(), fields[8].toInt(),
            fields[0].toInt(), parent);

        locomotives.append(locomotive);
    }

    return locomotives;
}

QVector<Car *>
TrainsReader::parseCars(const QString &carsStr,
                        QObject       *parent)
{
    QVector<Car *> cars;
    QStringList    carList = carsStr.split(";");

    for (const QString &car : carList)
    {
        QStringList fields = car.split(",");

        // Add a default car type if missing
        if (fields.size() < 8)
        {
            fields.append("0");
        }

        if (fields.size() != 8)
        {
            qCCritical(lcModel) << "Wrong Car structure!";
            qDeleteAll(cars);
            return QVector<Car *>();
        }

        // Create Car object
        Car *carObj = new Car(
            fields[4].toFloat(), fields[2].toFloat(),
            fields[3].toFloat(), fields[6].toFloat(),
            fields[5].toFloat(), fields[1].toInt(),
            fields[7].toInt(), fields[0].toInt(), parent);

        cars.append(carObj);
    }

    return cars;
}

QVector<int>
TrainsReader::splitStringToIntList(const QString &string)
{
    QVector<int> result;
    QStringList  parts = string.split(",");

    for (const QString &part : parts)
    {
        bool ok;
        int  value = part.toInt(&ok);
        if (ok)
        {
            result.append(value);
        }
    }

    return result;
}

} // namespace Backend
} // namespace CargoNetSim
