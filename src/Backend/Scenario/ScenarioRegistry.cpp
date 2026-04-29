#include "ScenarioRegistry.h"

#include "Backend/Commons/LogCategories.h"
#include "Backend/Models/Terminal.h"

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

ScenarioRegistry::ScenarioRegistry(QObject *parent)
    : QObject(parent)
{
}

ScenarioRegistry::~ScenarioRegistry()
{
    clear();
}

bool ScenarioRegistry::addTerminal(const QString &id, Terminal *t)
{
    qCDebug(lcScenario) << "ScenarioRegistry::addTerminal:" << id;
    if (!t) return false;
    if (m_terminals.contains(id))
    {
        qCWarning(lcScenario) << "ScenarioRegistry::addTerminal: duplicate id:" << id;
        delete t;              // reject + free to avoid leaks in caller code
        return false;
    }
    m_terminals.insert(id, t);
    return true;
}

Terminal *ScenarioRegistry::terminal(const QString &id) const
{
    auto *t = m_terminals.value(id, nullptr);
    if (!t)
        qCWarning(lcScenario) << "ScenarioRegistry::terminal: not found:" << id;
    return t;
}

int ScenarioRegistry::terminalCount() const
{
    return m_terminals.size();
}

void ScenarioRegistry::clear()
{
    qCDebug(lcScenario) << "ScenarioRegistry::clear: terminals:" << m_terminals.size()
                        << "previewRail:" << m_previewRail.size()
                        << "previewTruck:" << m_previewTruckCfg.size();
    qDeleteAll(m_terminals);
    m_terminals.clear();
    m_truckFleet = TruckFleetSpec{};

    qDeleteAll(m_previewRail);
    m_previewRail.clear();
    qDeleteAll(m_previewTruckCfg);
    m_previewTruckCfg.clear();
}

void ScenarioRegistry::setTruckFleet(const TruckFleetSpec &spec)
{
    qCDebug(lcScenario) << "ScenarioRegistry::setTruckFleet: set";
    m_truckFleet = spec;
}

const TruckFleetSpec &ScenarioRegistry::truckFleet() const
{
    return m_truckFleet;
}

QList<Terminal *> ScenarioRegistry::allTerminals() const
{
    return m_terminals.values();
}

// ---- Preview network accessors ----

void ScenarioRegistry::setPreviewRailNetwork(
    const QString &name, TrainClient::NeTrainSimNetwork *n)
{
    qCDebug(lcScenario) << "ScenarioRegistry::setPreviewRailNetwork:" << name;
    if (!n) return;
    if (auto *existing = m_previewRail.value(name, nullptr))
        delete existing;
    m_previewRail.insert(name, n);
}

void ScenarioRegistry::setPreviewTruckConfig(
    const QString &name, TruckClient::IntegrationSimulationConfig *c)
{
    qCDebug(lcScenario) << "ScenarioRegistry::setPreviewTruckConfig:" << name;
    if (!c) return;
    if (auto *existing = m_previewTruckCfg.value(name, nullptr))
        delete existing;
    m_previewTruckCfg.insert(name, c);
}

TrainClient::NeTrainSimNetwork *
ScenarioRegistry::previewRailNetwork(const QString &name) const
{
    auto *n = m_previewRail.value(name, nullptr);
    if (!n)
        qCWarning(lcScenario) << "ScenarioRegistry::previewRailNetwork: not found:" << name;
    return n;
}

TruckClient::IntegrationNetwork *
ScenarioRegistry::previewTruckNetwork(const QString &name) const
{
    auto *cfg = m_previewTruckCfg.value(name, nullptr);
    if (!cfg)
        qCWarning(lcScenario) << "ScenarioRegistry::previewTruckNetwork: not found:" << name;
    return cfg ? cfg->getNetwork() : nullptr;
}

TruckClient::IntegrationSimulationConfig *
ScenarioRegistry::previewTruckConfig(const QString &name) const
{
    auto *cfg = m_previewTruckCfg.value(name, nullptr);
    if (!cfg)
    {
        qCWarning(lcScenario)
            << "ScenarioRegistry::previewTruckConfig: not found:"
            << name;
    }
    return cfg;
}

QStringList ScenarioRegistry::previewRailNetworkNames() const
{
    return m_previewRail.keys();
}

QStringList ScenarioRegistry::previewTruckNetworkNames() const
{
    return m_previewTruckCfg.keys();
}

bool ScenarioRegistry::hasPreviewNetworks() const
{
    return !m_previewRail.isEmpty() || !m_previewTruckCfg.isEmpty();
}

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
