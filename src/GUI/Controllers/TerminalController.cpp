#include "TerminalController.h"

#include "Backend/Application/NetworkManagementService.h"
#include "Backend/Application/ScenarioEditService.h"
#include "Backend/Application/NetworkViewService.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/GuiApi/ScenarioDocumentApi.h"
#include "Backend/Scenario/ScenarioRuntime.h"
#include "GUI/Controllers/StatusReporter.h"
#include "GUI/Controllers/ToolbarController.h"
#include "GUI/Controllers/UtilityFunctions.h"
#include "GUI/Items/GlobalTerminalItem.h"
#include "GUI/Items/MapPoint.h"
#include "GUI/Items/TerminalItem.h"
#include "GUI/MainWindow.h"
#include "GUI/Scenario/ItemEventBinder.h"
#include "GUI/Widgets/GraphicsScene.h"
#include "GUI/Widgets/GraphicsView.h"
#include "GUI/Widgets/PropertiesPanel.h"

#include <QApplication>
#include <QMessageBox>
#include <cmath>

namespace CargoNetSim
{
namespace GUI
{

namespace
{

QString currentRegionName(MainWindow *mainWindow)
{
    auto *networkView =
        mainWindow ? mainWindow->networkViewService() : nullptr;
    return networkView ? networkView->currentRegionName()
                       : QString();
}

bool isFinitePoint(const QPointF &point)
{
    return std::isfinite(point.x()) && std::isfinite(point.y());
}

QList<Backend::NetworkKind> selectedNetworkKinds(
    const QList<NetworkType> &networkTypes)
{
    QList<Backend::NetworkKind> kinds;
    for (NetworkType type : networkTypes)
    {
        const Backend::NetworkKind kind =
            type == NetworkType::Train
                ? Backend::NetworkKind::Rail
                : Backend::NetworkKind::Truck;
        if (!kinds.contains(kind))
            kinds.append(kind);
    }
    return kinds;
}

QString networkKindLabel(Backend::NetworkKind kind)
{
    switch (kind)
    {
    case Backend::NetworkKind::Rail:
        return QStringLiteral("train");
    case Backend::NetworkKind::Truck:
        return QStringLiteral("truck");
    }
    return QStringLiteral("transport");
}

} // namespace

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
        auto *globalScene =
            m_globalMapView ? m_globalMapView->getScene()
                            : m_globalMapScene;
        if (!m_runtime || !m_globalMapView || !globalScene)
        {
            qCWarning(lcGuiView)
                << "TerminalController::updateGlobalMapItem:"
                << "missing runtime or global map view/scene"
                << "terminalId=" << terminal->getTerminalId()
                << "hasRuntime=" << (m_runtime != nullptr)
                << "hasGlobalView=" << (m_globalMapView != nullptr)
                << "hasGlobalScene=" << (globalScene != nullptr);
            return;
        }

        if (terminal->getGlobalTerminalItem())
        {
            qCDebug(lcGuiView)
                << "TerminalController::updateGlobalMapItem:"
                << "existing global item, updating position";
            terminal->getGlobalTerminalItem()
                ->updateFromLinkedTerminal();
            updateTerminalGlobalPosition(terminal);
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

            globalScene->addItemWithId(
                global_terminal,
                global_terminal->sceneRegistryKey());
            qCDebug(lcGuiView)
                << "TerminalController::updateGlobalMapItem:"
                << "addItemWithId ok";
            terminal->setGlobalTerminalItem(
                global_terminal);

            QObject::connect(
                terminal, &TerminalItem::positionChanged,
                global_terminal,
                [this, terminal]() {
                    updateTerminalGlobalPosition(terminal);
                });

            qCDebug(lcGuiView)
                << "TerminalController::updateGlobalMapItem:"
                << "calling updateTerminalGlobalPosition";
            global_terminal->updateFromLinkedTerminal();
            updateTerminalGlobalPosition(terminal);
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
            auto *globalScene =
                m_globalMapView ? m_globalMapView->getScene()
                                : m_globalMapScene;
            if (globalScene
                && globalScene
                       ->removeItemWithId<GlobalTerminalItem>(
                           item->sceneRegistryKey()))
            {
                qCDebug(lcGuiView)
                    << "TerminalController::updateGlobalMapItem:"
                    << "removeItemWithId ok";
            }
            else
            {
                if (auto *ownerScene = item->scene())
                    ownerScene->removeItem(item);
                delete item;
                qCWarning(lcGuiView)
                    << "TerminalController::updateGlobalMapItem:"
                    << "global mirror registry removal missed;"
                    << "deleted direct scene item for"
                    << terminal->getTerminalId();
            }
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

    if (terminalId.isEmpty()) return;
    auto *scene =
        m_globalMapView ? m_globalMapView->getScene()
                        : m_globalMapScene;
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
    // primary entities). Region global/local origin changes affect every
    // terminal mirror in the region, so reconcile each mirror through the
    // same lifecycle path used for terminal add/change events.
    const auto terminals =
        m_regionScene->getItemsByType<TerminalItem>();
    int refreshed = 0;
    for (TerminalItem *t : terminals)
    {
        if (!t || t->getRegion() != regionName) continue;
        updateGlobalMapItem(t);
        if (t->getGlobalTerminalItem()) ++refreshed;
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
    TerminalItem *terminal)
{
    if (!terminal || !m_runtime || !m_globalMapView)
    {
        qCWarning(lcGuiView)
            << "TerminalController::updateTerminalGlobalPosition:"
            << "missing terminal/runtime/global view"
            << "terminalId=" << (terminal ? terminal->getTerminalId()
                                           : QString())
            << "hasRuntime=" << (m_runtime != nullptr)
            << "hasGlobalView=" << (m_globalMapView != nullptr);
        return;
    }

    GlobalTerminalItem *globalITem =
        terminal->getGlobalTerminalItem();
    if (!globalITem)
    {
        qCDebug(lcGuiView)
            << "TerminalController::updateTerminalGlobalPosition:"
            << "no global item, returning";
        return;
    }

    const QString terminalId = terminal->getTerminalId();
    if (terminalId.isEmpty())
    {
        qCWarning(lcGuiView)
            << "TerminalController::updateTerminalGlobalPosition:"
            << "terminal is not bound to a ScenarioDocument placement";
        return;
    }

    const QPointF globalLonLat =
        m_runtime->document().globalPositionOf(terminalId);
    if (!isFinitePoint(globalLonLat))
    {
        qCWarning(lcGuiView)
            << "TerminalController::updateTerminalGlobalPosition:"
            << "document has no valid global lat/lon for terminal"
            << terminalId;
        return;
    }

    const QPointF globalPos =
        m_globalMapView->wgs84ToScene(globalLonLat);
    globalITem->setPos(globalPos);
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
    const QString id     = Backend::Application::ScenarioEditService::createTerminal(
        doc, terminalType, region, latLon, role);
    if (id.isEmpty())
    {
        qCWarning(lcGuiView)
            << "TerminalController::createTerminalAtPoint:"
            << "ScenarioEditService::createTerminal returned empty id";
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

    const QString terminalId = terminal->getTerminalId();
    if (!m_runtime || terminalId.isEmpty())
    {
        qCWarning(lcGuiView)
            << "TerminalController::linkTerminalToClosestNetworkPoint:"
            << "missing backend binding"
            << "terminalId=" << terminalId
            << "hasRuntime=" << (m_runtime != nullptr);
        m_status->showError(
            QString("Cannot link unbound terminal '%1'.")
                .arg(terminal->getProperty("Name").toString()),
            3000);
        return false;
    }

    Backend::Application::NetworkManagementService networkService;
    const auto result =
        networkService.linkTerminalToClosestNetworkNode(
            m_runtime->document(),
            terminalId,
            selectedNetworkKinds(networkTypes),
            Backend::Scenario::LinkageSource::Manual);

    if (!result.succeeded())
    {
        qCWarning(lcGuiView)
            << "TerminalController::linkTerminalToClosestNetworkPoint:"
            << "backend link failed"
            << "terminalId=" << terminalId
            << "status=" << static_cast<int>(result.status)
            << "message=" << result.message;
        m_status->showError(result.message, 3000);
        return false;
    }

    m_status->showMessage(
        QString("Terminal '%1' successfully linked to "
                "%2 network '%3'")
            .arg(terminal->getProperty("Name").toString())
            .arg(networkKindLabel(result.kind))
            .arg(result.networkName),
        3000);

    return true;
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

    const QString currentRegion =
        currentRegionName(m_mainWindow);

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
            Backend::Application::NetworkViewService
                networkView;
            Backend::NetworkKind kind{};
            const bool recognised =
                !networkView.networkNameOf(network, &kind)
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
    const QString terminalId = terminal->getTerminalId();
    if (!m_runtime || terminalId.isEmpty())
    {
        qCWarning(lcGuiView)
            << "unlinkTerminalFromNetworks:"
            << "terminal is not backend-bound"
            << "terminalId=" << terminalId
            << "hasRuntime=" << (m_runtime != nullptr);
        m_status->showError(
            QString("Cannot unlink unbound terminal '%1'.")
                .arg(terminal->getProperty("Name").toString()),
            3000);
        return false;
    }

    for (MapPoint *point : linkedPoints)
    {
        auto *linkage = point->linkageModel();
        if (!linkage || linkage->terminalId != terminalId)
        {
            qCWarning(lcGuiView)
                << "unlinkTerminalFromNetworks:"
                << "MapPoint has no matching backend linkage"
                << "terminalId=" << terminalId
                << "hasLinkage=" << (linkage != nullptr);
            continue;
        }

        Backend::Application::NetworkViewService networkView;
        Backend::NetworkKind networkKind{};
        const QString canonicalNetworkName = linkage->networkName;
        const int nodeId = linkage->nodeId;
        QString displayNetworkName = canonicalNetworkName;
        const bool recognisedNetwork =
            !networkView.networkNameOf(
                point->getReferenceNetwork(), &networkKind).isEmpty();
        QString networkTypeStr = "unknown";
        if (displayNetworkName.isEmpty())
        {
            displayNetworkName = "Unknown Network";
        }
        else if (recognisedNetwork)
        {
            networkTypeStr =
                networkKind == Backend::NetworkKind::Rail
                    ? "train"
                    : "truck";
        }

        const bool unlinked =
            Backend::Application::ScenarioEditService::
                unlinkTerminalFromNode(
                    &m_runtime->document(),
                    terminalId, canonicalNetworkName, nodeId);
        if (!unlinked)
        {
            qCWarning(lcGuiView)
                << "unlinkTerminalFromNetworks:"
                << "backend unlink failed"
                << "terminalId=" << terminalId
                << "network=" << canonicalNetworkName
                << "nodeId=" << nodeId;
            continue;
        }

        point->setLinkedTerminal(nullptr);
        unlinkCount++;

        m_status->showMessage(
            QString("Terminal '%1' unlinked from %2 "
                    "network '%3'")
                .arg(terminal->getProperty("Name")
                         .toString())
                .arg(networkTypeStr)
                .arg(displayNetworkName),
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

    const QString currentRegion =
        currentRegionName(m_mainWindow);

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
