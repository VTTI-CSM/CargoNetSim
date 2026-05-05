// NetworkController.cpp
#include "NetworkController.h"
#include "Backend/Commons/LogCategories.h"

namespace CargoNetSim
{
namespace Backend
{

NetworkController::NetworkController(QObject *parent)
    : QObject(parent)
{
}

NetworkController::~NetworkController()
{
    // Clean up all train networks
    QWriteLocker trainLocker(&m_trainNetworksLock);
    for (auto &regionNetworks : m_trainNetworks)
    {
        qDeleteAll(regionNetworks);
    }
    m_trainNetworks.clear();
    trainLocker.unlock();

    // Clean up all truck network configs
    QWriteLocker truckLocker(&m_truckNetworkConfigsLock);
    for (auto &regionConfigs : m_truckNetworkConfigs)
    {
        qDeleteAll(regionConfigs);
    }
    m_truckNetworkConfigs.clear();
}

bool NetworkController::addTrainNetwork(
    const QString &name, const QString &region,
    TrainClient::NeTrainSimNetwork *network)
{
    if (!network)
    {
        qCWarning(lcControllerNetwork) << "Attempted to add null train network:"
                   << name << "in region" << region;
        return false;
    }

    QWriteLocker locker(&m_trainNetworksLock);

    // Check if network with this name already exists in the
    // region
    if (m_trainNetworks.contains(region)
        && m_trainNetworks[region].contains(name))
    {
        qCWarning(lcControllerNetwork) << "Train network with name" << name
                   << "already exists in region" << region;
        return false;
    }

    // Add the network
    m_trainNetworks[region][name] = network;
    locker.unlock();

    // Set the network name
    network->setNetworkName(name);

    // Take ownership of the network
    network->setParent(this);

    // Emit signal
    emit trainNetworkAdded(name, region);
    return true;
}

bool NetworkController::addTruckNetworkConfig(
    const QString &name, const QString &region,
    TruckClient::IntegrationSimulationConfig *config)
{
    if (!config)
    {
        qCWarning(lcControllerNetwork)
            << "Attempted to add null truck network config:"
            << name << "in region" << region;
        return false;
    }

    QWriteLocker locker(&m_truckNetworkConfigsLock);

    // Check if config with this name already exists in the
    // region
    if (m_truckNetworkConfigs.contains(region)
        && m_truckNetworkConfigs[region].contains(name))
    {
        qCWarning(lcControllerNetwork) << "Truck network config with name"
                   << name << "already exists in region"
                   << region;
        return false;
    }

    // Add the config
    m_truckNetworkConfigs[region][name] = config;
    locker.unlock();

    // Set the network name in the underlying network
    if (config->getNetwork())
    {
        config->getNetwork()->setNetworkName(name);
    }

    // Take ownership of the config
    config->setParent(this);

    // Emit signal
    emit truckNetworkConfigAdded(name, region);
    return true;
}

TrainClient::NeTrainSimNetwork *
NetworkController::trainNetwork(const QString &name,
                                const QString &region) const
{
    QReadLocker locker(&m_trainNetworksLock);

    if (!m_trainNetworks.contains(region)
        || !m_trainNetworks[region].contains(name))
    {
        return nullptr;
    }

    return m_trainNetworks[region][name];
}

TruckClient::IntegrationSimulationConfig *
NetworkController::truckNetworkConfig(
    const QString &name, const QString &region) const
{
    QReadLocker locker(&m_truckNetworkConfigsLock);

    if (!m_truckNetworkConfigs.contains(region)
        || !m_truckNetworkConfigs[region].contains(name))
    {
        return nullptr;
    }

    return m_truckNetworkConfigs[region][name];
}

TruckClient::IntegrationNetwork *
NetworkController::truckNetwork(const QString &name,
                                const QString &region) const
{
    QReadLocker locker(&m_truckNetworkConfigsLock);

    if (!m_truckNetworkConfigs.contains(region)
        || !m_truckNetworkConfigs[region].contains(name))
    {
        return nullptr;
    }

    TruckClient::IntegrationSimulationConfig *config =
        m_truckNetworkConfigs[region][name];
    locker.unlock();

    // Get the network from the config
    return config->getNetwork();
}

bool NetworkController::removeTrainNetwork(
    const QString &name, const QString &region)
{
    QWriteLocker locker(&m_trainNetworksLock);

    if (!m_trainNetworks.contains(region)
        || !m_trainNetworks[region].contains(name))
    {
        return false;
    }

    // Store pointer to delete after removal from map
    TrainClient::NeTrainSimNetwork *network =
        m_trainNetworks[region][name];

    // Remove from map
    m_trainNetworks[region].remove(name);

    // Clean up empty regions
    if (m_trainNetworks[region].isEmpty())
    {
        m_trainNetworks.remove(region);
    }

    locker.unlock();

    // Delete the network
    delete network;

    // Emit signal
    emit trainNetworkRemoved(name, region);
    return true;
}

bool NetworkController::removeTruckNetworkConfig(
    const QString &name, const QString &region)
{
    QWriteLocker locker(&m_truckNetworkConfigsLock);

    if (!m_truckNetworkConfigs.contains(region)
        || !m_truckNetworkConfigs[region].contains(name))
    {
        return false;
    }

    // Store pointer to delete after removal from map
    TruckClient::IntegrationSimulationConfig *config =
        m_truckNetworkConfigs[region][name];

    // Remove from map
    m_truckNetworkConfigs[region].remove(name);

    // Clean up empty regions
    if (m_truckNetworkConfigs[region].isEmpty())
    {
        m_truckNetworkConfigs.remove(region);
    }

    locker.unlock();

    // Delete the config (which will also delete the
    // network)
    delete config;

    // Emit signal
    emit truckNetworkConfigRemoved(name, region);
    return true;
}

bool NetworkController::renameTrainNetwork(
    const QString &oldName, const QString &newName,
    const QString &region)
{
    QWriteLocker locker(&m_trainNetworksLock);

    // Check if source network exists
    if (!m_trainNetworks.contains(region)
        || !m_trainNetworks[region].contains(oldName))
    {
        qCWarning(lcControllerNetwork) << "Cannot rename train network: source"
                   << oldName << "not found in region"
                   << region;
        return false;
    }

    // Check if destination name already exists
    if (m_trainNetworks[region].contains(newName))
    {
        qCWarning(lcControllerNetwork)
            << "Cannot rename train network: destination"
            << newName << "already exists in region"
            << region;
        return false;
    }

    // Get the network
    TrainClient::NeTrainSimNetwork *network =
        m_trainNetworks[region][oldName];

    // Update internal name if network has such property
    network->setNetworkName(newName);

    // Move it to the new key
    m_trainNetworks[region][newName] = network;
    m_trainNetworks[region].remove(oldName);

    locker.unlock();

    // Emit signal
    emit trainNetworkRenamed(oldName, newName, region);

    return true;
}

bool NetworkController::renameTruckNetworkConfig(
    const QString &oldName, const QString &newName,
    const QString &region)
{
    QWriteLocker locker(&m_truckNetworkConfigsLock);

    // Check if source network exists
    if (!m_truckNetworkConfigs.contains(region)
        || !m_truckNetworkConfigs[region].contains(oldName))
    {
        qCWarning(lcControllerNetwork) << "Cannot rename truck network: source"
                   << oldName << "not found in region"
                   << region;
        return false;
    }

    // Check if destination name already exists
    if (m_truckNetworkConfigs[region].contains(newName))
    {
        qCWarning(lcControllerNetwork)
            << "Cannot rename truck network: destination"
            << newName << "already exists in region"
            << region;
        return false;
    }

    // Get the config
    TruckClient::IntegrationSimulationConfig *config =
        m_truckNetworkConfigs[region][oldName];

    // Update internal name if network has such property
    if (config->getNetwork())
    {
        config->getNetwork()->setNetworkName(newName);
    }

    // Move it to the new key
    m_truckNetworkConfigs[region][newName] = config;
    m_truckNetworkConfigs[region].remove(oldName);

    locker.unlock();

    // Emit signal
    emit truckNetworkConfigRenamed(oldName, newName,
                                   region);

    return true;
}

QMap<QString, TrainClient::NeTrainSimNetwork *>
NetworkController::trainNetworksInRegion(
    const QString &region) const
{
    QReadLocker locker(&m_trainNetworksLock);

    if (!m_trainNetworks.contains(region))
    {
        return QMap<QString,
                    TrainClient::NeTrainSimNetwork *>();
    }

    return m_trainNetworks[region];
}

QMap<QString, TruckClient::IntegrationSimulationConfig *>
NetworkController::truckNetworkConfigsInRegion(
    const QString &region) const
{
    QReadLocker locker(&m_truckNetworkConfigsLock);

    if (!m_truckNetworkConfigs.contains(region))
    {
        return QMap<
            QString,
            TruckClient::IntegrationSimulationConfig *>();
    }

    return m_truckNetworkConfigs[region];
}

QStringList NetworkController::regions() const
{
    QStringList result;

    {
        QReadLocker trainLocker(&m_trainNetworksLock);
        result = m_trainNetworks.keys();
    }

    {
        QReadLocker truckLocker(&m_truckNetworkConfigsLock);
        for (const QString &region :
             m_truckNetworkConfigs.keys())
        {
            if (!result.contains(region))
            {
                result.append(region);
            }
        }
    }

    return result;
}

void NetworkController::clear()
{
    clearTrainNetworks();
    clearTruckNetworks();
}

int NetworkController::clearTrainNetworks()
{
    QWriteLocker locker(&m_trainNetworksLock);

    int count = 0;
    for (auto &regionNetworks : m_trainNetworks)
    {
        count += regionNetworks.size();
        qDeleteAll(regionNetworks);
    }
    m_trainNetworks.clear();

    return count;
}

int NetworkController::clearTruckNetworks()
{
    QWriteLocker locker(&m_truckNetworkConfigsLock);

    int count = 0;
    for (auto &regionConfigs : m_truckNetworkConfigs)
    {
        count += regionConfigs.size();
        qDeleteAll(regionConfigs);
    }
    m_truckNetworkConfigs.clear();

    return count;
}

// New methods implementation

bool NetworkController::networkExistsInRegion(
    const QString &name, const QString &region) const
{
    return trainNetworkExists(name, region)
           || truckNetworkExists(name, region);
}

bool NetworkController::renameRegion(
    const QString &oldRegion, const QString &newRegion)
{
    if (oldRegion == newRegion)
    {
        return true; // Nothing to do
    }

    QStringList allRegions = regions();
    if (allRegions.contains(newRegion))
    {
        qCWarning(lcControllerNetwork)
            << "Cannot rename region: destination region"
            << newRegion << "already exists";
        return false;
    }

    // Move train networks
    {
        QWriteLocker locker(&m_trainNetworksLock);
        if (m_trainNetworks.contains(oldRegion))
        {
            m_trainNetworks[newRegion] =
                m_trainNetworks[oldRegion];
            m_trainNetworks.remove(oldRegion);
        }
    }

    // Move truck networks
    {
        QWriteLocker locker(&m_truckNetworkConfigsLock);
        if (m_truckNetworkConfigs.contains(oldRegion))
        {
            m_truckNetworkConfigs[newRegion] =
                m_truckNetworkConfigs[oldRegion];
            m_truckNetworkConfigs.remove(oldRegion);
        }
    }

    // Emit the signal that region was renamed
    emit regionRenamed(oldRegion, newRegion);

    return true;
}

QStringList NetworkController::trainNetworkNamesInRegion(
    const QString &region) const
{
    QReadLocker locker(&m_trainNetworksLock);

    if (!m_trainNetworks.contains(region))
    {
        return QStringList();
    }

    return m_trainNetworks[region].keys();
}

QStringList NetworkController::truckNetworkNamesInRegion(
    const QString &region) const
{
    QReadLocker locker(&m_truckNetworkConfigsLock);

    if (!m_truckNetworkConfigs.contains(region))
    {
        return QStringList();
    }

    return m_truckNetworkConfigs[region].keys();
}

bool NetworkController::trainNetworkExists(
    const QString &name, const QString &region) const
{
    QReadLocker locker(&m_trainNetworksLock);

    return m_trainNetworks.contains(region)
           && m_trainNetworks[region].contains(name);
}

bool NetworkController::truckNetworkExists(
    const QString &name, const QString &region) const
{
    QReadLocker locker(&m_truckNetworkConfigsLock);

    return m_truckNetworkConfigs.contains(region)
           && m_truckNetworkConfigs[region].contains(name);
}

int NetworkController::clearRegion(const QString &region)
{
    int count = 0;

    // Clear train networks
    {
        QWriteLocker locker(&m_trainNetworksLock);
        if (m_trainNetworks.contains(region))
        {
            count += m_trainNetworks[region].size();
            qDeleteAll(m_trainNetworks[region]);
            m_trainNetworks.remove(region);
        }
    }

    // Clear truck networks
    {
        QWriteLocker locker(&m_truckNetworkConfigsLock);
        if (m_truckNetworkConfigs.contains(region))
        {
            count += m_truckNetworkConfigs[region].size();
            qDeleteAll(m_truckNetworkConfigs[region]);
            m_truckNetworkConfigs.remove(region);
        }
    }

    if (count > 0)
    {
        emit regionCleared(region);
    }

    return count;
}

} // namespace Backend
} // namespace CargoNetSim
