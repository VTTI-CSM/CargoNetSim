#pragma once

#include "Backend/Scenario/SimulationSettings.h"
#include <QObject>
#include <QVariantMap>

namespace CargoNetSim
{
namespace GUI
{

class MainWindow;

class SettingsController : public QObject
{
    Q_OBJECT
public:
    explicit SettingsController(MainWindow *mainWindow,
                                QObject    *parent = nullptr);

    /// Write @p settings to ConfigController (config.xml) and
    /// ScenarioDocument (YAML). YAML-only fields (endTime, dwellMethod,
    /// dwellParams) that are nullopt in @p settings are copied from the
    /// existing doc.simulation before writing.
    void applySettings(
        const Backend::Scenario::SimulationSettings &settings);

    /// Emit configChanged() so SettingsWidget::refreshFromConfig() fires.
    /// ConfigController is already current — ScenarioApplier ran first.
    /// Called by MainWindow::setRuntime() after SceneRepopulator::repopulate().
    void loadFromDocument();

signals:
    void configChanged();

private:
    /// Convert @p s to the QVariantMap format expected by
    /// ConfigController::updateConfig(). Reads the current config first so
    /// unrelated keys are not wiped.
    static QVariantMap toConfigMap(
        const Backend::Scenario::SimulationSettings &s);

    MainWindow *m_mainWindow;
};

} // namespace GUI
} // namespace CargoNetSim
