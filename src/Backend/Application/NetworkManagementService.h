#pragma once

#include "Backend/Commons/NetworkKind.h"
#include "Backend/Commons/ShortestPathResult.h"
#include "Backend/Scenario/LinkageSource.h"

#include <QList>
#include <QString>
#include <QMap>
#include <QVariant>

namespace CargoNetSim
{

class CargoNetSimController;

namespace Backend
{
namespace Scenario
{
class ScenarioDocument;
}

namespace Application
{

enum class LinkTerminalToNodeStatus
{
    Success,
    InvalidRequest,
    AlreadyLinked,
    NoAvailableNode,
    CommitFailed,
};

struct LinkTerminalToNodeResult
{
    LinkTerminalToNodeStatus status =
        LinkTerminalToNodeStatus::InvalidRequest;
    QString message;
    QString networkName;
    int nodeId = -1;
    NetworkKind kind = NetworkKind::Rail;
    double distanceMeters = 0.0;

    bool succeeded() const
    {
        return status == LinkTerminalToNodeStatus::Success;
    }
};

class NetworkManagementService
{
public:
    explicit NetworkManagementService(
        ::CargoNetSim::CargoNetSimController *controller = nullptr);

    bool checkNetworkNameConflict(
        const QString &regionName,
        const QString &networkName) const;

    bool importNetwork(Scenario::ScenarioDocument   *doc,
                       const QString                &regionName,
                       const QString                &networkName,
                       NetworkKind                   kind,
                       const QMap<QString, QString> &files,
                       QString                      *error = nullptr) const;

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

    /// Link a terminal to the nearest available node among document-owned
    /// networks of the requested kinds. Availability is evaluated against
    /// scenario linkages, not GUI MapPoint state, so manual, batch, CLI, and
    /// GUI paths obey the same one-terminal-per-node invariant.
    LinkTerminalToNodeResult linkTerminalToClosestNetworkNode(
        Scenario::ScenarioDocument &doc,
        const QString              &terminalId,
        const QList<NetworkKind>   &kinds,
        Scenario::LinkageSource     source =
            Scenario::LinkageSource::Manual) const;

private:
    ::CargoNetSim::CargoNetSimController *controller() const;

    ::CargoNetSim::CargoNetSimController *m_controller = nullptr;
};

} // namespace Application
} // namespace Backend
} // namespace CargoNetSim
