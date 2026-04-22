#include "TerminalController.h"

#include "Backend/Commons/LogCategories.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Scenario/NetworkLookup.h"
#include "Backend/Scenario/ScenarioDocument.h"
#include "Backend/Scenario/ScenarioRuntime.h"
#include "GUI/Controllers/StatusReporter.h"
#include "GUI/Controllers/ToolbarController.h"
#include "GUI/Controllers/UtilityFunctions.h"
#include "GUI/Items/GlobalTerminalItem.h"
#include "GUI/Items/MapPoint.h"
#include "GUI/Items/RegionCenterPoint.h"
#include "GUI/Items/TerminalItem.h"
#include "GUI/MainWindow.h"
#include "GUI/Scenario/ItemEventBinder.h"
#include "GUI/Scenario/ScenarioMutator.h"
#include "GUI/Widgets/GraphicsScene.h"
#include "GUI/Widgets/GraphicsView.h"
#include "GUI/Widgets/PropertiesPanel.h"

#include <QApplication>
#include <QMessageBox>
#include <limits>

namespace CargoNetSim
{
namespace GUI
{

TerminalController::TerminalController(
    GraphicsScene  *regionScene,
    GraphicsView   *regionView,
    GraphicsScene  *globalMapScene,
    GraphicsView   *globalMapView,
    MainWindow     *mainWindow,
    StatusReporter *status,
    QObject        *parent)
    : QObject(parent)
    , m_regionScene(regionScene)
    , m_regionView(regionView)
    , m_globalMapScene(globalMapScene)
    , m_globalMapView(globalMapView)
    , m_mainWindow(mainWindow)
    , m_status(status)
{
    qCDebug(lcGuiView) << "TerminalController: created";
}

void TerminalController::setRuntime(
    Backend::Scenario::ScenarioRuntime *rt)
{
    m_runtime = rt;
    qCDebug(lcGuiView)
        << "TerminalController::setRuntime:"
        << (rt ? "valid" : "null");
}

// ----------------------------------------------------------------
// updateGlobalMapItem
// ----------------------------------------------------------------

void TerminalController::updateGlobalMapItem(
    TerminalItem *terminal)
{
    qCDebug(lcGuiView)
        << "TerminalController::updateGlobalMapItem: begin";
    if (!terminal)
    {
        qCDebug(lcGuiView)
            << "TerminalController::updateGlobalMapItem:"
            << "null terminal, returning";
        return;
    }
    auto props = terminal->getProperties();
    bool show =
        props.contains("Show on Global Map")
            ? props.value("Show on Global Map").toBool()
            : true;
    qCDebug(lcGuiView)
        << "TerminalController::updateGlobalMapItem:"
        << "show=" << show;

    if (show)
    {
        auto regionData =
            CargoNetSim::CargoNetSimController::
                getInstance()
                    .getRegionDataController()
                    ->getRegionData(terminal->getRegion());

        if (!regionData)
        {
            qCDebug(lcGuiView)
                << "TerminalController::updateGlobalMapItem:"
                << "regionData null, returning";
            return;
        }

        auto regionCenterPoint =
            regionData->getVariableAs<RegionCenterPoint *>(
                "regionCenterPoint", nullptr);

        if (!regionCenterPoint)
        {
            qCDebug(lcGuiView)
                << "TerminalController::updateGlobalMapItem:"
                << "regionCenterPoint null, returning";
            return;
        }

        if (terminal->getGlobalTerminalItem())
        {
            qCDebug(lcGuiView)
                << "TerminalController::updateGlobalMapItem:"
                << "existing global item, updating position";
            updateTerminalGlobalPosition(
                regionCenterPoint, terminal);
        }
        else
        {
            qCDebug(lcGuiView)
                << "TerminalController::updateGlobalMapItem:"
                << "creating GlobalTerminalItem";
            QPixmap pixmap       = terminal->getPixmap();
            auto global_terminal = new GlobalTerminalItem(
                pixmap, terminal, nullptr);
            qCDebug(lcGuiView)
                << "TerminalController::updateGlobalMapItem:"
                << "GlobalTerminalItem constructed";

            m_globalMapView->getScene()
                ->addItemWithId(global_terminal,
                                global_terminal->sceneRegistryKey());
            qCDebug(lcGuiView)
                << "TerminalController::updateGlobalMapItem:"
                << "addItemWithId ok";
            terminal->setGlobalTerminalItem(
                global_terminal);

            QObject::connect(
                terminal, &TerminalItem::positionChanged,
                [this, regionCenterPoint,
                 terminal]() {
                    if (!regionCenterPoint)
                    {
                        return;
                    }
                    updateTerminalGlobalPosition(
                        regionCenterPoint, terminal);
                });

            qCDebug(lcGuiView)
                << "TerminalController::updateGlobalMapItem:"
                << "calling updateTerminalGlobalPosition";
            updateTerminalGlobalPosition(
                regionCenterPoint, terminal);
            qCDebug(lcGuiView)
                << "TerminalController::updateGlobalMapItem:"
                << "updateTerminalGlobalPosition ok";
        }
    }
    else
    {
        qCDebug(lcGuiView)
            << "TerminalController::updateGlobalMapItem:"
            << "show=false, checking existing global item";
        if (terminal->getGlobalTerminalItem())
        {
            qCDebug(lcGuiView)
                << "TerminalController::updateGlobalMapItem:"
                << "existing global item present, removing";
            GlobalTerminalItem *item =
                terminal->getGlobalTerminalItem();
            terminal->setGlobalTerminalItem(nullptr);
            m_globalMapView->getScene()
                ->removeItemWithId<GlobalTerminalItem>(
                    item->sceneRegistryKey());
            qCDebug(lcGuiView)
                << "TerminalController::updateGlobalMapItem:"
                << "removeItemWithId ok";
        }
        else
        {
            qCDebug(lcGuiView)
                << "TerminalController::updateGlobalMapItem:"
                << "no existing global item, nothing to remove";
        }
    }
    qCDebug(lcGuiView)
        << "TerminalController::updateGlobalMapItem: end";
}

// ----------------------------------------------------------------
// removeGlobalMirror
// ----------------------------------------------------------------

void TerminalController::removeGlobalMirror(
    const QString &terminalId)
{
    qCDebug(lcGuiView)
        << "TerminalController::removeGlobalMirror:"
        << "terminalId=" << terminalId;

    if (terminalId.isEmpty() || !m_globalMapView) return;
    auto *scene = m_globalMapView->getScene();
    if (!scene) return;

    // Lookup by terminal id through the scene's own registry.
    // GlobalTerminalItem::domainKey() is the bound terminal id, so the
    // scene-registry key and the terminalId argument match directly.
    // This avoids dereferencing TerminalItem::getGlobalTerminalItem(),
    // which may already be dangling by the time terminalRemoved fires.
    scene->removeItemWithId<GlobalTerminalItem>(terminalId);
}

// ----------------------------------------------------------------
// refreshGlobalMirrorsForRegion
// ----------------------------------------------------------------

void TerminalController::refreshGlobalMirrorsForRegion(
    const QString &regionName)
{
    qCDebug(lcGuiView)
        << "TerminalController::refreshGlobalMirrorsForRegion:"
        << "region=" << regionName;

    if (regionName.isEmpty() || !m_regionScene) return;

    // Drive the refresh from the regionScene's TerminalItems (the
    // primary entities). Each one that is both (a) in this region
    // and (b) currently mirrored pushes its mirror to recompute via
    // updateFromLinkedTerminal(), which in turn reads the region's
    // (now-updated) globalPosition / localOrigin from the bound doc.
    // Lifecycle is out of scope here — we do not create mirrors for
    // terminals that weren't previously mirrored (that remains
    // updateGlobalMapItem's responsibility, triggered by
    // terminalAdded / terminalChanged).
    const auto terminals =
        m_regionScene->getItemsByType<TerminalItem>();
    int refreshed = 0;
    for (TerminalItem *t : terminals)
    {
        if (!t || t->getRegion() != regionName) continue;
        if (GlobalTerminalItem *mirror =
                t->getGlobalTerminalItem())
        {
            mirror->updateFromLinkedTerminal();
            ++refreshed;
        }
    }
    qCDebug(lcGuiView)
        << "TerminalController::refreshGlobalMirrorsForRegion:"
        << "region=" << regionName
        << "mirrors refreshed=" << refreshed;
}

// ----------------------------------------------------------------
// updateTerminalGlobalPosition (private)
// ----------------------------------------------------------------

void TerminalController::updateTerminalGlobalPosition(
    RegionCenterPoint *regionCenterPoint,
    TerminalItem      *terminal)
{
    if (!regionCenterPoint || !terminal)
    {
        qCDebug(lcGuiView)
            << "TerminalController::updateTerminalGlobalPosition:"
            << "null argument, returning";
        return;
    }

    auto props = regionCenterPoint->getProperties();
    auto center_shared_lat =
        props.contains("Shared Latitude")
            ? props.value("Shared Latitude").toDouble()
            : 0.0;
    auto center_shared_lon =
        props.contains("Shared Longitude")
            ? props.value("Shared Longitude").toDouble()
            : 0;

    auto center_lon =
        props.contains("Longitude")
            ? props.value("Longitude").toDouble()
            : 0;
    auto center_lat =
        props.contains("Latitude")
            ? props.value("Latitude").toDouble()
            : 0;

    auto out = m_regionView->sceneToWGS84(terminal->pos());

    double terminal_lon = out.x();
    double terminal_lat = out.y();

    double delta_lat = terminal_lat - center_lat;
    double delta_lon = terminal_lon - center_lon;

    double item_global_view_lon =
        center_shared_lon + delta_lon;
    double item_global_view_lat =
        center_shared_lat + delta_lat;

    GlobalTerminalItem *globalITem =
        terminal->getGlobalTerminalItem();
    if (!globalITem)
    {
        qCDebug(lcGuiView)
            << "TerminalController::updateTerminalGlobalPosition:"
            << "no global item, returning";
        return;
    }

    QPointF globalPos =
        m_globalMapView->wgs84ToScene(QPointF(
            item_global_view_lon, item_global_view_lat));
    globalITem->setPos(globalPos);
}

// ----------------------------------------------------------------
// updateTerminalPositionByGlobalPosition
// ----------------------------------------------------------------

bool TerminalController::updateTerminalPositionByGlobalPosition(
    TerminalItem *terminal,
    QPointF       globalGeoPos)
{
    qCDebug(lcGuiView)
        << "TerminalController::updateTerminalPositionByGlobalPosition:"
        << "begin";
    if (!terminal || !terminal->getGlobalTerminalItem())
    {
        qCDebug(lcGuiView)
            << "TerminalController::updateTerminalPositionByGlobalPosition:"
            << "null argument, returning false";
        return false;
    }

    QString currentRegion = terminal->getRegion();

    auto regionCenterPoint =
        CargoNetSim::CargoNetSimController::getInstance()
            .getRegionDataController()
            ->getRegionData(currentRegion)
            ->getVariableAs<RegionCenterPoint *>(
                "regionCenterPoint", nullptr);

    if (!regionCenterPoint)
    {
        qCWarning(lcGuiView)
            << "TerminalController::updateTerminalPositionByGlobalPosition:"
            << "regionCenterPoint is null for region"
            << currentRegion;
        return false;
    }

    auto props = regionCenterPoint->getProperties();

    auto center_lon =
        props.contains("Longitude")
            ? props.value("Longitude").toDouble()
            : 0;
    auto center_lat =
        props.contains("Latitude")
            ? props.value("Latitude").toDouble()
            : 0;

    QPointF terminalGeoPos =
        m_regionView->sceneToWGS84(terminal->pos());

    double delta_lat = terminalGeoPos.y() - center_lat;
    double delta_lon = terminalGeoPos.x() - center_lon;

    double new_shared_lat = globalGeoPos.y() - delta_lat;
    double new_shared_lon = globalGeoPos.x() - delta_lon;

    regionCenterPoint->setProperty("Shared Latitude",
                                   new_shared_lat);
    regionCenterPoint->setProperty("Shared Longitude",
                                   new_shared_lon);

    UtilitiesFunctions::updateGlobalMapForRegion(
        m_mainWindow, currentRegion);

    return true;
}

// ----------------------------------------------------------------
// globalToLocalLatLon
// ----------------------------------------------------------------

QPointF TerminalController::globalToLocalLatLon(
    const QString &region,
    const QPointF &globalLatLon) const
{
    auto *regionData =
        CargoNetSim::CargoNetSimController::getInstance()
            .getRegionDataController()
            ->getRegionData(region);
    if (!regionData)
    {
        qCWarning(lcGuiView)
            << "TerminalController::globalToLocalLatLon:"
            << "no region data for" << region
            << "— returning input unchanged";
        return globalLatLon;
    }

    auto *regionCenterPoint =
        regionData->getVariableAs<RegionCenterPoint *>(
            "regionCenterPoint", nullptr);
    if (!regionCenterPoint)
    {
        qCWarning(lcGuiView)
            << "TerminalController::globalToLocalLatLon:"
            << "no regionCenterPoint for" << region
            << "— returning input unchanged";
        return globalLatLon;
    }

    const auto props = regionCenterPoint->getProperties();
    const double localLat =
        props.contains("Latitude")
            ? props.value("Latitude").toDouble()
            : 0.0;
    const double localLon =
        props.contains("Longitude")
            ? props.value("Longitude").toDouble()
            : 0.0;
    const double sharedLat =
        props.contains("Shared Latitude")
            ? props.value("Shared Latitude").toDouble()
            : 0.0;
    const double sharedLon =
        props.contains("Shared Longitude")
            ? props.value("Shared Longitude").toDouble()
            : 0.0;

    // Offset between shared (global) origin and local origin is the
    // region-to-world translation; subtracting it converts a global
    // WGS84 point to the region-local lat/lon the document stores.
    const double newLocalLat =
        globalLatLon.y() - (sharedLat - localLat);
    const double newLocalLon =
        globalLatLon.x() - (sharedLon - localLon);

    // QPointF convention: x = lon, y = lat.
    return QPointF(newLocalLon, newLocalLat);
}

// ----------------------------------------------------------------
// createTerminalAtPoint
// ----------------------------------------------------------------

TerminalItem *TerminalController::createTerminalAtPoint(
    const QString &region,
    const QString &terminalType,
    const QPointF &point,
    Backend::Scenario::TerminalPlacement::TerminalRole role)
{
    qCDebug(lcGuiView)
        << "TerminalController::createTerminalAtPoint:"
        << "type=" << terminalType
        << "region=" << region
        << "point=" << point;

    if (!m_runtime)
    {
        qCWarning(lcGuiView)
            << "TerminalController::createTerminalAtPoint:"
            << "no runtime, cannot create terminal";
        return nullptr;
    }

    auto *doc   = &m_runtime->document();
    if (!m_regionView || !m_regionScene)
    {
        qCWarning(lcGuiView)
            << "TerminalController::createTerminalAtPoint:"
            << "null view or scene, cannot create terminal";
        return nullptr;
    }

    const QPointF latLon = m_regionView->sceneToWGS84(point);
    const QString id     = Scenario::ScenarioMutator::createTerminal(
        doc, terminalType, region, latLon, role);
    if (id.isEmpty())
    {
        qCWarning(lcGuiView)
            << "TerminalController::createTerminalAtPoint:"
            << "ScenarioMutator::createTerminal returned empty id";
        return nullptr;
    }

    return m_regionScene->getItemById<TerminalItem>(id);
}

// ----------------------------------------------------------------
// linkTerminalToClosestNetworkPoint
// ----------------------------------------------------------------

bool TerminalController::linkTerminalToClosestNetworkPoint(
    TerminalItem             *terminal,
    const QList<NetworkType> &networkTypes)
{
    qCDebug(lcGuiView)
        << "TerminalController::linkTerminalToClosestNetworkPoint:"
        << "begin";
    if (!terminal || networkTypes.isEmpty())
    {
        qCDebug(lcGuiView)
            << "TerminalController::linkTerminalToClosestNetworkPoint:"
            << "null terminal or empty networkTypes, returning false";
        return false;
    }

    QString           region = terminal->getRegion();
    QList<MapPoint *> networkPoints;
    QMap<NetworkType, QList<MapPoint *>>
        alreadyLinkedPointsByType;

    QList<MapPoint *> allMapPoints =
        m_regionScene->getItemsByType<MapPoint>();

    for (MapPoint *point : allMapPoints)
    {
        if (point->getRegion() != region)
        {
            continue;
        }

        QObject *network = point->getReferenceNetwork();
        if (!network)
        {
            continue;
        }

        NetworkType matchedType = NetworkType::Train;
        bool        isMatchingType = false;
        {
            Backend::NetworkKind kind{};
            const bool recognised = !Backend::Scenario::NetworkLookup
                                         ::networkNameOf(network, &kind)
                                         .isEmpty();
            if (recognised
                && kind == Backend::NetworkKind::Rail
                && networkTypes.contains(NetworkType::Train))
            {
                isMatchingType = true;
                matchedType    = NetworkType::Train;
            }
            else if (recognised
                     && kind == Backend::NetworkKind::Truck
                     && networkTypes.contains(NetworkType::Truck))
            {
                isMatchingType = true;
                matchedType    = NetworkType::Truck;
            }
        }

        if (isMatchingType)
        {
            if (point->getLinkedTerminal())
            {
                alreadyLinkedPointsByType[matchedType]
                    .append(point);
            }
            else
            {
                networkPoints.append(point);
            }
        }
    }

    QApplication::processEvents();

    if (networkPoints.isEmpty())
    {
        QString networkTypeStr;
        if (networkTypes.size() == 1)
        {
            networkTypeStr =
                networkTypes.first() == NetworkType::Train
                    ? "train"
                    : "truck";
        }
        else
        {
            networkTypeStr = "transport";
        }

        m_status->showError(
            QString("No available %1 network points found "
                    "in region '%2'")
                .arg(networkTypeStr)
                .arg(region),
            3000);
        return false;
    }

    for (NetworkType type : networkTypes)
    {
        const QList<MapPoint *> &typeLinkedPoints =
            alreadyLinkedPointsByType[type];
        for (MapPoint *point : typeLinkedPoints)
        {
            if (point->getLinkedTerminal() == terminal)
            {
                return false;
            }
        }

        QApplication::processEvents();
    }

    MapPoint *closestPoint = nullptr;
    qreal minDistance = std::numeric_limits<qreal>::max();

    QPointF terminalPos = terminal->pos();
    for (MapPoint *point : networkPoints)
    {
        QPointF pointPos = point->pos();
        qreal   distance =
            QLineF(terminalPos, pointPos).length();

        if (distance < minDistance)
        {
            minDistance  = distance;
            closestPoint = point;
        }
    }

    if (closestPoint)
    {
        UtilitiesFunctions::linkMapPointToTerminal(
            m_mainWindow, closestPoint, terminal);

        Backend::NetworkKind networkKind{};
        QString              networkName =
            Backend::Scenario::NetworkLookup::networkNameOf(
                closestPoint->getReferenceNetwork(), &networkKind);
        QString networkTypeStr = "transport";
        if (networkName.isEmpty())
        {
            networkName = "Unknown Network";
        }
        else
        {
            networkTypeStr =
                networkKind == Backend::NetworkKind::Rail
                    ? "train"
                    : "truck";
        }

        if (m_runtime && networkName != "Unknown Network")
        {
            const int nodeId = closestPoint
                                   ->getReferencedNetworkNodeID()
                                   .toInt();
            Scenario::ScenarioMutator::linkTerminalToNode(
                &m_runtime->document(),
                terminal->getTerminalId(), networkName, nodeId,
                Backend::Scenario::LinkageSource::Manual);
        }

        m_status->showMessage(
            QString("Terminal '%1' successfully linked to "
                    "%2 network '%3'")
                .arg(terminal->getProperty("Name")
                         .toString())
                .arg(networkTypeStr)
                .arg(networkName),
            3000);

        return true;
    }

    m_status->showError(
        "Failed to find a suitable network point to link.",
        3000);
    return false;
}

// ----------------------------------------------------------------
// linkAllVisibleTerminalsToNetwork
// ----------------------------------------------------------------

void TerminalController::linkAllVisibleTerminalsToNetwork(
    const QList<NetworkType> &networkTypes)
{
    qCDebug(lcGuiView)
        << "TerminalController::linkAllVisibleTerminalsToNetwork:"
        << "begin";
    if (networkTypes.isEmpty())
    {
        qCDebug(lcGuiView)
            << "TerminalController::linkAllVisibleTerminalsToNetwork:"
            << "empty networkTypes, returning";
        m_status->showError(
            "No network types selected for linking.", 3000);
        return;
    }

    m_status->storeButtons();
    m_status->disableButtons();
    m_status->startProgress();

    if (!m_regionScene)
    {
        qCWarning(lcGuiView)
            << "TerminalController::linkAllVisibleTerminalsToNetwork:"
            << "null scene, returning";
        m_status->showError(
            "No active scene found.", 3000);
        m_status->restoreButtons();
        m_status->stopProgress();
        return;
    }

    QString currentRegion =
        CargoNetSim::CargoNetSimController::getInstance()
            .getRegionDataController()
            ->getCurrentRegion();

    QList<TerminalItem *> allTerminals =
        m_regionScene->getItemsByType<TerminalItem>();
    QList<TerminalItem *> visibleTerminals;

    for (TerminalItem *terminal : allTerminals)
    {
        if (terminal && terminal->isVisible()
            && terminal->getRegion() == currentRegion)
        {
            visibleTerminals.append(terminal);
        }
    }

    if (visibleTerminals.isEmpty())
    {
        m_status->showError(
            QString(
                "No visible terminals found in region '%1'")
                .arg(currentRegion),
            3000);
        m_status->restoreButtons();
        m_status->stopProgress();
        return;
    }

    QMessageBox msgBox(m_mainWindow);
    msgBox.setWindowTitle("Link Terminals to Network");
    msgBox.setText(
        QString(
            "This will link all %1 visible terminals in "
            "region '%2' to their closest network points.")
            .arg(visibleTerminals.size())
            .arg(currentRegion));

    QString networkTypesStr;
    if (networkTypes.contains(NetworkType::Train)
        && networkTypes.contains(NetworkType::Truck))
    {
        networkTypesStr = "train and truck";
    }
    else if (networkTypes.contains(NetworkType::Train))
    {
        networkTypesStr = "train";
    }
    else if (networkTypes.contains(NetworkType::Truck))
    {
        networkTypesStr = "truck";
    }

    msgBox.setInformativeText(
        QString("Selected network types: %1\n\nDo you want "
                "to continue?")
            .arg(networkTypesStr));
    msgBox.setStandardButtons(QMessageBox::Yes
                              | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);

    int result = msgBox.exec();
    if (result != QMessageBox::Yes)
    {
        m_status->restoreButtons();
        m_status->stopProgress();
        return;
    }

    int successCount = 0;

    for (TerminalItem *terminal : visibleTerminals)
    {
        if (linkTerminalToClosestNetworkPoint(
                terminal, networkTypes))
        {
            successCount++;
            QApplication::processEvents();
        }

        QApplication::processEvents();
    }

    if (successCount > 0)
    {
        m_status->showMessage(
            QString(
                "Successfully linked %1 of %2 terminals to "
                "their closest network points")
                .arg(successCount)
                .arg(visibleTerminals.size()),
            5000);
    }
    else
    {
        m_status->showError(
            "No terminals could be linked to network "
            "points.",
            3000);
    }

    m_status->restoreButtons();
    m_status->stopProgress();
}

// ----------------------------------------------------------------
// unlinkTerminalFromNetworkPoints
// ----------------------------------------------------------------

bool TerminalController::unlinkTerminalFromNetworkPoints(
    TerminalItem             *terminal,
    const QList<NetworkType> &networkTypes)
{
    qCDebug(lcGuiView)
        << "TerminalController::unlinkTerminalFromNetworkPoints:"
        << "begin";
    if (!terminal || networkTypes.isEmpty())
    {
        qCDebug(lcGuiView)
            << "TerminalController::unlinkTerminalFromNetworkPoints:"
            << "null terminal or empty networkTypes, returning false";
        return false;
    }

    QString           region = terminal->getRegion();
    QList<MapPoint *> linkedPoints;

    QList<MapPoint *> allMapPoints =
        m_regionScene->getItemsByType<MapPoint>();

    for (MapPoint *point : allMapPoints)
    {
        if (point->getRegion() != region
            || point->getLinkedTerminal() != terminal)
        {
            continue;
        }

        QObject *network = point->getReferenceNetwork();
        if (!network)
        {
            continue;
        }

        bool isMatchingType = false;
        {
            Backend::NetworkKind kind{};
            const bool recognised = !Backend::Scenario::NetworkLookup
                                         ::networkNameOf(network, &kind)
                                         .isEmpty();
            if (recognised
                && kind == Backend::NetworkKind::Rail
                && networkTypes.contains(NetworkType::Train))
            {
                isMatchingType = true;
            }
            else if (recognised
                     && kind == Backend::NetworkKind::Truck
                     && networkTypes.contains(NetworkType::Truck))
            {
                isMatchingType = true;
            }
        }

        if (isMatchingType)
        {
            linkedPoints.append(point);
        }
    }

    if (linkedPoints.isEmpty())
    {
        QStringList typeNames;
        if (networkTypes.contains(NetworkType::Train))
            typeNames.append("train");
        if (networkTypes.contains(NetworkType::Truck))
            typeNames.append("truck");

        QString typesStr = typeNames.join(" or ");

        m_status->showError(
            QString("Terminal '%1' is not linked to any %2 "
                    "network points.")
                .arg(terminal->getProperty("Name")
                         .toString())
                .arg(typesStr),
            3000);
        return false;
    }

    int unlinkCount = 0;
    for (MapPoint *point : linkedPoints)
    {
        Backend::NetworkKind networkKind{};
        QString              networkName =
            Backend::Scenario::NetworkLookup::networkNameOf(
                point->getReferenceNetwork(), &networkKind);
        QString networkTypeStr = "unknown";
        if (networkName.isEmpty())
        {
            networkName = "Unknown Network";
        }
        else
        {
            networkTypeStr =
                networkKind == Backend::NetworkKind::Rail
                    ? "train"
                    : "truck";
        }

        point->setLinkedTerminal(nullptr);

        if (m_runtime && networkName != "Unknown Network")
        {
            const int nodeId =
                point->getReferencedNetworkNodeID().toInt();
            Scenario::ScenarioMutator::unlinkTerminalFromNode(
                &m_runtime->document(),
                terminal->getTerminalId(), networkName, nodeId);
        }

        unlinkCount++;

        m_status->showMessage(
            QString("Terminal '%1' unlinked from %2 "
                    "network '%3'")
                .arg(terminal->getProperty("Name")
                         .toString())
                .arg(networkTypeStr)
                .arg(networkName),
            1500);
    }

    if (unlinkCount > 0)
    {
        m_status->showMessage(
            QString("Successfully unlinked terminal '%1' "
                    "from %2 network point(s)")
                .arg(terminal->getProperty("Name")
                         .toString())
                .arg(unlinkCount),
            3000);

        if (m_mainWindow->propertiesPanel()->getCurrentItem()
            == terminal)
        {
            m_mainWindow->propertiesPanel()->displayProperties(
                terminal);
        }

        return true;
    }

    return false;
}

// ----------------------------------------------------------------
// unlinkAllVisibleTerminalsToNetwork
// ----------------------------------------------------------------

void TerminalController::unlinkAllVisibleTerminalsToNetwork(
    const QList<NetworkType> &networkTypes)
{
    qCDebug(lcGuiView)
        << "TerminalController::unlinkAllVisibleTerminalsToNetwork:"
        << "begin";
    if (networkTypes.isEmpty())
    {
        qCDebug(lcGuiView)
            << "TerminalController::unlinkAllVisibleTerminalsToNetwork:"
            << "empty networkTypes, returning";
        m_status->showError(
            "No network types selected for unlinking.",
            3000);
        return;
    }

    m_status->storeButtons();
    m_status->disableButtons();
    m_status->startProgress();

    if (!m_regionScene)
    {
        qCWarning(lcGuiView)
            << "TerminalController::unlinkAllVisibleTerminalsToNetwork:"
            << "null scene, returning";
        m_status->restoreButtons();
        m_status->stopProgress();
        return;
    }

    QString currentRegion =
        CargoNetSim::CargoNetSimController::getInstance()
            .getRegionDataController()
            ->getCurrentRegion();

    QList<TerminalItem *> allTerminals =
        m_regionScene->getItemsByType<TerminalItem>();
    QList<TerminalItem *> visibleTerminals;

    for (TerminalItem *terminal : allTerminals)
    {
        if (terminal && terminal->isVisible()
            && terminal->getRegion() == currentRegion)
        {
            visibleTerminals.append(terminal);
        }
    }

    if (visibleTerminals.isEmpty())
    {
        m_status->showError(
            QString(
                "No visible terminals found in region '%1'")
                .arg(currentRegion),
            3000);

        m_status->restoreButtons();
        m_status->stopProgress();
        return;
    }

    QMessageBox msgBox(m_mainWindow);
    msgBox.setWindowTitle("Unlink Terminals from Network");
    msgBox.setText(
        QString("This will unlink all %1 visible terminals "
                "in region '%2' from network points.")
            .arg(visibleTerminals.size())
            .arg(currentRegion));

    QString networkTypesStr;
    if (networkTypes.contains(NetworkType::Train)
        && networkTypes.contains(NetworkType::Truck))
    {
        networkTypesStr = "train and truck";
    }
    else if (networkTypes.contains(NetworkType::Train))
    {
        networkTypesStr = "train";
    }
    else if (networkTypes.contains(NetworkType::Truck))
    {
        networkTypesStr = "truck";
    }

    msgBox.setInformativeText(
        QString("Selected network types: %1\n\nDo you want "
                "to continue?")
            .arg(networkTypesStr));
    msgBox.setStandardButtons(QMessageBox::Yes
                              | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);

    int result = msgBox.exec();
    if (result != QMessageBox::Yes)
    {
        m_status->restoreButtons();
        m_status->stopProgress();
        return;
    }

    int successCount = 0;

    for (TerminalItem *terminal : visibleTerminals)
    {
        if (unlinkTerminalFromNetworkPoints(
                terminal, networkTypes))
        {
            successCount++;
        }

        QApplication::processEvents();
    }

    if (successCount > 0)
    {
        m_status->showMessage(
            QString("Successfully unlinked %1 of %2 "
                    "terminals from network points")
                .arg(successCount)
                .arg(visibleTerminals.size()),
            5000);
    }
    else
    {
        m_status->showError(
            "No terminals were unlinked from network "
            "points.",
            3000);
    }
    m_status->restoreButtons();
    m_status->stopProgress();
}

} // namespace GUI
} // namespace CargoNetSim
