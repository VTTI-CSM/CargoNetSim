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

    // Mutate the document via command when a canonical linkage is bound.
    if (ctx.document && ctx.commandBus) {
        if (auto* linkage = mp->linkageModel()) {
            ctx.commandBus->submit(std::make_unique<UnlinkTerminalCommand>(
                ctx.document.data(),
                linkage->terminalId,
                linkage->networkName,
                linkage->nodeId));
        } else {
            qCWarning(lcGuiInputMode)
                << "UnlinkTerminalMode: MapPoint has no bound NodeLinkage;"
                << "performing view-only unlink.";
        }
    }

    mp->setLinkedTerminal(nullptr);
    mp->update();

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
