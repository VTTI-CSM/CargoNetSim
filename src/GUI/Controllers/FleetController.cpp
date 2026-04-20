#include "FleetController.h"

#include "Backend/Commons/LogCategories.h"
#include "Backend/Scenario/FleetSpec.h"
#include "Backend/Scenario/ScenarioDocument.h"
#include "Backend/Scenario/ScenarioRuntime.h"
#include "GUI/MainWindow.h"
#include "GUI/Scenario/ScenarioMutator.h"

namespace CargoNetSim
{
namespace GUI
{

FleetController::FleetController(MainWindow *mainWindow, QObject *parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
{
    qCDebug(lcGuiView) << "FleetController: created";
}

void FleetController::appendTrainFiles(const QStringList &paths)
{
    if (paths.isEmpty()) return;
    if (!m_mainWindow || !m_mainWindow->runtime()) return;
    qCDebug(lcGuiView) << "FleetController::appendTrainFiles:" << paths;
    Backend::Scenario::FleetSpec updated =
        m_mainWindow->runtime()->document().fleet;
    updated.trainsFiles.append(paths);
    GUI::Scenario::ScenarioMutator::updateFleet(
        &m_mainWindow->runtime()->document(), updated);
}

void FleetController::appendShipFiles(const QStringList &paths)
{
    if (paths.isEmpty()) return;
    if (!m_mainWindow || !m_mainWindow->runtime()) return;
    qCDebug(lcGuiView) << "FleetController::appendShipFiles:" << paths;
    Backend::Scenario::FleetSpec updated =
        m_mainWindow->runtime()->document().fleet;
    updated.shipsFiles.append(paths);
    GUI::Scenario::ScenarioMutator::updateFleet(
        &m_mainWindow->runtime()->document(), updated);
}

} // namespace GUI
} // namespace CargoNetSim
