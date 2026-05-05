#pragma once

#include <QObject>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <optional>

#include "Backend/Commons/NetworkKind.h"

class QObject;

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

struct NetworkNodeView
{
    QString     regionName;
    QString     networkName;
    int         nodeId = -1;
    NetworkKind kind = NetworkKind::Rail;
    QPointF     projectedScenePoint;
    QObject    *networkObject = nullptr;

    bool isValid() const
    {
        return networkObject != nullptr
               && !regionName.isEmpty()
               && !networkName.isEmpty()
               && nodeId >= 0;
    }
};

struct NetworkResolvedView
{
    QString     regionName;
    QString     networkName;
    NetworkKind kind = NetworkKind::Rail;
    QObject    *networkObject = nullptr;

    bool isValid() const
    {
        return networkObject != nullptr
               && !regionName.isEmpty()
               && !networkName.isEmpty();
    }
};

class NetworkViewService : public QObject
{
    Q_OBJECT

public:
    explicit NetworkViewService(
        ::CargoNetSim::CargoNetSimController *controller = nullptr,
        QObject *parent = nullptr);

    QString currentRegionName() const;
    bool setCurrentRegion(const QString &regionName) const;

    QStringList regionNames() const;
    bool regionExists(const QString &regionName) const;
    QVariant regionVariable(const QString &regionName,
                            const QString &key) const;

    bool addRegion(const QString &regionName) const;
    bool renameRegion(const QString &oldName,
                      const QString &newName) const;
    bool removeRegion(const QString &regionName) const;

    QStringList trainNetworkNames(
        const QString &regionName) const;
    QStringList truckNetworkNames(
        const QString &regionName) const;
    QStringList networkNames(const QString &regionName,
                             NetworkKind    kind) const;

    bool regionOwnsNetwork(const QString &regionName,
                           const QString &networkName) const;
    QVariant networkVariable(const QString &regionName,
                             const QString &networkName,
                             const QString &key) const;

    QString owningRegionName(
        const Scenario::ScenarioDocument &document,
        const QString                    &networkName) const;

    QString networkNameOf(
        QObject     *networkObject,
        NetworkKind *outKind = nullptr) const;

    std::optional<NetworkResolvedView> resolveNetworkView(
        const QString &regionName,
        const QString &networkName) const;

    std::optional<NetworkNodeView> resolveNodeView(
        const QString &regionName,
        const QString &networkName,
        int            nodeId) const;

    bool setRegionVariable(const QString &regionName,
                           const QString &key,
                           const QVariant &value) const;
    bool removeRegionVariable(const QString &regionName,
                              const QString &key) const;
    bool setGlobalVariable(const QString &key,
                           const QVariant &value) const;
    bool removeGlobalVariable(const QString &key) const;

signals:
    void regionAdded(const QString &regionName);
    void regionRenamed(const QString &oldName,
                       const QString &newName);
    void regionRemoved(const QString &regionName);
    void currentRegionChanged(const QString &regionName);
    void regionsCleared();
    void regionVariableChanged(const QString &regionName,
                               const QString &key,
                               const QVariant &value);

private:
    ::CargoNetSim::CargoNetSimController *controller() const;

    ::CargoNetSim::CargoNetSimController *m_controller = nullptr;
};

} // namespace Application
} // namespace Backend
} // namespace CargoNetSim
