#pragma once

#include "Backend/Commons/NetworkKind.h"
#include "Backend/Commons/ShortestPathResult.h"

#include <QString>
#include <QVariant>

namespace CargoNetSim
{

class CargoNetSimController;

namespace Backend
{
namespace Application
{

class NetworkManagementService
{
public:
    explicit NetworkManagementService(
        ::CargoNetSim::CargoNetSimController *controller = nullptr);

    bool checkNetworkNameConflict(
        const QString &regionName,
        const QString &networkName) const;

    bool addTrainNetwork(const QString &regionName,
                         const QString &networkName,
                         const QString &nodeFile,
                         const QString &linkFile) const;

    bool addTruckNetwork(const QString &regionName,
                         const QString &networkName,
                         const QString &configFile) const;

    bool removeNetwork(const QString &regionName,
                       const QString &networkName,
                       NetworkKind    kind) const;

    bool renameNetwork(const QString &regionName,
                       const QString &oldName,
                       const QString &newName,
                       NetworkKind    kind) const;

    bool networkExists(const QString &regionName,
                       const QString &networkName,
                       NetworkKind    kind) const;

    bool setNetworkVariable(const QString  &regionName,
                            const QString  &networkName,
                            NetworkKind     kind,
                            const QString  &key,
                            const QVariant &value) const;

    ShortestPathResult findShortestPath(
        const QString &regionName,
        const QString &networkName,
        NetworkKind    kind,
        int            startNodeId,
        int            endNodeId) const;

private:
    ::CargoNetSim::CargoNetSimController *controller() const;

    ::CargoNetSim::CargoNetSimController *m_controller = nullptr;
};

} // namespace Application
} // namespace Backend
} // namespace CargoNetSim
