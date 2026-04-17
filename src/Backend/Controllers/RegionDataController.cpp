// RegionDataController.cpp
#include "RegionDataController.h"

#include "Backend/Commons/LogCategories.h"
#include <QDebug>
#include <QReadLocker>
#include <QWriteLocker>
#include <stdexcept>

namespace CargoNetSim
{
namespace Backend
{

// RegionData implementation

RegionData::RegionData(const QString     &regionName,
                       NetworkController *networkController,
                       QObject           *parent)
    : QObject(parent)
    , m_region(regionName)
    , m_networkController(networkController)
{
    // No need to initialize network lists as they are now
    // managed by NetworkController
}

RegionData::~RegionData()
{
    m_networkController = nullptr;
}

bool RegionData::checkNetworkNameConflict(
    const QString &name) const
{
    // Use NetworkController to check if a network with this
    // name exists in this region
    return m_networkController->networkExistsInRegion(
        name, m_region);
}

void RegionData::setRegionName(const QString &name)
{
    if (m_region == name)
    {
        return; // No change needed
    }

    // Use NetworkController to rename the region
    // This will move all networks from old region name to
    // new region name
    if (!m_networkController->renameRegion(m_region, name))
    {
        qCWarning(lcController) << "Failed to rename region from"
                   << m_region << "to" << name;
        return;
    }

    // Update our region name
    m_region = name;
}

void RegionData::addTrainNetwork(const QString &networkName,
                                 const QString &nodeFile,
                                 const QString &linkFile)
{
    qCDebug(lcController) << "RegionData::addTrainNetwork:"
                          << networkName << "region=" << m_region;
    // Check for name conflicts using NetworkController
    if (checkNetworkNameConflict(networkName))
    {
        throw std::runtime_error(
            QString("Network name '%1' already exists in "
                    "train or "
                    "truck networks")
                .arg(networkName)
                .toStdString());
    }

    try
    {
        // Create a new train network
        TrainClient::NeTrainSimNetwork *network =
            new TrainClient::NeTrainSimNetwork();
        network->loadNetwork(nodeFile, linkFile);

        // Add the network to NetworkController
        if (!m_networkController->addTrainNetwork(
                networkName, m_region, network))
        {
            delete network; // Clean up if adding failed
            throw std::runtime_error(
                "Failed to register train network "
                "with NetworkController");
        }

        // Notify listeners
        emit trainNetworkAdded(networkName);
    }
    catch (const std::exception &e)
    {
        throw std::runtime_error(
            QString("Failed to create train network: %1")
                .arg(e.what())
                .toStdString());
    }
}

void RegionData::addTruckNetwork(const QString &networkName,
                                 const QString &configFile)
{
    qCDebug(lcController) << "RegionData::addTruckNetwork:"
                          << networkName << "region=" << m_region;
    // Check for name conflicts using NetworkController
    if (checkNetworkNameConflict(networkName))
    {
        throw std::runtime_error(
            QString("Network name '%1' already exists "
                    "in train or truck networks")
                .arg(networkName)
                .toStdString());
    }

    try
    {
        // Parse the config file
        // Create a config directly without using the
        // reader's destructor
        TruckClient::IntegrationSimulationConfig *config =
            TruckClient::IntegrationSimulationConfigReader::
                readConfig(configFile);

        // Add the config to NetworkController
        if (!m_networkController->addTruckNetworkConfig(
                networkName, m_region, config))
        {
            delete config; // Clean up if adding failed
            throw std::runtime_error(
                "Failed to register truck network "
                "config with NetworkController");
        }

        // Notify listeners
        emit truckNetworkAdded(networkName);
    }
    catch (const std::exception &e)
    {
        throw std::runtime_error(
            QString("Failed to create truck network: %1")
                .arg(e.what())
                .toStdString());
    }
}

bool RegionData::renameTrainNetwork(const QString &oldName,
                                    const QString &newName)
{
    // Check if old name exists using NetworkController
    if (!trainNetworkExists(oldName))
    {
        throw std::runtime_error(
            QString(
                "Train network '%1' not found in region")
                .arg(oldName)
                .toStdString());
        return false;
    }

    // Check if new name would conflict using
    // NetworkController
    if (oldName != newName
        && checkNetworkNameConflict(newName))
    {
        throw std::runtime_error(
            QString("Network name '%1' already exists")
                .arg(newName)
                .toStdString());
        return false;
    }

    // Use the NetworkController to rename the network
    if (!m_networkController->renameTrainNetwork(
            oldName, newName, m_region))
    {
        throw std::runtime_error(
            "Failed to rename train network");
        return false;
    }

    // Notify listeners
    emit trainNetworkRenamed(oldName, newName);
    return true;
}

bool RegionData::renameTruckNetwork(const QString &oldName,
                                    const QString &newName)
{
    // Check if old name exists using NetworkController
    if (!truckNetworkExists(oldName))
    {
        throw std::runtime_error(
            QString(
                "Truck network '%1' not found in region")
                .arg(oldName)
                .toStdString());
        return false;
    }

    // Check if new name would conflict using
    // NetworkController
    if (oldName != newName
        && checkNetworkNameConflict(newName))
    {
        throw std::runtime_error(
            QString("Network name '%1' already exists")
                .arg(newName)
                .toStdString());
        return false;
    }

    // Use the NetworkController to rename the network
    if (!m_networkController->renameTruckNetworkConfig(
            oldName, newName, m_region))
    {
        throw std::runtime_error(
            "Failed to rename truck network");
        return false;
    }

    // Notify listeners
    emit truckNetworkRenamed(oldName, newName);
    return true;
}

bool RegionData::removeTrainNetwork(const QString &name)
{
    qCDebug(lcController) << "RegionData::removeTrainNetwork:"
                          << name << "region=" << m_region;
    // Check if network exists using NetworkController
    if (!trainNetworkExists(name))
    {
        throw std::runtime_error(
            QString(
                "Train network '%1' not found in region")
                .arg(name)
                .toStdString());
        return false;
    }

    try
    {
        // Remove from NetworkController (this will delete
        // the network object)
        if (!m_networkController->removeTrainNetwork(
                name, m_region))
        {
            throw std::runtime_error(
                "Failed to remove network from "
                "NetworkController");
            return false;
        }

        // Notify listeners
        emit trainNetworkRemoved(name);

        return true;
    }
    catch (const std::exception &e)
    {
        throw std::runtime_error(
            QString("Failed to remove train network: %1")
                .arg(e.what())
                .toStdString());
        return false;
    }
}

bool RegionData::removeTruckNetwork(const QString &name)
{
    qCDebug(lcController) << "RegionData::removeTruckNetwork:"
                          << name << "region=" << m_region;
    // Check if network exists using NetworkController
    if (!truckNetworkExists(name))
    {
        throw std::runtime_error(
            QString(
                "Truck network '%1' not found in region")
                .arg(name)
                .toStdString());
        return false;
    }

    try
    {
        // Remove from NetworkController (this will delete
        // the config)
        if (!m_networkController->removeTruckNetworkConfig(
                name, m_region))
        {
            throw std::runtime_error(
                "Failed to remove network config "
                "from NetworkController");

            return false;
        }

        // Notify listeners
        emit truckNetworkRemoved(name);

        return true;
    }
    catch (const std::exception &e)
    {
        throw std::runtime_error(
            QString("Failed to remove truck network: %1")
                .arg(e.what())
                .toStdString());

        return false;
    }
}

bool RegionData::trainNetworkExists(
    const QString &name) const
{
    // Use NetworkController to check if the train network
    // exists
    return m_networkController->trainNetworkExists(
        name, m_region);
}

bool RegionData::truckNetworkExists(
    const QString &name) const
{
    // Use NetworkController to check if the truck network
    // exists
    return m_networkController->truckNetworkExists(
        name, m_region);
}

TrainClient::NeTrainSimNetwork *
RegionData::getTrainNetwork(const QString &name) const
{
    // Use NetworkController to get the train network
    return m_networkController->trainNetwork(name,
                                             m_region);
}

TruckClient::IntegrationNetwork *
RegionData::getTruckNetwork(const QString &name) const
{
    // Use NetworkController to get the truck network
    return m_networkController->truckNetwork(name,
                                             m_region);
}

TruckClient::IntegrationSimulationConfig *
RegionData::getTruckNetworkConfig(const QString &name) const
{
    // Use NetworkController to get the truck network config
    return m_networkController->truckNetworkConfig(
        name, m_region);
}

QObject *RegionData::findNetworkByName(const QString     &name,
                                       NetworkKind *outKind) const
{
    if (auto *rail = getTrainNetwork(name))
    {
        if (outKind) *outKind = NetworkKind::Rail;
        return rail;
    }
    if (auto *truck = getTruckNetwork(name))
    {
        if (outKind) *outKind = NetworkKind::Truck;
        return truck;
    }
    return nullptr;
}

QStringList RegionData::getTrainNetworks() const
{
    // Get train network names from NetworkController
    return m_networkController->trainNetworkNamesInRegion(
        m_region);
}

QStringList RegionData::getTruckNetworks() const
{
    // Get truck network names from NetworkController
    return m_networkController->truckNetworkNamesInRegion(
        m_region);
}

QMap<QString, QVariant> RegionData::toMap() const
{
    QMap<QString, QVariant> map;

    map["region"] = m_region;

    // Serialize the network lists using NetworkController
    QVariantList trainList;
    for (const QString &network : getTrainNetworks())
    {
        trainList.append(network);
    }
    map["rail_networks"] = trainList;

    QVariantList truckList;
    for (const QString &network : getTruckNetworks())
    {
        truckList.append(network);
    }
    map["truck_networks"] = truckList;

    // Add the variables map
    map["variables"] = QVariant::fromValue(m_variables);

    return map;
}

RegionData *
RegionData::fromMap(const QMap<QString, QVariant> &data,
                    NetworkController *networkController,
                    QObject           *parent)
{
    QString regionName = data["region"].toString();

    // Create new RegionData - networks will be managed by
    // NetworkController
    RegionData *regionData = new RegionData(
        regionName, networkController, parent);

    // Deserialize the variables if they exist
    if (data.contains("variables"))
    {
        regionData->m_variables = data["variables"].toMap();
    }

    // NOTE: We don't deserialize the network lists here as
    // they're now managed by NetworkController. The
    // NetworkController should be populated separately.

    return regionData;
}

// ---------------------------------------------------------------------------

// RegionDataController implementation

RegionDataController::RegionDataController(
    NetworkController *networkController, QObject *parent)
    : QObject(parent)
    , m_networkController(networkController)
{
}

RegionDataController::~RegionDataController()
{
    // Do not call clear() during destruction. Three reasons:
    //   1. m_networkController is a sibling under the same QObject
    //      parent and may already have been destroyed (Qt destroys
    //      children FIFO, so the controller created before us is
    //      destroyed before us). Calling m_networkController->clear()
    //      here is use-after-free.
    //   2. Regions are QObject children of this controller; Qt's
    //      ~QObject deletes them automatically after this body
    //      finishes. Calling qDeleteAll ourselves would be fine but
    //      redundant.
    //   3. Emitting regionsCleared during destruction invokes slots
    //      in GUI widgets that may call back into the singleton,
    //      violating lifetime invariants during teardown.
    m_networkController = nullptr;
}

RegionData *
RegionDataController::getRegionData(const QString &name)
{
    if (!m_regions.contains(name))
    {
        qCWarning(lcController) << "RegionDataController::getRegionData:"
                                << "region not found:" << name;
    }
    return m_regions.value(name, nullptr);
}

QStringList RegionDataController::getAllRegionNames() const
{
    return m_regions.keys();
}

bool RegionDataController::addRegion(const QString &name)
{
    qCDebug(lcController) << "RegionDataController::addRegion:" << name;
    if (m_regions.contains(name))
    {
        return false;
    }

    m_regions[name] =
        new RegionData(name, m_networkController, this);

    // Emit signal that a new region was added
    emit regionAdded(name);

    return true;
}

bool RegionDataController::renameRegion(
    const QString &oldName, const QString &newName)
{
    if (!m_regions.contains(oldName)
        || m_regions.contains(newName))
    {
        return false;
    }

    // Get the RegionData object
    RegionData *data = m_regions.take(oldName);

    // Update name in RegionData (this will use
    // NetworkController to update networks)
    data->setRegionName(newName);

    // Update our map
    m_regions[newName] = data;

    // Update current region if it was renamed
    if (m_currentRegion == oldName)
    {
        m_currentRegion = newName;
        // Emit that current region has changed
        emit currentRegionChanged(newName);
    }

    // Emit signal that a region was renamed
    emit regionRenamed(oldName, newName);

    return true;
}

bool RegionDataController::removeRegion(const QString &name)
{
    qCDebug(lcController) << "RegionDataController::removeRegion:" << name;
    if (!m_regions.contains(name))
    {
        return false;
    }

    // Check if we're removing the current region
    bool isCurrentRegion = (m_currentRegion == name);

    // Get region data
    RegionData *data = m_regions.take(name);

    // Clear all networks in this region using
    // NetworkController
    m_networkController->clearRegion(name);

    // Delete the region data object
    delete data;

    // Emit signal that a region was removed
    emit regionRemoved(name);

    // If current region was removed, update it and notify
    if (isCurrentRegion)
    {
        m_currentRegion = QString();
        emit currentRegionChanged(m_currentRegion);
    }

    return true;
}

RegionData *
RegionDataController::getCurrentRegionData() const
{
    if (m_currentRegion.isEmpty())
    {
        return nullptr;
    }
    return m_regions.value(m_currentRegion, nullptr);
}

bool RegionDataController::setCurrentRegion(
    const QString &name)
{
    qCDebug(lcController) << "RegionDataController::setCurrentRegion:" << name;
    // If name is empty, clear the current region
    if (name.isEmpty())
    {
        if (!m_currentRegion.isEmpty())
        {
            m_currentRegion = QString();
            emit currentRegionChanged(m_currentRegion);
        }
        return true;
    }

    // Check if the region exists
    if (!m_regions.contains(name))
    {
        return false;
    }

    // Set the current region if it's different
    if (m_currentRegion != name)
    {
        m_currentRegion = name;
        emit currentRegionChanged(m_currentRegion);
    }

    return true;
}

void RegionDataController::clear()
{
    qCInfo(lcController) << "RegionDataController::clear:"
                         << m_regions.size() << "regions";
    // Use NetworkController to clear all networks
    m_networkController->clear();

    // Delete all RegionData objects
    qDeleteAll(m_regions);
    m_regions.clear();

    m_currentRegion = QString(); // Reset current region
    m_globalVariables.clear();   // Clear global variables

    // Emit signals
    emit regionsCleared();
    emit currentRegionChanged(m_currentRegion);
}

QMap<QString, QVariant> RegionDataController::toMap() const
{
    QMap<QString, QVariant> map;
    QMap<QString, QVariant> regionsMap;

    for (auto it = m_regions.constBegin();
         it != m_regions.constEnd(); ++it)
    {
        regionsMap[it.key()] = it.value()->toMap();
    }

    map["regions"]        = QVariant::fromValue(regionsMap);
    map["current_region"] = m_currentRegion;

    // Add global variables
    map["global_variables"] =
        QVariant::fromValue(m_globalVariables);

    return map;
}

bool RegionDataController::fromMap(
    NetworkController             *networkController,
    const QMap<QString, QVariant> &data)
{
    qCDebug(lcController) << "RegionDataController::fromMap: deserializing";
    // Clear existing data (this will emit regionsCleared)
    clear();

    m_networkController = networkController;

    try
    {
        QMap<QString, QVariant> regionsMap =
            data["regions"].toMap();

        for (auto it = regionsMap.constBegin();
             it != regionsMap.constEnd(); ++it)
        {
            QString                 regionName = it.key();
            QMap<QString, QVariant> regionData =
                it.value().toMap();

            // Create RegionData object from serialized data
            RegionData *region = RegionData::fromMap(
                regionData, networkController, this);
            m_regions[regionName] = region;

            // Emit signal for each region added
            emit regionAdded(regionName);
        }

        // Restore current region if it exists
        if (data.contains("current_region"))
        {
            QString newCurrentRegion =
                data["current_region"].toString();
            if (!newCurrentRegion.isEmpty()
                && m_regions.contains(newCurrentRegion))
            {
                m_currentRegion = newCurrentRegion;
                emit currentRegionChanged(m_currentRegion);
            }
        }

        // Restore global variables if they exist
        if (data.contains("global_variables"))
        {
            m_globalVariables =
                data["global_variables"].toMap();
        }

        return true;
    }
    catch (const std::exception &e)
    {
        qCWarning(lcController) << "Error deserializing regions data:"
                   << e.what();
        clear();
        return false;
    }
}

} // namespace Backend
} // namespace CargoNetSim
