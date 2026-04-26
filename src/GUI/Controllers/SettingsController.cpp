#include "SettingsController.h"

#include "Backend/Commons/LogCategories.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Scenario/PropertyKeys.h"
#include "Backend/Scenario/ScenarioDocument.h"
#include "Backend/Scenario/ScenarioRuntime.h"
#include "GUI/MainWindow.h"
#include "GUI/Scenario/ScenarioMutator.h"

namespace PK = CargoNetSim::Backend::Scenario::PropertyKeys;

namespace CargoNetSim
{
namespace GUI
{

SettingsController::SettingsController(MainWindow *mainWindow,
                                       QObject    *parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
{
    qCDebug(lcGuiView) << "SettingsController: created";
}

void SettingsController::applySettings(
    const Backend::Scenario::SimulationSettings &settings)
{
    qCDebug(lcGuiView) << "SettingsController::applySettings";

    // 1. ConfigController (config.xml)
    auto *cfg = CargoNetSim::CargoNetSimController::getInstance()
                    .getConfigController();
    cfg->updateConfig(toConfigMap(settings));
    cfg->saveConfig();

    // 2. ScenarioDocument — preserve YAML-only fields not owned by the GUI
    if (m_mainWindow && m_mainWindow->runtime())
    {
        Backend::Scenario::SimulationSettings merged = settings;
        const auto &existing =
            m_mainWindow->runtime()->document().simulation;
        if (!merged.endTime.has_value())
            merged.endTime = existing.endTime;
        if (!merged.dwellMethod.has_value())
            merged.dwellMethod = existing.dwellMethod;
        if (!merged.dwellParams.has_value())
            merged.dwellParams = existing.dwellParams;

        if (!GUI::Scenario::ScenarioMutator::updateSimulationSettings(
                &m_mainWindow->runtime()->document(), merged))
            qCWarning(lcGuiView)
                << "SettingsController::applySettings:"
                   " updateSimulationSettings failed";
    }

    emit configChanged();
}

void SettingsController::loadFromDocument()
{
    qCDebug(lcGuiView) << "SettingsController::loadFromDocument";
    // ConfigController was already updated by ScenarioApplier::applySettings.
    // Signal the widget to refresh its spinboxes from ConfigController.
    emit configChanged();
}

QVariantMap SettingsController::toConfigMap(
    const Backend::Scenario::SimulationSettings &s)
{
    auto *cfg = CargoNetSim::CargoNetSimController::getInstance()
                    .getConfigController();
    QVariantMap root = cfg->getAllParams();

    // Simulation section
    QVariantMap sim = root.value("simulation").toMap();
    if (const auto timeStep = s.timeStepUnits())
        sim["time_step"] = static_cast<int>(timeStep->value());
    if (s.shortestPathsN.has_value())
        sim["shortest_paths"]                 = s.shortestPathsN.value();
    if (s.timeValueOfMoney.has_value())
        sim[PK::Simulation::TimeValueOfMoney] = s.timeValueOfMoney.value();
    if (s.useSpecificTimeValues.has_value())
        sim[PK::Simulation::UseModeSpecific]  = s.useSpecificTimeValues.value();
    root["simulation"] = sim;

    // Carbon taxes
    QVariantMap taxes = root.value("carbon_taxes").toMap();
    if (s.carbonRate.has_value())
        taxes["rate"]             = s.carbonRate.value();
    if (s.shipMultiplier.has_value())
        taxes["ship_multiplier"]  = s.shipMultiplier.value();
    if (s.railMultiplier.has_value())
        taxes["rail_multiplier"]  = s.railMultiplier.value();
    if (s.truckMultiplier.has_value())
        taxes["truck_multiplier"] = s.truckMultiplier.value();
    root["carbon_taxes"] = taxes;

    // Per-mode overlay helper — only sets keys present in m
    auto applyMode =
        [](const Backend::Scenario::SimulationSettings::Mode &m,
           QVariantMap existing) -> QVariantMap
    {
        if (const auto speed = m.speedUnits())
            existing[PK::Mode::AverageSpeed] = speed->value();
        if (m.fuelRate.has_value())
            existing[PK::Mode::AverageFuelConsumption] = m.fuelRate.value();
        if (m.containers.has_value())
            existing[PK::Mode::AverageContainerNumber] = m.containers.value();
        if (const auto risk = m.riskUnits())
            existing[PK::Mode::RiskFactor] = risk->value();
        if (m.fuelType.has_value())
            existing[PK::Mode::FuelType]               = m.fuelType.value();
        if (m.timeValue.has_value())
            existing[PK::Mode::TimeValueOfMoney]       = m.timeValue.value();
        if (m.useNetwork.has_value())
            existing[PK::Mode::UseNetwork]             = m.useNetwork.value();
        return existing;
    };

    QVariantMap modes = root.value("transport_modes").toMap();
    modes["ship"]  = applyMode(s.ship,  modes.value("ship").toMap());
    modes["rail"]  = applyMode(s.rail,  modes.value("rail").toMap());
    modes["truck"] = applyMode(s.truck, modes.value("truck").toMap());
    root["transport_modes"] = modes;

    // Fuel data — only written when the widget has fuel entries
    if (!s.fuelTypes.isEmpty())
    {
        QVariantMap fuelEnergy, fuelPrices, fuelCarbon;
        for (auto it  = s.fuelTypes.constBegin();
                  it != s.fuelTypes.constEnd(); ++it)
        {
            if (it.value().energy.has_value())
                fuelEnergy[it.key()] = it.value().energy.value();
            if (it.value().price.has_value())
                fuelPrices[it.key()] = it.value().price.value();
            if (it.value().carbon.has_value())
                fuelCarbon[it.key()] = it.value().carbon.value();
        }
        root["fuel_energy"]         = fuelEnergy;
        root["fuel_prices"]         = fuelPrices;
        root["fuel_carbon_content"] = fuelCarbon;
    }

    return root;
}

} // namespace GUI
} // namespace CargoNetSim
