#include "ConnectionController.h"
#include "Backend/Application/NetworkViewService.h"
#include "Backend/Application/RouteAuthoringService.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/GuiApi/ScenarioContractsApi.h"
#include "Backend/GuiApi/ScenarioDocumentApi.h"
#include "Backend/Scenario/ScenarioRuntime.h"
#include "GUI/Commons/NetworkType.h"
#include "GUI/Items/ConnectionLine.h"
#include "GUI/Items/GlobalTerminalItem.h"
#include "GUI/Items/TerminalItem.h"
#include "GUI/MainWindow.h"
#include "GUI/Scenario/ConnectionLineFactory.h"
#include "GUI/Widgets/GraphicsScene.h"
#include "GUI/Widgets/InterfaceSelectionDialog.h"
#include "StatusReporter.h"
#include "UtilityFunctions.h"
#include <QVariantMap>
#include <QtWidgets/qapplication.h>

namespace CargoNetSim
{
namespace GUI
{

namespace
{

void applyCanonicalPropertiesToLine(
    ConnectionLine *line,
    const QVariantMap &canonicalProperties)
{
    if (!line) return;
    for (auto it = canonicalProperties.constBegin();
         it != canonicalProperties.constEnd(); ++it)
    {
        line->setProperty(it.key(), it.value());
    }
}

} // namespace

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
    if (!m_runtime)
    {
        qCWarning(lcGuiView)
            << "ConnectionController::createConnectionLine:"
            << "no ScenarioRuntime; refusing unbound connection";
        if (m_status)
            m_status->showError(
                QStringLiteral("Cannot create an unbound connection."),
                3000);
        return nullptr;
    }

    auto *doc         = &m_runtime->document();

    // Case 1: two region-view TerminalItems in the same
    // region -> create a document-owned regional connection.
    auto *SP = dynamic_cast<TerminalItem *>(startItem);
    auto *EP = dynamic_cast<TerminalItem *>(endItem);
    qCDebug(lcGuiView) << "createConnectionLine: SP=" << SP << "EP=" << EP;
    if (SP && EP && SP->getRegion() == EP->getRegion())
    {
        qCDebug(lcGuiView) << "createConnectionLine: Case1 same-region";
        const QString sId = SP->getTerminalId();
        const QString eId = EP->getTerminalId();
        auto &controller =
            CargoNetSim::CargoNetSimController::getInstance();
        Backend::Application::RouteAuthoringService routeAuthoringService(
            &controller);
        if (sId.isEmpty() || eId.isEmpty())
        {
            qCWarning(lcGuiView)
                << "createConnectionLine: same-region endpoint is unbound"
                << "sId=" << sId << "eId=" << eId;
            if (m_status)
                m_status->showError(
                    QStringLiteral("Cannot create a connection for an unbound terminal."),
                    3000);
            return nullptr;
        }
        if (auto *existingLine =
                Scenario::ConnectionLineFactory::findRegionConnection(
                    m_regionScene, sId, eId, connectionType))
        {
            return existingLine;
        }
        if (auto *existing = doc->findConnection(
                sId, eId, connectionType))
        {
            return Scenario::ConnectionLineFactory::fromConnection(
                doc, existing, m_regionScene, m_mainWindow);
        }
        if (checkExistingConnection(startItem, endItem, connectionType))
        {
            qCDebug(lcGuiView)
                << "createConnectionLine: visual connection already exists in reverse direction";
            return nullptr;
        }
        QVariantMap routeProperties;
        const auto propertyResult =
            routeAuthoringService
                .computeEndpointCanonicalRouteProperties(
                    *doc, sId, eId, connectionType);
        if (!propertyResult.succeeded())
        {
            if (m_status)
                m_status->showError(
                    propertyResult.message.isEmpty()
                        ? QStringLiteral(
                              "Failed to compute connection metrics.")
                        : propertyResult.message,
                    3000);
            return nullptr;
        }
        routeProperties = propertyResult.canonicalProperties;
        const auto createResult =
            routeAuthoringService.createConnection(
                *doc, sId, eId, connectionType,
                routeProperties,
                Backend::Scenario::LinkageSource::Manual);
        if (!createResult.succeeded())
        {
            m_status->showError(
                createResult.message.isEmpty()
                    ? QStringLiteral("Failed to create connection.")
                    : createResult.message,
                3000);
            return nullptr;
        }
        auto *line = Scenario::ConnectionLineFactory::
            findRegionConnection(m_regionScene, sId, eId,
                                 connectionType);
        if (line)
            applyCanonicalPropertiesToLine(line, routeProperties);
        return line;
    }

    // Case 2: two region-view TerminalItems in different regions are invalid
    // in region view; cross-region routes are authored from the global map.
    if (SP && EP && SP->getRegion() != EP->getRegion())
    {
        qCWarning(lcGuiView)
            << "createConnectionLine: cross-region connection requested in region view";
        if (m_status)
            m_status->showError(
                QStringLiteral("Cannot create a connection between two different regions in region view."),
                3000);
        return nullptr;
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
        if (!sTerm || !eTerm)
        {
            qCWarning(lcGuiView)
                << "createConnectionLine: global terminal has no linked regional terminal";
            if (m_status)
                m_status->showError(
                    QStringLiteral("Cannot create a route for an unbound global terminal."),
                    3000);
            return nullptr;
        }
        qCDebug(lcGuiView) << "createConnectionLine: Case3 sRegion=" << sTerm->getRegion() << "eRegion=" << eTerm->getRegion();
        if (sTerm->getRegion() == eTerm->getRegion())
        {
            qCWarning(lcGuiView)
                << "createConnectionLine: same-region global terminals are invalid";
            if (m_status)
                m_status->showError(
                    QStringLiteral("Cannot link terminals in the same region in global map."),
                    3000);
            return nullptr;
        }

        const QString sId = sTerm->getTerminalId();
        const QString eId = eTerm->getTerminalId();
        auto &controller =
            CargoNetSim::CargoNetSimController::getInstance();
        Backend::Application::RouteAuthoringService routeAuthoringService(
            &controller);
        qCDebug(lcGuiView) << "createConnectionLine: Case3 diff-region sId=" << sId << "eId=" << eId;
        if (sId.isEmpty() || eId.isEmpty())
        {
            qCWarning(lcGuiView)
                << "createConnectionLine: global endpoint is unbound"
                << "sId=" << sId << "eId=" << eId;
            if (m_status)
                m_status->showError(
                    QStringLiteral("Cannot create a global route for an unbound terminal."),
                    3000);
            return nullptr;
        }
        if (auto *existingLine =
                Scenario::ConnectionLineFactory::findGlobalLink(
                    m_globalMapScene, sId, eId, connectionType))
        {
            return existingLine;
        }
        if (auto *existing = doc->findGlobalLink(
                sId, eId, connectionType))
        {
            return Scenario::ConnectionLineFactory::fromGlobalLink(
                doc, existing, m_globalMapScene, m_mainWindow);
        }
        if (checkExistingConnection(startItem, endItem, connectionType))
        {
            qCDebug(lcGuiView)
                << "createConnectionLine: visual global link already exists in reverse direction";
            return nullptr;
        }
        QVariantMap routeProperties;
        const auto propertyResult =
            routeAuthoringService
                .computeEndpointCanonicalRouteProperties(
                    *doc, sId, eId, connectionType);
        if (!propertyResult.succeeded())
        {
            if (m_status)
                m_status->showError(
                    propertyResult.message.isEmpty()
                        ? QStringLiteral(
                              "Failed to compute connection metrics.")
                        : propertyResult.message,
                    3000);
            return nullptr;
        }
        routeProperties = propertyResult.canonicalProperties;
        const auto createResult =
            routeAuthoringService.createGlobalLink(
                *doc, sId, eId, connectionType,
                routeProperties,
                Backend::Scenario::LinkageSource::Manual);
        if (!createResult.succeeded())
        {
            m_status->showError(
                createResult.message.isEmpty()
                    ? QStringLiteral("Failed to create connection.")
                    : createResult.message,
                3000);
            return nullptr;
        }
        auto *found = Scenario::ConnectionLineFactory::
            findGlobalLink(m_globalMapScene, sId, eId,
                           connectionType);
        if (found)
            applyCanonicalPropertiesToLine(found, routeProperties);
        qCDebug(lcGuiView) << "createConnectionLine: findGlobalLink=" << found;
        return found;
    }

    if (SPG && EPG && SPG == EPG)
    {
        if (m_status)
            m_status->showError(
                "Cannot link a terminal to itself.", 3000);
        return nullptr;
    }

    qCWarning(lcGuiView)
        << "createConnectionLine: unsupported or unbound endpoint combination";
    if (m_status)
        m_status->showError(
            QStringLiteral("Cannot create a route from these endpoints."),
            3000);
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

    if (!m_runtime)
    {
        qCWarning(lcGuiView)
            << "ConnectionController::removeConnectionLine:"
            << "no ScenarioRuntime; refusing scene-only removal";
        if (m_status)
            m_status->showError(
                QStringLiteral("Cannot remove an unbound connection."),
                3000);
        return false;
    }

    try
    {
        if (m_runtime && connectionLine->isConnectionBinding())
        {
            auto *doc = &m_runtime->document();
            auto &controller =
                CargoNetSim::CargoNetSimController::getInstance();
            Backend::Application::RouteAuthoringService routeAuthoringService(
                &controller);
            const auto removeResult =
                routeAuthoringService.removeRoute(
                    *doc,
                    connectionLine->boundFromTerminalId(),
                    connectionLine->boundToTerminalId(),
                    connectionLine->connectionType());
            if (removeResult.succeeded())
                m_status->showMessage(
                    QString("Connection removed successfully."),
                    2000);
            else
                m_status->showError(
                    removeResult.message.isEmpty()
                        ? QStringLiteral("Failed to remove connection.")
                        : removeResult.message,
                    3000);
            return removeResult.succeeded();
        }
        if (m_runtime && connectionLine->isGlobalLinkBinding())
        {
            auto *doc = &m_runtime->document();
            auto &controller =
                CargoNetSim::CargoNetSimController::getInstance();
            Backend::Application::RouteAuthoringService routeAuthoringService(
                &controller);
            const auto removeResult =
                routeAuthoringService.removeRoute(
                    *doc,
                    connectionLine->boundFromTerminalId(),
                    connectionLine->boundToTerminalId(),
                    connectionLine->connectionType());
            if (removeResult.succeeded())
                m_status->showMessage(
                    QString("Connection removed successfully."),
                    2000);
            else
                m_status->showError(
                    removeResult.message.isEmpty()
                        ? QStringLiteral("Failed to remove connection.")
                        : removeResult.message,
                    3000);
            return removeResult.succeeded();
        }

        qCWarning(lcGuiView)
            << "ConnectionController::removeConnectionLine:"
            << "connection line has no backend binding";
        if (m_status)
            m_status->showError(
                QStringLiteral("Cannot remove an unbound connection line."),
                3000);
        return false;
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

    if (!m_runtime)
    {
        qCWarning(lcGuiView)
            << "ConnectionController: no runtime available";
        m_status->showError(
            "No active scenario is available for terminal "
            "connection authoring.",
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

        if (m_mainWindow && m_mainWindow->networkViewService())
        {
            if (!m_mainWindow->networkViewService()
                     ->trainNetworkNames(currentRegion)
                     .isEmpty())
            {
                availableNetworks.insert("Rail");
            }

            if (!m_mainWindow->networkViewService()
                     ->truckNetworkNames(currentRegion)
                     .isEmpty())
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
                auto *networkView =
                    m_mainWindow
                        ? m_mainWindow->networkViewService()
                        : nullptr;

                if (selectedNetworks.contains("Rail")
                    && commonModes.contains(Mode::Train)
                    && networkView
                    && !networkView->trainNetworkNames(currentRegion)
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
                    && networkView
                    && !networkView->truckNetworkNames(currentRegion)
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
    else
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
