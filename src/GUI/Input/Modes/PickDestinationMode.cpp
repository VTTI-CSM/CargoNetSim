#include "PickDestinationMode.h"

#include "../../../Backend/Commons/LogCategories.h"
#include "../../Items/GlobalTerminalItem.h"
#include "../../Items/MapPoint.h"
#include "../../Items/TerminalItem.h"
#include "../../MainWindow.h"
#include "../ClickContext.h"
#include "../InteractionController.h"
#include "../Modes/NormalMode.h"

#include <QGraphicsItem>
#include <QLoggingCategory>
#include <QStatusBar>

namespace CargoNetSim::GUI::Input {

namespace {

// Attempt to extract (id, name) from the item under the click. Returns true on success.
bool extractTerminal(QGraphicsItem* item, QString& outId, QString& outName)
{
    if (!item) return false;
    if (auto* t = dynamic_cast<TerminalItem*>(item)) {
        outId   = t->getTerminalId();
        outName = t->getProperty("Name").toString();
        if (outName.isEmpty()) outName = outId;
        return !outId.isEmpty();
    }
    if (auto* g = dynamic_cast<GlobalTerminalItem*>(item)) {
        outId = g->getTerminalId();
        if (auto* linked = g->getLinkedTerminalItem()) {
            outName = linked->getProperty("Name").toString();
        }
        if (outName.isEmpty()) outName = outId;
        return !outId.isEmpty();
    }
    if (auto* mp = dynamic_cast<MapPoint*>(item)) {
        if (auto* linked = mp->getLinkedTerminal()) {
            outId   = linked->getTerminalId();
            outName = linked->getProperty("Name").toString();
            if (outName.isEmpty()) outName = outId;
            return !outId.isEmpty();
        }
    }
    return false;
}

} // anonymous namespace

void PickDestinationMode::onEnter(const ClickContext& ctx)
{
    qCInfo(lcGuiInputMode) << "PickDestinationMode::onEnter";
    if (ctx.mainWindow) {
        if (auto* sb = ctx.mainWindow->statusBar()) {
            sb->showMessage(QStringLiteral("Click a terminal to pick as destination (Esc to cancel)."));
        }
    }
}

void PickDestinationMode::onExit(const ClickContext& ctx)
{
    qCInfo(lcGuiInputMode) << "PickDestinationMode::onExit";
    if (ctx.mainWindow) {
        if (auto* sb = ctx.mainWindow->statusBar()) {
            sb->clearMessage();
        }
    }
}

Handled PickDestinationMode::onPress(const PressEvent& e, const ClickContext& ctx)
{
    if (e.button != Qt::LeftButton) return Handled::PassThrough;

    QString id, name;
    for (QGraphicsItem* item : ctx.itemsUnderCursor) {
        if (extractTerminal(item, id, name)) break;
    }
    if (id.isEmpty()) {
        qCDebug(lcGuiInputMode) << "PickDestinationMode: no terminal under cursor";
        return Handled::Yes;   // consume but do not exit; user must click on a terminal
    }

    qCInfo(lcGuiInputMode) << "PickDestinationMode: picked" << id << name;
    emit ctx.controller->destinationPicked(id, name);
    ctx.controller->setMode<NormalMode>();
    return Handled::Yes;
}

Handled PickDestinationMode::onKeyPress(const KeyPressEvent& e, const ClickContext& ctx)
{
    if (e.key == Qt::Key_Escape) {
        qCDebug(lcGuiInputMode) << "PickDestinationMode: Escape — cancel";
        ctx.controller->setMode<NormalMode>();
        return Handled::Yes;
    }
    return Handled::PassThrough;
}

} // namespace CargoNetSim::GUI::Input
