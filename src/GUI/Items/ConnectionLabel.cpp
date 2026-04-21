#include "ConnectionLabel.h"
#include "Backend/Commons/LogCategories.h"
#include "GUI/Input/ClickContext.h"
#include "GUI/Input/InteractionController.h"
#include "GUI/Items/ConnectionLine.h"

#include <QCursor>
#include <QGraphicsScene>
#include <QLoggingCategory>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

namespace CargoNetSim
{
namespace GUI
{

ConnectionLabel::ConnectionLabel(QGraphicsItem *parent)
    : GraphicsObjectBase(parent)
    , m_text("")
    , m_color(Qt::black)
    , m_isHovered(false)
    , m_isSelected(false)
    , m_boundingRect(-16, -16, 32, 32) // Fixed size 32x32
{
    qCInfo(lcGuiScene)
        << "ConnectionLabel::ConnectionLabel: created";

    // Set flags
    setFlag(QGraphicsItem::ItemIgnoresTransformations);
    setFlag(QGraphicsItem::ItemIsSelectable);

    // Enable hover events
    setAcceptHoverEvents(true);

    // Set Z-value slightly higher than parent connection
    // line
    setZValue(5);
}

void ConnectionLabel::setText(const QString &text)
{
    if (m_text != text)
    {
        qCDebug(lcGuiScene)
            << "ConnectionLabel::setText:"
            << "old=" << m_text << "new=" << text;
        m_text = text;
        update();
        emit textChanged(text);
    }
}

void ConnectionLabel::setColor(const QColor &color)
{
    if (m_color != color)
    {
        qCDebug(lcGuiScene)
            << "ConnectionLabel::setColor:"
            << "color=" << color.name();
        m_color = color;
        update();
        emit colorChanged(color);
    }
}

void ConnectionLabel::setSelected(bool selected)
{
    if (m_isSelected != selected)
    {
        qCDebug(lcGuiScene)
            << "ConnectionLabel::setSelected:"
            << "selected=" << selected;
        m_isSelected = selected;
        update();
        emit selectionChanged(selected);
    }
}

QRectF ConnectionLabel::boundingRect() const
{
    return m_boundingRect;
}

void ConnectionLabel::paint(
    QPainter                       *painter,
    const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    qCDebug(lcGuiScene)
        << "ConnectionLabel::paint:"
        << "text=" << m_text
        << "hovered=" << m_isHovered;

    // Draw label background
    if (m_isHovered)
    {
        painter->setBrush(QBrush(QColor(
            255, 255, 0))); // Light yellow when hovered
    }
    else
    {
        painter->setBrush(QBrush(Qt::white));
    }

    painter->setPen(QPen(Qt::black, 1));
    painter->drawRect(boundingRect());

    // Draw the text
    QFont font = painter->font();
    font.setPointSize(15);
    painter->setFont(font);
    painter->setPen(QPen(m_color));
    painter->drawText(boundingRect(), Qt::AlignCenter,
                      m_text);

    // // Draw selection indicator if selected
    // if (m_isSelected)
    // {
    //     painter->setPen(QPen(Qt::red, 2, Qt::DashLine));
    //     painter->setBrush(Qt::NoBrush);
    //     painter->drawRect(
    //         boundingRect().adjusted(-2, -2, 2, 2));
    // }
}

Input::Handled ConnectionLabel::onLeftClick(
    const Input::ClickContext &ctx)
{
    qCDebug(lcGuiInputItem)
        << "ConnectionLabel::onLeftClick; forwarding to "
           "parent line";
    auto *parentLine =
        dynamic_cast<ConnectionLine *>(parentItem());
    if (!parentLine)
    {
        qCWarning(lcGuiInputItem)
            << "ConnectionLabel: null parent ConnectionLine";
        return Input::Handled::PassThrough;
    }
    if (ctx.controller)
    {
        ctx.controller->selectItem(parentLine,
                                   /*exclusive*/ true);
    }
    return Input::Handled::Yes;
}

void ConnectionLabel::onHoverEnter(
    const Input::ClickContext &)
{
    m_isHovered = true;
    update();
}

void ConnectionLabel::onHoverLeave(
    const Input::ClickContext &)
{
    m_isHovered = false;
    update();
}

QMap<QString, QVariant> ConnectionLabel::toDict() const
{
    qCDebug(lcGuiScene)
        << "ConnectionLabel::toDict:"
        << "text=" << m_text;

    QMap<QString, QVariant> data;

    data["text"]  = m_text;
    data["color"] = m_color.name();

    // Store position
    QMap<QString, QVariant> posMap;
    posMap["x"]      = pos().x();
    posMap["y"]      = pos().y();
    data["position"] = posMap;

    data["z_value"] = zValue();
    data["visible"] = isVisible();

    return data;
}

ConnectionLabel *ConnectionLabel::fromDict(
    const QMap<QString, QVariant> &data,
    QGraphicsItem                 *parent)
{
    qCInfo(lcGuiScene)
        << "ConnectionLabel::fromDict:"
        << "text=" << data.value("text").toString();

    ConnectionLabel *instance = new ConnectionLabel(parent);

    // Set properties
    instance->setText(data.value("text", "").toString());
    instance->setColor(
        QColor(data.value("color", "#000000").toString()));

    // Set position
    QMap<QString, QVariant> posMap =
        data.value("position").toMap();
    QPointF pos(posMap.value("x", 0).toDouble(),
                posMap.value("y", 0).toDouble());
    instance->setPos(pos);

    // Set other properties
    instance->setZValue(
        data.value("z_value", 5).toDouble());
    instance->setVisible(
        data.value("visible", true).toBool());

    return instance;
}

} // namespace GUI
} // namespace CargoNetSim
