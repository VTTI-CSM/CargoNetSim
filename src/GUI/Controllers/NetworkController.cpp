#include "NetworkController.h"
#include "../MainWindow.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Controllers/RegionDataController.h"
#include "GUI/Controllers/UtilityFunctions.h"
#include "GUI/Controllers/ViewController.h"

#include <QColor>
#include <QFileDialog>
#include <QInputDialog>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QStatusBar>
#include <QString>
#include <exception>
#include <memory>

namespace
{
Q_LOGGING_CATEGORY(lcRail, "cargonetsim.rail")
}

namespace CargoNetSim
{
namespace GUI
{

QString NetworkController::importNetwork(
    MainWindow *mainWindow, NetworkType networkType,
    Backend::RegionData *regionData)
{
    // Add validation logic to check for existing networks
    // first
    QStringList networkNames;
    if (networkType == NetworkType::Train)
    {
        networkNames = regionData->getTrainNetworks();
    }
    else if (networkType == NetworkType::Truck)
    {
        networkNames = regionData->getTruckNetworks();
    }
    else if (networkType == NetworkType::Ship)
    {
        throw std::runtime_error(
            "Ship network import is not supported yet.");
        return "";
    }

    // Check for existing network - same check as in
    // NetworkManagerDialog
    if (!networkNames.isEmpty())
    {
        QString typeString =
            getNetworkTypeString(networkType);
        QMessageBox::warning(
            mainWindow, "Warning",
            QString(
                "One %1 Network is allowed for region '%2'")
                .arg(typeString.toLower())
                .arg(regionData->getRegion()));
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
            return QString();
        }

        networkName = networkName_user;

        // Check for name conflicts
        try
        {
            if (regionData->checkNetworkNameConflict(
                    networkName))
            {
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
                mainWindow, regionData, networkName)
            == true)
        {
            // Update the network list in the Network
            // Manager dialog
            if (mainWindow->networkManagerDock_)
            {
                mainWindow->networkManagerDock_
                    ->updateNetworkList("Rail Network");
            }
            return networkName;
        }
    }
    else if (networkType == NetworkType::Truck)
    {
        if (NetworkController::importTruckNetwork(
                mainWindow, regionData, networkName))
        {
            // Update the network list in the Network
            // Manager dialog
            if (mainWindow->networkManagerDock_)
            {
                mainWindow->networkManagerDock_
                    ->updateNetworkList("Truck Network");
            }
            return networkName;
        }
    }

    return QString();
}

bool NetworkController::importTrainNetwork(
    MainWindow *mainWindow, Backend::RegionData *regionData,
    QString &networkName)
{
    // Select node file
    QString nodeFile = QFileDialog::getOpenFileName(
        mainWindow, "Select Train Network Node File",
        QString(), "All Files (*.*)", nullptr,
        QFileDialog::DontUseNativeDialog);

    if (nodeFile.isEmpty())
    {
        return false;
    }

    // Select link file
    QString linkFile = QFileDialog::getOpenFileName(
        mainWindow, "Select Train Network Link File",
        QString(), "All Files (*.*)", nullptr,
        QFileDialog::DontUseNativeDialog);

    if (linkFile.isEmpty())
    {
        return false;
    }

    try
    {
        qCDebug(lcRail) << "[RailLoad] importTrainNetwork: calling"
                    " addTrainNetwork, name=" << networkName;
        // load the network
        regionData->addTrainNetwork(networkName, nodeFile,
                                    linkFile);
        qCDebug(lcRail) << "[RailLoad] importTrainNetwork:"
                    " addTrainNetwork returned";
        // Draw the network on map
        qCDebug(lcRail) << "[RailLoad] importTrainNetwork: calling"
                    " drawNetwork";
        ViewController::drawNetwork(mainWindow, regionData,
                                    NetworkType::Train,
                                    networkName);
        qCDebug(lcRail) << "[RailLoad] importTrainNetwork:"
                    " drawNetwork returned";

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
    MainWindow *mainWindow, Backend::RegionData *regionData,
    QString &networkName)
{
    // Select node file
    QString configFile = QFileDialog::getOpenFileName(
        mainWindow, "Select Truck Network Master File",
        QString(), "All Files (*.*)", nullptr,
        QFileDialog::DontUseNativeDialog);

    if (configFile.isEmpty())
    {
        return false;
    }

    try
    {
        // load the network
        regionData->addTruckNetwork(networkName,
                                    configFile);

        // Draw the network on map
        ViewController::drawNetwork(mainWindow, regionData,
                                    NetworkType::Truck,
                                    networkName);
        mainWindow->showStatusBarMessage(
            "Importing truck network!", 2000);
        return true;
    }
    catch (const std::exception &e)
    {
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
    QString &networkName, Backend::RegionData *regionData)
{
    // handle the visual delete first since it depends on
    // the backend
    ViewController::removeNetwork(mainWindow, networkType,
                                  regionData, networkName);

    // remove the network from the backend
    if (networkType == NetworkType::Train)
    {
        return regionData->removeTrainNetwork(networkName);
    }
    else if (networkType == NetworkType::Truck)
    {
        return regionData->removeTruckNetwork(networkName);
    }
    else if (networkType == NetworkType::Ship)
    {
        throw std::runtime_error(
            "Ship network removal is not supported yet.");
    }
    return false;
}

bool NetworkController::renameNetwork(
    MainWindow *mainWindow, NetworkType networkType,
    const QString &oldName, const QString &newName,
    Backend::RegionData *regionData)
{
    if (!mainWindow || !regionData)
        return false;

    try
    {
        // Check if network exists
        bool networkExists = false;
        if (networkType == NetworkType::Train)
        {
            networkExists =
                regionData->trainNetworkExists(newName);
        }
        else if (networkType == NetworkType::Truck)
        {
            networkExists =
                regionData->truckNetworkExists(newName);
        }
        else if (networkType == NetworkType::Ship)
        {
            throw std::runtime_error(
                "Ship network rename is not supported "
                "yet.");
        }

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
            if (networkType == NetworkType::Train)
            {
                success = regionData->renameTrainNetwork(
                    oldName, newName);
            }
            else if (networkType == NetworkType::Truck)
            {
                success = regionData->renameTruckNetwork(
                    oldName, newName);
            }
            else if (networkType == NetworkType::Ship)
            {
                throw std::runtime_error(
                    "Ship network rename is not supported "
                    "yet.");
            }
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

            // Update network manager lists
            if (mainWindow->networkManagerDock_)
            {
                if (networkType == NetworkType::Train)
                {
                    mainWindow->networkManagerDock_
                        ->updateNetworkList("Rail Network");
                }
                else if (networkType == NetworkType::Truck)
                {
                    mainWindow->networkManagerDock_
                        ->updateNetworkList(
                            "Truck Network");
                }
                else if (networkType == NetworkType::Ship)
                {
                    throw std::runtime_error(
                        "Ship network rename is not "
                        "supported yet.");
                }
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
    Backend::RegionData *regionData)
{
    if (!mainWindow || !regionData || !newColor.isValid())
        return false;

    try
    {
        BaseNetwork *network = nullptr;

        // Get network based on type
        if (networkType == NetworkType::Train)
        {
            network =
                regionData->getTrainNetwork(networkName);
        }
        else if (networkType == NetworkType::Truck)
        {
            network =
                regionData->getTruckNetwork(networkName);
        }
        else if (networkType == NetworkType::Ship)
        {
            throw std::runtime_error(
                "Ship network color is not "
                "supported yet.");
        }

        if (network)
        {
            // Set the color variable
            network->setVariable("color", newColor);

            // Update color of all network items in the
            // scene
            ViewController::changeNetworkColor(
                mainWindow, networkName, newColor);

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
        if (networkType
            == CargoNetSim::GUI::NetworkType::Train)
        {
            auto network =
                CargoNetSim::CargoNetSimController::
                    getInstance()
                        .getRegionDataController()
                        ->getRegionData(regionName)
                        ->getTrainNetwork(networkName);

            // Find the shortest path
            return network->findShortestPath(startNodeId,
                                             endNodeId);
        }
        else if (networkType
                 == CargoNetSim::GUI::NetworkType::Truck)
        {
            auto network =
                CargoNetSim::CargoNetSimController::
                    getInstance()
                        .getRegionDataController()
                        ->getRegionData(regionName)
                        ->getTruckNetwork(networkName);

            return network->findShortestPath(startNodeId,
                                             endNodeId);
        }
        else if (networkType
                 == CargoNetSim::GUI::NetworkType::Ship)
        {
            throw std::runtime_error(
                "Ship network shortest path is not "
                "supported yet.");
        }
    }
    catch (const std::exception &e)
    {
        qWarning() << "Error finding shortest path:"
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

    auto regionController =
        CargoNetSim::CargoNetSimController::getInstance()
            .getRegionDataController();
    auto regionNames =
        regionController->getAllRegionNames();

    for (auto regionName : regionNames)
    {
        Backend::RegionData *regionData =
            regionController->getRegionData(regionName);
        if (regionData)
        {
            for (auto netName :
                 regionData->getTrainNetworks())
            {
                // Remove the network from the canvas
                CargoNetSim::GUI::ViewController::
                    removeNetwork(mainWindow,
                                  NetworkType::Train,
                                  regionData, netName);
            }

            for (auto netName :
                 regionData->getTruckNetworks())
            {
                // Remove the network from the canvas
                CargoNetSim::GUI::ViewController::
                    removeNetwork(mainWindow,
                                  NetworkType::Truck,
                                  regionData, netName);
            }
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
    Backend::RegionData *regionData)
{
    if (!mainWindow || !regionData)
        return false;

    try
    {
        // Get the network based on type
        BaseNetwork *network = nullptr;

        if (networkType == NetworkType::Train)
        {
            network =
                regionData->getTrainNetwork(networkName);
        }
        else if (networkType == NetworkType::Truck)
        {
            network =
                regionData->getTruckNetwork(networkName);
        }
        else if (networkType == NetworkType::Ship)
        {
            throw std::runtime_error(
                "Ship network moving is not supported "
                "yet.");
        }

        if (!network)
            return false;

        // Perform the actual movement through the
        // ViewController
        bool success = ViewController::moveNetworkItems(
            mainWindow, networkType, networkName, offset,
            regionData->getRegion());

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
