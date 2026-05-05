#include "GlobalTerminalItem.h"
#include "Backend/Commons/LogCategories.h"
#include "GUI/Input/ClickContext.h"
#include "GUI/Input/Commands/CreateGlobalLinkCommand.h"
#include "TerminalItem.h"

#include "Backend/GuiApi/ScenarioDocumentApi.h"

#include <QCursor>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QLoggingCategory>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

namespace CargoNetSim
{
namespace GUI
{

GlobalTerminalItem::GlobalTerminalItem(
    const QPixmap &pixmap, TerminalItem *terminalItem,
    QGraphicsItem *parent)
    : GraphicsObjectBase(parent)
    , originalPixmap(pixmap)
    , linkedTerminalItem(terminalItem)
{
    qCInfo(lcGuiScene)
        << "GlobalTerminalItem::GlobalTerminalItem:"
        << "linked=" << (terminalItem ? "yes" : "no")
        << "pixmap=" << pixmap.size();

    // Enable hover events
    setAcceptHoverEvents(true);

    // Enable position change notification
    setFlags(QGraphicsItem::ItemSendsGeometryChanges
             | QGraphicsItem::ItemIgnoresTransformations
             | QGraphicsItem::ItemIsSelectable);

    // Scale down the pixmap for global map display (24x24)
    scaledPixmap =
        originalPixmap.scaled(24, 24, Qt::KeepAspectRatio,
                              Qt::SmoothTransformation);

    // Set tooltip with terminal name if available
    updateFromLinkedTerminal();
}

void GlobalTerminalItem::setLinkedTerminalItem(
    TerminalItem *terminalItem)
{
    if (linkedTerminalItem != terminalItem)
    {
        qCDebug(lcGuiScene)
            << "GlobalTerminalItem::setLinkedTerminalItem:"
            << "old=" << (linkedTerminalItem ? "valid" : "null")
            << "new=" << (terminalItem ? "valid" : "null");

        TerminalItem *oldTerminal = linkedTerminalItem;
        linkedTerminalItem        = terminalItem;

        updateFromLinkedTerminal();

        // Emit signal about terminal change
        emit linkedTerminalChanged(oldTerminal,
                                   linkedTerminalItem);
    }
}

QString GlobalTerminalItem::getTerminalId() const
{
    return linkedTerminalItem ? linkedTerminalItem->getTerminalId()
                              : QString();
}

std::unique_ptr<QUndoCommand> GlobalTerminalItem::createConnectCommandTo(
    const GraphicsObjectBase*                        other,
    Backend::TransportationTypes::TransportationMode mode,
    Backend::Scenario::ScenarioDocument*             doc) const
{
    // GlobalLink lives between two GlobalTerminalItems. A TerminalItem
    // partner is rejected — region Connections are a different entity,
    // produced by TerminalItem::createConnectCommandTo.
    auto* otherGlobal = dynamic_cast<const GlobalTerminalItem*>(other);
    if (!otherGlobal || !doc) return nullptr;
    const QString fromId = getTerminalId();
    const QString toId   = otherGlobal->getTerminalId();
    if (fromId.isEmpty() || toId.isEmpty()) return nullptr;
    return std::make_unique<Input::CreateGlobalLinkCommand>(
        doc, fromId, toId, mode);
}

Backend::Scenario::InterfaceConversion::InterfaceMap
GlobalTerminalItem::availableInterfaces() const
{
    return linkedTerminalItem
               ? linkedTerminalItem->availableInterfaces()
               : Backend::Scenario::InterfaceConversion::InterfaceMap{};
}

void GlobalTerminalItem::updateFromLinkedTerminal()
{
    qCDebug(lcGuiScene)
        << "GlobalTerminalItem::updateFromLinkedTerminal:"
        << "linked=" << (linkedTerminalItem != nullptr);

    if (linkedTerminalItem)
    {
        QString terminalName =
            linkedTerminalItem
                ->getProperty(QStringLiteral("Name"),
                              QStringLiteral("Terminal"));
        if (terminalName.isEmpty())
            terminalName = QStringLiteral("Terminal");

        setToolTip(terminalName);

        const QPixmap terminalPixmap =
            linkedTerminalItem->getPixmap();
        if (!terminalPixmap.isNull()
            && terminalPixmap.cacheKey()
                   != originalPixmap.cacheKey())
        {
            originalPixmap = terminalPixmap;
            scaledPixmap   = originalPixmap.scaled(
                24, 24, Qt::KeepAspectRatio,
                Qt::SmoothTransformation);
            update(); // Trigger repaint
        }
    }
    else
    {
        setToolTip("Terminal");
    }
}

QRectF GlobalTerminalItem::boundingRect() const
{
    // Center the bounds on the pos
    return QRectF(-scaledPixmap.width() / 2,
                  -scaledPixmap.height() / 2,
                  scaledPixmap.width(),
                  scaledPixmap.height());
}

void GlobalTerminalItem::paint(
    QPainter                       *painter,
    const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    qCDebug(lcGuiScene)
        << "GlobalTerminalItem::paint:"
        << "pos=" << pos()
        << "tooltip=" << toolTip();

    // Draw the scaled pixmap centered on the pos
    painter->drawPixmap(-scaledPixmap.width() / 2,
                        -scaledPixmap.height() / 2,
                        scaledPixmap);

    // Draw selection outline if selected
    if (option->state & QStyle::State_Selected)
    {
        QPen pen(Qt::red, 1, Qt::DashLine);
        painter->setPen(pen);
        painter->drawRect(boundingRect());
    }
}

void GlobalTerminalItem::onDragEnd(const QPointF           &finalPos,
                                   const Input::ClickContext &)
{
    qCDebug(lcGuiInputItem)
        << "GlobalTerminalItem::onDragEnd:" << finalPos;
    emit positionChanged(finalPos);
}

QMap<QString, QVariant> GlobalTerminalItem::toDict() const
{
    qCDebug(lcGuiScene)
        << "GlobalTerminalItem::toDict:"
        << "tooltip=" << toolTip();

    QMap<QString, QVariant> data;

    // Store position
    QMap<QString, QVariant> posMap;
    posMap["x"]      = pos().x();
    posMap["y"]      = pos().y();
    data["position"] = posMap;

    // Store z-value, visibility and selection state
    data["z_value"]  = zValue();
    data["visible"]  = isVisible();
    data["selected"] = isSelected();
    data["tooltip"]  = toolTip();

    // Store linked terminal ID if available
    if (linkedTerminalItem)
    {
        QMap<QString, QVariant> props =
            linkedTerminalItem->property("properties")
                .toMap();
        data["linked_terminal_id"] =
            props.value("ID", QVariant());
    }

    return data;
}

GlobalTerminalItem *GlobalTerminalItem::fromDict(
    const QMap<QString, QVariant> &data,
    const QPixmap &pixmap, QGraphicsItem *parent)
{
    qCInfo(lcGuiScene)
        << "GlobalTerminalItem::fromDict:"
        << "tooltip=" << data.value("tooltip").toString();

    // Create new instance with pixmap
    GlobalTerminalItem *instance =
        new GlobalTerminalItem(pixmap, nullptr, parent);

    // Set position if available
    if (data.contains("position"))
    {
        QMap<QString, QVariant> posMap =
            data["position"].toMap();
        QPointF pos(posMap.value("x", 0).toDouble(),
                    posMap.value("y", 0).toDouble());
        instance->setPos(pos);
    }

    // Set other properties
    instance->setZValue(
        data.value("z_value", 0).toDouble());
    instance->setVisible(
        data.value("visible", true).toBool());
    instance->setSelected(
        data.value("selected", false).toBool());
    instance->setToolTip(
        data.value("tooltip", "Terminal").toString());

    // Note: The linked terminal needs to be set separately
    // after all terminals have been created, using the
    // "linked_terminal_id" stored in the data

    return instance;
}

Input::Handled GlobalTerminalItem::onLeftClick(
    const Input::ClickContext &)
{
    qCDebug(lcGuiInputItem)
        << "GlobalTerminalItem::onLeftClick; linked ="
        << linkedTerminalItem.data();
    return Input::Handled::PassThrough;
}

} // namespace GUI
} // namespace CargoNetSim
