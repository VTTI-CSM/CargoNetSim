#include "NormalMode.h"

#include "../../../Backend/Commons/LogCategories.h"
#include "../../../Backend/Controllers/CargoNetSimController.h"
#include "../../../Backend/Controllers/RegionDataController.h"
#include "../../../Backend/Scenario/Connection.h"
#include "../../../Backend/Scenario/ScenarioDocument.h"
#include "../../Items/ConnectionLine.h"
#include "../../Items/TerminalItem.h"
#include "../../MainWindow.h"
#include "../../Widgets/GraphicsScene.h"
#include "../../Widgets/GraphicsView.h"
#include "../ClickContext.h"
#include "../Commands/CommandBus.h"
#include "../Commands/CreateTerminalAtPointCommand.h"
#include "../Commands/DeleteItemCommand.h"
#include "../InteractionController.h"

#include <QDataStream>
#include <QGraphicsItem>
#include <QLoggingCategory>
#include <QMimeData>

namespace CargoNetSim::GUI::Input {

Handled NormalMode::onKeyPress(const KeyPressEvent& e, const ClickContext& ctx)
{
    if (e.key != Qt::Key_Delete && e.key != Qt::Key_Backspace) return Handled::PassThrough;
    if (!ctx.scene) return Handled::PassThrough;

    const QList<QGraphicsItem*> selected = ctx.scene->selectedItems();
    if (selected.isEmpty()) return Handled::PassThrough;

    qCInfo(lcGuiInputMode) << "NormalMode: Delete on" << selected.size() << "items";
    if (!ctx.document) {
        qCWarning(lcGuiInputMode) << "NormalMode: delete requested with null document";
        return Handled::Yes;
    }

    ctx.commandBus->beginMacro(QStringLiteral("Delete %1 item(s)").arg(selected.size()));
    for (QGraphicsItem* item : selected) {
        if (auto* t = dynamic_cast<TerminalItem*>(item)) {
            ctx.commandBus->submit(
                DeleteItemCommand::forTerminal(
                    ctx.document.data(), t->getTerminalId()));
        } else if (auto* c = dynamic_cast<ConnectionLine*>(item)) {
            // Prefer the bound Connection model for the (from,to,mode) key;
            // unbound lines cannot be mapped to a document entity.
            if (auto* conn = c->connectionModel()) {
                ctx.commandBus->submit(
                    DeleteItemCommand::forConnection(
                        ctx.document.data(),
                        conn->fromTerminalId,
                        conn->toTerminalId,
                        conn->mode));
            } else {
                qCWarning(lcGuiInputMode)
                    << "NormalMode::onKeyPress: ConnectionLine has no bound"
                    << "Connection model; cannot delete from document.";
            }
        }
        // Other types added as follow-ups.
    }
    ctx.commandBus->endMacro();
    return Handled::Yes;
}

Handled NormalMode::onDrop(const DropEvent& e, const ClickContext& ctx)
{
    if (!e.mime) return Handled::PassThrough;
    if (!e.mime->hasFormat(QStringLiteral("application/x-qabstractitemmodeldatalist")))
        return Handled::PassThrough;

    if (!ctx.document || !ctx.commandBus || !ctx.view) {
        qCWarning(lcGuiInputMode) << "NormalMode::onDrop: missing document, commandBus, or view";
        return Handled::PassThrough;
    }

    // Parse terminal-type string from the same model-data mime stream the
    // library list uses (pattern preserved from the legacy GraphicsView::dropEvent).
    QByteArray data = e.mime->data(QStringLiteral("application/x-qabstractitemmodeldatalist"));
    QDataStream stream(&data, QIODevice::ReadOnly);
    QString terminalType;
    while (!stream.atEnd()) {
        int row = 0, col = 0, mapItems = 0;
        stream >> row >> col >> mapItems;
        for (int i = 0; i < mapItems; ++i) {
            int key = 0; QVariant value;
            stream >> key >> value;
            if (key == Qt::DisplayRole && terminalType.isEmpty())
                terminalType = value.toString();
        }
    }
    if (terminalType.isEmpty()) {
        qCWarning(lcGuiInputMode) << "NormalMode::onDrop: could not extract terminal type";
        return Handled::PassThrough;
    }

    const QString region =
        CargoNetSimController::getInstance()
            .getRegionDataController()->getCurrentRegion();

    // Same conversion TerminalController::createTerminalAtPoint performs
    // internally (scene-point -> WGS84 lat/lon) so the command sees the
    // identical input domain.
    const QPointF localLatLon = ctx.view->sceneToWGS84(e.scenePos);

    ctx.commandBus->submit(std::make_unique<CreateTerminalAtPointCommand>(
        ctx.document.data(), region, terminalType, localLatLon));
    qCInfo(lcGuiInputMode) << "NormalMode::onDrop: submitted CreateTerminalAtPointCommand"
                           << "type=" << terminalType
                           << "region=" << region
                           << "scenePos=" << e.scenePos;
    return Handled::Yes;
}

Handled NormalMode::onDoubleClick(const DoubleClickEvent& e, const ClickContext& ctx)
{
    if (e.button != Qt::MiddleButton) return Handled::PassThrough;
    if (!ctx.view || !ctx.scene) return Handled::PassThrough;

    qCInfo(lcGuiInputMode) << "NormalMode: middle-dblclick — fit to view";

    QList<TerminalItem*> terminals;
    for (QGraphicsItem* it : ctx.scene->items()) {
        if (auto* t = dynamic_cast<TerminalItem*>(it)) terminals.push_back(t);
    }
    if (!terminals.isEmpty()) {
        QRectF bounds = terminals.first()->sceneBoundingRect();
        for (TerminalItem* t : terminals) bounds = bounds.united(t->sceneBoundingRect());
        bounds.adjust(-50.0, -50.0, 50.0, 50.0);
        ctx.view->fitInView(bounds, Qt::KeepAspectRatio);
    }
    return Handled::Yes;
}

} // namespace CargoNetSim::GUI::Input
