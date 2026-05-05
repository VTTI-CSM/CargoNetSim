#include "UnlinkTerminalMode.h"

#include "../../../Backend/Commons/LogCategories.h"
#include "../../../Backend/Scenario/NodeLinkage.h"
#include "../../Items/MapPoint.h"
#include "../../MainWindow.h"
#include "../ClickContext.h"
#include "../Commands/CommandBus.h"
#include "../Commands/UnlinkTerminalCommand.h"
#include "../InteractionController.h"
#include "../Modes/NormalMode.h"

#include <QGraphicsItem>
#include <QLoggingCategory>
#include <QStatusBar>

namespace CargoNetSim::GUI::Input {

void UnlinkTerminalMode::onEnter(const ClickContext& ctx)
{
    qCInfo(lcGuiInputMode) << "UnlinkTerminalMode::onEnter";
    if (ctx.mainWindow) {
        if (auto* sb = ctx.mainWindow->statusBar()) {
            sb->showMessage(QStringLiteral("Click a linked network node to unlink it."));
        }
    }
}

void UnlinkTerminalMode::onExit(const ClickContext& ctx)
{
    qCInfo(lcGuiInputMode) << "UnlinkTerminalMode::onExit";
    if (ctx.mainWindow) {
        if (auto* sb = ctx.mainWindow->statusBar()) {
            sb->clearMessage();
        }
    }
}

Handled UnlinkTerminalMode::onPress(const PressEvent& e, const ClickContext& ctx)
{
    if (e.button != Qt::LeftButton) return Handled::PassThrough;

    MapPoint* mp = nullptr;
    for (QGraphicsItem* item : ctx.itemsUnderCursor) {
        if (auto* p = dynamic_cast<MapPoint*>(item)) {
            if (p->getLinkedTerminal()) { mp = p; break; }
        }
    }
    if (!mp) {
        qCDebug(lcGuiInputMode) << "UnlinkTerminalMode: no linked MapPoint at cursor";
        return Handled::Reject;
    }

    qCInfo(lcGuiInputMode) << "UnlinkTerminalMode: unlinking MapPoint" << mp;

    auto* linkage = mp->linkageModel();
    if (!ctx.document || !ctx.commandBus || !linkage) {
        qCWarning(lcGuiInputMode)
            << "UnlinkTerminalMode: missing backend binding;"
            << "refusing view-only unlink"
            << "hasDocument=" << static_cast<bool>(ctx.document)
            << "hasCommandBus=" << (ctx.commandBus != nullptr)
            << "hasLinkage=" << (linkage != nullptr);
        if (ctx.mainWindow) {
            if (auto* sb = ctx.mainWindow->statusBar()) {
                sb->showMessage(QStringLiteral("Cannot unlink an unbound network node."), 3000);
            }
        }
        ctx.controller->setMode<NormalMode>();
        return Handled::Yes;
    }

    const bool submitted =
        ctx.commandBus->submit(std::make_unique<UnlinkTerminalCommand>(
            ctx.document.data(),
            linkage->terminalId,
            linkage->networkName,
            linkage->nodeId));
    if (submitted) {
        // Immediate view-cache reconciliation after the document mutation.
        mp->setLinkedTerminal(nullptr);
        mp->update();
    } else if (ctx.mainWindow) {
        if (auto* sb = ctx.mainWindow->statusBar()) {
            sb->showMessage(QStringLiteral("Failed to unlink terminal from node."), 3000);
        }
    }

    ctx.controller->setMode<NormalMode>();
    return Handled::Yes;
}

Handled UnlinkTerminalMode::onKeyPress(const KeyPressEvent& e, const ClickContext& ctx)
{
    if (e.key == Qt::Key_Escape) {
        ctx.controller->setMode<NormalMode>();
        return Handled::Yes;
    }
    return Handled::PassThrough;
}

} // namespace CargoNetSim::GUI::Input
