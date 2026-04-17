#pragma once

// Header pulls in the canonical truck path: config owns the network.
#include "Backend/Clients/TrainClient/TrainNetwork.h"
#include "Backend/Clients/TruckClient/TruckNetwork.h"  // IntegrationSimulationConfig + IntegrationNetwork
#include "TruckFleetSpec.h"
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>

namespace CargoNetSim
{
namespace Backend
{
class Terminal;

namespace Scenario
{

/// Owns the mutable backend objects the applier creates from a ScenarioDocument.
///
/// Lifetime: normally owned by a ScenarioRuntime (Plan 3). In Plan 2 tests, a
/// stack-allocated instance is sufficient. clear() frees everything.
class ScenarioRegistry : public QObject
{
    Q_OBJECT
public:
    explicit ScenarioRegistry(QObject *parent = nullptr);
    ~ScenarioRegistry() override;

    // Delete copy / move — owns heap-allocated Terminal objects.
    ScenarioRegistry(const ScenarioRegistry &)            = delete;
    ScenarioRegistry &operator=(const ScenarioRegistry &) = delete;

    /// Insert a terminal under its scenario id. Takes ownership: the
    /// registry will `delete` it on clear() / destruction. On a duplicate
    /// id the method returns false and deletes the rejected pointer to
    /// avoid leaks.
    bool addTerminal(const QString &id, Terminal *t);

    /// Non-owning lookup (nullptr if not present).
    Terminal *terminal(const QString &id) const;

    int terminalCount() const;

    /// Free all owned terminals and reset truck fleet.
    void clear();

    // Truck-fleet passthrough (plain value storage; Plan 3 consumes).
    void                    setTruckFleet(const TruckFleetSpec &spec);
    const TruckFleetSpec   &truckFleet() const;

    // Iteration support for linker rules.
    QList<Terminal *>       allTerminals() const;

    // Preview-only graph registries.
    // Rail: registry owns the NeTrainSimNetwork.
    // Truck: registry owns the IntegrationSimulationConfig, which in turn
    //        owns its IntegrationNetwork (see TruckNetwork.h:342).
    void setPreviewRailNetwork (const QString &name, TrainClient::NeTrainSimNetwork           *n);
    void setPreviewTruckConfig (const QString &name, TruckClient::IntegrationSimulationConfig *c);

    TrainClient::NeTrainSimNetwork  *previewRailNetwork (const QString &name) const;
    TruckClient::IntegrationNetwork *previewTruckNetwork(const QString &name) const;  // returns config->getNetwork()

    QStringList previewRailNetworkNames()  const;
    QStringList previewTruckNetworkNames() const;

    bool hasPreviewNetworks() const;

private:
    QMap<QString, Terminal *> m_terminals;
    TruckFleetSpec            m_truckFleet;

    QMap<QString, TrainClient::NeTrainSimNetwork           *> m_previewRail;        // owned
    QMap<QString, TruckClient::IntegrationSimulationConfig *> m_previewTruckCfg;    // owned (owns its network)
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
