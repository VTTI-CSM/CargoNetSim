#include "GlobalPositionMode.h"

#include "../../../Backend/Commons/LogCategories.h"
#include "../../../Backend/Scenario/ScenarioDocument.h"
#include "../../Controllers/TerminalController.h"
#include "../../Items/GlobalTerminalItem.h"
#include "../../Items/TerminalItem.h"
#include "../../MainWindow.h"
#include "../../Widgets/GraphicsView.h"
#include "../../Widgets/SetCoordinatesDialog.h"
#include "../ClickContext.h"
#include "../Commands/CommandBus.h"
#include "../Commands/UpdateTerminalPositionCommand.h"
#include "../InteractionController.h"
#include "../Modes/NormalMode.h"

#include <QDialog>
#include <QGraphicsItem>
#include <QLoggingCategory>
#include <QStatusBar>

namespace CargoNetSim::GUI::Input {

void GlobalPositionMode::onEnter(const ClickContext& ctx)
{
    qCInfo(lcGuiInputMode) << "GlobalPositionMode::onEnter";
    if (ctx.mainWindow) {
        if (auto* sb = ctx.mainWindow->statusBar()) {
            sb->showMessage(QStringLiteral("Click a global terminal to set its position (Esc to cancel)."));
        }
    }
}

void GlobalPositionMode::onExit(const ClickContext& ctx)
{
    qCInfo(lcGuiInputMode) << "GlobalPositionMode::onExit";
    if (ctx.mainWindow) {
        if (auto* sb = ctx.mainWindow->statusBar()) {
            sb->clearMessage();
        }
    }
}

Handled GlobalPositionMode::onPress(const PressEvent& e, const ClickContext& ctx)
{
    if (e.button != Qt::LeftButton) return Handled::PassThrough;

    GlobalTerminalItem* gItem = nullptr;
    for (QGraphicsItem* item : ctx.itemsUnderCursor) {
        if (auto* g = dynamic_cast<GlobalTerminalItem*>(item)) {
            if (g->getLinkedTerminalItem()) { gItem = g; break; }
        }
    }
    if (!gItem) {
        qCDebug(lcGuiInputMode) << "GlobalPositionMode: no linked GlobalTerminalItem at" << e.scenePos;
        return Handled::Yes;   // swallow; keep mode active
    }

    TerminalItem* terminal = gItem->getLinkedTerminalItem();
    if (!terminal || !ctx.mainWindow || !ctx.mainWindow->globalMapView()) {
        qCWarning(lcGuiInputMode) << "GlobalPositionMode: missing mainWindow/view/terminal";
        ctx.controller->setMode<NormalMode>();
        return Handled::Yes;
    }

    // Current global position as WGS84 (lon/lat).
    const QPointF globalGeoPos =
        ctx.mainWindow->globalMapView()->sceneToWGS84(gItem->pos());

    // Show the coordinate entry dialog inline — this is UI, not mutation.
    SetCoordinatesDialog dialog(
        terminal->getProperties().value("Name").toString(),
        globalGeoPos, ctx.mainWindow);
    if (dialog.exec() != QDialog::Accepted) {
        qCInfo(lcGuiInputMode) << "GlobalPositionMode: dialog cancelled";
        ctx.controller->setMode<NormalMode>();
        return Handled::Yes;
    }

    // Convert user-entered global WGS84 → terminal's region-local lat/lon
    // and submit as a reversible command on the undo stack.
    const QPointF userGeoPoint = dialog.getCoordinates();
    const QPointF newLocal     = ctx.mainWindow->terminalCtrl()
                                     ->globalToLocalLatLon(
                                         terminal->getRegion(), userGeoPoint);
    if (ctx.document && ctx.commandBus) {
        ctx.commandBus->submit(std::make_unique<UpdateTerminalPositionCommand>(
            ctx.document.data(), terminal->getTerminalId(), newLocal));
        qCInfo(lcGuiInputMode)
            << "GlobalPositionMode: submitted UpdateTerminalPositionCommand"
            << "id=" << terminal->getTerminalId()
            << "newLocal=" << newLocal;
    } else {
        qCWarning(lcGuiInputMode)
            << "GlobalPositionMode: null document or commandBus; skipping submit";
    }

    ctx.controller->setMode<NormalMode>();
    return Handled::Yes;
}

Handled GlobalPositionMode::onKeyPress(const KeyPressEvent& e, const ClickContext& ctx)
{
    if (e.key == Qt::Key_Escape) {
        qCDebug(lcGuiInputMode) << "GlobalPositionMode: Escape — cancel";
        ctx.controller->setMode<NormalMode>();
        return Handled::Yes;
    }
    return Handled::PassThrough;
}

} // namespace CargoNetSim::GUI::Input
