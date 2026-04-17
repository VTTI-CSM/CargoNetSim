#include "VehicleController.h"
#include "Backend/Commons/LogCategories.h"
#include <QRandomGenerator>

namespace CargoNetSim
{
namespace Backend
{

VehicleController::VehicleController(QObject *parent)
    : QObject(parent)
{
}

VehicleController::~VehicleController()
{
    clear();
}

// Ship Management Methods
bool VehicleController::loadShipsFromFile(
    const QString &filePath)
{
    qCInfo(lcController) << "VehicleController::loadShipsFromFile:"
                         << filePath;

    // Clear existing ships
    qDeleteAll(m_ships);
    m_ships.clear();

    // Load ships from file
    QVector<Ship *> loadedShips =
        ShipsReader::readShipsFile(filePath, this);
    if (loadedShips.isEmpty())
    {
        qCWarning(lcController) << "VehicleController::loadShipsFromFile:"
                                << "no ships parsed from" << filePath;
        return false;
    }

    // Add ships to map
    for (Ship *ship : loadedShips)
    {
        m_ships[ship->getUserId()] = ship;
        connect(ship, &Ship::shipChanged, this,
                [this, ship]() { emit shipUpdated(ship); });
    }

    qCDebug(lcController) << "VehicleController::loadShipsFromFile:"
                          << "loaded" << loadedShips.size() << "ships";
    emit shipsLoaded(loadedShips.size());
    return true;
}

Ship *
VehicleController::getShip(const QString &shipId) const
{
    return m_ships.value(shipId, nullptr);
}

QVector<Ship *> VehicleController::getAllShips() const
{
    return m_ships.values().toVector();
}

bool VehicleController::addShip(Ship *ship)
{
    if (!ship || m_ships.contains(ship->getUserId()))
    {
        qCDebug(lcController) << "VehicleController::addShip:"
                              << "rejected (null or duplicate)";
        return false;
    }
    qCDebug(lcController) << "VehicleController::addShip:"
                          << ship->getUserId();

    // Take ownership of the ship
    ship->setParent(this);

    // Connect to signals
    connect(ship, &Ship::shipChanged, this,
            [this, ship]() { emit shipUpdated(ship); });

    // Add to map
    m_ships[ship->getUserId()] = ship;

    emit shipAdded(ship);
    return true;
}

bool VehicleController::removeShip(const QString &shipId)
{
    qCDebug(lcController) << "VehicleController::removeShip:" << shipId;
    if (!m_ships.contains(shipId))
    {
        return false;
    }

    Ship *ship = m_ships.take(shipId);
    emit  shipRemoved(shipId);

    delete ship;
    return true;
}

bool VehicleController::updateShip(Ship *ship)
{
    if (!ship || !m_ships.contains(ship->getUserId()))
    {
        qCDebug(lcController) << "VehicleController::updateShip:"
                              << "rejected (null or not found)";
        return false;
    }
    qCDebug(lcController) << "VehicleController::updateShip:"
                          << ship->getUserId();

    // Remove old ship
    Ship *oldShip = m_ships[ship->getUserId()];
    // Check if it's the same object
    if (oldShip == ship)
    {
        // No need to update anything, it's already the same
        // object
        return true;
    }

    // It's a different object, so delete the old one
    delete oldShip;

    // Add updated ship
    ship->setParent(this);
    m_ships[ship->getUserId()] = ship;

    // Connect to signals
    connect(
        ship, &Ship::shipChanged, this,
        [this, ship]() { emit shipUpdated(ship); },
        Qt::UniqueConnection);

    emit shipUpdated(ship);
    return true;
}

bool VehicleController::updateShips(QVector<Ship *> ships)
{
    // Create a set of new ship IDs for quick lookup
    QSet<QString> newShipIds;
    for (Ship *ship : ships)
    {
        newShipIds.insert(ship->getUserId());
    }

    // Find ships to remove (those in current map but not in
    // new set)
    QVector<QString> shipsToRemove;
    for (auto it = m_ships.begin(); it != m_ships.end();
         ++it)
    {
        if (!newShipIds.contains(it.key()))
        {
            shipsToRemove.append(it.key());
        }
    }

    // Remove ships that are no longer present
    for (const QString &shipId : shipsToRemove)
    {
        removeShip(shipId);
    }

    // Update or add new ships
    bool success = true;
    for (Ship *ship : ships)
    {
        if (m_ships.contains(ship->getUserId()))
        {
            success &= updateShip(ship);
        }
        else
        {
            success &= addShip(ship);
        }
    }

    return success;
}

int VehicleController::shipCount() const
{
    return m_ships.size();
}

// Train Management Methods
bool VehicleController::loadTrainsFromFile(
    const QString &filePath)
{
    qCInfo(lcController) << "VehicleController::loadTrainsFromFile:"
                         << filePath;

    // Clear existing trains
    qDeleteAll(m_trains);
    m_trains.clear();

    // Load trains from file
    QVector<Train *> loadedTrains =
        TrainsReader::readTrainsFile(filePath, this);
    if (loadedTrains.isEmpty())
    {
        qCWarning(lcController) << "VehicleController::loadTrainsFromFile:"
                                << "no trains parsed from" << filePath;
        return false;
    }

    // Add trains to map
    for (Train *train : loadedTrains)
    {
        m_trains[train->getUserId()] = train;
        connect(
            train, &Train::trainChanged, this,
            [this, train]() { emit trainUpdated(train); });
    }

    qCDebug(lcController) << "VehicleController::loadTrainsFromFile:"
                          << "loaded" << loadedTrains.size() << "trains";
    emit trainsLoaded(loadedTrains.size());
    return true;
}

Train *
VehicleController::getTrain(const QString &userId) const
{
    return m_trains.value(userId, nullptr);
}

QVector<Train *> VehicleController::getAllTrains() const
{
    return m_trains.values().toVector();
}

bool VehicleController::addTrain(Train *train)
{
    if (!train || m_trains.contains(train->getUserId()))
    {
        qCDebug(lcController) << "VehicleController::addTrain:"
                              << "rejected (null or duplicate)";
        return false;
    }
    qCDebug(lcController) << "VehicleController::addTrain:"
                          << train->getUserId();

    // Take ownership of the train
    train->setParent(this);

    // Connect to signals
    connect(train, &Train::trainChanged, this,
            [this, train]() { emit trainUpdated(train); });

    // Add to map
    m_trains[train->getUserId()] = train;

    emit trainAdded(train);
    return true;
}

bool VehicleController::removeTrain(const QString &userId)
{
    qCDebug(lcController) << "VehicleController::removeTrain:" << userId;
    if (!m_trains.contains(userId))
    {
        return false;
    }

    Train *train = m_trains.take(userId);
    emit   trainRemoved(userId);

    delete train;
    return true;
}

bool VehicleController::updateTrain(Train *train)
{
    if (!train || !m_trains.contains(train->getUserId()))
    {
        qCDebug(lcController) << "VehicleController::updateTrain:"
                              << "rejected (null or not found)";
        return false;
    }
    qCDebug(lcController) << "VehicleController::updateTrain:"
                          << train->getUserId();

    // Remove old train
    Train *oldTrain = m_trains[train->getUserId()];
    // Check if it's the same object
    if (oldTrain == train)
    {
        // No need to update anything, it's already the same
        // object
        return true;
    }

    // It's a different object, so delete the old one
    delete oldTrain;

    // Add updated train
    train->setParent(this);
    m_trains[train->getUserId()] = train;

    // Connect to signals
    connect(
        train, &Train::trainChanged, this,
        [this, train]() { emit trainUpdated(train); },
        Qt::UniqueConnection);

    emit trainUpdated(train);
    return true;
}

bool VehicleController::updateTrains(
    QVector<Train *> trains)
{
    // Create a set of new train IDs for quick lookup
    QSet<QString> newTrainIds;
    for (Train *train : trains)
    {
        newTrainIds.insert(train->getUserId());
    }

    // Find trains to remove (those in current map but not
    // in new set)
    QVector<QString> trainsToRemove;
    for (auto it = m_trains.begin(); it != m_trains.end();
         ++it)
    {
        if (!newTrainIds.contains(it.key()))
        {
            trainsToRemove.append(it.key());
        }
    }

    // Remove trains that are no longer present
    for (const QString &trainId : trainsToRemove)
    {
        removeTrain(trainId);
    }

    // Update or add new trains
    bool success = true;
    for (Train *train : trains)
    {
        if (m_trains.contains(train->getUserId()))
        {
            success &= updateTrain(train);
        }
        else
        {
            success &= addTrain(train);
        }
    }

    return success;
}

int VehicleController::trainCount() const
{
    return m_trains.size();
}

// General operations
void VehicleController::clear()
{
    qCInfo(lcController) << "VehicleController::clear:"
                         << m_ships.size() << "ships,"
                         << m_trains.size() << "trains";
    // Clear ships
    qDeleteAll(m_ships);
    m_ships.clear();
    emit shipsCleared();

    // Clear trains
    qDeleteAll(m_trains);
    m_trains.clear();
    emit trainsCleared();
}

Ship *VehicleController::getRandomShip() const
{
    if (m_ships.isEmpty())
    {
        qCWarning(lcController) << "VehicleController::getRandomShip:"
                                << "ship collection is empty";
        return nullptr;
    }

    // Get all ships as a vector
    QVector<Ship *> ships = m_ships.values().toVector();

    // Generate a random index
    QRandomGenerator generator =
        QRandomGenerator::securelySeeded();
    int randomIndex = generator.bounded(ships.size());

    // Return the ship at the random index
    return ships.at(randomIndex);
}

Train *VehicleController::getRandomTrain() const
{
    if (m_trains.isEmpty())
    {
        qCWarning(lcController) << "VehicleController::getRandomTrain:"
                                << "train collection is empty";
        return nullptr;
    }

    // Get all trains as a vector
    QVector<Train *> trains = m_trains.values().toVector();

    // Generate a random index
    QRandomGenerator generator =
        QRandomGenerator::securelySeeded();
    int randomIndex = generator.bounded(trains.size());

    // Return the train at the random index
    return trains.at(randomIndex);
}

} // namespace Backend
} // namespace CargoNetSim
