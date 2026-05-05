/**
 * @file ContainerManager.cpp
 * @brief Implements container tracking across vehicles
 * @author Ahmed Aredah
 * @date 2025-03-22
 */

#include "ContainerManager.h"
#include "Backend/Commons/LogCategories.h"

namespace CargoNetSim
{
namespace Backend
{
namespace TruckClient
{

ContainerManager::ContainerManager(QObject *parent)
    : QObject(parent)
{
}

void ContainerManager::assignContainersToVehicle(
    const QString                           &vehicleId,
    const QList<ContainerCore::Container *> &containers)
{
    qCDebug(lcClientTruck)
        << "ContainerManager::assignContainersToVehicle:"
        << "vehicleId=" << vehicleId
        << "containerCount=" << containers.size();

    if (containers.isEmpty())
    {
        qCWarning(lcClientTruck)
            << "ContainerManager::"
               "assignContainersToVehicle:"
            << "empty container list"
            << "vehicleId=" << vehicleId;
        return;
    }

    // Create list to track assigned container IDs for
    // signal
    QStringList containerIds;

    for (auto *container : containers)
    {
        // Skip null containers
        if (!container)
        {
            continue;
        }

        // Check if container is already assigned elsewhere
        if (m_vehicleByContainer.contains(container))
        {
            QString currentVehicle =
                m_vehicleByContainer[container];

            // Skip if already assigned to this vehicle
            if (currentVehicle == vehicleId)
            {
                continue;
            }

            // Remove from previous vehicle
            m_containersByVehicle[currentVehicle].removeOne(
                container);
        }

        // Update container location
        container->setContainerCurrentLocation(vehicleId);

        // Add to mappings
        m_containersByVehicle[vehicleId].append(container);
        m_vehicleByContainer[container] = vehicleId;

        // Add ID to signal list
        containerIds.append(container->getContainerID());
    }

    // Emit signal if any containers were assigned
    if (!containerIds.isEmpty())
    {
        qCDebug(lcClientTruck)
            << "ContainerManager::"
               "assignContainersToVehicle:"
            << "assigned" << containerIds.size()
            << "containers to vehicleId=" << vehicleId;
        emit containersAssigned(vehicleId, containerIds);
    }
    else
    {
        int skipped = containers.size();
        qCWarning(lcClientTruck)
            << "ContainerManager::"
               "assignContainersToVehicle:"
            << "all" << skipped
            << "containers were null or already assigned"
            << "vehicleId=" << vehicleId;
    }
}

QList<ContainerCore::Container *>
ContainerManager::removeContainersFromVehicle(
    const QString                           &vehicleId,
    const QList<ContainerCore::Container *> &containers)
{
    qCDebug(lcClientTruck)
        << "ContainerManager::"
           "removeContainersFromVehicle:"
        << "vehicleId=" << vehicleId
        << "containerCount=" << containers.size();

    if (!m_containersByVehicle.contains(vehicleId))
    {
        qCWarning(lcClientTruck)
            << "ContainerManager::"
               "removeContainersFromVehicle:"
            << "vehicle not found"
            << "vehicleId=" << vehicleId;
        return QList<ContainerCore::Container *>();
    }

    if (containers.isEmpty())
    {
        qCWarning(lcClientTruck)
            << "ContainerManager::"
               "removeContainersFromVehicle:"
            << "empty container list"
            << "vehicleId=" << vehicleId;
        return QList<ContainerCore::Container *>();
    }

    QList<ContainerCore::Container *> removedContainers;
    QStringList                       containerIds;

    for (auto *container : containers)
    {
        // Skip null containers
        if (!container)
        {
            continue;
        }

        // Check if container is assigned to this vehicle
        if (m_vehicleByContainer.value(container)
            == vehicleId)
        {
            // Remove from mappings
            m_containersByVehicle[vehicleId].removeOne(
                container);
            m_vehicleByContainer.remove(container);

            // Update container state
            container->setContainerCurrentLocation(
                "unassigned");

            // Add to results
            removedContainers.append(container);
            containerIds.append(
                container->getContainerID());
        }
    }

    // Emit signal if any containers were removed
    if (!containerIds.isEmpty())
    {
        emit containersRemoved(vehicleId, containerIds);
    }

    return removedContainers;
}

QList<ContainerCore::Container *>
ContainerManager::removeAllContainersFromVehicle(
    const QString &vehicleId)
{
    qCDebug(lcClientTruck)
        << "ContainerManager::"
           "removeAllContainersFromVehicle:"
        << "vehicleId=" << vehicleId;

    if (!m_containersByVehicle.contains(vehicleId))
    {
        qCWarning(lcClientTruck)
            << "ContainerManager::"
               "removeAllContainersFromVehicle:"
            << "vehicle not found"
            << "vehicleId=" << vehicleId;
        return QList<ContainerCore::Container *>();
    }

    // Get all containers for the vehicle
    QList<ContainerCore::Container *> allContainers =
        m_containersByVehicle.value(vehicleId);

    // Create container ID list for signal
    QStringList containerIds;
    for (const auto *container : allContainers)
    {
        containerIds.append(container->getContainerID());
    }

    // Update mappings
    for (auto *container : allContainers)
    {
        container->setContainerCurrentLocation(
            "unassigned");
        m_vehicleByContainer.remove(container);
    }

    m_containersByVehicle.remove(vehicleId);

    // Emit signal if any containers were removed
    if (!containerIds.isEmpty())
    {
        qCDebug(lcClientTruck)
            << "ContainerManager::"
               "removeAllContainersFromVehicle:"
            << "removed" << containerIds.size()
            << "containers from vehicleId=" << vehicleId;
        emit containersRemoved(vehicleId, containerIds);
    }

    return allContainers;
}

bool ContainerManager::transferContainers(
    const QString &sourceVehicleId,
    const QString &destVehicleId,
    const QList<ContainerCore::Container *> &containers)
{
    qCDebug(lcClientTruck)
        << "ContainerManager::transferContainers:"
        << "from=" << sourceVehicleId
        << "to=" << destVehicleId
        << "containerCount=" << containers.size();

    if (containers.isEmpty())
    {
        qCWarning(lcClientTruck)
            << "ContainerManager::transferContainers:"
            << "empty container list";
        return false;
    }

    // Validate that containers are on source vehicle
    for (const auto *container : containers)
    {
        if (!isContainerAssignedToVehicle(sourceVehicleId,
                                          container))
        {
            return false;
        }
    }

    // Remove from source vehicle (without emitting signal)
    QList<ContainerCore::Container *> transferContainers;
    QStringList                       containerIds;

    for (auto *container : containers)
    {
        // Remove from source
        m_containersByVehicle[sourceVehicleId].removeOne(
            container);
        transferContainers.append(container);
        containerIds.append(container->getContainerID());
    }

    // Add to destination vehicle
    for (auto *container : transferContainers)
    {
        container->setContainerCurrentLocation(
            destVehicleId);
        m_containersByVehicle[destVehicleId].append(
            container);
        m_vehicleByContainer[container] = destVehicleId;
    }

    // Emit transfer signal
    emit containersTransferred(sourceVehicleId,
                               destVehicleId, containerIds);

    return true;
}

QList<ContainerCore::Container *>
ContainerManager::getContainersForVehicle(
    const QString &vehicleId) const
{
    auto result = m_containersByVehicle.value(vehicleId);
    qCDebug(lcClientTruck)
        << "ContainerManager::getContainersForVehicle:"
        << "vehicleId=" << vehicleId
        << "count=" << result.size();
    return result;
}

QString ContainerManager::getVehicleForContainer(
    const ContainerCore::Container *container) const
{
    auto result =
        m_vehicleByContainer.value(container, QString());
    qCDebug(lcClientTruck)
        << "ContainerManager::getVehicleForContainer:"
        << "result=" << result;
    return result;
}

bool ContainerManager::isContainerAssignedToVehicle(
    const QString                  &vehicleId,
    const ContainerCore::Container *container) const
{
    bool result =
        m_vehicleByContainer.value(container) == vehicleId;
    qCDebug(lcClientTruck)
        << "ContainerManager::"
           "isContainerAssignedToVehicle:"
        << "vehicleId=" << vehicleId
        << "assigned=" << result;
    return result;
}

} // namespace TruckClient
} // namespace Backend
} // namespace CargoNetSim
