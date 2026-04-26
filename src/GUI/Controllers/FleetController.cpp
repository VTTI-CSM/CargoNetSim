#include "FleetController.h"

#include "Backend/Application/ScenarioEditService.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/GuiApi/ScenarioContractsApi.h"
#include "Backend/GuiApi/ScenarioDocumentApi.h"
#include "Backend/Scenario/ScenarioRuntime.h"
#include "GUI/MainWindow.h"

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
    Backend::Application::ScenarioEditService::updateFleet(
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
    Backend::Application::ScenarioEditService::updateFleet(
        &m_mainWindow->runtime()->document(), updated);
}

} // namespace GUI
} // namespace CargoNetSim
