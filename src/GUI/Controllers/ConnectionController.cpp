#include "ConnectionController.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Scenario/ScenarioDocument.h"
#include "Backend/Scenario/ScenarioRuntime.h"
#include "GUI/Commons/NetworkType.h"
#include "GUI/Items/ConnectionLine.h"
#include "GUI/Items/GlobalTerminalItem.h"
#include "GUI/Items/TerminalItem.h"
#include "GUI/MainWindow.h"
#include "GUI/Scenario/ConnectionLineFactory.h"
#include "GUI/Scenario/ItemEventBinder.h"
#include "GUI/Scenario/ScenarioMutator.h"
#include "GUI/Widgets/GraphicsScene.h"
#include "GUI/Widgets/GraphicsView.h"
#include "GUI/Widgets/InterfaceSelectionDialog.h"
#include "StatusReporter.h"
#include "UtilityFunctions.h"
#include <QtWidgets/qapplication.h>

namespace CargoNetSim
{
namespace GUI
{

ConnectionController::ConnectionController(
    GraphicsScene  *regionScene,
    GraphicsScene  *globalMapScene,
    MainWindow     *mainWindow,
    StatusReporter *status,
    QObject        *parent)
    : QObject(parent)
    , m_regionScene(regionScene)
    , m_globalMapScene(globalMapScene)
    , m_mainWindow(mainWindow)
    , m_status(status)
{
    qCDebug(lcGuiView)
        << "ConnectionController: created";
}

void ConnectionController::setRuntime(
    Backend::Scenario::ScenarioRuntime *rt)
{
    m_runtime = rt;
    qCDebug(lcGuiView)
        << "ConnectionController::setRuntime:"
        << (rt ? "valid" : "null");
}

// ── checkExistingConnection ──────────────────────────────

bool ConnectionController::checkExistingConnection(
    QGraphicsItem *startItem,
    QGraphicsItem *endItem,
    Backend::TransportationTypes::TransportationMode
        connectionType)
{
    qCDebug(lcGuiView)
        << "ConnectionController::checkExistingConnection:"
        << "enter";

    if (!startItem || !endItem)
    {
        qCDebug(lcGuiView)
            << "ConnectionController::checkExistingConnection:"
            << "null argument, returning false";
        return false;
    }

    // Determine which scene to use based on item types
    QList<ConnectionLine *> viewConnectionLines;

    const bool isRegionStart =
        dynamic_cast<TerminalItem *>(startItem) != nullptr;
    const bool isRegionEnd =
        dynamic_cast<TerminalItem *>(endItem) != nullptr;

    if (isRegionStart && isRegionEnd)
    {
        viewConnectionLines =
            m_regionScene
                ->getItemsByType<ConnectionLine>();
    }
    else
    {
        const bool isGlobalStart =
            dynamic_cast<GlobalTerminalItem *>(startItem)
            != nullptr;
        const bool isGlobalEnd =
            dynamic_cast<GlobalTerminalItem *>(endItem)
            != nullptr;

        if (isGlobalStart && isGlobalEnd)
        {
            viewConnectionLines =
                m_globalMapScene
                    ->getItemsByType<ConnectionLine>();
        }
        else
        {
            // Mixed types or unrecognized types
            qCDebug(lcGuiView)
                << "ConnectionController::checkExistingConnection:"
                << "mixed item types, returning false";
            return false;
        }
    }

    if (viewConnectionLines.empty())
    {
        return false;
    }

    // Look for matching connection
    for (const ConnectionLine *line : viewConnectionLines)
    {
        if (line->connectionType() != connectionType)
        {
            continue;
        }

        if ((line->startItem() == startItem
             && line->endItem() == endItem)
            || (line->startItem() == endItem
                && line->endItem() == startItem))
        {
            return true;
        }
    }

    return false;
}

// ── createConnectionLine ─────────────────────────────────

ConnectionLine *ConnectionController::createConnectionLine(
    QGraphicsItem *startItem,
    QGraphicsItem *endItem,
    Backend::TransportationTypes::TransportationMode
        connectionType)
{
    qCDebug(lcGuiView)
        << "ConnectionController::createConnectionLine:"
        << "enter";

    if (!startItem || !endItem)
    {
        qCDebug(lcGuiView)
            << "ConnectionController::createConnectionLine:"
            << "null argument, returning nullptr";
        return nullptr;
    }
    // No scenario runtime -> no document to mutate. Use the
    // ad-hoc legacy path; the line lives in the scene only.
    if (!m_runtime)
    {
        if (checkExistingConnection(startItem, endItem,
                                    connectionType))
        {
            qCDebug(lcGuiView)
                << "ConnectionController::createConnectionLine:"
                << "legacy scene connection already exists, returning nullptr";
            return nullptr;
        }
        return createConnectionLineLegacy(startItem, endItem,
                                          connectionType);
    }

    auto *doc         = &m_runtime->document();

    // Case 1: two region-view TerminalItems in the same
    // region -> delegate to ScenarioMutator::createConnection.
    auto *SP = dynamic_cast<TerminalItem *>(startItem);
    auto *EP = dynamic_cast<TerminalItem *>(endItem);
    qCDebug(lcGuiView) << "createConnectionLine: SP=" << SP << "EP=" << EP;
    if (SP && EP && SP->getRegion() == EP->getRegion())
    {
        qCDebug(lcGuiView) << "createConnectionLine: Case1 same-region";
        const QString sId = SP->getTerminalId();
        const QString eId = EP->getTerminalId();
        if (sId.isEmpty() || eId.isEmpty())
            return createConnectionLineLegacy(startItem,
                                              endItem,
                                              connectionType);
        if (auto *existingLine =
                Scenario::ConnectionLineFactory::findRegionConnection(
                    m_regionScene, sId, eId, connectionType))
            return existingLine;
        if (auto *existing = doc->findConnection(
                sId, eId, connectionType))
            return Scenario::ConnectionLineFactory::fromConnection(
                doc, existing, m_regionScene, m_mainWindow);
        if (checkExistingConnection(startItem, endItem, connectionType))
        {
            qCDebug(lcGuiView)
                << "createConnectionLine: visual connection already exists in reverse direction";
            return nullptr;
        }
        if (!Scenario::ScenarioMutator::createConnection(
                doc, sId, eId, connectionType))
            return nullptr;
        return Scenario::ConnectionLineFactory::
            findRegionConnection(m_regionScene, sId, eId,
                                 connectionType);
    }

    // Case 2: two region-view TerminalItems in different
    // regions -> fall to legacy.
    if (SP && EP && SP->getRegion() != EP->getRegion())
    {
        qCDebug(lcGuiView) << "createConnectionLine: Case2 cross-region legacy";
        return createConnectionLineLegacy(startItem, endItem,
                                          connectionType);
    }

    // Case 3: two GlobalTerminalItems -> cross-region
    // GlobalLink.
    auto *SPG = dynamic_cast<GlobalTerminalItem *>(startItem);
    auto *EPG = dynamic_cast<GlobalTerminalItem *>(endItem);
    qCDebug(lcGuiView) << "createConnectionLine: SPG=" << SPG << "EPG=" << EPG;
    if (SPG && EPG && SPG != EPG)
    {
        auto *sTerm = SPG->getLinkedTerminalItem();
        auto *eTerm = EPG->getLinkedTerminalItem();
        qCDebug(lcGuiView) << "createConnectionLine: Case3 sTerm=" << sTerm << "eTerm=" << eTerm;
        if (!sTerm || !eTerm) return nullptr;
        qCDebug(lcGuiView) << "createConnectionLine: Case3 sRegion=" << sTerm->getRegion() << "eRegion=" << eTerm->getRegion();
        if (sTerm->getRegion() == eTerm->getRegion())
        {
            qCDebug(lcGuiView) << "createConnectionLine: Case3 same-region -> legacy";
            return createConnectionLineLegacy(startItem,
                                              endItem,
                                              connectionType);
        }

        const QString sId = sTerm->getTerminalId();
        const QString eId = eTerm->getTerminalId();
        qCDebug(lcGuiView) << "createConnectionLine: Case3 diff-region sId=" << sId << "eId=" << eId;
        if (sId.isEmpty() || eId.isEmpty())
            return createConnectionLineLegacy(startItem,
                                              endItem,
                                              connectionType);
        if (auto *existingLine =
                Scenario::ConnectionLineFactory::findGlobalLink(
                    m_globalMapScene, sId, eId, connectionType))
            return existingLine;
        if (auto *existing = doc->findGlobalLink(
                sId, eId, connectionType))
            return Scenario::ConnectionLineFactory::fromGlobalLink(
                doc, existing, m_globalMapScene, m_mainWindow);
        if (checkExistingConnection(startItem, endItem, connectionType))
        {
            qCDebug(lcGuiView)
                << "createConnectionLine: visual global link already exists in reverse direction";
            return nullptr;
        }
        qCDebug(lcGuiView) << "createConnectionLine: calling createGlobalLink";
        if (!Scenario::ScenarioMutator::createGlobalLink(
                doc, sId, eId, connectionType))
        {
            qCDebug(lcGuiView) << "createConnectionLine: createGlobalLink failed";
            return nullptr;
        }
        qCDebug(lcGuiView) << "createConnectionLine: calling findGlobalLink";
        auto *found = Scenario::ConnectionLineFactory::
            findGlobalLink(m_globalMapScene, sId, eId,
                           connectionType);
        qCDebug(lcGuiView) << "createConnectionLine: findGlobalLink=" << found;
        return found;
    }

    qCDebug(lcGuiView) << "createConnectionLine: mixed/unknown -> legacy";
    // Mixed types / self-link / unknown — legacy path emits
    // the appropriate error message (or returns nullptr).
    return createConnectionLineLegacy(startItem, endItem,
                                      connectionType);
}

// ── createConnectionLineLegacy ───────────────────────────

ConnectionLine *
ConnectionController::createConnectionLineLegacy(
    QGraphicsItem *startItem,
    QGraphicsItem *endItem,
    Backend::TransportationTypes::TransportationMode
        connectionType)
{
    qCDebug(lcGuiView)
        << "ConnectionController::createConnectionLineLegacy:"
        << "enter";

    auto SP = dynamic_cast<TerminalItem *>(startItem);
    auto EP = dynamic_cast<TerminalItem *>(endItem);

    if (SP && EP && SP->getRegion() == EP->getRegion())
    {
        auto line = new ConnectionLine(
            SP, EP, connectionType, {}, SP->getRegion());
        m_regionScene->addItemWithId(
            line, line->sceneRegistryKey());

        Scenario::ItemEventBinder::bindConnectionLine(
            line, m_mainWindow);

        return line;
    }
    else if (SP && EP
             && SP->getRegion() != EP->getRegion())
    {
        m_status->showError(
            "Cannot create a connection between two "
            "different regions in region view.",
            3000);
        return nullptr;
    }
    else if (!SP && !EP)
    {
        auto SPG =
            dynamic_cast<GlobalTerminalItem *>(startItem);
        auto EPG =
            dynamic_cast<GlobalTerminalItem *>(endItem);

        if (SPG && EPG && SPG != EPG)
        {
            auto *sLink = SPG->getLinkedTerminalItem();
            auto *eLink = EPG->getLinkedTerminalItem();
            qCDebug(lcGuiView)
                << "createConnectionLineLegacy:"
                << "global sLink=" << sLink
                << "eLink=" << eLink;
            if (!sLink || !eLink)
            {
                qCWarning(lcGuiView)
                    << "createConnectionLineLegacy:"
                    << "null linked terminal in GlobalTerminalItem";
                return nullptr;
            }
            if (sLink->getRegion() == eLink->getRegion())
            {
                m_status->showError(
                    "Cannot link terminals in the same "
                    "region in global map.",
                    3000);
                return nullptr;
            }

            auto line = new ConnectionLine(
                SPG, EPG, connectionType, {}, "Global");
            m_globalMapScene->addItemWithId(
                line, line->sceneRegistryKey());

            Scenario::ItemEventBinder::bindConnectionLine(
                line, m_mainWindow);

            return line;
        }
        else if (SPG && EPG && SPG == EPG)
        {
            m_status->showError(
                "Cannot link a terminal to itself.", 3000);
        }
    }

    return nullptr;
}

// ── removeConnectionLine ─────────────────────────────────

bool ConnectionController::removeConnectionLine(
    ConnectionLine *connectionLine)
{
    qCDebug(lcGuiView)
        << "ConnectionController::removeConnectionLine:"
        << "enter";

    if (!connectionLine)
    {
        qCDebug(lcGuiView)
            << "ConnectionController::removeConnectionLine:"
            << "null argument, returning false";
        return false;
    }

    try
    {
        if (m_runtime && connectionLine->isConnectionBinding())
        {
            auto *doc = &m_runtime->document();
            const bool removed = Scenario::ScenarioMutator::removeConnection(
                doc,
                connectionLine->boundFromTerminalId(),
                connectionLine->boundToTerminalId(),
                connectionLine->connectionType());
            if (removed)
                m_status->showMessage(
                    QString("Connection removed successfully."),
                    2000);
            else
                m_status->showError(
                    QString("Failed to remove connection."),
                    3000);
            return removed;
        }
        if (m_runtime && connectionLine->isGlobalLinkBinding())
        {
            auto *doc = &m_runtime->document();
            const bool removed = Scenario::ScenarioMutator::removeGlobalLink(
                doc,
                connectionLine->boundFromTerminalId(),
                connectionLine->boundToTerminalId(),
                connectionLine->connectionType());
            if (removed)
                m_status->showMessage(
                    QString("Connection removed successfully."),
                    2000);
            else
                m_status->showError(
                    QString("Failed to remove connection."),
                    3000);
            return removed;
        }

        // Determine which scene the connection belongs to
        GraphicsScene *scene = nullptr;

        if (connectionLine->getRegion() != "Global")
        {
            scene = m_regionScene;
        }
        else
        {
            scene = m_globalMapScene;
        }

        if (!scene)
        {
            qCWarning(lcGuiView)
                << "ConnectionController::removeConnectionLine:"
                << "scene is null";
            return false;
        }

        QString connectionID =
            connectionLine->sceneRegistryKey();

        if (scene->removeItemWithId<ConnectionLine>(
                connectionID))
        {
            m_status->showMessage(
                QString("Connection removed successfully."),
                2000);

            // Signal the panel to clear if it was showing
            // this connection
            emit connectionRemoved(connectionLine);

            return true;
        }
        else
        {
            m_status->showError(
                QString("Failed to remove connection."),
                3000);
            return false;
        }
    }
    catch (const std::exception &e)
    {
        qCWarning(lcGuiView)
            << "Error removing connection line:"
            << e.what();
        m_status->showError(
            QString("Error removing connection: %1")
                .arg(e.what()),
            3000);
        return false;
    }
}

// ── connectVisibleTerminalsByNetworks ────────────────────

void ConnectionController::
    connectVisibleTerminalsByNetworks(
        const QString &currentRegion,
        bool           isGlobalView)
{
    qCDebug(lcGuiView)
        << "ConnectionController::"
           "connectVisibleTerminalsByNetworks: enter";

    auto vehicleController =
        CargoNetSim::CargoNetSimController::getInstance()
            .getVehicleController();

    if (vehicleController->getAllShips().isEmpty())
    {
        m_status->showError(
            "No ships available! Load ships first!", 3000);
        return;
    }
    if (vehicleController->getAllTrains().isEmpty())
    {
        m_status->showError(
            "No trains available! Load trains first!",
            3000);
        return;
    }

    // Store the current button states and disable all
    m_status->storeButtons();
    m_status->disableButtons();
    m_status->startProgress();

    QApplication::processEvents();

    GraphicsScene *currentScene =
        isGlobalView ? m_globalMapScene
                     : m_regionScene;

    QList<TerminalItem *>       terminals;
    QList<GlobalTerminalItem *> globalTerminals;
    QSet<QString>               visibleTerminalTypes;
    QSet<QString>               availableNetworks;

    if (!m_runtime
        || !m_runtime->document().hasAnyOrigin())
    {
        qCWarning(lcGuiView)
            << "ConnectionController: no runtime or "
               "no origin terminals";
        m_status->showError(
            "No origin terminal with containers in "
            "this scenario.",
            3000);
        m_status->restoreButtons();
        m_status->stopProgress();
        return;
    }

    QApplication::processEvents();

    if (isGlobalView)
    {
        globalTerminals =
            UtilitiesFunctions::getGlobalTerminalItems(
                m_globalMapScene, "*", "*",
                UtilitiesFunctions::ConnectionType::Any,
                UtilitiesFunctions::LinkType::Any);

        for (auto terminal : globalTerminals)
        {
            if (terminal->getLinkedTerminalItem())
            {
                visibleTerminalTypes.insert(
                    terminal->getLinkedTerminalItem()
                        ->getTerminalType());
            }
        }

        availableNetworks.insert("Ship");
    }
    else
    {
        terminals =
            UtilitiesFunctions::getTerminalItems(
                m_regionScene, currentRegion, "*",
                UtilitiesFunctions::ConnectionType::Any,
                UtilitiesFunctions::LinkType::Any);

        for (auto terminal : terminals)
        {
            visibleTerminalTypes.insert(
                terminal->getTerminalType());
        }

        QApplication::processEvents();

        auto regionData =
            CargoNetSim::CargoNetSimController::
                getInstance()
                    .getRegionDataController()
                    ->getCurrentRegionData();

        if (regionData)
        {
            if (!regionData->getTrainNetworks().isEmpty())
            {
                availableNetworks.insert("Rail");
            }

            if (!regionData->getTruckNetworks().isEmpty())
            {
                availableNetworks.insert("Truck");
            }
        }
    }

    if ((terminals.empty() && !isGlobalView)
        || (globalTerminals.empty() && isGlobalView))
    {
        QString msgHndler =
            isGlobalView ? "view" : "region";
        QString mssg =
            QString("There is no terminal "
                    "in the current %1")
                .arg(msgHndler);
        m_status->showError(mssg, 3000);

        m_status->restoreButtons();
        m_status->stopProgress();
        return;
    }
    else if ((terminals.size() == 1 && !isGlobalView)
             || (globalTerminals.size() == 1
                 && isGlobalView))
    {
        QString msgHndler =
            isGlobalView ? "view" : "region";
        QString mssg =
            QString("There is only one terminal "
                    "in the current %1.")
                .arg(msgHndler);
        m_status->showError(mssg, 3000);

        m_status->restoreButtons();
        m_status->stopProgress();
        return;
    }

    if (availableNetworks.isEmpty())
    {
        m_status->showError(
            "No available network types found for "
            "connecting terminals.",
            3000);

        m_status->restoreButtons();
        m_status->stopProgress();
        return;
    }

    QApplication::processEvents();

    // Show network selection dialog
    InterfaceSelectionDialog dialog(
        availableNetworks, visibleTerminalTypes,
        InterfaceSelectionDialog::NetworkSelection,
        m_mainWindow);

    if (dialog.exec() != QDialog::Accepted)
    {
        m_status->restoreButtons();
        m_status->stopProgress();
        return;
    }

    QList<QString> selectedNetworks =
        dialog.getSelectedNetworkTypes();

    QMap<QString, bool> includedTerminalTypes =
        dialog.getIncludedTerminalTypes();

    if (selectedNetworks.isEmpty())
    {
        m_status->showMessage(
            "No network types selected for connection.",
            3000);

        m_status->restoreButtons();
        m_status->stopProgress();
        return;
    }

    bool anyTerminalTypeSelected = false;
    for (auto it = includedTerminalTypes.begin();
         it != includedTerminalTypes.end(); ++it)
    {
        if (it.value())
        {
            anyTerminalTypeSelected = true;
            break;
        }
    }

    if (!anyTerminalTypeSelected)
    {
        m_status->showMessage(
            "No terminal types selected for connection.",
            3000);

        m_status->restoreButtons();
        m_status->stopProgress();
        return;
    }

    QApplication::processEvents();

    bool anyConnectionCreated = false;
    bool errorOccurred        = false;
    int  processCount         = 0;

    if (!isGlobalView)
    {
        for (auto &sourceTerminal : terminals)
        {
            if (!includedTerminalTypes.value(
                    sourceTerminal->getTerminalType(),
                    true))
            {
                continue;
            }

            for (auto &targetTerminal : terminals)
            {
                if (sourceTerminal == targetTerminal)
                {
                    continue;
                }

                if (!includedTerminalTypes.value(
                        targetTerminal->getTerminalType(),
                        true))
                {
                    continue;
                }

                if (++processCount % 10 == 0)
                {
                    QApplication::processEvents();
                }

                const QSet<Backend::TransportationTypes::
                                TransportationMode>
                    commonModes =
                        UtilitiesFunctions::getCommonModes(
                            sourceTerminal, targetTerminal);
                using Mode = Backend::TransportationTypes::
                    TransportationMode;

                if (selectedNetworks.contains("Rail")
                    && commonModes.contains(Mode::Train)
                    && !CargoNetSim::CargoNetSimController::
                            getInstance()
                                .getRegionDataController()
                                ->getCurrentRegionData()
                                ->getTrainNetworks()
                                .isEmpty())
                {
                    bool isConnected =
                        UtilitiesFunctions::
                            processNetworkModeConnection(
                                m_mainWindow, sourceTerminal,
                                targetTerminal,
                                NetworkType::Train);
                    if (isConnected)
                    {
                        anyConnectionCreated = true;
                    }
                }

                if (selectedNetworks.contains("Truck")
                    && commonModes.contains(Mode::Truck)
                    && !CargoNetSim::CargoNetSimController::
                            getInstance()
                                .getRegionDataController()
                                ->getCurrentRegionData()
                                ->getTruckNetworks()
                                .isEmpty())
                {
                    bool isConnected =
                        UtilitiesFunctions::
                            processNetworkModeConnection(
                                m_mainWindow, sourceTerminal,
                                targetTerminal,
                                NetworkType::Truck);
                    if (isConnected)
                    {
                        anyConnectionCreated = true;
                    }
                }
            }
        }
    }
    else // Global view
    {
        for (auto &sourceTerminal : globalTerminals)
        {
            TerminalItem *sourceLinkedTerminal =
                sourceTerminal->getLinkedTerminalItem();
            if (!sourceLinkedTerminal
                || !includedTerminalTypes.value(
                    sourceLinkedTerminal->getTerminalType(),
                    true))
            {
                continue;
            }

            if (errorOccurred)
                break;

            for (auto &targetTerminal : globalTerminals)
            {
                TerminalItem *targetLinkedTerminal =
                    targetTerminal->getLinkedTerminalItem();
                if (!targetLinkedTerminal
                    || !includedTerminalTypes.value(
                        targetLinkedTerminal
                            ->getTerminalType(),
                        true))
                {
                    continue;
                }

                if (errorOccurred)
                    break;

                if (sourceTerminal == targetTerminal)
                {
                    continue;
                }

                if (++processCount % 10 == 0)
                {
                    QApplication::processEvents();
                }

                const QSet<Backend::TransportationTypes::
                                TransportationMode>
                    commonModes =
                        UtilitiesFunctions::getCommonModes(
                            sourceTerminal, targetTerminal);
                using Mode = Backend::TransportationTypes::
                    TransportationMode;

                for (auto mode : commonModes)
                {
                    if (errorOccurred)
                        break;

                    const QString selectKey =
                        Backend::
                            interfaceModeCanonicalString(
                                mode);
                    if (!selectedNetworks.contains(selectKey))
                    {
                        continue;
                    }

                    bool handled = false;
                    Backend::TransportationTypes::
                        TransportationMode connectionType;
                    if (mode == Mode::Ship)
                    {
                        connectionType = Mode::Ship;
                        handled = true;
                    }

                    if (handled)
                    {
                        auto connectionLine =
                            createConnectionLine(
                                sourceTerminal,
                                targetTerminal,
                                connectionType);
                        if (connectionLine)
                        {
                            CargoNetSim::Backend::
                                ShortestPathResult result;

                            QPointF sourceGeoPoint =
                                m_mainWindow->globalMapView()
                                    ->sceneToWGS84(
                                        sourceTerminal
                                            ->pos());
                            QPointF targetGeoPoint =
                                m_mainWindow->globalMapView()
                                    ->sceneToWGS84(
                                        targetTerminal
                                            ->pos());

                            result.totalLength =
                                UtilitiesFunctions::
                                    getApproximateGeoDistance(
                                        sourceGeoPoint,
                                        targetGeoPoint);
                            result.optimizationCriterion =
                                "distance";

                            NetworkType networkType =
                                NetworkType::Ship;

                            bool propertiesSet =
                                UtilitiesFunctions::
                                    setConnectionProperties(
                                        m_mainWindow,
                                        connectionLine,
                                        result,
                                        networkType);
                            if (!propertiesSet)
                            {
                                removeConnectionLine(
                                    connectionLine);
                                errorOccurred = true;
                                break;
                            }

                            anyConnectionCreated = true;
                        }
                    }
                }
            }

            QApplication::processEvents();
        }
    }

    QApplication::processEvents();

    if (anyConnectionCreated)
    {
        m_status->showMessage(
            "Terminal connections created based on "
            "selected networks and terminal types.");
    }
    else if (!errorOccurred)
    {
        m_status->showMessage(
            "No new connections were created.", 3000);
    }

    m_status->restoreButtons();
    m_status->stopProgress();
}

// ── connectVisibleTerminalsByInterfaces ──────────────────

void ConnectionController::
    connectVisibleTerminalsByInterfaces(
        const QString &currentRegion,
        bool           isGlobalView)
{
    qCDebug(lcGuiView)
        << "ConnectionController::"
           "connectVisibleTerminalsByInterfaces: enter";

    // Store the current button states and disable all
    m_status->storeButtons();
    m_status->disableButtons();
    m_status->startProgress();

    QApplication::processEvents();

    GraphicsScene *currentScene =
        isGlobalView ? m_globalMapScene
                     : m_regionScene;

    // Get all visible terminals in the current view
    QList<QGraphicsItem *> visibleTerminals;
    QSet<QString>          visibleTerminalTypes;

    QApplication::processEvents();

    if (isGlobalView)
    {
        QList<GlobalTerminalItem *> allTerminals =
            currentScene
                ->getItemsByType<GlobalTerminalItem>();
        for (auto terminal : allTerminals)
        {
            if (terminal && terminal->isVisible())
            {
                visibleTerminals.append(terminal);

                TerminalItem *linkedTerminal =
                    terminal->getLinkedTerminalItem();
                if (linkedTerminal)
                {
                    visibleTerminalTypes.insert(
                        linkedTerminal->getTerminalType());
                }
            }
        }
    }
    else
    {
        QList<TerminalItem *> allTerminals =
            currentScene->getItemsByType<TerminalItem>();

        for (auto terminal : allTerminals)
        {
            if (terminal && terminal->isVisible()
                && terminal->getRegion() == currentRegion)
            {
                visibleTerminals.append(terminal);
                visibleTerminalTypes.insert(
                    terminal->getTerminalType());
            }
        }
    }

    QApplication::processEvents();

    if (visibleTerminals.empty())
    {
        m_status->showError(
            "No visible terminals found in the current "
            "view.",
            3000);

        m_status->restoreButtons();
        m_status->stopProgress();
        return;
    }

    // Find all available common interfaces
    QSet<QString> availableInterfaces;
    int           processCount = 0;

    for (int i = 0; i < visibleTerminals.size(); ++i)
    {
        for (int j = i + 1; j < visibleTerminals.size();
             ++j)
        {
            if (++processCount % 10 == 0)
            {
                QApplication::processEvents();
            }

            QGraphicsItem *sourceItem = visibleTerminals[i];
            QGraphicsItem *targetItem = visibleTerminals[j];

            const auto commonModes =
                UtilitiesFunctions::getCommonModes(
                    sourceItem, targetItem);
            for (auto mode : commonModes)
            {
                const QString label =
                    Backend::
                        interfaceModeCanonicalString(mode);
                if (!label.isEmpty())
                    availableInterfaces.insert(label);
            }
        }
    }

    QApplication::processEvents();

    if (availableInterfaces.isEmpty())
    {
        m_status->showError(
            "No common interfaces found between "
            "terminals.",
            3000);

        m_status->restoreButtons();
        m_status->stopProgress();
        return;
    }

    // Show interface selection dialog
    qCDebug(lcGuiView)
        << "connectVisibleTerminalsByInterfaces:"
        << "BEFORE dialog.exec visibleTerminals.size()="
        << visibleTerminals.size();
    if (isGlobalView)
    {
        for (auto *item : visibleTerminals)
        {
            auto *gti =
                qgraphicsitem_cast<GlobalTerminalItem *>(item);
            qCDebug(lcGuiView)
                << "connectVisibleTerminalsByInterfaces:"
                << "pre-dialog item=" << (void *)item
                << "linked="
                << (gti ? (void *)gti->getLinkedTerminalItem()
                        : nullptr);
        }
    }
    InterfaceSelectionDialog dialog(
        availableInterfaces, visibleTerminalTypes,
        InterfaceSelectionDialog::InterfaceSelection,
        m_mainWindow);
    if (dialog.exec() != QDialog::Accepted)
    {
        m_status->restoreButtons();
        m_status->stopProgress();
        return;
    }

    // Log scene state immediately after dialog to detect pointer staleness
    qCDebug(lcGuiView)
        << "connectVisibleTerminalsByInterfaces:"
        << "AFTER dialog.exec visibleTerminals.size()="
        << visibleTerminals.size()
        << "scene GlobalTerminalItem count="
        << (isGlobalView
                ? currentScene
                      ->getItemsByType<GlobalTerminalItem>()
                      .size()
                : -1);
    if (isGlobalView)
    {
        for (auto *item : visibleTerminals)
        {
            auto *gti =
                qgraphicsitem_cast<GlobalTerminalItem *>(item);
            qCDebug(lcGuiView)
                << "connectVisibleTerminalsByInterfaces:"
                << "post-dialog item=" << (void *)item
                << "linked="
                << (gti ? (void *)gti->getLinkedTerminalItem()
                        : nullptr)
                << "linkedValid="
                << (gti && gti->getLinkedTerminalItem()
                        ? gti->getLinkedTerminalItem()
                              ->getTerminalId()
                        : QString("NULL"));
        }
    }

    QList<QString> selectedInterfaces =
        dialog.getSelectedInterfaces();

    QMap<QString, bool> includedTerminalTypes =
        dialog.getIncludedTerminalTypes();

    bool useCoordinateDistance =
        dialog.useCoordinateDistance();

    if (selectedInterfaces.isEmpty())
    {
        m_status->showMessage(
            "No interfaces selected for connection.",
            3000);

        m_status->restoreButtons();
        m_status->stopProgress();
        return;
    }

    bool anyTerminalTypeSelected = false;
    for (auto it = includedTerminalTypes.begin();
         it != includedTerminalTypes.end(); ++it)
    {
        if (it.value())
        {
            anyTerminalTypeSelected = true;
            break;
        }
    }

    if (!anyTerminalTypeSelected)
    {
        m_status->showMessage(
            "No terminal types selected for connection.",
            3000);

        m_status->restoreButtons();
        m_status->stopProgress();
        return;
    }

    // If using coordinate distance, perform additional checks
    if (useCoordinateDistance)
    {
        if (!m_runtime
            || !m_runtime->document().hasAnyOrigin())
        {
            qCWarning(lcGuiView)
                << "ConnectionController: no runtime or "
                   "no origin terminals "
                   "(coordinate distance)";
            m_status->showError(
                "No origin terminal with containers in "
                "this scenario.",
                3000);
            m_status->restoreButtons();
            m_status->stopProgress();
            return;
        }

        auto vehicleController =
            CargoNetSim::CargoNetSimController::
                getInstance()
                    .getVehicleController();

        if (vehicleController->getAllShips().isEmpty())
        {
            m_status->showError(
                "No ships available! Load ships first.",
                3000);

            m_status->restoreButtons();
            m_status->stopProgress();
            return;
        }

        if (vehicleController->getAllTrains().isEmpty())
        {
            m_status->showError(
                "No trains available! Load trains first.",
                3000);

            m_status->restoreButtons();
            m_status->stopProgress();
            return;
        }
    }

    QApplication::processEvents();

    // Connect terminals based on selected interfaces
    int connectionsCreated = 0;
    processCount           = 0;

    for (int i = 0; i < visibleTerminals.size(); ++i)
    {
        for (int j = i + 1; j < visibleTerminals.size();
             ++j)
        {
            if (i == j)
            {
                continue;
            }

            if (++processCount % 10 == 0)
            {
                QApplication::processEvents();
            }

            QGraphicsItem *sourceItem = visibleTerminals[i];
            QGraphicsItem *targetItem = visibleTerminals[j];

            const QSet<Backend::TransportationTypes::
                            TransportationMode>
                commonModes =
                    UtilitiesFunctions::getCommonModes(
                        sourceItem, targetItem);

            qCDebug(lcGuiView)
                << "connectVisibleTerminalsByInterfaces:"
                << "pair" << i << j
                << "commonModes=" << commonModes.size();

            bool    skipConnection = false;
            QString sourceType;
            QString targetType;

            if (TerminalItem *source =
                    qgraphicsitem_cast<TerminalItem *>(
                        sourceItem))
            {
                sourceType = source->getTerminalType();
            }
            else if (GlobalTerminalItem *globalSource =
                         qgraphicsitem_cast<
                             GlobalTerminalItem *>(
                             sourceItem))
            {
                TerminalItem *linkedSource =
                    globalSource->getLinkedTerminalItem();
                qCDebug(lcGuiView)
                    << "connectVisibleTerminalsByInterfaces:"
                    << "source linkedTerminal="
                    << (linkedSource ? linkedSource->getTerminalId() : "NULL");
                if (linkedSource)
                {
                    sourceType =
                        linkedSource->getTerminalType();
                }
            }

            if (TerminalItem *target =
                    qgraphicsitem_cast<TerminalItem *>(
                        targetItem))
            {
                targetType = target->getTerminalType();
            }
            else if (GlobalTerminalItem *globalTarget =
                         qgraphicsitem_cast<
                             GlobalTerminalItem *>(
                             targetItem))
            {
                TerminalItem *linkedTarget =
                    globalTarget->getLinkedTerminalItem();
                qCDebug(lcGuiView)
                    << "connectVisibleTerminalsByInterfaces:"
                    << "target linkedTerminal="
                    << (linkedTarget ? linkedTarget->getTerminalId() : "NULL");
                if (linkedTarget)
                {
                    targetType =
                        linkedTarget->getTerminalType();
                }
            }

            qCDebug(lcGuiView)
                << "connectVisibleTerminalsByInterfaces:"
                << "sourceType=" << sourceType
                << "targetType=" << targetType
                << "skip=" << skipConnection;

            if ((!sourceType.isEmpty()
                 && !includedTerminalTypes.value(
                     sourceType, true))
                || (!targetType.isEmpty()
                    && !includedTerminalTypes.value(
                        targetType, true)))
            {
                skipConnection = true;
            }

            if (!skipConnection)
            {
                for (auto mode : commonModes)
                {
                    const QString modeLabel =
                        Backend::
                            interfaceModeCanonicalString(
                                mode);
                    qCDebug(lcGuiView)
                        << "connectVisibleTerminalsByInterfaces:"
                        << "mode=" << modeLabel
                        << "selected="
                        << selectedInterfaces.contains(modeLabel);
                    if (!modeLabel.isEmpty()
                        && selectedInterfaces.contains(
                            modeLabel))
                    {
                        qCDebug(lcGuiView)
                            << "connectVisibleTerminalsByInterfaces:"
                            << "calling createConnectionLine mode="
                            << modeLabel;
                        ConnectionLine *connection =
                            createConnectionLine(
                                sourceItem, targetItem,
                                mode);
                        qCDebug(lcGuiView)
                            << "connectVisibleTerminalsByInterfaces:"
                            << "createConnectionLine returned"
                            << (connection ? "non-null" : "null");
                        if (connection)
                        {
                            if (useCoordinateDistance)
                            {
                                QPointF sourcePos;
                                QPointF targetPos;

                                if (isGlobalView)
                                {
                                    sourcePos =
                                        m_mainWindow
                                            ->globalMapView()
                                            ->sceneToWGS84(
                                                sourceItem
                                                    ->pos());
                                    targetPos =
                                        m_mainWindow
                                            ->globalMapView()
                                            ->sceneToWGS84(
                                                targetItem
                                                    ->pos());
                                }
                                else
                                {
                                    sourcePos =
                                        m_mainWindow
                                            ->regionView()
                                            ->sceneToWGS84(
                                                sourceItem
                                                    ->pos());
                                    targetPos =
                                        m_mainWindow
                                            ->regionView()
                                            ->sceneToWGS84(
                                                targetItem
                                                    ->pos());
                                }

                                double distanceMeters =
                                    UtilitiesFunctions::
                                        getApproximateGeoDistance(
                                            sourcePos,
                                            targetPos);

                                CargoNetSim::Backend::
                                    ShortestPathResult
                                        result;
                                result.totalLength =
                                    distanceMeters;
                                result
                                    .optimizationCriterion =
                                    "distance";

                                using Mode =
                                    Backend::
                                        TransportationTypes::
                                            TransportationMode;
                                NetworkType networkType;
                                if (mode == Mode::Train)
                                {
                                    networkType =
                                        NetworkType::Train;
                                }
                                else if (mode == Mode::Truck)
                                {
                                    networkType =
                                        NetworkType::Truck;
                                }
                                else
                                {
                                    networkType =
                                        NetworkType::Ship;
                                }

                                UtilitiesFunctions::
                                    setConnectionProperties(
                                        m_mainWindow,
                                        connection, result,
                                        networkType, false);
                            }

                            connectionsCreated++;
                        }
                    }
                }
            }
        }

        QApplication::processEvents();
    }

    QApplication::processEvents();

    if (connectionsCreated > 0)
    {
        m_status->showMessage(
            QString("Created %1 terminal connections based "
                    "on selected interfaces.")
                .arg(connectionsCreated),
            3000);
    }
    else
    {
        m_status->showMessage(
            "No new connections were created.", 3000);
    }

    m_status->restoreButtons();
    m_status->stopProgress();
}

} // namespace GUI
} // namespace CargoNetSim
