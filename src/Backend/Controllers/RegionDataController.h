/**
 * @file RegionDataController.h
 * @brief Manages simulation regions and their associated
 *        data.
 * @author Ahmed Aredah
 * @date 2025-03-19
 * @copyright Copyright (c) 2025
 */

#pragma once

#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>

#include "Backend/Commons/NetworkKind.h"
#include "NetworkController.h"

// Forward declarations
namespace CargoNetSim
{
class CargoNetSimController;
}

namespace CargoNetSim
{
namespace Backend
{

/**
 * @class RegionData
 * @brief Stores and manages data for a specific region
 *        in the simulation.
 *
 * This class encapsulates all region-specific data,
 * associated networks, and provides methods to manage
 * the region's resources.
 */
class RegionData : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructor for RegionData.
     * @param regionName The name of the region.
     * @param networkController Pointer to the network
     * controller.
     * @param parent The parent QObject.
     */
    RegionData(const QString     &regionName,
               NetworkController *networkController,
               QObject           *parent = nullptr);

    /**
     * @brief Destructor for RegionData.
     */
    ~RegionData();

    // Delete copy constructor and assignment operator
    RegionData(const RegionData &)            = delete;
    RegionData &operator=(const RegionData &) = delete;

    /**
     * @brief Check if a network name exists in either train
     *        or truck networks.
     * @param name The network name to check.
     * @return True if the name conflicts with existing
     *         network names.
     */
    bool
    checkNetworkNameConflict(const QString &name) const;

    /**
     * @brief Set the region name and update related
     *        references.
     * @param name The new region name.
     */
    void setRegionName(const QString &name);

    /**
     * @brief Add a train network to the region.
     * @param networkName Name for the new network.
     * @param nodeFile Path to the node definition file.
     * @param linkFile Path to the link definition file.
     * @throws std::runtime_error if the network already
     *         exists or creation fails.
     */
    void addTrainNetwork(const QString &networkName,
                         const QString &nodeFile,
                         const QString &linkFile);

    /**
     * @brief Add a truck network to the region.
     * @param networkName Name for the new network.
     * @param configFile Path to the configuration file.
     * @throws std::runtime_error if the network already
     *         exists or creation fails.
     */
    void addTruckNetwork(const QString &networkName,
                         const QString &configFile);

    /**
     * @brief Rename a train network.
     * @param oldName Current name of the network.
     * @param newName New name for the network.
     * @return True if the network was renamed, false if O.W
     * @throws std::runtime_error if the old name doesn't
     *         exist or the new name already exists.
     */
    bool renameTrainNetwork(const QString &oldName,
                            const QString &newName);

    /**
     * @brief Rename a truck network.
     * @param oldName Current name of the network.
     * @param newName New name for the network.
     * @return True if the network was renamed, false if O.W
     * @throws std::runtime_error if the old name doesn't
     *         exist or the new name already exists.
     */
    bool renameTruckNetwork(const QString &oldName,
                            const QString &newName);

    /**
     * @brief Remove a train network.
     * @param name Name of the network to remove.
     * @throws std::runtime_error if the network doesn't
     *         exist.
     *
     * @return True if the network was removed. False O.W.
     */
    bool removeTrainNetwork(const QString &name);

    /**
     * @brief Remove a truck network.
     * @param name Name of the network to remove.
     * @throws std::runtime_error if the network doesn't
     *         exist.
     *
     * @return True if the network was removed. False O.W.
     */
    bool removeTruckNetwork(const QString &name);

    /**
     * @brief Check if a train network exists.
     * @param name Name of the network to check.
     * @return True if the network exists.
     */
    bool trainNetworkExists(const QString &name) const;

    /**
     * @brief Check if a truck network exists.
     * @param name Name of the network to check.
     * @return True if the network exists.
     */
    bool truckNetworkExists(const QString &name) const;

    /**
     * @brief Get a train network by name.
     * @param name Name of the network to retrieve.
     * @return Pointer to the network, or nullptr if not
     *         found.
     */
    TrainClient::NeTrainSimNetwork *
    getTrainNetwork(const QString &name) const;

    /**
     * @brief Get a truck network by name.
     * @param name Name of the network to retrieve.
     * @return Pointer to the network, or nullptr if not
     *         found.
     */
    TruckClient::IntegrationNetwork *
    getTruckNetwork(const QString &name) const;

    /**
     * @brief Get a truck network configuration by name.
     * @param name Name of the network to retrieve.
     * @return Pointer to the truck network configuration,
     *         or nullptr if not found.
     */
    TruckClient::IntegrationSimulationConfig *
    getTruckNetworkConfig(const QString &name) const;

    /**
     * @brief Locate a network by name regardless of kind.
     *
     * Centralises the "try rail first, then truck" dispatch so callers
     * never duplicate the two-lookup dance. The per-region validator
     * rule forbids the same name appearing with both kinds, so the
     * rail-first preference is unambiguous on well-formed scenarios.
     *
     * @param name     Network name to look up.
     * @param outKind  Optional out-param — if non-null and a network is
     *                 found, set to the matching kind.
     * @return QObject pointer to the network (downcastable by @p outKind),
     *         or nullptr if neither a rail nor a truck network of that
     *         name exists in this region.
     */
    QObject *findNetworkByName(const QString     &name,
                               NetworkKind *outKind = nullptr) const;

    /**
     * @brief Store a variable in this region's data store.
     * @param key The name/key of the variable.
     * @param value The value to store.
     */
    void setVariable(const QString  &key,
                     const QVariant &value)
    {
        m_variables[key] = value;
    }

    /**
     * @brief Retrieve a variable from this region's data
     *        store.
     * @param key The name/key of the variable to retrieve.
     * @param defaultValue The default value to return if
     *        the key doesn't exist.
     * @return The stored value or defaultValue if not
     *         found.
     */
    QVariant getVariable(
        const QString  &key,
        const QVariant &defaultValue = QVariant()) const
    {
        return m_variables.value(key, defaultValue);
    }

    /**
     * @brief Retrieve a variable from this region's data
     *        store with type conversion.
     * @tparam T The type to convert the value to.
     * @param key The name/key of the variable to retrieve.
     * @param defaultValue The default value to return if
     *        the key doesn't exist.
     * @return The stored value converted to type T or
     *         defaultValue if not found.
     */
    template <typename T>
    T getVariableAs(const QString &key,
                    const T       &defaultValue = T()) const
    {
        QVariant var = m_variables.value(key, QVariant());
        if (var.isValid() && var.canConvert<T>())
        {
            return var.value<T>();
        }
        return defaultValue;
    }

    /**
     * @brief Check if a variable exists in this region's
     *        data store.
     * @param key The name/key of the variable to check.
     * @return True if the variable exists.
     */
    bool hasVariable(const QString &key) const
    {
        return m_variables.contains(key);
    }

    /**
     * @brief Remove a variable from this region's data
     *        store.
     * @param key The name/key of the variable to remove.
     * @return True if the variable was removed, false if
     *         it didn't exist.
     */
    bool removeVariable(const QString &key)
    {
        return m_variables.remove(key) > 0;
    }

    /**
     * @brief Get all m_variables for this region.
     * @return A copy of the m_variables map.
     */
    QVariantMap getAllVariables() const
    {
        return m_variables;
    }

    /**
     * @brief Convert the region data to a serializable map.
     * @return A QMap containing all region data.
     */
    QMap<QString, QVariant> toMap() const;

    /**
     * @brief Create a RegionData instance from a
     *        serialized map.
     * @param data The map containing serialized region
     *        data.
     * @param parent The parent QObject.
     * @return A new RegionData instance with the
     *         deserialized data.
     */
    static RegionData *
    fromMap(const QMap<QString, QVariant> &data,
            NetworkController *networkController,
            QObject           *parent = nullptr);

    /**
     * @brief Get the m_region name.
     * @return The name of the m_region.
     */
    const QString &getRegion() const
    {
        return m_region;
    }

    /**
     * @brief Get the list of train network names in this
     *        region.
     * @return QStringList of train network names.
     */
    QStringList getTrainNetworks() const;

    /**
     * @brief Get the list of truck network names in this
     *        region.
     * @return QStringList of truck network names.
     */
    QStringList getTruckNetworks() const;

signals:
    /**
     * @brief Signal emitted when a train network is added
     *        to the region.
     * @param networkName Name of the added network.
     */
    void trainNetworkAdded(const QString &networkName);

    /**
     * @brief Signal emitted when a train network is
     *        removed from the region.
     * @param networkName Name of the removed network.
     */
    void trainNetworkRemoved(const QString &networkName);

    /**
     * @brief Signal emitted when a train network is
     *        renamed.
     * @param oldName Previous name of the network.
     * @param newName New name of the network.
     */
    void trainNetworkRenamed(const QString &oldName,
                             const QString &newName);

    /**
     * @brief Signal emitted when a truck network is added
     *        to the region.
     * @param networkName Name of the added network.
     */
    void truckNetworkAdded(const QString &networkName);

    /**
     * @brief Signal emitted when a truck network is
     *        removed from the region.
     * @param networkName Name of the removed network.
     */
    void truckNetworkRemoved(const QString &networkName);

    /**
     * @brief Signal emitted when a truck network is
     *        renamed.
     * @param oldName Previous name of the network.
     * @param newName New name of the network.
     */
    void truckNetworkRenamed(const QString &oldName,
                             const QString &newName);

private:
    /** @brief Name of the region */
    QString m_region;

    /** @brief Map of custom variables for this region */
    QVariantMap m_variables;

    /** @brief Store the network controller pointer */
    NetworkController *m_networkController;
};

/**
 * @class RegionDataController
 * @brief Manages all regions in the simulation.
 *
 * This class maintains the collection of regions and
 * provides methods to add, remove, and query regions and
 * their associated data.
 */
class RegionDataController : public QObject
{
    Q_OBJECT

    // Make the CargoNetSimController a friend
    friend class CargoNetSim::CargoNetSimController;

public:
    /**
     * @brief Constructor for RegionDataController.
     * @param networkController Pointer to the network
     * controller.
     * @param parent Optional parent QObject.
     */
    explicit RegionDataController(
        NetworkController *networkController,
        QObject           *parent = nullptr);

    /**
     * @brief Destructor for RegionDataController.
     */
    ~RegionDataController();

    // Delete copy constructor and assignment operator
    RegionDataController(const RegionDataController &) =
        delete;
    RegionDataController &
    operator=(const RegionDataController &) = delete;

    /**
     * @brief Get region data for a specific region.
     * @param name The name of the region.
     * @return Pointer to the region data, or nullptr if
     *         not found.
     */
    RegionData *getRegionData(const QString &name);

    /**
     * @brief Get a list of all region names.
     * @return QStringList containing all region names.
     */
    QStringList getAllRegionNames() const;

    /**
     * @brief Add a new region.
     * @param name The name of the new region.
     * @return True if the region was added successfully.
     */
    bool addRegion(const QString &name);

    /**
     * @brief Rename a region.
     * @param oldName Current name of the region.
     * @param newName New name for the region.
     * @return True if the region was renamed successfully.
     */
    bool renameRegion(const QString &oldName,
                      const QString &newName);

    /**
     * @brief Remove a region.
     * @param name Name of the region to remove.
     * @return True if the region was removed successfully.
     */
    bool removeRegion(const QString &name);

    /**
     * @brief Get the current active region name.
     * @return Current region name, or empty string if none
     *         selected.
     */
    QString getCurrentRegion() const
    {
        return m_currentRegion;
    }

    /**
     * @brief Set the current active region.
     * @param name The name of the region to set as current.
     * @return True if the region exists and was set as
     *         current, false otherwise.
     */
    bool setCurrentRegion(const QString &name);

    /**
     * @brief Get the current active region data.
     * @return Pointer to the current region data, or
     *         nullptr if none selected.
     */
    RegionData *getCurrentRegionData() const;

    /**
     * @brief Store a global variable not associated with
     *        any specific region.
     * @param key The name/key of the variable.
     * @param value The value to store.
     */
    void setGlobalVariable(const QString  &key,
                           const QVariant &value)
    {
        m_globalVariables[key] = value;
        emit globalVariableChanged(key, value);
    }

    /**
     * @brief Check if a global variable exists.
     * @param key The name/key of the variable to check.
     * @return True if the variable exists.
     */
    bool hasGlobalVariable(const QString &key) const
    {
        return m_globalVariables.contains(key);
    }

    /**
     * @brief Remove a global variable.
     * @param key The name/key of the variable to remove.
     * @return True if the variable was removed, false if
     *         it didn't exist.
     */
    bool removeGlobalVariable(const QString &key)
    {
        if (m_globalVariables.contains(key))
        {
            m_globalVariables.remove(key);
            emit globalVariableRemoved(key);
            return true;
        }
        return false;
    }

    /**
     * @brief Get all global variables.
     * @return A copy of the global variables map.
     */
    QVariantMap getAllGlobalVariables() const
    {
        return m_globalVariables;
    }

    /**
     * @brief Set a variable for a specific region.
     * @param regionName The name of the region.
     * @param key The name/key of the variable.
     * @param value The value to store.
     * @return True if the region exists and variable was
     *         set, false otherwise.
     */
    bool setRegionVariable(const QString  &regionName,
                           const QString  &key,
                           const QVariant &value)
    {
        RegionData *region = getRegionData(regionName);
        if (region)
        {
            region->setVariable(key, value);
            emit regionVariableChanged(regionName, key,
                                       value);
            return true;
        }
        return false;
    }

    /**
     * @brief Retrieve a global variable with type
     *        conversion.
     * @tparam T The type to convert the value to.
     * @param key The name/key of the variable to retrieve.
     * @param defaultValue The default value to return if
     *        the key doesn't exist.
     * @return The stored value converted to type T or
     *         defaultValue if not found.
     */
    template <typename T>
    T getGlobalVariableAs(const QString &key,
                          const T &defaultValue = T()) const
    {
        QVariant var =
            m_globalVariables.value(key, QVariant());
        if (var.isValid() && var.canConvert<T>())
        {
            return var.value<T>();
        }
        return defaultValue;
    }

    /**
     * @brief Get a specific variable from all m_regions
     * with its region name as key.
     * @tparam T The type to convert the value to.
     * @param key The name/key of the variable to retrieve.
     * @return A map of region names to variable values for
     *         m_regions that have the variable.
     */
    template <typename T>
    QMap<QString, T>
    getAllRegionVariableAs(const QString &key) const
    {
        QMap<QString, T> result;
        for (auto it = m_regions.cbegin();
             it != m_regions.cend(); ++it)
        {
            QVariant var = it.value()->getVariable(key);
            if (var.isValid() && var.canConvert<T>())
            {
                result.insert(it.key(), var.value<T>());
            }
        }
        return result;
    }

    /**
     * @brief Get a variable from a specific region with
     *        type conversion.
     * @tparam T The type to convert the value to.
     * @param regionName The name of the region.
     * @param key The name/key of the variable.
     * @param defaultValue The default value to return if
     *        the key doesn't exist.
     * @return The stored value converted to type T,
     *         defaultValue if key not found, or default
     *         constructed T if region not found.
     */
    template <typename T>
    T getRegionVariableAs(const QString &regionName,
                          const QString &key,
                          const T &defaultValue = T()) const
    {
        auto it = m_regions.find(regionName);
        if (it != m_regions.end())
        {
            return it.value()->getVariableAs<T>(
                key, defaultValue);
        }
        return T();
    }

    /**
     * @brief Clear all region data.
     */
    void clear();

    /**
     * @brief Serialize the RegionDataController to a map.
     * @return A QMap containing all regions data.
     */
    QMap<QString, QVariant> toMap() const;

    /**
     * @brief Deserialize from a map.
     * @param data The map containing serialized regions
     *        data.
     * @return True if deserialization was successful.
     */
    bool fromMap(NetworkController *networkController,
                 const QMap<QString, QVariant> &data);

signals:
    /**
     * @brief Signal emitted when a new region is added.
     * @param regionName Name of the newly added region.
     */
    void regionAdded(const QString &regionName);

    /**
     * @brief Signal emitted when a region is renamed.
     * @param oldName Previous name of the region.
     * @param newName New name of the region.
     */
    void regionRenamed(const QString &oldName,
                       const QString &newName);

    /**
     * @brief Signal emitted when a region is removed.
     * @param regionName Name of the removed region.
     */
    void regionRemoved(const QString &regionName);

    /**
     * @brief Signal emitted when the current active region
     *        changes.
     * @param regionName New current region name (empty
     *        string if no active region).
     */
    void currentRegionChanged(const QString &regionName);

    /**
     * @brief Signal emitted when all regions are cleared.
     */
    void regionsCleared();

    /**
     * @brief Signal emitted when a global variable changes.
     * @param key The name/key of the variable that changed.
     * @param value The new value.
     */
    void globalVariableChanged(const QString  &key,
                               const QVariant &value);

    /**
     * @brief Signal emitted when a global variable is
     *        removed.
     * @param key The name/key of the variable that was
     *        removed.
     */
    void globalVariableRemoved(const QString &key);

    /**
     * @brief Signal emitted when a region-specific
     *        variable changes.
     * @param regionName The name of the region.
     * @param key The name/key of the variable that changed.
     * @param value The new value.
     */
    void regionVariableChanged(const QString  &regionName,
                               const QString  &key,
                               const QVariant &value);

private:
    /** @brief Map of region name to RegionData */
    QMap<QString, RegionData *> m_regions;

    /** @brief Stores the currently active region name */
    QString m_currentRegion;

    /** @brief Map of global variables not tied to regions
     */
    QVariantMap m_globalVariables;

    /** @brief Store the network controller pointer */
    NetworkController *m_networkController;
};

} // namespace Backend
} // namespace CargoNetSim

// Register custom types with Qt's meta-object system
Q_DECLARE_METATYPE(CargoNetSim::Backend::RegionData)
Q_DECLARE_METATYPE(CargoNetSim::Backend::RegionData *)
Q_DECLARE_METATYPE(
    CargoNetSim::Backend::RegionDataController)
Q_DECLARE_METATYPE(
    CargoNetSim::Backend::RegionDataController *)
