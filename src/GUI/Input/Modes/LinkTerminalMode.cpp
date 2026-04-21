#include "LinkTerminalMode.h"

#include "../../../Backend/Commons/LogCategories.h"
#include "../../../Backend/Scenario/LinkageSource.h"
#include "../../../Backend/Scenario/NetworkLookup.h"
#include "../../Controllers/UtilityFunctions.h"
#include "../../Items/MapPoint.h"
#include "../../Items/TerminalItem.h"
#include "../../MainWindow.h"
#include "../ClickContext.h"
#include "../Commands/CommandBus.h"
#include "../Commands/LinkTerminalToMapPointCommand.h"
#include "../InteractionController.h"
#include "../Modes/NormalMode.h"

#include <QGraphicsItem>
#include <QLoggingCategory>
#include <QStatusBar>

namespace CargoNetSim::GUI::Input {

void LinkTerminalMode::onEnter(const ClickContext& ctx)
{
    qCInfo(lcGuiInputMode) << "LinkTerminalMode::onEnter";
    if (ctx.mainWindow) {
        if (auto* sb = ctx.mainWindow->statusBar()) {
            sb->showMessage(QStringLiteral("Click a terminal, then a network node to link."));
        }
    }
}

void LinkTerminalMode::onExit(const ClickContext& ctx)
{
    qCInfo(lcGuiInputMode) << "LinkTerminalMode::onExit";
    m_firstTerminal = nullptr;
    if (ctx.mainWindow) {
        if (auto* sb = ctx.mainWindow->statusBar()) {
            sb->clearMessage();
        }
    }
}

Handled LinkTerminalMode::onPress(const PressEvent& e, const ClickContext& ctx)
{
    if (e.button != Qt::LeftButton) return Handled::PassThrough;

    if (!m_firstTerminal) {
        // First click — must be a TerminalItem.
        TerminalItem* t = nullptr;
        for (QGraphicsItem* item : ctx.itemsUnderCursor) {
            if (auto* ti = dynamic_cast<TerminalItem*>(item)) { t = ti; break; }
        }
        if (!t) {
            qCDebug(lcGuiInputMode) << "LinkTerminalMode: first click missed terminal";
            return Handled::Reject;
        }
        m_firstTerminal = t;
        if (ctx.mainWindow) {
            if (auto* sb = ctx.mainWindow->statusBar()) {
                sb->showMessage(QStringLiteral("Now click a network node (MapPoint) to link."));
            }
        }
        qCInfo(lcGuiInputMode) << "LinkTerminalMode: first terminal selected" << t;
        return Handled::Yes;
    }

    // Second click — must be a MapPoint.
    MapPoint* mp = nullptr;
    for (QGraphicsItem* item : ctx.itemsUnderCursor) {
        if (auto* p = dynamic_cast<MapPoint*>(item)) { mp = p; break; }
    }
    if (!mp) {
        qCDebug(lcGuiInputMode) << "LinkTerminalMode: second click missed MapPoint";
        return Handled::Reject;
    }

    if (!ctx.mainWindow) {
        qCWarning(lcGuiInputMode) << "LinkTerminalMode: null mainWindow";
        ctx.controller->setMode<NormalMode>();
        return Handled::Yes;
    }

    qCInfo(lcGuiInputMode) << "LinkTerminalMode: linking terminal"
                           << m_firstTerminal.data() << "to MapPoint" << mp;

    // Resolve canonical (networkName, nodeId) tuple the same way
    // TerminalController does for auto-link on drag-drop.
    const QString networkName = Backend::Scenario::NetworkLookup::networkNameOf(
        mp->getReferenceNetwork());
    const int nodeId = mp->getReferencedNetworkNodeID().toInt();

    if (!ctx.document || !ctx.commandBus || networkName.isEmpty()) {
        qCWarning(lcGuiInputMode)
            << "LinkTerminalMode: missing document, commandBus, or networkName;"
            << "falling back to view-only link"
            << "networkName=" << networkName;
        // Best-effort: still update the view so the click has a visible result.
        UtilitiesFunctions::linkMapPointToTerminal(
            ctx.mainWindow, mp, m_firstTerminal.data());
        ctx.controller->setMode<NormalMode>();
        return Handled::Yes;
    }

    // Preserve view-side linkage (icon / properties-panel) — the mutation
    // below drives document state which in turn drives observers, but the
    // MapPoint's direct pointer to TerminalItem is a legacy view cache that
    // factories don't re-wire on linkageAdded today.
    UtilitiesFunctions::linkMapPointToTerminal(
        ctx.mainWindow, mp, m_firstTerminal.data());

    ctx.commandBus->submit(std::make_unique<LinkTerminalToMapPointCommand>(
        ctx.document.data(),
        m_firstTerminal->getTerminalId(),
        networkName,
        nodeId,
        Backend::Scenario::LinkageSource::Manual));

    ctx.controller->setMode<NormalMode>();
    return Handled::Yes;
}

Handled LinkTerminalMode::onKeyPress(const KeyPressEvent& e, const ClickContext& ctx)
{
    if (e.key == Qt::Key_Escape) {
        qCDebug(lcGuiInputMode) << "LinkTerminalMode: Escape — cancel";
        ctx.controller->setMode<NormalMode>();
        return Handled::Yes;
    }
    return Handled::PassThrough;
}

} // namespace CargoNetSim::GUI::Input
