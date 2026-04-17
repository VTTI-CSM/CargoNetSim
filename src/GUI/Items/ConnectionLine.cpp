#include "ConnectionLine.h"
#include "../Items/GlobalTerminalItem.h"
#include "../Items/TerminalItem.h"
#include "ConnectionLabel.h"

#include "AnimationObject.h"
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QPainter>
#include <QPropertyAnimation>
#include <QStyleOptionGraphicsItem>
#include <cmath>
#include "Backend/Commons/LogCategories.h"
#include "Backend/Scenario/Connection.h"
#include "Backend/Scenario/GlobalLink.h"

namespace CargoNetSim
{
namespace GUI
{

// Initialize static members
int ConnectionLine::CONNECTION_LINE_ID = 0;

// Define connection type styles with valid pen styles. Keyed by the
// strongly-typed `TransportationMode` enum — no case-mismatch between
// display-form ("Truck") and data-form ("truck") is possible.
const QMap<Backend::TransportationTypes::TransportationMode,
           QMap<QString, QVariant>>
    ConnectionLine::CONNECTION_STYLES = {
        {Backend::TransportationTypes::TransportationMode::Truck,
         {{"color", QColor(Qt::magenta)},
          {"width", 5},
          {"style", static_cast<int>(Qt::SolidLine)},
          {"offset", 0}}},
        {Backend::TransportationTypes::TransportationMode::Train,
         {{"color", QColor(Qt::darkGray)},
          {"width", 5},
          {"style", static_cast<int>(Qt::SolidLine)},
          {"offset", 0}}},
        {Backend::TransportationTypes::TransportationMode::Ship,
         {{"color", QColor(Qt::blue)},
          {"width", 5},
          {"style", static_cast<int>(Qt::SolidLine)},
          {"offset", 0}}}};

ConnectionLine::ConnectionLine(
    QGraphicsItem *startItem, QGraphicsItem *endItem,
    Backend::TransportationTypes::TransportationMode connectionType,
    const QMap<QString, QVariant> &properties,
    const QString &region, QGraphicsItem *parent)
    : GraphicsObjectBase(parent)
    , m_startItem(startItem)
    , m_endItem(endItem)
    , m_connectionType(connectionType)
    , m_properties(properties)
    , m_id(getNewConnectionID())
    , m_isHovered(false)
{
    qCDebug(lcGuiScene) << "ConnectionLine::ConnectionLine:"
                        << "id=" << m_id
                        << "type=" << Backend::transportationModeToString(m_connectionType);
    // Set higher z-value to ensure visibility
    setZValue(4);

    // Enable hover events
    setAcceptHoverEvents(true);

    // Set flags
    setFlags(QGraphicsItem::ItemIsSelectable);

    // Initialize properties if needed
    if (m_properties.isEmpty())
    {
        initializeProperties(region);
    }
    else
    {
        m_properties["Region"] = region;
    }

    // Create label. Label text is the YAML-canonical first letter —
    // "t" / "r" / "s" uppercased → "T" / "R" / "S". Distinct across
    // the three active modes (Truck / Train→"rail" / Ship).
    m_label = new ConnectionLabel(this);
    m_label->setText(
        Backend::transportationModeToString(m_connectionType)
            .toUpper()
            .left(1));

    // Set label color based on connection type
    QColor color = CONNECTION_STYLES.value(m_connectionType)
                       .value("color")
                       .value<QColor>();
    m_label->setColor(color);

    // Connect signals
    createConnections();

    // Initialize position and geometry
    updatePosition();
}

ConnectionLine::~ConnectionLine()
{
    // Label is automatically deleted as a child item
}

void ConnectionLine::setRegion(const QString &region)
{
    if (m_properties["Region"].toString() != region)
    {
        m_properties["Region"] = region;
        emit regionChanged(region);
    }
}

void ConnectionLine::setConnectionType(
    Backend::TransportationTypes::TransportationMode type)
{
    if (!CONNECTION_STYLES.contains(type))
    {
        qCWarning(lcGuiScene) << "ConnectionLine::setConnectionType:"
                              << "invalid type" << static_cast<int>(type);
        return;
    }
    if (m_connectionType != type)
    {
        m_connectionType                = type;
        m_properties["Connection type"] =
            Backend::transportationModeToString(type);

        // Update label
        m_label->setText(
            Backend::transportationModeToString(type)
                .toUpper()
                .left(1));
        QColor color = CONNECTION_STYLES.value(type)
                           .value("color")
                           .value<QColor>();
        m_label->setColor(color);

        // Update geometry and redraw
        updatePosition();
        update();

        emit connectionTypeChanged(type);
    }
}

void ConnectionLine::setProperty(const QString  &key,
                                 const QVariant &value)
{
    if (m_properties.value(key) != value)
    {
        m_properties[key] = value;
        update(); // Redraw if necessary
        emit propertyChanged(key, value);
    }
}

void ConnectionLine::createConnections()
{
    qCDebug(lcGuiScene) << "ConnectionLine::createConnections:" << "id=" << m_id;
    // Connect to start and end items' position changes
    if (dynamic_cast<TerminalItem *>(m_startItem))
    {
        connect(
            dynamic_cast<TerminalItem *>(m_startItem),
            &TerminalItem::positionChanged, this,
            &ConnectionLine::onStartItemPositionChanged);
    }
    else if (dynamic_cast<GlobalTerminalItem *>(
                 m_startItem))
    {
        connect(
            dynamic_cast<GlobalTerminalItem *>(m_startItem),
            &GlobalTerminalItem::positionChanged, this,
            &ConnectionLine::onStartItemPositionChanged);
    }

    if (dynamic_cast<TerminalItem *>(m_endItem))
    {
        connect(dynamic_cast<TerminalItem *>(m_endItem),
                &TerminalItem::positionChanged, this,
                &ConnectionLine::onEndItemPositionChanged);
    }
    else if (dynamic_cast<GlobalTerminalItem *>(m_endItem))
    {
        connect(
            dynamic_cast<GlobalTerminalItem *>(m_endItem),
            &GlobalTerminalItem::positionChanged, this,
            &ConnectionLine::onEndItemPositionChanged);
    }

    // Connect label's clicked signal to our clicked signal
    connect(m_label, &ConnectionLabel::clicked,
            [this]() { emit clicked(this); });
}

void ConnectionLine::initializeProperties(QString region)
{
    qCDebug(lcGuiScene) << "ConnectionLine::initializeProperties:"
                        << "id=" << m_id << "region=" << region;
    m_properties = {
        {"Type", "Connection"},
        {"Connection type",
         Backend::transportationModeToString(m_connectionType)},
        {"Region", region},
        {"cost", "0.0"},             // USD
        {"travelTime", "0.0"},       // Hours
        {"distance", "0.0"},         // Km
        {"carbonEmissions", "0.0"},  // ton CO2
        {"risk", "0.0"},             // percentage [0-100]
        {"energyConsumption", "0.0"} // kWh
    };
}

void ConnectionLine::onStartItemPositionChanged(
    const QPointF &newPos)
{
    updatePosition(newPos, true);
    emit startPositionChanged(newPos);
}

void ConnectionLine::onEndItemPositionChanged(
    const QPointF &newPos)
{
    updatePosition(newPos, false);
    emit endPositionChanged(newPos);
}

void ConnectionLine::updatePosition(const QPointF &newPos,
                                    bool           isStart)
{
    prepareGeometryChange();

    // Calculate terminal centers
    QPointF startCenter;
    QPointF endCenter;

    if (isStart && !newPos.isNull())
    {
        startCenter = newPos;
        endCenter = m_endItem->scenePos();
    }
    else if (!isStart && !newPos.isNull())
    {
        startCenter = m_startItem->scenePos();
        endCenter = newPos;
    }
    else
    {
        startCenter = m_startItem->scenePos();
        endCenter = m_endItem->scenePos();
    }

    // Create base line and apply offset
    QLineF baseLine(startCenter, endCenter);
    m_line = calculateOffsetLine(baseLine);

    // Calculate midpoint and line properties
    qreal midX       = (m_line.x1() + m_line.x2()) / 2;
    qreal midY       = (m_line.y1() + m_line.y2()) / 2;
    qreal dx         = m_line.x2() - m_line.x1();
    qreal dy         = m_line.y2() - m_line.y1();
    qreal lineLength = std::sqrt(dx * dx + dy * dy);

    // Default control point and label position
    qreal ctrlX = midX, ctrlY = midY;
    qreal labelX = midX, labelY = midY;

    if (lineLength > 0)
    {
        // Determine curve direction based on line orientation
        if (m_connectionType != Backend::TransportationTypes::TransportationMode::Truck)
        {
            // Choose perpendicular offset direction
            bool  isVertical = std::abs(dy) > std::abs(dx);
            qreal offset     = 30; // Control point offset

            if (isVertical)
            {
                // For vertical alignments, curve horizontally
                qreal nx = (m_connectionType == Backend::TransportationTypes::TransportationMode::Ship) ? 1.0 : -1.0;
                qreal ny = 0.0;

                // Set control point
                ctrlX = midX + nx * offset;
                ctrlY = midY + ny * offset;
            }
            else
            {
                // For horizontal alignments, curve vertically
                qreal nx = 0.0;
                qreal ny = (m_connectionType == Backend::TransportationTypes::TransportationMode::Train) ? -1.0 : 1.0;

                // Set control point
                ctrlX = midX + nx * offset;
                ctrlY = midY + ny * offset;
            }

            // Calculate label position at t=0.5 (middle of Bezier curve)
            qreal t = 0.5;
            labelX  = (1 - t) * (1 - t) * m_line.x1()
                     + 2 * (1 - t) * t * ctrlX
                     + t * t * m_line.x2();

            labelY = (1 - t) * (1 - t) * m_line.y1()
                     + 2 * (1 - t) * t * ctrlY
                     + t * t * m_line.y2();
        }
    }

    // Store control points for painting
    m_ctrlPoint = QPointF(ctrlX, ctrlY);

    // Set label position
    m_label->setPos(labelX, labelY);

    // Update bounding rectangle
    int padding = std::max(
        5, CONNECTION_STYLES.value(m_connectionType)
               .value("width")
               .toInt());

    if (m_connectionType == Backend::TransportationTypes::TransportationMode::Truck)
    {
        // Simple rectangle for straight lines
        m_boundingRect = QRectF(
            std::min(m_line.x1(), m_line.x2()) - padding,
            std::min(m_line.y1(), m_line.y2()) - padding,
            std::abs(m_line.x2() - m_line.x1())
                + (2 * padding),
            std::abs(m_line.y2() - m_line.y1())
                + (2 * padding));
    }
    else
    {
        // For curves, include the control point
        QList<QPointF> points = {m_line.p1(), m_ctrlPoint,
                                 m_line.p2()};
        qreal minX = std::numeric_limits<qreal>::max();
        qreal minY = std::numeric_limits<qreal>::max();
        qreal maxX = std::numeric_limits<qreal>::min();
        qreal maxY = std::numeric_limits<qreal>::min();

        for (const QPointF &p : points)
        {
            minX = std::min(minX, p.x());
            minY = std::min(minY, p.y());
            maxX = std::max(maxX, p.x());
            maxY = std::max(maxY, p.y());
        }

        m_boundingRect =
            QRectF(minX - padding, minY - padding,
                   maxX - minX + 2 * padding,
                   maxY - minY + 2 * padding);
    }

    update();
}

QLineF ConnectionLine::calculateOffsetLine(
    const QLineF &originalLine) const
{
    // Get the offset distance for this connection type
    qreal offset = CONNECTION_STYLES.value(m_connectionType)
                       .value("offset")
                       .toDouble();

    if (offset == 0)
    {
        return originalLine;
    }

    // Calculate the perpendicular vector
    qreal dx     = originalLine.dx();
    qreal dy     = originalLine.dy();
    qreal length = std::sqrt(dx * dx + dy * dy);

    if (length == 0)
    {
        qCDebug(lcGuiScene) << "ConnectionLine::calculateOffsetLine:"
                            << "zero-length line, returning original";
        return originalLine;
    }

    // Normalize and rotate 90 degrees
    qreal perpX = -dy / length;
    qreal perpY = dx / length;

    // Create the offset line
    qreal startX = originalLine.x1() + offset * perpX;
    qreal startY = originalLine.y1() + offset * perpY;
    qreal endX   = originalLine.x2() + offset * perpX;
    qreal endY   = originalLine.y2() + offset * perpY;

    return QLineF(startX, startY, endX, endY);
}

QRectF ConnectionLine::boundingRect() const
{
    return m_boundingRect;
}

QPainterPath ConnectionLine::shape() const
{
    // For selection purposes, use a simplified selection
    // area that focuses more on the label and less on the
    // line itself
    QPainterPath path;

    // Add the label's shape (with some padding)
    QRectF labelSceneRect = m_label->sceneBoundingRect();
    QRectF labelLocalRect =
        mapFromScene(labelSceneRect).boundingRect();

    // Add padding around the label for easier selection
    int padding = 10;
    labelLocalRect.adjust(-padding, -padding, padding,
                          padding);
    path.addRect(labelLocalRect);

    // Add a narrow path along the line for selection
    qreal lineWidth = 10; // Narrow selection width

    // Get line control points
    QPointF start = m_line.p1();
    QPointF end   = m_line.p2();

    // Create a narrow rectangle along the line for
    // selection
    if (m_connectionType == Backend::TransportationTypes::TransportationMode::Truck)
    {
        // For straight line
        qreal angle =
            m_line.angle()
            * (M_PI / 180.0); // Convert to radians
        qreal dx = lineWidth * 0.5 * std::sin(angle);
        qreal dy = lineWidth * 0.5 * std::cos(angle);

        QPolygonF poly;
        poly.append(
            QPointF(start.x() + dx, start.y() - dy));
        poly.append(
            QPointF(start.x() - dx, start.y() + dy));
        poly.append(QPointF(end.x() - dx, end.y() + dy));
        poly.append(QPointF(end.x() + dx, end.y() - dy));
        path.addPolygon(poly);
    }
    else
    {
        // For curved lines, use a simplified approach
        qreal midX = (start.x() + end.x()) / 2;
        qreal midY = (start.y() + end.y()) / 2;

        // Add midpoint with larger radius for better
        // selection of curved lines
        path.addEllipse(QPointF(midX, midY), 15, 15);

        // Add small circles at endpoints
        path.addEllipse(start, 5, 5);
        path.addEllipse(end, 5, 5);
    }

    return path;
}

void ConnectionLine::paint(
    QPainter                       *painter,
    const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    // Get style for this connection type
    QMap<QString, QVariant> style =
        CONNECTION_STYLES.value(m_connectionType);

    // Set up pen with appropriate scale
    QGraphicsView *view      = scene()->views().first();
    qreal          viewScale = view->transform().m11();

    // Create pen
    QPen pen(style.value("color").value<QColor>(),
             style.value("width").toInt() / viewScale,
             static_cast<Qt::PenStyle>(
                 style.value("style").toInt()));

    painter->setPen(pen);

    // Draw the path
    QPainterPath path;
    path.moveTo(m_line.p1());

    if (m_connectionType == Backend::TransportationTypes::TransportationMode::Truck)
    {
        path.lineTo(m_line.p2());
    }
    else
    {
        path.quadTo(m_ctrlPoint, m_line.p2());
    }

    painter->drawPath(path);

    // Draw debug bounding rect if needed
    if (false)
    { // Set to true for debugging
        QPen debugPen(QColor(0, 255, 0), 1, Qt::DashLine);
        debugPen.setCosmetic(true);
        painter->setPen(debugPen);
        painter->drawRect(boundingRect());
    }
}

void ConnectionLine::updateProperties(
    const QMap<QString, QVariant> &newProperties)
{
    qCDebug(lcGuiScene) << "ConnectionLine::updateProperties:"
                        << "id=" << m_id
                        << "propertyCount=" << newProperties.size();
    for (auto it = newProperties.constBegin();
         it != newProperties.constEnd(); ++it)
    {
        m_properties[it.key()] = it.value();
    }
    emit propertiesChanged();
}

bool ConnectionLine::isSelected() const
{
    return m_label->isSelected();
}

void ConnectionLine::setSelected(bool selected)
{
    // Set both custom and Qt selection
    QGraphicsItem::setSelected(selected);
    m_label->setSelected(selected);
    m_label->update();
}

void ConnectionLine::mousePressEvent(
    QGraphicsSceneMouseEvent *event)
{
    // Prevent selection by ignoring the event
    event->ignore();
}

void ConnectionLine::hoverEnterEvent(
    QGraphicsSceneHoverEvent *event)
{
    m_isHovered = true;
    update();
    QGraphicsObject::hoverEnterEvent(event);
}

void ConnectionLine::hoverLeaveEvent(
    QGraphicsSceneHoverEvent *event)
{
    m_isHovered = false;
    update();
    QGraphicsObject::hoverLeaveEvent(event);
}

QMap<QString, QVariant> ConnectionLine::toDict() const
{
    qCDebug(lcGuiScene) << "ConnectionLine::toDict:" << "id=" << m_id;
    QMap<QString, QVariant> data;

    // Store IDs for serialization
    int     startItemId = -1;
    int     endItemId   = -1;
    QString startItemType;
    QString endItemType;

    // Extract IDs from terminal items
    if (TerminalItem *terminal =
            dynamic_cast<TerminalItem *>(m_startItem))
    {
        startItemId =
            terminal->getProperties().value("ID").toInt();
        startItemType = "TerminalItem";
    }
    else if (GlobalTerminalItem *globalTerminal =
                 dynamic_cast<GlobalTerminalItem *>(
                     m_startItem))
    {
        if (globalTerminal->getLinkedTerminalItem())
        {
            startItemId =
                globalTerminal->getLinkedTerminalItem()
                    ->getProperties()
                    .value("ID")
                    .toInt();
            startItemType = "GlobalTerminalItem";
        }
    }

    if (TerminalItem *terminal =
            dynamic_cast<TerminalItem *>(m_endItem))
    {
        endItemId =
            terminal->getProperties().value("ID").toInt();
        endItemType = "TerminalItem";
    }
    else if (GlobalTerminalItem *globalTerminal =
                 dynamic_cast<GlobalTerminalItem *>(
                     m_endItem))
    {
        if (globalTerminal->getLinkedTerminalItem())
        {
            endItemId =
                globalTerminal->getLinkedTerminalItem()
                    ->getProperties()
                    .value("ID")
                    .toInt();
            endItemType = "GlobalTerminalItem";
        }
    }

    data["id"]              = m_id;
    data["start_item_id"]   = startItemId;
    data["start_item_type"] = startItemType;
    data["end_item_id"]     = endItemId;
    data["end_item_type"]   = endItemType;
    data["connection_type"] =
        Backend::transportationModeToString(m_connectionType);
    data["properties"]      = m_properties;
    data["selected"]        = isSelected();
    data["z_value"]         = zValue();
    data["visible"]         = isVisible();

    // Add label data
    data["label"] = m_label->toDict();

    return data;
}

ConnectionLine *ConnectionLine::fromDict(
    const QMap<QString, QVariant>    &data,
    const QMap<int, QGraphicsItem *> &terminalsByID,
    QGraphicsScene *globalScene, QGraphicsItem *parent)
{
    qCDebug(lcGuiScene) << "ConnectionLine::fromDict:"
                        << "startId=" << data.value("start_item_id")
                        << "endId=" << data.value("end_item_id")
                        << "type=" << data.value("connection_type");
    // Find terminals
    int startId = data["start_item_id"].toInt();
    int endId   = data["end_item_id"].toInt();

    QGraphicsItem *startItem = terminalsByID.value(startId);
    QGraphicsItem *endItem   = terminalsByID.value(endId);

    if (!startItem)
    {
        qCWarning(lcGuiScene) << "Start terminal with ID" << startId
                             << "not found";
        return nullptr;
    }

    if (!endItem)
    {
        qCWarning(lcGuiScene) << "End terminal with ID" << endId
                             << "not found";
        return nullptr;
    }

    // Handle global items if needed
    if (globalScene
        && data["start_item_type"].toString()
               == "GlobalTerminalItem")
    {
        for (QGraphicsItem *item : globalScene->items())
        {
            GlobalTerminalItem *globalItem =
                dynamic_cast<GlobalTerminalItem *>(item);
            if (globalItem
                && globalItem->getLinkedTerminalItem()
                && globalItem->getLinkedTerminalItem()
                           ->getProperties()
                           .value("ID")
                           .toInt()
                       == startId)
            {
                startItem = globalItem;
                break;
            }
        }
    }

    if (globalScene
        && data["end_item_type"].toString()
               == "GlobalTerminalItem")
    {
        for (QGraphicsItem *item : globalScene->items())
        {
            GlobalTerminalItem *globalItem =
                dynamic_cast<GlobalTerminalItem *>(item);
            if (globalItem
                && globalItem->getLinkedTerminalItem()
                && globalItem->getLinkedTerminalItem()
                           ->getProperties()
                           .value("ID")
                           .toInt()
                       == endId)
            {
                endItem = globalItem;
                break;
            }
        }
    }

    // Create connection line. Legacy serialization stored the type
    // as a string; accept either case-form via the case-insensitive
    // `transportationModeFromString` helper.
    ConnectionLine *connection = new ConnectionLine(
        startItem, endItem,
        Backend::transportationModeFromString(
            data.value("connection_type", "truck").toString()),
        data.value("properties").toMap(),
        data.value("region", "Default Region").toString(),
        parent);

    // Set ID
    connection->m_id = data["id"].toInt();

    // Set visual properties
    connection->setSelected(
        data.value("selected", false).toBool());
    connection->setZValue(
        data.value("z_value", 4).toDouble());
    connection->setVisible(
        data.value("visible", true).toBool());

    // Update position
    connection->updatePosition();

    return connection;
}

void ConnectionLine::resetClassIDs()
{
    CONNECTION_LINE_ID = 0;
}

void ConnectionLine::setClassIDs(
    const QMap<int, ConnectionLine *> &allConnectionsById)
{
    if (allConnectionsById.isEmpty())
    {
        CONNECTION_LINE_ID = 0;
        return;
    }

    // Find maximum ID
    int maxId = 0;
    for (ConnectionLine *connection :
         allConnectionsById.values())
    {
        maxId = std::max(maxId, connection->connectionId());
    }

    CONNECTION_LINE_ID = maxId;
}

int ConnectionLine::getNewConnectionID()
{
    CONNECTION_LINE_ID++;
    return CONNECTION_LINE_ID;
}

void ConnectionLine::clearAnimationVisuals()
{
    GraphicsObjectBase::clearAnimationVisuals();
}

void ConnectionLine::createAnimationVisual(
    const QColor &color)
{
    // Create a path item as an overlay to follow the line
    QPainterPath path;
    qreal        penWidth =
        CONNECTION_STYLES.value(m_connectionType)
            .value("width")
            .toInt()
        * 3;

    path.moveTo(m_line.p1());

    if (m_connectionType == Backend::TransportationTypes::TransportationMode::Truck)
    {
        path.lineTo(m_line.p2());
    }
    else
    {
        path.quadTo(m_ctrlPoint, m_line.p2());
    }

    QGraphicsPathItem *overlay = new QGraphicsPathItem();
    overlay->setPath(path);
    overlay->setPen(QPen(color, penWidth, Qt::SolidLine));
    overlay->setZValue(100);

    // Add to scene properly
    overlay->setParentItem(this);

    m_animObject->setOverlay(overlay);
}

bool ConnectionLine::matchesConnection(
    const QString &fromId, const QString &toId,
    Backend::TransportationTypes::TransportationMode mode) const
{
    if (m_connectionType != mode) return false;

    QString sId, eId;

    if (auto *t = dynamic_cast<TerminalItem *>(m_startItem))
        sId = t->getTerminalId();
    else if (auto *g =
                 dynamic_cast<GlobalTerminalItem *>(m_startItem))
        sId = g->getTerminalId();

    if (auto *t = dynamic_cast<TerminalItem *>(m_endItem))
        eId = t->getTerminalId();
    else if (auto *g =
                 dynamic_cast<GlobalTerminalItem *>(m_endItem))
        eId = g->getTerminalId();

    if (sId.isEmpty() || eId.isEmpty()) return false;

    return (sId == fromId && eId == toId)
        || (sId == toId && eId == fromId);
}

void ConnectionLine::refreshFromModel()
{
    qCDebug(lcGuiScene)
        << "ConnectionLine::refreshFromModel:"
        << "id=" << m_id;

    if (m_connection)
    {
        m_properties = m_connection->properties;
        m_properties["Connection type"] =
            Backend::transportationModeToString(
                m_connection->mode);
        m_properties["Region"] = m_connection->region;
    }
    else if (m_global)
    {
        m_properties = m_global->properties;
        m_properties["Connection type"] =
            Backend::transportationModeToString(
                m_global->mode);
    }

    update();
}

} // namespace GUI
} // namespace CargoNetSim
