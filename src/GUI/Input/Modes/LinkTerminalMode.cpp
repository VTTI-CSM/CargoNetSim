#include "LinkTerminalMode.h"

#include "../../../Backend/Application/NetworkViewService.h"
#include "../../../Backend/Commons/LogCategories.h"
#include "../../../Backend/Scenario/LinkageSource.h"
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
    if (!m_firstTerminal) {
        qCWarning(lcGuiInputMode)
            << "LinkTerminalMode: selected terminal was deleted before linking";
        ctx.controller->setMode<NormalMode>();
        return Handled::Yes;
    }

    qCInfo(lcGuiInputMode) << "LinkTerminalMode: linking terminal"
                           << m_firstTerminal.data() << "to MapPoint" << mp;

    // Resolve canonical (networkName, nodeId) tuple the same way
    // TerminalController does for auto-link on drag-drop.
    Backend::Application::NetworkViewService networkView;
    const QString networkName =
        networkView.networkNameOf(mp->getReferenceNetwork());
    const int nodeId = mp->getReferencedNetworkNodeID().toInt();

    const QString terminalId = m_firstTerminal->getTerminalId();
    if (!ctx.document || !ctx.commandBus || networkName.isEmpty()
        || terminalId.isEmpty()) {
        qCWarning(lcGuiInputMode)
            << "LinkTerminalMode: missing backend binding;"
            << "refusing view-only link"
            << "networkName=" << networkName
            << "terminalId=" << terminalId
            << "hasDocument=" << static_cast<bool>(ctx.document)
            << "hasCommandBus=" << (ctx.commandBus != nullptr);
        if (auto* sb = ctx.mainWindow->statusBar())
            sb->showMessage(
                QStringLiteral("Cannot link an unbound terminal or network node."),
                3000);
        ctx.controller->setMode<NormalMode>();
        return Handled::Yes;
    }

    const bool submitted =
        ctx.commandBus->submit(std::make_unique<LinkTerminalToMapPointCommand>(
            ctx.document.data(),
            terminalId,
            networkName,
            nodeId,
            Backend::Scenario::LinkageSource::Manual));
    if (submitted) {
        // Keep the MapPoint's direct terminal pointer in sync immediately
        // after the authoritative document mutation. The document linkage
        // remains the source of truth for undo/redo and file persistence.
        UtilitiesFunctions::linkMapPointToTerminal(
            ctx.mainWindow, mp, m_firstTerminal.data());
    } else if (auto* sb = ctx.mainWindow->statusBar()) {
        sb->showMessage(QStringLiteral("Failed to link terminal to node."), 3000);
    }

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
