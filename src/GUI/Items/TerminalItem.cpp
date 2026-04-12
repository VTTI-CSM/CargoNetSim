#include "TerminalItem.h"

#include <QApplication>
#include <QCursor>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QPainter>
#include <QPropertyAnimation>
#include <QStyleOptionGraphicsItem>
#include <containerLib/container.h>

namespace CargoNetSim
{
namespace GUI
{

// Initialize static variables
QMap<QString, int> TerminalItem::TERMINAL_TYPES_IDs;

TerminalItem::TerminalItem(
    const QPixmap                 &pixmap,
    const QMap<QString, QVariant> &properties,
    const QString &region, QGraphicsItem *parent,
    const QString &terminalType)
    : GraphicsObjectBase(parent)
    , m_pixmap(pixmap)
    , m_region(region)
    , m_terminalType(terminalType)
    , m_properties(properties)
    , m_wasSelected(false)
    , m_globalTerminalItem(nullptr)
{
    // Set a higher Z-value for terminals (will be drawn on
    // top)
    setZValue(11);

    // Initialize properties if not provided
    if (properties.isEmpty())
    {
        initializeDefaultProperties();
    }

    // Enable ItemSendsGeometryChanges flag to receive
    // position change notifications
    setFlags(QGraphicsItem::ItemIsSelectable
             | QGraphicsItem::ItemIsMovable
             | QGraphicsItem::ItemSendsGeometryChanges
             | QGraphicsItem::ItemIgnoresTransformations);

    setAcceptHoverEvents(true);

    // Initialize bounding rectangle
    int pixmapWidth  = this->m_pixmap.width();
    int pixmapHeight = this->m_pixmap.height();
    m_boundingRectValue =
        QRectF(-pixmapWidth / 2, -pixmapHeight / 2,
               pixmapWidth, pixmapHeight);
}

TerminalItem::~TerminalItem()
{
}

void TerminalItem::initializeDefaultProperties()
{
    QString typeId = getNewTerminalID(m_terminalType);

    if (m_terminalType == "Origin"
        || m_terminalType == "Destination")
    {
        QMap<QString, QVariant> interfaces;
        QStringList             landSide, seaSide;
        landSide << "Rail" << "Truck";
        seaSide << "Ship";
        interfaces["land_side"] = landSide;
        interfaces["sea_side"]  = seaSide;

        this->m_properties["Name"] =
            QString("%1%2").arg(m_terminalType).arg(typeId);
        this->m_properties["Show on Global Map"] = true;
        this->m_properties["Available Interfaces"] =
            interfaces;
    }
    else
    {
        this->m_properties["Name"] =
            QString("%1%2").arg(m_terminalType).arg(typeId);
        this->m_properties["Region"] = this->m_region;
        this->m_properties["Show on Global Map"] = true;

        QMap<QString, QVariant> cost;
        cost["fixed_fees"]         = "400";
        cost["customs_fees"]       = "100";
        cost["risk_factor"]        = "0.015";
        this->m_properties["cost"] = cost;

        QMap<QString, QVariant> dwellTime;
        QMap<QString, QVariant> parameters;
        parameters["mean"]               = "2880";
        parameters["std_dev"]            = "720";
        dwellTime["method"]              = "normal";
        dwellTime["parameters"]          = parameters;
        this->m_properties["dwell_time"] = dwellTime;

        if (m_terminalType == "Sea Port Terminal"
            || m_terminalType == "Intermodal Land Terminal")
        {
            QMap<QString, QVariant> customs;
            customs["probability"]        = "0.08";
            customs["delay_mean"]         = "48";
            customs["delay_variance"]     = "24";
            this->m_properties["customs"] = customs;

            QMap<QString, QVariant> capacity;
            capacity["max_capacity"]       = 100000;
            capacity["critical_threshold"] = 0.8;
            this->m_properties["capacity"] = capacity;
        }

        // Set interfaces based on terminal type
        QMap<QString, QVariant> interfaces;
        QStringList             landSide, seaSide;

        if (m_terminalType == "Sea Port Terminal")
        {
            landSide << "Truck" << "Rail";
            seaSide << "Ship";
        }
        else if (m_terminalType
                 == "Intermodal Land Terminal")
        {
            landSide << "Truck" << "Rail";
            this->m_properties["Show on Global Map"] =
                false;
        }
        else if (m_terminalType == "Train Stop/Depot")
        {
            landSide << "Rail";
            this->m_properties["Show on Global Map"] =
                false;
        }
        else if (m_terminalType == "Truck Parking")
        {
            landSide << "Truck";
            this->m_properties["Show on Global Map"] =
                false;
        }
        else
        {
            // Default case
            landSide << "Truck";
        }

        interfaces["land_side"] = landSide;
        interfaces["sea_side"]  = seaSide;
        this->m_properties["Available Interfaces"] =
            interfaces;
    }

    if (m_terminalType == "Origin")
    {
        this->m_properties["Containers"] =
            QVariant::fromValue(
                QList<ContainerCore::Container *>());
    }
}

void TerminalItem::setRegion(const QString &newRegion)
{
    if (m_region != newRegion)
    {
        QString oldRegion      = m_region;
        m_region               = newRegion;
        m_properties["Region"] = newRegion;
        emit regionChanged(newRegion);
    }
}

void TerminalItem::setGlobalTerminalItem(
    GlobalTerminalItem *globalTerminalItem)
{
    m_globalTerminalItem = globalTerminalItem;
}

void TerminalItem::updateProperties(
    const QMap<QString, QVariant> &newProperties)
{
    for (auto it = newProperties.constBegin();
         it != newProperties.constEnd(); ++it)
    {
        m_properties[it.key()] = it.value();
    }
    emit propertiesChanged();
}

void TerminalItem::setProperty(const QString  &key,
                               const QVariant &value)
{
    // Check if property exists and is different
    if (!m_properties.contains(key)
        || m_properties[key] != value)
    {
        m_properties[key] = value;
        emit propertyChanged(key, value);

        // If this is a visual property, update the item
        if (key == "Name" || key == "Show on Global Map")
        {
            update();
        }
    }
}

QVariant TerminalItem::getProperty(
    const QString &key, const QVariant &defaultValue) const
{
    return m_properties.value(key, defaultValue);
}

void TerminalItem::resetClassIDs()
{
    TERMINAL_TYPES_IDs.clear();
}

void TerminalItem::setClassIDs(
    const QMap<int, TerminalItem *> &allTerminalsById)
{
    if (allTerminalsById.isEmpty())
    {
        TERMINAL_TYPES_IDs.clear();
        return;
    }

    // Reset TERMINAL_TYPES_IDs by iterating over all
    // terminals
    TERMINAL_TYPES_IDs.clear();
    for (auto terminal : allTerminalsById)
    {
        QString terminalType = terminal->m_terminalType;
        int     terminalId =
            terminal->m_properties["ID"].toInt();

        if (!TERMINAL_TYPES_IDs.contains(terminalType))
        {
            TERMINAL_TYPES_IDs[terminalType] = terminalId;
        }
        else
        {
            TERMINAL_TYPES_IDs[terminalType] =
                qMax(TERMINAL_TYPES_IDs[terminalType],
                     terminalId);
        }
    }

    // Ensure the next ID for each terminal type starts from
    // max_ID + 1
    for (auto it = TERMINAL_TYPES_IDs.begin();
         it != TERMINAL_TYPES_IDs.end(); ++it)
    {
        *it = *it + 1;
    }
}

QString
TerminalItem::getNewTerminalID(const QString &terminalType)
{

    int value = TERMINAL_TYPES_IDs.value(terminalType, 0);
    value++;
    TERMINAL_TYPES_IDs[terminalType] = value;
    return QString::number(value);
}

QRectF TerminalItem::boundingRect() const
{
    return m_boundingRectValue;
}

void TerminalItem::paint(
    QPainter                       *painter,
    const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    if (!m_pixmap.isNull())
    {
        int pixmapWidth  = m_pixmap.width();
        int pixmapHeight = m_pixmap.height();
        painter->drawPixmap(-pixmapWidth / 2,
                            -pixmapHeight / 2, m_pixmap);
    }

    if (option->state & QStyle::State_Selected)
    {
        QPen pen(Qt::red, 2, Qt::DashLine);
        painter->setPen(pen);
        painter->drawRect(boundingRect());
    }
}

void TerminalItem::mousePressEvent(
    QGraphicsSceneMouseEvent *event)
{
    // Store the initial click position relative to the
    // item's origin
    m_dragOffset = event->pos();

    // Emit clicked signal
    emit clicked(this);

    // Call base class to handle selection
    QGraphicsObject::mousePressEvent(event);
}

QVariant TerminalItem::itemChange(GraphicsItemChange change,
                                  const QVariant    &value)
{
    if (change == ItemPositionChange && scene())
    {
        // If this is a position change and we have a drag
        // offset, adjust the position
        if (m_dragOffset != QPointF()
            && scene()->mouseGrabberItem() == this)
        {
            // Get the proposed new position
            QPointF newPos = value.toPointF();

            // Get current mouse position in scene
            // coordinates
            if (scene()->mouseGrabberItem() == this)
            {
                QGraphicsView *view =
                    scene()->views().first();
                QPointF mousePos = view->mapToScene(
                    view->mapFromGlobal(QCursor::pos()));

                auto newPos = mousePos - m_dragOffset;

                // Adjust position to keep item under the
                // mouse at the right offset
                return newPos;
            }
        }
    }
    else if (change == ItemPositionHasChanged && scene())
    {
        // Emit position changed signal when position has
        // been changed
        emit positionChanged(pos());
    }
    else if (change == ItemSelectedChange)
    {
        bool selected = value.toBool();
        if (selected != m_wasSelected)
        {
            m_wasSelected = selected;
            emit selectionChanged(selected);
        }
    }

    return QGraphicsObject::itemChange(change, value);
}

void TerminalItem::hoverEnterEvent(
    QGraphicsSceneHoverEvent *event)
{
    setCursor(QCursor(Qt::PointingHandCursor));
    QGraphicsObject::hoverEnterEvent(event);
}

void TerminalItem::hoverLeaveEvent(
    QGraphicsSceneHoverEvent *event)
{
    unsetCursor();
    QGraphicsObject::hoverLeaveEvent(event);
}

QMap<QString, QVariant> TerminalItem::toDict() const
{
    QMap<QString, QVariant> data;

    // Store position
    QMap<QString, QVariant> posMap;
    posMap["x"]      = pos().x();
    posMap["y"]      = pos().y();
    data["position"] = posMap;

    // Store other properties
    data["terminal_type"] = m_terminalType;
    data["region"]        = m_region;
    data["properties"]    = m_properties;
    data["selected"]      = isSelected();
    data["visible"]       = isVisible();
    data["z_value"]       = zValue();

    return data;
}

TerminalItem *
TerminalItem::fromDict(const QMap<QString, QVariant> &data,
                       const QPixmap &pixmap,
                       QGraphicsItem *parent)
{
    // Create new instance with essential data
    TerminalItem *instance =
        new TerminalItem(pixmap, data["properties"].toMap(),
                         data["region"].toString(), parent,
                         data["terminal_type"].toString());

    // Set position
    QMap<QString, QVariant> posMap =
        data["position"].toMap();
    QPointF pos(posMap.value("x", 0).toDouble(),
                posMap.value("y", 0).toDouble());
    instance->setPos(pos);

    // Set other properties
    instance->setSelected(
        data.value("selected", false).toBool());
    instance->setVisible(
        data.value("visible", true).toBool());
    instance->setZValue(
        data.value("z_value", 11).toDouble());

    return instance;
}

} // namespace GUI
} // namespace CargoNetSim
