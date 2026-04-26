#include "NetworkController.h"
#include "../MainWindow.h"
#include "Backend/Application/NetworkManagementService.h"
#include "Backend/Application/NetworkViewService.h"
#include "Backend/Application/ScenarioEditService.h"
#include "Backend/GuiApi/ScenarioDocumentApi.h"
#include "Backend/Scenario/ScenarioRuntime.h"
#include "GUI/Controllers/UtilityFunctions.h"
#include "GUI/Controllers/SceneVisibilityController.h"
#include "GUI/Controllers/NetworkDrawingController.h"

#include <QColor>
#include <QFileDialog>
#include <QInputDialog>
#include "Backend/Commons/LogCategories.h"
#include <QMessageBox>
#include <QStatusBar>
#include <QString>
#include <exception>
#include <memory>

namespace CargoNetSim
{
namespace GUI
{

namespace
{

CargoNetSim::Backend::NetworkKind toNetworkKind(
    CargoNetSim::GUI::NetworkType type)
{
    switch (type)
    {
    case CargoNetSim::GUI::NetworkType::Train:
        return CargoNetSim::Backend::NetworkKind::Rail;
    case CargoNetSim::GUI::NetworkType::Truck:
        return CargoNetSim::Backend::NetworkKind::Truck;
    case CargoNetSim::GUI::NetworkType::Ship:
        break;
    }

    throw std::runtime_error(
        "Ship network operations are not supported yet.");
}

} // namespace

QString NetworkController::importNetwork(
    MainWindow *mainWindow, NetworkType networkType,
    const QString &regionName)
{
    if (!mainWindow || regionName.isEmpty())
        return QString();

    Backend::Application::NetworkViewService viewService;
    Backend::Application::NetworkManagementService
        managementService;

    qCDebug(lcGuiNetwork) << "NetworkController::importNetwork:"
                          << "type=" << static_cast<int>(networkType)
                          << "region=" << regionName;
    // Add validation logic to check for existing networks
    // first
    QStringList networkNames;
    if (networkType == NetworkType::Train)
        networkNames = viewService.trainNetworkNames(regionName);
    else if (networkType == NetworkType::Truck)
        networkNames = viewService.truckNetworkNames(regionName);
    else if (networkType == NetworkType::Ship)
        throw std::runtime_error(
            "Ship network import is not supported yet.");

    // Check for existing network - same check as in
    // NetworkManagerDialog
    if (!networkNames.isEmpty())
    {
        qCWarning(lcGuiNetwork) << "NetworkController::importNetwork:"
                                << "network already exists for region"
                                << regionName;
        QString typeString =
            getNetworkTypeString(networkType);
        QMessageBox::warning(
            mainWindow, "Warning",
            QString(
                "One %1 Network is allowed for region '%2'")
                .arg(typeString.toLower())
                .arg(regionName));
        return QString();
    }

    QString networkName;
    // Network name input loop
    while (true)
    {
        QString networkTypeStr =
            NetworkController::getNetworkTypeString(
                networkType);
        bool    ok;
        QString networkName_user;
        networkName_user = QInputDialog::getText(
            mainWindow, "Network Name",
            QString("Enter a name for the %1 network:")
                .arg(networkTypeStr),
            QLineEdit::Normal, QString(), &ok);

        if (!ok || networkName_user.trimmed().isEmpty())
        {
            qCDebug(lcGuiNetwork) << "NetworkController::importNetwork:"
                                  << "user cancelled name input";
            return QString();
        }

        networkName = networkName_user;

        // Check for name conflicts
        try
        {
            if (managementService.checkNetworkNameConflict(
                    regionName,
                    networkName))
            {
                qCDebug(lcGuiNetwork) << "NetworkController::importNetwork:"
                                      << "name conflict for" << networkName;
                QMessageBox::warning(
                    mainWindow, "Name Already Exists",
                    QString("A network named '%1' already "
                            "exists. Please choose a "
                            "different name.")
                        .arg(networkName_user));
                continue;
            }
            break;
        }
        catch (const std::exception &e)
        {
            QMessageBox::warning(mainWindow, "Invalid Name",
                                 e.what());
            continue;
        }
    }

    if (networkType == NetworkType::Train)
    {
        if (NetworkController::importTrainNetwork(
                mainWindow, regionName, networkName))
            return networkName;
    }
    else if (networkType == NetworkType::Truck)
    {
        if (NetworkController::importTruckNetwork(
                mainWindow, regionName, networkName))
            return networkName;
    }

    return QString();
}

namespace {

/// Dual-write: after the legacy RegionData load + drawNetwork have run,
/// also record the NetworkSpec in the currently-loaded ScenarioDocument
/// so that `File → Save Scenario` (Plan 4 Task 19) exports the network
/// and the CLI sees the same shape the GUI built. No-op when no
/// runtime is loaded. Kept file-local because this is a GUI-layer
/// translation from "user picked a file" to "doc declaration"; other
/// callers add networks via `ScenarioApplier` (YAML-load path) which
/// writes to RegionData directly.
void recordNetworkInDocument(
    CargoNetSim::GUI::MainWindow *mainWindow,
    const QString                &regionName,
    const QString                &networkName,
    CargoNetSim::Backend::NetworkKind                 kind,
    std::initializer_list<std::pair<QString, QString>> files)
{
    qCDebug(lcGuiNetwork) << "recordNetworkInDocument:"
                          << "network=" << networkName
                          << "region=" << regionName;
    if (!mainWindow || !mainWindow->runtime()) return;
    auto &doc = mainWindow->runtime()->document();
    CargoNetSim::Backend::Scenario::NetworkSpec spec;
    spec.name = networkName;
    spec.type = kind;
    for (const auto &role_path : files)
        spec.files.insert(role_path.first, role_path.second);
    if (doc.regions.contains(regionName))
    {
        const auto &origin = doc.regions[regionName].localOrigin;
        // x = longitude, y = latitude — matches the Qt geo-point convention used elsewhere.
        spec.referencePoint.x = origin.longitude;
        spec.referencePoint.y = origin.latitude;
    }
    if (!Backend::Application::ScenarioEditService::addNetwork(&doc, regionName, spec))
        qCWarning(lcGuiNetwork)
            << "recordNetworkInDocument:"
            << "ScenarioEditService::addNetwork failed for"
            << networkName;
}

} // namespace

bool NetworkController::importTrainNetwork(
    MainWindow *mainWindow, const QString &regionName,
    QString &networkName)
{
    // Select node file
    QString nodeFile = QFileDialog::getOpenFileName(
        mainWindow, "Select Train Network Node File",
        QString(), "All Files (*.*)", nullptr,
        QFileDialog::DontUseNativeDialog);

    if (nodeFile.isEmpty())
    {
        qCDebug(lcGuiNetwork) << "NetworkController::importTrainNetwork:"
                              << "user cancelled node file selection";
        return false;
    }

    qCDebug(lcGuiNetwork) << "NetworkController::importTrainNetwork:"
                          << "nodeFile=" << nodeFile;
    // Select link file
    QString linkFile = QFileDialog::getOpenFileName(
        mainWindow, "Select Train Network Link File",
        QString(), "All Files (*.*)", nullptr,
        QFileDialog::DontUseNativeDialog);

    if (linkFile.isEmpty())
    {
        qCDebug(lcGuiNetwork) << "NetworkController::importTrainNetwork:"
                              << "user cancelled link file selection";
        return false;
    }

    qCInfo(lcGuiNetwork) << "NetworkController::importTrainNetwork:"
                         << "starting import" << networkName;
    try
    {
        Backend::Application::NetworkManagementService
            managementService;
        qCDebug(lcRail) << "[RailLoad] importTrainNetwork: calling"
                    " addTrainNetwork, name=" << networkName;
        // load the network
        if (!managementService.addTrainNetwork(
                regionName, networkName, nodeFile, linkFile))
            return false;
        qCDebug(lcRail) << "[RailLoad] importTrainNetwork:"
                    " addTrainNetwork returned";
        // Draw the network on map
        qCDebug(lcRail) << "[RailLoad] importTrainNetwork: calling"
                    " drawNetwork";
        mainWindow->networkDrawing()->drawNetwork(
            regionName, NetworkType::Train, networkName);
        qCDebug(lcRail) << "[RailLoad] importTrainNetwork:"
                    " drawNetwork returned";

        // Record the declaration in the ScenarioDocument so save/CLI
        // paths see what the GUI just imported. No-op without runtime.
        recordNetworkInDocument(
            mainWindow, regionName, networkName,
            Backend::NetworkKind::Rail,
            {{QStringLiteral("nodes"), nodeFile},
             {QStringLiteral("links"), linkFile}});

        mainWindow->showStatusBarMessage(
            "Importing train network!", 2000);
        return true;
    }
    catch (const std::exception &e)
    {
        qCCritical(lcRail)
            << "[RailLoad] importTrainNetwork exception:"
            << e.what();
        QMessageBox::warning(mainWindow, "Error", e.what());
        return false;
    }
}

bool NetworkController::importTruckNetwork(
    MainWindow *mainWindow, const QString &regionName,
    QString &networkName)
{
    // Select node file
    QString configFile = QFileDialog::getOpenFileName(
        mainWindow, "Select Truck Network Master File",
        QString(), "All Files (*.*)", nullptr,
        QFileDialog::DontUseNativeDialog);

    if (configFile.isEmpty())
    {
        qCDebug(lcGuiNetwork) << "NetworkController::importTruckNetwork:"
                              << "user cancelled file selection";
        return false;
    }

    qCInfo(lcGuiNetwork) << "NetworkController::importTruckNetwork:"
                         << "starting import" << networkName
                         << "file=" << configFile;
    try
    {
        Backend::Application::NetworkManagementService
            managementService;
        // load the network
        if (!managementService.addTruckNetwork(
                regionName, networkName, configFile))
            return false;

        // Draw the network on map
        mainWindow->networkDrawing()->drawNetwork(
            regionName, NetworkType::Truck, networkName);

        // Record in document for CLI/GUI convergence (see
        // importTrainNetwork comment).
        recordNetworkInDocument(
            mainWindow, regionName, networkName,
            Backend::NetworkKind::Truck,
            {{QStringLiteral("config"), configFile}});

        mainWindow->showStatusBarMessage(
            "Importing truck network!", 2000);
        return true;
    }
    catch (const std::exception &e)
    {
        qCWarning(lcGuiNetwork) << "NetworkController::importTruckNetwork:"
                                << "exception:" << e.what();
        QMessageBox::warning(mainWindow, "Error", e.what());
        return false;
    }
}

QString
NetworkController::getNetworkTypeString(NetworkType type)
{
    switch (type)
    {
    case NetworkType::Train:
        return "Rail";
    case NetworkType::Truck:
        return "Truck";
    default:
        return "";
    }
}

bool NetworkController::removeNetwork(
    MainWindow *mainWindow, NetworkType networkType,
    QString &networkName, const QString &regionName)
{
    if (!mainWindow || regionName.isEmpty())
        return false;

    // handle the visual delete first since it depends on
    // the backend
    mainWindow->networkDrawing()->removeNetwork(
        networkType, regionName, networkName);

    // remove the network from the backend
    Backend::Application::NetworkManagementService
        managementService;
    bool ok = managementService.removeNetwork(
        regionName, networkName, toNetworkKind(networkType));

    if (ok && mainWindow && mainWindow->runtime())
    {
        if (!Backend::Application::ScenarioEditService::removeNetwork(
                &mainWindow->runtime()->document(),
                regionName, networkName))
            qCWarning(lcGuiNetwork)
                << "NetworkController::removeNetwork:"
                << "ScenarioEditService::removeNetwork failed for"
                << networkName;
    }
    return ok;
}

bool NetworkController::renameNetwork(
    MainWindow *mainWindow, NetworkType networkType,
    const QString &oldName, const QString &newName,
    const QString &regionName)
{
    if (!mainWindow || regionName.isEmpty())
        return false;

    try
    {
        Backend::Application::NetworkManagementService
            managementService;
        // Check if network exists
        const bool networkExists =
            managementService.networkExists(
                regionName, newName, toNetworkKind(networkType));

        if (networkExists && newName != oldName)
        {
            QMessageBox::warning(
                mainWindow, "Name Already Exists",
                QString(
                    "A network named '%1' already exists. "
                    "Please choose a different name.")
                    .arg(newName));
            return false;
        }

        bool success = false;

        try
        {
            // Rename network based on type
            success = managementService.renameNetwork(
                regionName, oldName, newName,
                toNetworkKind(networkType));
        }
        catch (std::exception &e)
        {
            QMessageBox::critical(
                mainWindow, "Error",
                QString("Failed to rename network: %1")
                    .arg(e.what()));
            return false;
        }

        if (success)
        {
            if (mainWindow && mainWindow->runtime())
            {
                if (!Backend::Application::ScenarioEditService::renameNetwork(
                        &mainWindow->runtime()->document(),
                        regionName, oldName, newName))
                    qCWarning(lcGuiNetwork)
                        << "NetworkController::renameNetwork:"
                        << "ScenarioEditService::renameNetwork failed for"
                        << oldName << "->" << newName;
            }
        }

        return success;
    }
    catch (const std::exception &e)
    {
        QMessageBox::critical(
            mainWindow, "Error",
            QString("Failed to rename network: %1")
                .arg(e.what()));
        return false;
    }
}

bool NetworkController::changeNetworkColor(
    MainWindow *mainWindow, NetworkType networkType,
    const QString &networkName, const QColor &newColor,
    const QString &regionName)
{
    if (!mainWindow || regionName.isEmpty()
        || !newColor.isValid())
        return false;

    try
    {
        Backend::Application::NetworkManagementService
            managementService;

        if (managementService.setNetworkVariable(
                regionName, networkName,
                toNetworkKind(networkType),
                QStringLiteral("color"), newColor))
        {
            // Update color of all network items in the
            // scene
            mainWindow->sceneVisibility()->changeNetworkColor(
                networkName, newColor);

            return true;
        }

        return false;
    }
    catch (const std::exception &e)
    {
        QMessageBox::critical(
            mainWindow, "Error",
            QString("Failed to change network color: %1")
                .arg(e.what()));
        return false;
    }
}

CargoNetSim::Backend::ShortestPathResult CargoNetSim::GUI::
    NetworkController::findNetworkShortestPath(
        const QString                &regionName,
        const QString                &networkName,
        CargoNetSim::GUI::NetworkType networkType,
        int startNodeId, int endNodeId)
{
    try
    {
        Backend::Application::NetworkManagementService
            managementService;
        return managementService.findShortestPath(
            regionName, networkName, toNetworkKind(networkType),
            startNodeId, endNodeId);
    }
    catch (const std::exception &e)
    {
        qCWarning(lcGuiNetwork) << "Error finding shortest path:"
                               << e.what();
    }

    return CargoNetSim::Backend::ShortestPathResult();
}

bool NetworkController::clearAllNetworks(
    MainWindow *mainWindow)
{
    if (!mainWindow)
    {
        return false;
    }

    Backend::Application::NetworkViewService viewService;
    const auto regionNames = viewService.regionNames();

    for (auto regionName : regionNames)
    {
        for (const auto &netName :
             viewService.trainNetworkNames(regionName))
        {
            mainWindow->networkDrawing()->removeNetwork(
                NetworkType::Train, regionName, netName);
        }

        for (const auto &netName :
             viewService.truckNetworkNames(regionName))
        {
            mainWindow->networkDrawing()->removeNetwork(
                NetworkType::Truck, regionName, netName);
        }
    }

    return true;
}

QList<MapLine *> NetworkController::getShortestPathMapLines(
    MainWindow *mainWindow, const QString &regionName,
    const QString &networkName, NetworkType networkType,
    int startNodeId, int endNodeId)
{
    QList<MapLine *> pathMapLines;
    if (!mainWindow || !mainWindow->regionScene_)
    {
        return pathMapLines;
    }

    // Get shortest path result
    Backend::ShortestPathResult result =
        findNetworkShortestPath(regionName, networkName,
                                networkType, startNodeId,
                                endNodeId);

    if (result.pathLinks.empty()
        || result.pathLinks.size() < 1)
    {
        return pathMapLines;
    }

    // Get all map lines in the scene
    QList<MapLine *> allMapLines =
        mainWindow->regionScene_->getItemsByType<MapLine>();

    // Find map lines that correspond to segments in the
    // shortest path
    for (int linkID : result.pathLinks)
    {

        for (MapLine *mapLine : allMapLines)
        {
            if (mapLine->getReferencedNetworkLinkID()
                == QString::number(linkID))
            {
                pathMapLines.append(mapLine);
                break; // Found the line for this segment,
                       // move to next
            }
        }
    }

    return pathMapLines;
}

bool NetworkController::moveNetwork(
    MainWindow *mainWindow, NetworkType networkType,
    const QString &networkName, const QPointF &offset,
    const QString &regionName)
{
    if (!mainWindow || regionName.isEmpty())
        return false;

    try
    {
        Backend::Application::NetworkManagementService
            managementService;
        if (!managementService.networkExists(
                regionName, networkName,
                toNetworkKind(networkType)))
            return false;

        // Perform the actual movement through the
        // ViewController
        bool success =
            mainWindow->networkDrawing()->moveNetworkItems(
                networkType, networkName, offset,
                regionName);

        if (success)
        {
            mainWindow->showStatusBarMessage(
                QString("Network '%1' moved successfully.")
                    .arg(networkName),
                3000);
        }

        return success;
    }
    catch (const std::exception &e)
    {
        QMessageBox::critical(
            mainWindow, "Error",
            QString("Failed to move network: %1")
                .arg(e.what()));
        return false;
    }
}

} // namespace GUI
} // namespace CargoNetSim
