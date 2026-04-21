#include "MainWindow.h"
#include "GUI/Utils/ApplicationLogger.h"
#include "Backend/Commons/LogCategories.h"

#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Controllers/RegionDataController.h"
#include "Backend/Scenario/Connection.h"
#include "Backend/Scenario/GlobalLink.h"
#include "Backend/Scenario/NodeLinkage.h"
#include "Backend/Scenario/RegionSpec.h"
#include "Backend/Scenario/ScenarioDocument.h"
#include "Backend/Scenario/ScenarioRuntime.h"
#include "Backend/Scenario/TerminalPlacement.h"
#include "GUI/Items/ConnectionLine.h"
#include "GUI/Items/MapPoint.h"
#include "GUI/Scenario/ConnectionLineFactory.h"
#include "GUI/Scenario/MapPointFactory.h"
#include "GUI/Scenario/RegionCenterPointFactory.h"
#include "GUI/Scenario/SceneRepopulator.h"
#include "GUI/Scenario/TerminalItemFactory.h"
#include "GUI/Input/InteractionController.h"
#include "GUI/Input/Modes/ConnectMode.h"
#include "GUI/Input/Modes/NormalMode.h"

#include <QApplication>
#include <QBuffer>
#include <QByteArray>
#include <QCloseEvent>
#include <QCursor>
#include <QDateTime>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QMessageBox>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplashScreen>
#include <QSplitter>
#include <QStatusBar>
#include <QPointer>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include "GUI/Items/MapLine.h"
#include "Widgets/GraphicsScene.h"
#include "Widgets/GraphicsView.h"
#include "Widgets/NetworkManagerDialog.h"
#include "Widgets/PropertiesPanel.h"
#include "Widgets/RegionManagerWidget.h"
#include "Widgets/SettingsWidget.h"
#include "Widgets/ShortestPathTable.h"

#include "Items/BackgroundPhotoItem.h"
#include "Scenario/BackgroundPhotoItemFactory.h"
#include "Items/ConnectionLabel.h"
#include "Items/ConnectionLine.h"
#include "Items/GlobalTerminalItem.h"
#include "Items/MapPoint.h"
#include "Items/RegionCenterPoint.h"
#include "Items/TerminalItem.h"

#include "Utils/IconCreator.h"

#include "Backend/Controllers/CargoNetSimController.h"
#include "Controllers/BasicButtonController.h"
#include "Controllers/ConnectionController.h"
#include "Controllers/NetworkController.h"
#include "Controllers/NetworkDrawingController.h"
#include "Controllers/RegionController.h"
#include "Controllers/SettingsController.h"
#include "Controllers/FleetController.h"
#include "Controllers/SceneVisibilityController.h"
#include "Controllers/TerminalController.h"
#include "Controllers/ToolbarController.h"
#include "Controllers/UtilityFunctions.h"
#include "Backend/Scenario/NetworkLookup.h"

#include "Serializers/ProjectSerializer.h"

namespace CargoNetSim
{
namespace GUI
{

std::atomic<MainWindow *> MainWindow::s_instance{nullptr};

MainWindow::MainWindow()
    : CustomMainWindow()
    , heartbeatController_(nullptr)
{
    // Tier 1 lifetime: release-build hard checks. Mirror the
    // CargoNetSimController guards for API symmetry.
    if (QCoreApplication::instance()
        && QThread::currentThread()
               != QCoreApplication::instance()->thread())
    {
        qFatal("MainWindow: must be constructed on the main "
               "(QCoreApplication) thread.");
    }
    // Atomically claim the single-instance slot.
    MainWindow *expected = nullptr;
    if (!s_instance.compare_exchange_strong(
            expected, this, std::memory_order_release,
            std::memory_order_relaxed))
    {
        qFatal("MainWindow: attempted to construct a second "
               "instance. Only one may live at a time.");
    }

    qCInfo(lcGui) << "MainWindow::MainWindow: begin";

    tableWasVisible_  = false;
    previousTabIndex_ = 0;

    // Shared CommandBus must exist before InteractionControllers
    // are wired below. Created before scenes/views because Task 1.13
    // keeps the bus lifetime >= any controller that references it.
    m_commandBus = new Input::CommandBus(this);

    // SettingsController must be created BEFORE initializeUI()
    // because settingsWidget_ is created in initializeUI()
    // and the connect() call needs m_settingsCtrl to be non-null.
    m_settingsCtrl = new SettingsController(this, this);

    // Setup UI components (must happen before controllers
    // and setRuntime because both need scenes/views).
    initializeUI();

    // Create sub-controllers BEFORE setRuntime — setRuntime
    // triggers changeRegion which calls
    // sceneVisibility()->updateSceneVisibility(). Controllers
    // must exist before that call.
    m_fleetCtrl = new FleetController(this, this);
    m_sceneVisibility = new SceneVisibilityController(
        regionScene_, globalMapScene_, this, this);
    m_terminalCtrl = new TerminalController(
        regionScene_, regionView_, globalMapScene_,
        globalMapView_, this, this, this);
    m_networkDrawing = new NetworkDrawingController(
        regionScene_, regionView_, m_terminalCtrl,
        this, this, this);
    m_connectionCtrl = new ConnectionController(
        regionScene_, globalMapScene_, this, this, this);
    m_regionCtrl = new RegionController(
        regionScene_, m_sceneVisibility, this, this, this);

    qCDebug(lcGui)
        << "MainWindow::MainWindow: creating initial"
           " empty scenario";
    // Create an empty scenario with a default region.
    // This ensures runtime() is NEVER null during normal
    // operation — all terminal creation goes through the
    // modern ScenarioMutator path with ScenarioDocument
    // binding.
    {
        auto doc = std::make_unique<
            Backend::Scenario::ScenarioDocument>();
        Backend::Scenario::RegionSpec defaultRegion;
        defaultRegion.name =
            QStringLiteral("Default Region");
        defaultRegion.color = QStringLiteral("#00FF00");
        defaultRegion.localOrigin    = {0.0, 0.0};
        defaultRegion.globalPosition = {0.0, 0.0};
        doc->addRegion(defaultRegion);
        auto runtime = std::make_unique<
            Backend::Scenario::ScenarioRuntime>(
            std::move(doc));
        if (runtime->load())
            setRuntime(std::move(runtime));
        else
            qCCritical(lcGui)
                << "MainWindow: failed to initialize"
                   " empty scenario at startup";
    }

    // Construct the two InteractionControllers now that scenes,
    // views and the initial runtime all exist. Each scene/view
    // pair is bound to its own controller; both share the single
    // CommandBus created above.
    {
        auto* initialDoc = (m_runtime ? &m_runtime->document() : nullptr);

        m_regionInputController = new Input::InteractionController(
            /* mainWindow */ this,
            /* scene      */ regionScene_,
            /* view       */ regionView_,
            /* document   */ initialDoc,
            /* commandBus */ m_commandBus,
            /* parent     */ this);

        m_globalInputController = new Input::InteractionController(
            /* mainWindow */ this,
            /* scene      */ globalMapScene_,
            /* view       */ globalMapView_,
            /* document   */ initialDoc,
            /* commandBus */ m_commandBus,
            /* parent     */ this);

        regionScene_   ->setInputController(m_regionInputController);
        regionView_    ->setInputController(m_regionInputController);
        globalMapScene_->setInputController(m_globalInputController);
        globalMapView_ ->setInputController(m_globalInputController);
    }

    connect(m_networkDrawing,
            &NetworkDrawingController::coordinatesNeedUpdate,
            this, &MainWindow::updateAllCoordinates);
    connect(m_connectionCtrl,
            &ConnectionController::connectionRemoved,
            propertiesPanel_,
            &PropertiesPanel::clearIfShowing);
    connect(m_connectionCtrl,
            &ConnectionController::terminalUnlinked,
            propertiesPanel_,
            &PropertiesPanel::clearIfShowing);

    // Plan 3 — PropertiesPanel reacts to selection changes on either
    // scene instead of per-item `clicked` signals. The panel holds
    // QPointers to both scenes plus the "active" one (tab-driven).
    propertiesPanel_->setScenes(regionScene_, globalMapScene_);
    connect(regionScene_, &QGraphicsScene::selectionChanged,
            propertiesPanel_, &PropertiesPanel::refreshFromSelection);
    connect(globalMapScene_, &QGraphicsScene::selectionChanged,
            propertiesPanel_, &PropertiesPanel::refreshFromSelection);
    connect(tabWidget_, &QTabWidget::currentChanged, propertiesPanel_,
            [this](int index) {
                // Index 0 = region tab, 1 = global map tab (matches
                // getCurrentScene(); currentWidget() returns the tab
                // container, not the view, so index comparison is used).
                const bool onGlobalTab = (index == 1);
                GraphicsScene *active =
                    onGlobalTab ? globalMapScene_ : regionScene_;
                propertiesPanel_->setActiveScene(active);
                // Properties dock is only meaningful on the region tab.
                if (propertiesDock_) {
                    propertiesDock_->setVisible(!onGlobalTab);
                }
            });
    propertiesPanel_->setActiveScene(
        tabWidget_->currentIndex() == 1 ? globalMapScene_
                                        : regionScene_);

    qCDebug(lcGui)
        << "MainWindow::MainWindow: initializing"
           " heartbeat controller";
    heartbeatController_ = new HeartbeatController(this);
    heartbeatController_->initialize();

    setWindowTitle("CargoNetSim: Multimodal Freight "
                   "Operations Optimizer");
    resize(1000, 700);

    showStatusBarMessage("Ready.");

    BasicButtonController::updateRegionComboBox(this);
    BasicButtonController::setupSignals(this);
}

void MainWindow::setRuntime(
    std::unique_ptr<Backend::Scenario::ScenarioRuntime> rt)
{
    qCInfo(lcGui) << "MainWindow::setRuntime: begin";
    // Clear BOTH scenes before swapping — scene items hold non-owning
    // pointers into the outgoing runtime's ScenarioDocument. Destroying
    // the old runtime while those pointers are still live would leave
    // the items dangling. clearAll() drops the type-registry first
    // (see Task 8's GraphicsScene::clearAll), then deletes items.
    if (regionScene_)    regionScene_->clearAll();
    if (globalMapScene_) globalMapScene_->clearAll();

    m_runtime = std::move(rt);

    auto *raw = m_runtime.get();
    if (m_terminalCtrl)   m_terminalCtrl->setRuntime(raw);
    if (m_connectionCtrl) m_connectionCtrl->setRuntime(raw);

    // Propagate the new ScenarioDocument to both input controllers
    // and clear the (shared) undo stack exactly once.
    {
        auto *newDoc = (m_runtime ? &m_runtime->document() : nullptr);
        if (m_regionInputController) m_regionInputController->setDocument(newDoc);
        if (m_globalInputController) m_globalInputController->setDocument(newDoc);
        if (m_commandBus) {
            m_commandBus->undoStack()->clear();
            qCInfo(lcGuiInput) << "MainWindow::setRuntime: cleared shared undo stack";
        }
    }

    subscribeDocumentObservers();

    // Initial backfill: subscribe happens AFTER the document was parsed,
    // so the per-item signals (regionAdded, terminalAdded, …) have
    // already fired with no listener. Do one full rebuild here so the
    // scene reflects the loaded document. No-op when rt is null
    // (tear-down path). Callers are expected to have completed
    // runtime->load() before calling setRuntime so the backend
    // (RegionDataController) is ready for MapPoint rebuild.
    if (m_runtime)
    {
        auto *doc = &m_runtime->document();
        GUI::Scenario::SceneRepopulator::repopulate(doc, this);

        m_settingsCtrl->loadFromDocument();

        qCDebug(lcGui) << "MainWindow::setRuntime:"
                       << "regions=" << doc->regions.size()
                       << "terminals=" << doc->terminals.size()
                       << "connections=" << doc->connections.size();

        // Auto-select the first loaded region so updateSceneVisibility
        // hides non-current regions. Without this, every region's
        // RegionCenterPoint and TerminalItems would stack visibly on
        // the same regionScene (all regions share one scene; visibility
        // is filtered by the current-region selector).
        if (!doc->regions.isEmpty())
        {
            BasicButtonController::changeRegion(
                this, doc->regions.firstKey());
        }
    }
}

void MainWindow::subscribeDocumentObservers()
{
    qCDebug(lcGui) << "MainWindow::subscribeDocumentObservers: wiring" << 19 << "observers";
    using namespace Backend::Scenario;
    if (!m_runtime) return;
    auto *doc = &m_runtime->document();
    auto *regionScene = regionScene_;

    // Full rebuild on documentReset — same entry point used by
    // File → Open Scenario (Task 19 when that lands).
    connect(doc, &ScenarioDocument::documentReset,
            this, [this, doc] {
                GUI::Scenario::SceneRepopulator::repopulate(doc, this);
            });

    // Region lifecycle → RegionCenterPoint factory / removal.
    connect(doc, &ScenarioDocument::regionAdded, this,
            [this, doc, regionScene](const QString &name) {
                auto it = doc->regions.find(name);
                if (it != doc->regions.end())
                    GUI::Scenario::RegionCenterPointFactory::fromRegionSpec(
                        &it.value(), regionScene, this);
            });
    connect(doc, &ScenarioDocument::regionRemoved, this,
            [regionScene](const QString &name) {
                if (regionScene)
                    regionScene->removeItemWithId<RegionCenterPoint>(name);
            });

    // Terminal lifecycle → TerminalItem factory / removal.
    connect(doc, &ScenarioDocument::terminalAdded, this,
            [this, doc, regionScene](const QString &id) {
                auto it = doc->terminals.find(id);
                if (it == doc->terminals.end()) return;
                auto *item =
                    GUI::Scenario::TerminalItemFactory::fromPlacement(
                        &it.value(), regionScene, this);
                if (item && m_terminalCtrl)
                    m_terminalCtrl->updateGlobalMapItem(item);
            });
    connect(doc, &ScenarioDocument::terminalRemoved, this,
            [regionScene](const QString &id) {
                if (regionScene)
                    regionScene->removeItemWithId<TerminalItem>(id);
            });

    // Terminal data changed -> refresh the TerminalItem binding
    // so role badge repaints and PropertiesPanel re-displays.
    connect(doc, &ScenarioDocument::terminalChanged, this,
            [this, doc, regionScene](const QString &id) {
                qCDebug(lcGui)
                    << "MainWindow: terminalChanged:" << id;
                auto it = doc->terminals.find(id);
                if (it == doc->terminals.end()) return;
                auto *item =
                    regionScene
                        ? regionScene->getItemById<TerminalItem>(id)
                        : nullptr;
                if (item)
                    item->setPlacement(&it.value());
                if (item && m_terminalCtrl)
                    m_terminalCtrl->updateGlobalMapItem(item);
                // Refresh PropertiesPanel if it's showing this
                // terminal. Deferred to avoid re-entrancy: the
                // role combobox signal is still on the call stack
                // when terminalChanged fires; displayProperties →
                // clearLayout would delete the sender mid-signal.
                qCDebug(lcGui)
                    << "MainWindow: terminalChanged panel check:"
                    << "item=" << (void *)item
                    << "currentItem=" << (void *)propertiesPanel_->getCurrentItem()
                    << "match=" << (item && propertiesPanel_->getCurrentItem() == item);
                if (item
                    && propertiesPanel_->getCurrentItem()
                           == item)
                {
                    QPointer<TerminalItem> safeItem = item;
                    QTimer::singleShot(0, this, [this, safeItem]() {
                        if (safeItem)
                            propertiesPanel_->displayProperties(
                                safeItem);
                    });
                }
            });

    // Connection property changed → refresh ConnectionLine
    connect(doc, &ScenarioDocument::connectionChanged, this,
        [this, regionScene](
            const QString &fromId, const QString &toId,
            Backend::TransportationTypes::TransportationMode
                mode) {
            qCDebug(lcGui)
                << "MainWindow: connectionChanged:"
                << fromId << "->" << toId;
            for (auto *line :
                 regionScene->getItemsByType<
                     ConnectionLine>())
            {
                if (line->matchesConnection(
                        fromId, toId, mode))
                {
                    line->refreshFromModel();
                    break;
                }
            }
            if (globalMapScene_)
            {
                for (auto *line :
                     globalMapScene_->getItemsByType<
                         ConnectionLine>())
                {
                    if (line->matchesConnection(
                            fromId, toId, mode))
                    {
                        line->refreshFromModel();
                        break;
                    }
                }
            }
        });

    // GlobalLink property changed → refresh ConnectionLine
    connect(doc, &ScenarioDocument::globalLinkChanged, this,
        [this, regionScene](
            const QString &fromId, const QString &toId,
            Backend::TransportationTypes::TransportationMode
                mode) {
            qCDebug(lcGui)
                << "MainWindow: globalLinkChanged:"
                << fromId << "->" << toId;
            for (auto *line :
                 regionScene->getItemsByType<
                     ConnectionLine>())
            {
                if (line->matchesConnection(
                        fromId, toId, mode))
                {
                    line->refreshFromModel();
                    break;
                }
            }
            if (globalMapScene_)
            {
                for (auto *line :
                     globalMapScene_->getItemsByType<
                         ConnectionLine>())
                {
                    if (line->matchesConnection(
                            fromId, toId, mode))
                    {
                        line->refreshFromModel();
                        break;
                    }
                }
            }
        });

    // Region data changed → refresh RegionCenterPoint
    connect(doc, &ScenarioDocument::regionChanged, this,
        [this, doc, regionScene](const QString &name) {
            qCDebug(lcGui)
                << "MainWindow: regionChanged:" << name;
            auto *center =
                regionScene
                    ->getItemById<RegionCenterPoint>(name);
            if (center)
            {
                auto it = doc->regions.find(name);
                if (it != doc->regions.end())
                    center->refreshFromSpec(&it.value());
            }
        });

    // Linkage data changed → refresh MapPoint binding
    connect(doc, &ScenarioDocument::linkageChanged, this,
        [this, doc, regionScene](
            const QString &terminalId,
            const QString &networkName, int nodeId) {
            qCDebug(lcGui)
                << "MainWindow: linkageChanged:"
                << terminalId << networkName << nodeId;
            auto *mp = GUI::Scenario::MapPointFactory::
                findByNetworkAndNode(
                    regionScene, networkName, nodeId);
            if (mp)
            {
                for (auto &l : doc->linkages)
                {
                    if (l.terminalId == terminalId
                        && l.networkName == networkName
                        && l.nodeId == nodeId)
                    {
                        mp->setLinkageModel(&l);
                        break;
                    }
                }
            }
        });

    // Linkage lifecycle → MapPoint factory / removal. The owning region
    // is the one whose RegionData::findNetworkByName (Task 5.5) answers
    // non-null for the linkage's networkName — one dispatch helper does
    // the "rail or truck" disambiguation on our behalf.
    connect(doc, &ScenarioDocument::linkageAdded, this,
            [this, regionScene](const NodeLinkage &link) {
                // Find-or-create. When a network has already been
                // imported (legacy or factory path), its MapPoints live
                // in the scene without a linkageModel. We bind them to
                // the fresh NodeLinkage record here. If no MapPoint
                // exists yet, fall through to the factory.
                auto *mp = GUI::Scenario::MapPointFactory::findByNetworkAndNode(
                    regionScene, link.networkName, link.nodeId);
                // Look up the stored linkage pointer (signal delivers a
                // copy; factory / model binding want a doc-owned ptr).
                Backend::Scenario::NodeLinkage *stored = nullptr;
                for (auto &s : m_runtime->document().linkages)
                {
                    if (s.terminalId  == link.terminalId
                     && s.networkName == link.networkName
                     && s.nodeId      == link.nodeId)
                    {
                        stored = &s;
                        break;
                    }
                }
                if (!mp)
                {
                    auto *rdc = CargoNetSim::CargoNetSimController
                                    ::getInstance().getRegionDataController();
                    if (!rdc || !regionScene || !stored) return;
                    // Walk regions to find the one hosting this network
                    // so the factory can project the node's coordinates.
                    for (auto it = m_runtime->document().regions.begin();
                         it != m_runtime->document().regions.end(); ++it)
                    {
                        auto *rd = rdc->getRegionData(it.key());
                        if (!rd) continue;
                        if (!rd->findNetworkByName(link.networkName)) continue;
                        mp = GUI::Scenario::MapPointFactory::fromNodeLinkage(
                            stored, rd, regionScene, this);
                        break;
                    }
                    return;
                }
                // Existing MapPoint → bind model + visual terminal.
                if (stored) mp->setLinkageModel(stored);
                if (auto *term = regionScene
                                    ->getItemById<TerminalItem>(
                                        link.terminalId))
                    mp->setLinkedTerminal(term);
            });
    connect(doc, &ScenarioDocument::linkageRemoved, this,
            [regionScene](const QString &, const QString &networkName,
                          int nodeId) {
                // Unbind, don't delete. The MapPoint may outlive the
                // linkage: legacy network-import paths create a
                // MapPoint per node (linked or not). Removing the
                // linkage returns the point to unlinked state; it
                // stays visible until the network itself is removed.
                auto *mp = GUI::Scenario::MapPointFactory::findByNetworkAndNode(
                    regionScene, networkName, nodeId);
                if (!mp) return;
                mp->setLinkageModel(nullptr);
                mp->setLinkedTerminal(nullptr);
            });

    // Connection lifecycle (region-local). The signal passes the
    // connection by value; we look up the stored mutable pointer in the
    // document (factory wants a non-owning pointer into doc->connections
    // so ConnectionLine::connectionModel() can track it).
    connect(doc, &ScenarioDocument::connectionAdded, this,
            [this, regionScene](const Connection &c) {
                if (!regionScene) return;
                for (auto &stored : m_runtime->document().connections)
                {
                    if (stored.fromTerminalId == c.fromTerminalId
                     && stored.toTerminalId   == c.toTerminalId
                     && stored.mode           == c.mode)
                    {
                        GUI::Scenario::ConnectionLineFactory::fromConnection(
                            &stored, regionScene, this);
                        return;
                    }
                }
            });
    connect(doc, &ScenarioDocument::connectionRemoved, this,
            [regionScene](const QString &from, const QString &to,
                          Backend::TransportationTypes::TransportationMode mode) {
                auto *line =
                    GUI::Scenario::ConnectionLineFactory::findRegionConnection(
                        regionScene, from, to, mode);
                if (line && regionScene)
                    regionScene->removeItemWithId<ConnectionLine>(
                        line->sceneRegistryKey());
            });

    // GlobalLink lifecycle (cross-region, rendered in globalMapScene).
    connect(doc, &ScenarioDocument::globalLinkAdded, this,
            [this](const GlobalLink &g) {
                if (!globalMapScene_) return;
                for (auto &stored : m_runtime->document().globalLinks)
                {
                    if (stored.fromTerminalId == g.fromTerminalId
                     && stored.toTerminalId   == g.toTerminalId
                     && stored.mode           == g.mode)
                    {
                        GUI::Scenario::ConnectionLineFactory::fromGlobalLink(
                            &stored, globalMapScene_, this);
                        return;
                    }
                }
            });
    connect(doc, &ScenarioDocument::globalLinkRemoved, this,
            [this](const QString &from, const QString &to,
                   Backend::TransportationTypes::TransportationMode mode) {
                auto *line =
                    GUI::Scenario::ConnectionLineFactory::findGlobalLink(
                        globalMapScene_, from, to, mode);
                if (line && globalMapScene_)
                    globalMapScene_->removeItemWithId<ConnectionLine>(
                        line->sceneRegistryKey());
            });

    // Network lifecycle → refresh NetworkManagerDialog lists.
    connect(doc, &ScenarioDocument::networkAdded, this,
            [this](const QString &region, const QString &) {
                if (networkManagerDock_)
                    networkManagerDock_->updateNetworkListForChangedRegion(
                        region);
            });
    connect(doc, &ScenarioDocument::networkRemoved, this,
            [this](const QString &region, const QString &) {
                if (networkManagerDock_)
                    networkManagerDock_->updateNetworkListForChangedRegion(
                        region);
            });
    connect(doc, &ScenarioDocument::networkRenamed, this,
            [this](const QString &region, const QString &,
                   const QString &) {
                if (networkManagerDock_)
                    networkManagerDock_->updateNetworkListForChangedRegion(
                        region);
            });
}

MainWindow::~MainWindow()
{
    // Symmetric thread-affinity check with the constructor.
    if (QCoreApplication::instance()
        && QThread::currentThread()
               != QCoreApplication::instance()->thread())
    {
        qFatal("MainWindow: must be destroyed on the main "
               "(QCoreApplication) thread.");
    }

    // Cleanup resources
    delete heartbeatController_;

    // Clear log timers
    if (logTimer_)
    {
        logTimer_->stop();
        delete logTimer_;
    }

    if (progressTimer_)
    {
        progressTimer_->stop();
        delete progressTimer_;
    }

    // Scene items will be cleaned up by Qt's parent-child
    // mechanism

    // Tier 1 lifetime: clear the singleton slot LAST so any late
    // observer using instance() sees the window as gone. Release
    // store pairs with acquire loads in instance() / getInstance().
    s_instance.store(nullptr, std::memory_order_release);
}

MainWindow &MainWindow::getInstance()
{
    MainWindow *p = s_instance.load(std::memory_order_acquire);
    if (p == nullptr)
    {
        qFatal("MainWindow::getInstance: MainWindow has not been "
               "constructed yet. Construct it in main() before "
               "calling getInstance(). Use instance() for nullable "
               "access during startup or shutdown windows.");
    }
    return *p;
}

MainWindow *MainWindow::instance()
{
    return s_instance.load(std::memory_order_acquire);
}

void MainWindow::initializeUI()
{
    qCDebug(lcGui) << "MainWindow::initializeUI: begin";
    // Load the window icon
    QString imagePath      = ":/Logo25";
    auto    originalPixmap = QPixmap(imagePath);

    if (originalPixmap.isNull())
    {
        qCWarning(lcGui) << "Failed to load logo image:"
                         << imagePath;
        // Create a default splash pixmap
        originalPixmap = QPixmap(25, 25);
        originalPixmap.fill(Qt::white);

        QPainter painter(&originalPixmap);
        painter.setPen(Qt::black);
        painter.setFont(QFont("Arial", 5, QFont::Bold));
        painter.drawText(originalPixmap.rect(),
                         Qt::AlignCenter, "CNS");
    }

    QIcon appIcon(originalPixmap);
    if (!appIcon.isNull())
    {
        setWindowIcon(appIcon);
    }

    qCDebug(lcGui) << "MainWindow::initializeUI: scene creation";
    // Create tab widget for main view and global map
    tabWidget_ = new QTabWidget(this);
    tabWidget_->setTabsClosable(false);

    // Create main view tab
    QWidget     *mainViewTab = new QWidget();
    QVBoxLayout *mainViewLayout =
        new QVBoxLayout(mainViewTab);
    mainViewLayout->setContentsMargins(0, 0, 0, 0);

    // Setup scene and view
    setupRegionMapScene();
    mainViewLayout->addWidget(regionView_);

    // Create global map tab
    QWidget     *globalMapTab = new QWidget();
    QVBoxLayout *globalMapLayout =
        new QVBoxLayout(globalMapTab);
    globalMapLayout->setContentsMargins(0, 0, 0, 0);

    // Setup global map scene
    setupGlobalMapScene();
    globalMapLayout->addWidget(globalMapView_);

    // Create logging tab
    loggingTab_ = new QWidget();
    setupLoggingTab();

    // Add tabs to tab widget
    tabWidget_->addTab(mainViewTab, "Region View");
    tabWidget_->addTab(globalMapTab, "Global Map");
    tabWidget_->addTab(loggingTab_, "Servers Log");

    // Set tab widget as central widget
    centerSplitter->addWidget(tabWidget_);

    // Connect tab change signal
    connect(tabWidget_, &QTabWidget::currentChanged, this,
            &MainWindow::handleTabChange);

    // Setup connection menu
    connectionMenu_ = new QMenu(this);
    using Mode = Backend::TransportationTypes::TransportationMode;
    connectionTypes_ << Mode::Truck << Mode::Train << Mode::Ship;
    currentConnectionType_ = Mode::Truck;

    // Add connection types to menu. Use the display-friendly
    // capitalized form from `TransportationTypes::toString` for the
    // action label (users see "Truck" / "Train" / "Ship"), and keep
    // the enum value in the triggered lambda.
    for (Mode connType : connectionTypes_)
    {
        QAction *action = connectionMenu_->addAction(
            Backend::TransportationTypes::toString(connType));
        action->setCheckable(true);
        connect(action, &QAction::triggered,
                [this, connType](bool) {
                    setConnectionType(connType);
                });
    }

    // Set initial checked state
    connectionMenu_->actions().at(0)->setChecked(true);

    // Setup docks
    setupDocks();

    qCDebug(lcGui) << "MainWindow::initializeUI: toolbar setup";
    // Setup toolbar
    ToolbarController::setupToolbar(this);

    qCDebug(lcGui) << "MainWindow::initializeUI: status bar setup";
    // Setup status bar
    setupStatusBar();

    // Start queue processing
    startQueueProcessing();
}

void MainWindow::setupRegionMapScene()
{
    qCDebug(lcGui) << "MainWindow::setupRegionMapScene: begin";
    // Setup scene and view
    regionScene_ = new GraphicsScene(this);
    regionView_  = new GraphicsView(regionScene_);
    regionView_->setScene(regionScene_);
}

void MainWindow::setupGlobalMapScene()
{
    qCDebug(lcGui) << "MainWindow::setupGlobalMapScene: begin";
    globalMapScene_ = new GraphicsScene(this);
    globalMapView_  = new GraphicsView(globalMapScene_);

    // Force geodetic coordinates for global map
    globalMapView_->setUsingProjectedCoords(false);
    globalMapView_->setScene(globalMapScene_);
}

void MainWindow::setupDocks()
{
    qCDebug(lcGui) << "MainWindow::setupDocks: begin";
    // Properties panel dock
    propertiesDock_  = new QDockWidget("Properties", this);
    propertiesPanel_ = new PropertiesPanel(this);
    propertiesDock_->setWidget(propertiesPanel_);
    addDockWidget(Qt::RightDockWidgetArea, propertiesDock_);
    propertiesDock_->show();

    // Settings dock
    settingsDock_ =
        new QDockWidget("Simulation Settings", this);
    settingsWidget_ = new SettingsWidget(this);
    settingsDock_->setWidget(settingsWidget_);
    addDockWidget(Qt::RightDockWidgetArea, settingsDock_);

    // Tabify properties and settings docks
    tabifyDockWidget(propertiesDock_, settingsDock_);

    // Make settings visible by default instead of hiding
    // properties
    settingsDock_->raise();

    // Shortest paths table dock
    shortestPathTableDock_ =
        new QDockWidget("Shortest Paths Table", this);
    shortestPathTable_ = new ShortestPathsTable(this);
    shortestPathTableDock_->setWidget(shortestPathTable_);
    centerSplitter->addWidget(shortestPathTableDock_);
    shortestPathTableDock_
        ->hide(); // Start with shortest paths table hidden

    connect(m_settingsCtrl,  &SettingsController::configChanged,
            settingsWidget_, &SettingsWidget::refreshFromConfig);

    // Terminal library dock
    setupTerminalLibrary();

    // Region manager dock
    setupRegionManager();

    // Network manager dock
    networkManagerDock_ = new NetworkManagerDialog(this);

    // Tabify the region manager and network manager docks
    tabifyDockWidget(regionManagerDock_,
                     networkManagerDock_);

    // Ensure region manager is visible by default
    regionManagerDock_->raise();
}

void MainWindow::setupTerminalLibrary()
{
    libraryDock_ =
        new QDockWidget("Terminal Library", this);
    libraryList_ = new QListWidget();
    libraryList_->setIconSize(QSize(32, 32));
    libraryList_->setDragEnabled(true);

    // Create terminal icons
    QMap<QString, QPixmap> terminalIcons =
        IconFactory::createTerminalIcons();

    // Add items with custom icons
    for (auto it = terminalIcons.constBegin();
         it != terminalIcons.constEnd(); ++it)
    {
        QListWidgetItem *item = new QListWidgetItem(
            QIcon(it.value()), it.key());
        item->setData(
            Qt::UserRole,
            it.value()); // Store pixmap for later use
        libraryList_->addItem(item);
    }

    libraryDock_->setWidget(libraryList_);
    addDockWidget(Qt::LeftDockWidgetArea, libraryDock_);
}

void MainWindow::setupRegionManager()
{
    regionManagerDock_ =
        new QDockWidget("Region Manager", this);
    regionManager_ = new RegionManagerWidget(this);
    regionManagerDock_->setWidget(regionManager_);
    addDockWidget(Qt::LeftDockWidgetArea,
                  regionManagerDock_);
    regionManagerDock_->resize(regionManagerDock_->width(),
                               200);
}

void MainWindow::setupLoggingTab()
{
    qCDebug(lcGui) << "MainWindow::setupLoggingTab: begin";
    // Create main layout for logging tab
    QGridLayout *layout = new QGridLayout(loggingTab_);

    // Client names
    clientNames_ << "ShipClient" << "TrainClient"
                 << "TruckClient"
                 << "TerminalClient" << "CargoNetSim";

    // Create 2x2 grid of logging widgets for first 4
    // clients
    for (int i = 0; i < 2; ++i)
    {
        for (int j = 0; j < 2; ++j)
        {
            int clientIndex = i * 2 + j;
            if (clientIndex >= clientNames_.size() - 1)
            {
                continue;
            }

            QString clientName = clientNames_[clientIndex];

            // Create group box for each client
            QGroupBox   *group = new QGroupBox(clientName);
            QVBoxLayout *groupLayout =
                new QVBoxLayout(group);

            // Progress bar and label
            QHBoxLayout *progressLayout = new QHBoxLayout();
            QProgressBar *progressBar = new QProgressBar();
            progressBar->setMaximum(100);
            progressBar->setValue(0);
            progressLayout->addWidget(progressBar);

            groupLayout->addLayout(progressLayout);

            // Text widget and scrollbar
            QTextEdit *textWidget = new QTextEdit();
            textWidget->setReadOnly(true);
            groupLayout->addWidget(textWidget);

            // Store references
            logTextWidgets_.append(textWidget);
            progressBars_.append(progressBar);

            // Add to main layout
            layout->addWidget(group, i, j);
        }
    }

    // Create general log section (CargoNetSim)
    QGroupBox *generalGroup =
        new QGroupBox(clientNames_.last());
    QVBoxLayout *generalLayout =
        new QVBoxLayout(generalGroup);

    // Progress bar and label for general log
    QHBoxLayout  *generalProgressLayout = new QHBoxLayout();
    QProgressBar *generalProgressBar = new QProgressBar();
    generalProgressBar->setMaximum(100);
    generalProgressLayout->addWidget(generalProgressBar);

    generalLayout->addLayout(generalProgressLayout);

    // Text widget for general log
    QTextEdit *generalText = new QTextEdit();
    generalText->setReadOnly(true);
    generalLayout->addWidget(generalText);

    // Store references
    logTextWidgets_.append(generalText);
    progressBars_.append(generalProgressBar);

    // Add general section to main layout
    layout->addWidget(generalGroup, 2, 0, 1, 2);
}

void MainWindow::startQueueProcessing()
{
    // Direct signal connections replace the old timer-based
    // polling. ApplicationLogger emits signals from the main
    // thread after processing its event queue.
    connect(ApplicationLogger::getInstance(),
            &ApplicationLogger::newLogMessage, this,
            &MainWindow::appendLog);

    connect(
        ApplicationLogger::getInstance(),
        &ApplicationLogger::progressUpdated, this,
        [this](int progress, int clientIndex) {
            if (clientIndex >= 0
                && clientIndex < progressBars_.size())
                progressBars_[clientIndex]->setValue(
                    progress);
        });
}

void MainWindow::processLogQueue() {}

void MainWindow::processProgressQueue() {}

void MainWindow::appendLog(const QString &message,
                           int widgetIndex, bool isError)
{
    if (widgetIndex >= logTextWidgets_.size())
    {
        return;
    }

    QTextEdit  *textWidget = logTextWidgets_[widgetIndex];
    QTextCursor cursor     = textWidget->textCursor();
    cursor.movePosition(QTextCursor::End);
    textWidget->setTextCursor(cursor);

    // Create format for error messages
    if (isError)
    {
        QTextCharFormat format;
        format.setForeground(QColor("red"));
        cursor.setCharFormat(format);
    }

    cursor.insertText(message + "\n");
    textWidget->verticalScrollBar()->setValue(
        textWidget->verticalScrollBar()->maximum());
}

void MainWindow::setupStatusBar()
{
    // First, get the existing status bar
    QStatusBar *statusBar = this->statusBar();

    // Create a custom widget to fill the entire status bar
    QWidget     *mainContainer = new QWidget();
    QHBoxLayout *mainLayout =
        new QHBoxLayout(mainContainer);
    mainLayout->setContentsMargins(4, 0, 4, 0);
    mainLayout->setSpacing(6);

    // 1. LEFT SECTION - Status messages and spinner
    QWidget     *leftContainer = new QWidget();
    QHBoxLayout *leftLayout =
        new QHBoxLayout(leftContainer);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(6);

    // Add the spinner BEFORE the status label
    // Spinner widget
    statusSpinner_ = new SpinnerWidget();
    statusSpinner_->setFixedSize(16, 16);
    // Get the application palette's text color
    QColor textColor = palette().color(QPalette::Text);
    // Use that color for the spinner
    statusSpinner_->setSpinnerColor(textColor);
    statusSpinner_->setVisibleWhenIdle(false);
    leftLayout->addWidget(statusSpinner_);

    // Status label - add AFTER the spinner
    statusLabel_ = new QLabel("Ready.");
    statusLabel_->setMinimumWidth(300);
    statusLabel_->setMaximumWidth(400);
    leftLayout->addWidget(statusLabel_);

    mainLayout->addWidget(leftContainer);

    // 2. CENTER SECTION - Server indicators
    // Use a separate layout to ensure proper centering
    QWidget     *centerContainer = new QWidget();
    QHBoxLayout *centerLayout =
        new QHBoxLayout(centerContainer);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(8);
    centerLayout->setAlignment(Qt::AlignCenter);

    // Server indicators label
    QLabel *serverLabel = new QLabel("Servers:");
    centerLayout->addWidget(serverLabel);

    // Define servers with their display names
    QMap<QString, QMap<QString, QString>> serverInfo;
    serverInfo["NeTrainSim"] = {
        {"shortDesc", "Trains Server"},
        {"longDesc", "Open Source Large-Scale Network "
                     "Train Simulator"}};
    serverInfo["ShipNetSim"] = {
        {"shortDesc", "Ships Server"},
        {"longDesc", "Open Source Large-Scale Maritime "
                     "Transport Simulator"}};
    serverInfo["INTEGRATION"] = {
        {"shortDesc", "Trucks Server"},
        {"longDesc",
         "Free-ware Large-Scale Traffic Simulator"}};
    serverInfo["TerminalSim"] = {
        {"shortDesc", "Terminal Graph Server"},
        {"longDesc",
         "Intermodal Terminal Management System"}};

    // Add indicators for each server
    QMap<QString, QMap<QString, QVariant>> serverIndicators;

    for (auto it = serverInfo.constBegin();
         it != serverInfo.constEnd(); ++it)
    {
        QString serverId  = it.key();
        QString shortDesc = it.value()["shortDesc"];
        QString longDesc  = it.value()["longDesc"];

        // Create indicator container
        QWidget     *container = new QWidget();
        QHBoxLayout *layout    = new QHBoxLayout(container);
        layout->setContentsMargins(2, 0, 2, 0);
        layout->setSpacing(4);

        // Status indicator dot
        QLabel *indicator = new QLabel();
        indicator->setFixedSize(10, 10);
        indicator->setStyleSheet(
            "background-color: #808080; border-radius: "
            "5px;");
        indicator->setToolTip(shortDesc
                              + " - Disconnected");

        // Server label
        QLabel *label = new QLabel(serverId);
        label->setToolTip(longDesc);

        // Add widgets to layout
        layout->addWidget(indicator);
        layout->addWidget(label);

        // Add to center layout
        centerLayout->addWidget(container);

        // Store reference
        QMap<QString, QVariant> indicatorData;
        indicatorData["indicator"] =
            QVariant::fromValue<QLabel *>(indicator);
        indicatorData["label"] =
            QVariant::fromValue<QLabel *>(label);
        indicatorData["description"]          = shortDesc;
        indicatorData["detailed_description"] = longDesc;
        indicatorData["last_heartbeat"]       = 0;
        serverIndicators[serverId] = indicatorData;
    }

    // Add the center container with a stretch on both sides
    // to keep it centered
    mainLayout->addStretch(1);
    mainLayout->addWidget(centerContainer);
    mainLayout->addStretch(1);

    // 3. RIGHT SECTION - Backend message (fixed width)
    QWidget     *rightContainer = new QWidget();
    QHBoxLayout *rightLayout =
        new QHBoxLayout(rightContainer);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(4);

    // Backend message icon
    backendIcon_ = new QLabel();
    backendIcon_->setFixedSize(10, 10);
    backendIcon_->setStyleSheet(
        "background-color: #0066cc; border-radius: 5px;");
    backendIcon_->setVisible(false); // Hide initially
    rightLayout->addWidget(backendIcon_);

    // Backend message label
    backendReportLabel_ = new QLabel("");
    backendReportLabel_->setMinimumWidth(300);
    backendReportLabel_->setMaximumWidth(
        400); // Limit width
    rightLayout->addWidget(backendReportLabel_);

    // Add the right container
    mainLayout->addWidget(rightContainer);

    // Add our custom widget to take over the entire status
    // bar
    statusBar->addWidget(
        mainContainer,
        1); // Stretch factor 1 makes it fill the bar

    // Initialize message queue
    isProcessingMessageQueue_ = false;

    QTimer *messageQueueTimer_ = new QTimer(this);
    connect(messageQueueTimer_, &QTimer::timeout, this,
            &MainWindow::processMessageQueue);
    messageQueueTimer_->start(
        100); // Check queue every 100ms
}

void MainWindow::setConnectionType(
    Backend::TransportationTypes::TransportationMode connectionType)
{
    currentConnectionType_ = connectionType;

    // Uncheck all other actions — actions were labelled with the
    // display string via TransportationTypes::toString, so we match
    // on that.
    const QString currentLabel =
        Backend::TransportationTypes::toString(connectionType);
    for (QAction *action : connectionMenu_->actions())
        action->setChecked(action->text() == currentLabel);

    // Live-update ConnectMode if currently active.
    if (auto* ctrl = inputController()) {
        if (auto* cm = dynamic_cast<Input::ConnectMode*>(ctrl->currentMode())) {
            cm->setConnectionType(connectionType);
        }
    }

    showStatusBarMessage(
        QString("Connection type set to: %1").arg(currentLabel),
        2000);
}

GraphicsView *MainWindow::getCurrentView() const
{
    if (tabWidget_->currentIndex() == 0)
    { // Main view tab
        return regionView_;
    }
    else if (tabWidget_->currentIndex() == 1)
    { // Global map tab
        return globalMapView_;
    }
    return nullptr;
}

GraphicsScene *MainWindow::getCurrentScene() const
{
    if (tabWidget_->currentIndex() == 0)
    { // Main view tab
        return regionScene_;
    }
    else if (tabWidget_->currentIndex() == 1)
    { // Global map tab
        return globalMapScene_;
    }
    return nullptr;
}

bool MainWindow::isGlobalViewActive() const
{
    return tabWidget_->currentIndex() == 1;
}

bool MainWindow::isRegionViewActive() const
{
    return tabWidget_->currentIndex() == 0;
}

void MainWindow::handleTabChange(int index)
{
    bool isGlobalMap =
        index == tabWidget_->indexOf(tabWidget_->widget(1));
    bool isLoggingTab =
        index == tabWidget_->indexOf(tabWidget_->widget(2));
    bool isMainView = !(isGlobalMap || isLoggingTab);

    // Common state reset — drop both controllers back to NormalMode.
    if (m_regionInputController)
        m_regionInputController->setMode<Input::NormalMode>();
    if (m_globalInputController)
        m_globalInputController->setMode<Input::NormalMode>();

    measureButton_->setChecked(false);
    BasicButtonController::resetOtherButtons(this);

    // Handle tool button visibility based on current tab
    for (auto it = toolsButtonsVisibility_.begin();
         it != toolsButtonsVisibility_.end(); ++it)
    {
        QWidget     *button     = it.key();
        QList<int>   tabIndices = it.value();
        button->setVisible(tabIndices.contains(index));
    }

    // Handle tab visibility in the toolbar
    for (auto it = tabsVisibility_.begin();
         it != tabsVisibility_.end(); ++it)
    {
        int        toolbarTabIndex = it.key();
        QList<int> tabIndices      = it.value();
        toolbar_->setTabVisible(toolbarTabIndex,
                                tabIndices.contains(index));
    }

    // Handle dock window visibility
    for (auto it = windowVisibility_.begin();
         it != windowVisibility_.end(); ++it)
    {
        QDockWidget            *dockWindow = it.key();
        QMap<QString, QVariant> config     = it.value();

        bool isTabAllowed =
            config["tabs"].toList().contains(index);
        bool isButtonChecked = config["button"]
                                   .value<QToolButton *>()
                                   ->isChecked();
        config["button"].value<QToolButton *>()->setEnabled(
            isTabAllowed);
        dockWindow->setVisible(isTabAllowed
                               && isButtonChecked);
    }

    // Update group visibility
    updateGroupVisibility(toolsGroup_, toolsButtons_);
    updateGroupVisibility(measurementsGroup_,
                          measurementsButtons_);
    updateGroupVisibility(regionGroup_, regionWidgets_);
    updateGroupVisibility(networkImportGroup_,
                          networkImportButtons_);
    updateGroupVisibility(navigationGroup_,
                          navigationButtons_);
    updateGroupVisibility(windowsGroup_, windowsButtons_);
    updateGroupVisibility(logsGroup_, logsButtons_);
    updateGroupVisibility(networkToolsGroup_,
                          networkToolsButtons_);
    updateGroupVisibility(projectGroup_, projectButtons_);
    updateGroupVisibility(simulationToolsGroup_,
                          simulationToolsButtons_);
    updateGroupVisibility(transportationVehiclesGroup_,
                          transportationVehiclesButtons_);
    updateGroupVisibility(visibilityGroup_,
                          visibilityButtons_);

    // Handle shortest paths table visibility
    if (isLoggingTab)
    {
        // Save current visibility state before hiding
        tableWasVisible_ =
            shortestPathTableDock_->isVisible();
        shortestPathTableDock_->hide();
    }
    else if (previousTabIndex_
             == tabWidget_->indexOf(tabWidget_->widget(2)))
    { // Coming from logging tab
        if (tableWasVisible_)
        {
            shortestPathTableDock_->show();
        }
    }

    // Store current tab index for next time
    previousTabIndex_ = index;
}

void MainWindow::updateGroupVisibility(
    QGroupBox *group, const QList<QWidget *> &buttons)
{
    int  currentTab         = tabWidget_->currentIndex();
    bool anyShouldBeVisible = false;

    for (QWidget *button : buttons)
    {
        // If button has tab visibility rules, check them
        if (toolsButtonsVisibility_.contains(button))
        {
            if (toolsButtonsVisibility_[button].contains(
                    currentTab))
            {
                anyShouldBeVisible = true;
                break;
            }
        }
        else
        {
            // If button has no visibility rules, assume it
            // should be visible
            anyShouldBeVisible = true;
            break;
        }
    }

    group->setVisible(anyShouldBeVisible);
}

void MainWindow::updateAllCoordinates()
{
    if (!regionScene_ || !regionView_)
        return;

    // Get all items in the scene
    QList<QGraphicsItem*> allItems = regionScene_->items();

    // Update the properties panel if it's showing coordinates
    if (propertiesPanel_ && propertiesPanel_->getCurrentItem()) {
        QGraphicsItem* currentItem = propertiesPanel_->getCurrentItem();
        if (dynamic_cast<RegionCenterPoint*>(currentItem) ||
            dynamic_cast<MapPoint*>(currentItem) ||
            dynamic_cast<TerminalItem*>(currentItem) ||
            dynamic_cast<BackgroundPhotoItem*>(currentItem)) {
            propertiesPanel_->displayProperties(currentItem);
        }
    }

    // Update the view
    regionView_->viewport()->update();
}

void MainWindow::showStatusBarMessage(QString message,
                                      int     timeout)
{
    // Add message to queue
    StatusMessage newMessage;
    newMessage.message = message;
    newMessage.timeout =
        timeout > 0
            ? timeout
            : 5000; // Default 5 seconds if not specified
    newMessage.timestamp = QDateTime::currentDateTime();
    newMessage.isError   = false;

    messageQueue_.append(newMessage);

    // Limit non-error messages to the latest 2
    int nonErrorCount = 0;
    for (int i = messageQueue_.size() - 1; i >= 0; i--)
    {
        if (!messageQueue_[i].isError)
        {
            nonErrorCount++;
            if (nonErrorCount > 2)
            {
                // Remove the older message
                messageQueue_.removeAt(i);
            }
        }
    }
}

void MainWindow::showStatusBarError(QString message,
                                    int     timeout)
{
    // Add error message to queue
    StatusMessage newMessage;
    newMessage.message = message;
    newMessage.timeout =
        timeout > 0
            ? timeout
            : 5000; // Default 5 seconds if not specified
    newMessage.timestamp = QDateTime::currentDateTime();
    newMessage.isError   = true;

    // Remove all non-error messages
    for (int i = messageQueue_.size() - 1; i >= 0; i--)
    {
        if (!messageQueue_[i].isError)
        {
            messageQueue_.removeAt(i);
        }
    }

    messageQueue_.append(newMessage);
}

void MainWindow::startStatusProgress()
{
    // Only start if we have a spinner
    if (statusSpinner_)
    {
        statusSpinner_->startSpinning();
    }
}

void MainWindow::stopStatusProgress()
{
    // Only stop if we have a spinner
    if (statusSpinner_)
    {
        statusSpinner_->stopSpinning();
    }
}

// ---------------------------------------------------------------------------
// StatusReporter overrides
// ---------------------------------------------------------------------------

void MainWindow::showMessage(const QString &msg, int ms)
{
    showStatusBarMessage(msg, ms);
}

void MainWindow::showError(const QString &msg, int ms)
{
    showStatusBarError(msg, ms);
}

void MainWindow::startProgress()
{
    startStatusProgress();
}

void MainWindow::stopProgress()
{
    stopStatusProgress();
}

void MainWindow::storeButtons()
{
    ToolbarController::storeButtonStates(this);
}

void MainWindow::disableButtons()
{
    ToolbarController::disableAllButtons(this);
}

void MainWindow::restoreButtons()
{
    ToolbarController::restoreButtonStates(this);
}

void MainWindow::processMessageQueue()
{
    // If already processing a message, do nothing
    if (isProcessingMessageQueue_)
    {
        return;
    }

    // If queue is empty, set default message based on
    // spinner state
    if (messageQueue_.isEmpty())
    {
        if (statusSpinner_ && statusSpinner_->isSpinning())
        {
            statusLabel_->setText("Processing...");
        }
        else
        {
            statusLabel_->setText("Ready.");
        }
        statusLabel_->setStyleSheet("");
        return;
    }

    // Mark as processing
    isProcessingMessageQueue_ = true;

    // Prioritize error messages - find the first error
    // message if any
    int messageIndex = 0;
    for (int i = 0; i < messageQueue_.size(); i++)
    {
        if (messageQueue_[i].isError)
        {
            messageIndex = i;
            break;
        }
    }

    // Get the prioritized message
    StatusMessage currentMessage =
        messageQueue_[messageIndex];

    // Display the message
    statusLabel_->setText(currentMessage.message);

    // Apply styling if it's an error message
    if (currentMessage.isError)
    {
        statusLabel_->setStyleSheet("color: red;");
    }
    else
    {
        statusLabel_->setStyleSheet("");
    }

    // Schedule message removal after timeout
    QTimer::singleShot(
        currentMessage.timeout, this,
        [this, messageIndex]() {
            // Remove the message from the queue
            if (messageIndex < messageQueue_.size())
            {
                messageQueue_.removeAt(messageIndex);
            }

            // Reset processing flag
            isProcessingMessageQueue_ = false;

            // Process next message immediately if available
            if (!messageQueue_.isEmpty())
            {
                processMessageQueue();
            }
            else
            {
                // Set text based on spinner state
                if (statusSpinner_
                    && statusSpinner_->isSpinning())
                {
                    statusLabel_->setText("Processing...");
                }
                else
                {
                    statusLabel_->setText("Ready.");
                }
                statusLabel_->setStyleSheet("");
            }
        });
}

void MainWindow::togglePanMode()
{
    // Toggle between pan modes
    GraphicsView *view = getCurrentView();
    if (!view)
        return;

    QString currentMode = view->getCurrentPanMode();
    QString newMode     = (currentMode == "middle_mouse")
                              ? "ctrl_left"
                              : "middle_mouse";

    // Update both views
    regionView_->setCurrentPanMode(newMode);
    globalMapView_->setCurrentPanMode(newMode);

    // Update button text
    QString buttonText = (newMode == "middle_mouse")
                             ? "Pan Mode:\nMiddle Mouse"
                             : "Pan Mode:\nCtrl + Left";
    panModeButton_->setText(buttonText);

    // Show status message
    QString modeText = (newMode == "middle_mouse")
                           ? "middle mouse button"
                           : "Ctrl + left mouse button";
    showStatusBarMessage(
        QString("Panning mode changed to %1").arg(modeText),
        2000);
}


void MainWindow::toggleShortestPathsTable(bool show)
{
    // if (!show) {
    //     shortestPathTableDock_->hide();
    //     // Save the current sizes before hiding
    //     savedSplitterSizes_ = centerSplitter->sizes();
    //     // Give all space to the tab widget
    //     int totalHeight = savedSplitterSizes_.at(0) +
    //     savedSplitterSizes_.at(1);
    //     centerSplitter->setSizes({totalHeight, 0});
    // } else {
    //     shortestPathTableDock_->show();
    //     // Restore previous sizes if they were saved
    //     if (!savedSplitterSizes_.isEmpty()) {
    //         centerSplitter->setSizes(savedSplitterSizes_);
    //     } else {
    //         // Default split if no saved sizes
    //         int totalHeight = centerSplitter->height();
    //         centerSplitter->setSizes({int(totalHeight *
    //         0.7), int(totalHeight * 0.3)});
    //     }
    // }
}

void MainWindow::showErrorDialog(const QString &errorText)
{
    QMessageBox msg;
    msg.setIcon(QMessageBox::Critical);
    msg.setText("An error occurred");
    msg.setDetailedText(errorText);
    msg.setWindowTitle("Error");
    msg.exec();
}

void MainWindow::updateServerHeartbeat(
    const QString &serverId, float timestamp)
{
    // This method no longer does anything, as we've removed
    // heartbeat functionality and rely exclusively on
    // consumer checks
}

void MainWindow::updateBackendMessage(
    const QString &message, const QString &status,
    int timeout)
{
    // Show the backend icon
    backendIcon_->setVisible(true);

    // Set style based on message type
    if (status.toLower() == "error"
        || message.toLower().contains("not exist")
        || message.toLower().contains("failed"))
    {
        backendReportLabel_->setStyleSheet(
            "color: #cc0000; font-weight: bold;"); // Red
                                                   // for
                                                   // errors
    }
    else if (status.toLower() == "success"
             || message.toLower().contains("success")
             || message.toLower().contains("created")
             || message.toLower().contains("established"))
    {
        backendReportLabel_->setStyleSheet(
            "color: #007700;"); // Green for success
    }
    else
    {
        backendReportLabel_->setStyleSheet(
            "color: #0066cc;"); // Blue for info
    }

    // Update the text
    backendReportLabel_->setText(message);

    // Auto-clear after a timeout if specified
    if (timeout > 0)
    {
        QTimer::singleShot(
            timeout, this,
            &MainWindow::clearBackendMessage);
    }
}

void MainWindow::clearBackendMessage()
{
    backendReportLabel_->setText("");
    backendIcon_->setVisible(false);
}

void MainWindow::shutdown()
{
    qCInfo(lcGui) << "MainWindow::shutdown: begin";
    // Signal application shutdown
    QApplication::quit();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    CustomMainWindow::resizeEvent(event);
}

void MainWindow::assignSelectedToCurrentRegion()
{
    for (QGraphicsItem *item :
         regionScene_->selectedItems())
    {
        RegionCenterPoint *centerPoint =
            dynamic_cast<RegionCenterPoint *>(item);
        if (centerPoint)
        {
            showStatusBarError(
                "Region center point cannot be assigned to "
                "other regions.",
                3000);
            return;
        }

        QString currentRegion =
            CargoNetSim::CargoNetSimController::
                getInstance()
                    .getRegionDataController()
                    ->getCurrentRegion();

        // Handle items with a 'region' property
        if (item->type() == QGraphicsItem::UserType + 1)
        { // TerminalItem
            TerminalItem *terminal =
                static_cast<TerminalItem *>(item);
            terminal->setRegion(currentRegion);
        }
        else if (item->type()
                 == QGraphicsItem::UserType + 2)
        { // ConnectionLine
            ConnectionLine *connection =
                static_cast<ConnectionLine *>(item);
            connection->setRegion(currentRegion);
        }
        else if (item->type()
                 == QGraphicsItem::UserType + 4)
        { // BackgroundPhotoItem
            BackgroundPhotoItem *photo =
                static_cast<BackgroundPhotoItem *>(item);
            photo->setRegion(currentRegion);
        }
        else if (item->type()
                 == QGraphicsItem::UserType + 7)
        { // MapPoint
            MapPoint *point = static_cast<MapPoint *>(item);
            point->setRegion(currentRegion);
        }
        else if (item->type()
                 == QGraphicsItem::UserType + 8)
        { // MapLine
            MapLine *line = static_cast<MapLine *>(item);
            line->setRegion(currentRegion);
        }
    }

    // sceneVisibility()->updateSceneVisibility();
    // ViewController::updateGlobalMapScene(this);
    showStatusBarMessage(
        "Selected items assigned to current region.", 2000);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Ask for confirmation
    QMessageBox::StandardButton reply =
        QMessageBox::question(
            this, "Exit Application",
            "Are you sure you want to exit?",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);

    if (reply == QMessageBox::Yes)
    {
        // Save any application state if needed

        // Perform shutdown procedures
        shutdown();
        event->accept();
    }
    else
    {
        event->ignore();
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    // Delete/Backspace are handled inside the Input subsystem
    // (NormalMode::onKeyPress → CommandBus), dispatching via the
    // polymorphic GraphicsObjectBase::createDeleteCommand hook. This keeps
    // deletion uniformly undoable and keeps MainWindow out of per-type
    // dispatch. Only Escape remains here; everything else falls through
    // to the base class.
    if (event->key() == Qt::Key_Escape)
    {
        // Clear selection in the current scene
        GraphicsScene *currentScene = getCurrentScene();
        if (currentScene)
        {
            currentScene->clearSelection();
        }

        // Reset all toggle button states
        connectButton_->setChecked(false);
        linkTerminalButton_->setChecked(false);
        unlinkTerminalButton_->setChecked(false);
        measureButton_->setChecked(false);

        // Drop both controllers back to NormalMode; mode exit cleans tool state.
        if (m_regionInputController)
            m_regionInputController->setMode<Input::NormalMode>();
        if (m_globalInputController)
            m_globalInputController->setMode<Input::NormalMode>();

        unsetCursor();

        event->accept();
    }
    else
    {
        CustomMainWindow::keyPressEvent(event);
    }
}

void MainWindow::flashPathLines(int pathId)
{
    if (!shortestPathTable_)
    {
        return;
    }

    // Get the selected path data
    const ShortestPathsTable::PathData *pathData =
        shortestPathTable_->getDataByPathId(pathId);

    if (!pathData || !pathData->path)
    {
        qCWarning(lcGui)
            << "Cannot flash path: Invalid path data for ID"
            << pathId;
        return;
    }

    // Get segments and terminals from the path
    const QList<Backend::PathSegment *> &segments =
        pathData->path->getSegments();
    const QList<Backend::Terminal *> &terminals =
        pathData->path->getTerminalsInPath();

    // Process each segment
    for (int i = 0; i < segments.size(); ++i)
    {
        Backend::PathSegment *segment = segments[i];
        if (!segment)
            continue;

        // Get terminals for this segment
        Backend::Terminal *startTerminal = terminals[i];
        Backend::Terminal *endTerminal   = terminals[i + 1];

        if (!startTerminal || !endTerminal)
        {
            qCWarning(lcGui) << "Cannot flash path: Missing "
                                "terminals for segment"
                             << i;
            continue;
        }

        // Find corresponding terminal items in the scene
        TerminalItem *startTerminalItem = nullptr;
        TerminalItem *endTerminalItem   = nullptr;

        for (auto terminal :
             regionScene_->getItemsByType<TerminalItem>())
        {
            QString terminalName =
                terminal->getProperty("Name").toString();

            if (terminalName
                == startTerminal->getDisplayName())
            {
                startTerminalItem = terminal;
            }
            else if (terminalName
                     == endTerminal->getDisplayName())
            {
                endTerminalItem = terminal;
            }

            if (startTerminalItem && endTerminalItem)
                break;
        }

        if (!startTerminalItem || !endTerminalItem)
        {
            qCWarning(lcGui) << "Cannot flash path: Unable to "
                                "find terminal items for segment"
                             << i;
            continue;
        }

        // Get transportation mode
        const Backend::TransportationTypes::TransportationMode
            segmentMode = segment->getMode();

        // Find connection line between these terminals
        ConnectionLine *connection = nullptr;
        for (auto line :
             regionScene_->getItemsByType<ConnectionLine>())
        {
            if (((line->startItem() == startTerminalItem
                  && line->endItem() == endTerminalItem)
                 || (line->startItem() == endTerminalItem
                     && line->endItem()
                            == startTerminalItem))
                && line->connectionType() == segmentMode)
            {
                connection = line;
                break;
            }
        }

        if (!connection)
        {
            qCWarning(lcGui) << "Cannot flash path: Unable to "
                                "find connection line for segment"
                             << i;
            continue;
        }

        // If ship mode, flash the connection line only
        if (segmentMode
            == Backend::TransportationTypes::
                TransportationMode::Ship)
        {
            // Flash the connection line
            connection->flash(
                true,
                QColor(Qt::blue)); // Blue for ship
        }
        // For train or truck, flash the network map lines
        else
        {
            // Determine network type
            NetworkType networkType;
            if (segmentMode
                == Backend::TransportationTypes::
                    TransportationMode::Train)
            {
                networkType = NetworkType::Train;
                // Choose dark gray for train
                QColor flashColor = QColor(80, 80, 80);
            }
            else if (segmentMode
                     == Backend::TransportationTypes::
                         TransportationMode::Truck)
            {
                networkType = NetworkType::Truck;
                // Choose magenta for truck
                QColor flashColor = QColor(255, 0, 255);
            }

            QString regionName =
                startTerminalItem->getRegion();

            // Get map points for both terminals
            QList<MapPoint *> sourcePoints =
                UtilitiesFunctions::getMapPointsOfTerminal(
                    regionScene_, startTerminalItem,
                    regionName, "*", networkType);

            QList<MapPoint *> targetPoints =
                UtilitiesFunctions::getMapPointsOfTerminal(
                    regionScene_, endTerminalItem,
                    regionName, "*", networkType);

            // Find matching network points
            auto networkPairs = UtilitiesFunctions::
                getCommonNetworksOfNetworkType(sourcePoints,
                                               targetPoints,
                                               networkType);

            if (networkPairs.isEmpty())
            {
                qCWarning(lcGui)
                    << "Cannot flash path: No common "
                       "network points found for segment"
                    << i;
                continue;
            }

            // Take the first pair
            MapPoint *sourcePoint =
                networkPairs.first().first;
            MapPoint *targetPoint =
                networkPairs.first().second;

            if (!sourcePoint || !targetPoint)
            {
                qCWarning(lcGui) << "Cannot flash path: Invalid "
                                    "network points";
                continue;
            }

            // Get network information
            QObject *networkObj =
                sourcePoint->getReferenceNetwork();
            if (!networkObj)
            {
                qCWarning(lcGui) << "Cannot flash path: Unable "
                                    "to get reference network";
                continue;
            }

            Backend::NetworkKind kind{};
            const QString        networkName =
                Backend::Scenario::NetworkLookup::networkNameOf(
                    networkObj, &kind);
            // Sanity: the point's referenced network must match the
            // flash-path's declared network type.
            const bool kindMatches =
                !networkName.isEmpty()
                && ((networkType == NetworkType::Train
                     && kind == Backend::NetworkKind::Rail)
                    || (networkType == NetworkType::Truck
                        && kind == Backend::NetworkKind::Truck));
            if (!kindMatches)
            {
                qCWarning(lcGui) << "Cannot flash path: network type "
                                    "mismatch between point and path";
                continue;
            }

            if (networkName.isEmpty())
            {
                qCWarning(lcGui) << "Cannot flash path: Unable "
                                    "to determine network name";
                continue;
            }

            // Get node IDs
            QString sourceNodeId =
                sourcePoint->getReferencedNetworkNodeID();
            QString targetNodeId =
                targetPoint->getReferencedNetworkNodeID();

            bool validSourceID, validTargetID;
            int  sourceID =
                sourceNodeId.toInt(&validSourceID);
            int targetID =
                targetNodeId.toInt(&validTargetID);

            if (!validSourceID || !validTargetID)
            {
                qCWarning(lcGui) << "Cannot flash path: Invalid "
                                    "node IDs";
                continue;
            }

            // Get map lines for the shortest path
            QList<MapLine *> pathMapLines =
                NetworkController::getShortestPathMapLines(
                    this, regionName, networkName,
                    networkType, sourceID, targetID);

            // Flash each map line with appropriate color
            QColor flashColor =
                (networkType == NetworkType::Train)
                    ? QColor(Qt::darkGray) // Dark gray for
                                           // train
                    : QColor(
                          Qt::magenta); // Magenta for truck

            for (MapLine *mapLine : pathMapLines)
            {
                mapLine->flash(false, flashColor);
            }
        }
    }
}

void MainWindow::addBackgroundPhoto()
{
    try
    {
        // Open file dialog (using non-native dialog for
        // consistency)
        QString fileName = QFileDialog::getOpenFileName(
            nullptr, "Select Background Photo", "",
            "Images (*.png *.jpg *.bmp)", nullptr,
            QFileDialog::DontUseNativeDialog);

        if (fileName.isEmpty())
        {
            return;
        }

        QPixmap pixmap(fileName);
        if (pixmap.isNull())
        {
            QMessageBox::warning(this, "Error",
                                 "Failed to load image.");
            return;
        }

        // Check which tab is currently active
        // Build a BackgroundPhotoSpec centered on the active view and delegate
        // creation/registration/wiring to the factory. Qt convention:
        // QPointF returned by sceneToWGS84 is (x=longitude, y=latitude).
        const bool isGlobal =
            tabWidget_->currentWidget() != tabWidget_->widget(0);
        GraphicsView  *view  = isGlobal ? globalMapView_  : regionView_;
        GraphicsScene *scene = isGlobal ? globalMapScene_ : regionScene_;
        const QString  region =
            isGlobal
                ? QStringLiteral("global")
                : CargoNetSim::CargoNetSimController::
                      getInstance()
                          .getRegionDataController()
                          ->getCurrentRegion();

        const QPointF viewCenter =
            view->mapToScene(view->viewport()->rect().center());
        const QPointF wgs = view->sceneToWGS84(viewCenter);

        GUI::Scenario::BackgroundPhotoSpec spec;
        spec.pixmap   = pixmap;
        spec.region   = region;
        spec.scenePos = viewCenter;
        spec.properties = {
            {QStringLiteral("Type"),
             QStringLiteral("Background - %1").arg(region)},
            {QStringLiteral("Region"),    region},
            {QStringLiteral("Scale"),     1.0},
            {QStringLiteral("Latitude"),  QString::number(wgs.y(), 'f', 6)},
            {QStringLiteral("Longitude"), QString::number(wgs.x(), 'f', 6)},
            {QStringLiteral("Locked"),    false},
            {QStringLiteral("Opacity"),   1.0},
        };

        GUI::Scenario::BackgroundPhotoItemFactory::create(spec, scene, this);
    }
    catch (const std::exception &e)
    {
        qCWarning(lcGui) << "Error in addBackgroundPhoto:"
                         << e.what();
        QMessageBox::warning(
            this, "Error",
            QString("Failed to add background photo: %1")
                .arg(e.what()));
    }
}

} // namespace GUI
} // namespace CargoNetSim
