#include "TerminalItem.h"
#include "GlobalTerminalItem.h"

#include "Backend/Commons/LogCategories.h"
#include "Backend/Scenario/TerminalPlacement.h"
#include "Backend/Scenario/TerminalTypeDefaults.h"
#include "Backend/Scenario/ScenarioRuntime.h"
#include "GUI/MainWindow.h"
#include "GUI/Scenario/ScenarioMutator.h"
#include "GUI/Utils/IconCreator.h"
#include "GUI/Widgets/GraphicsView.h"

#include <QApplication>
#include <QCursor>
#include <QGraphicsScene>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QMenu>
#include <QGraphicsView>
#include <QPainter>
#include <QPropertyAnimation>
#include <QStyleOptionGraphicsItem>

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
    qCInfo(lcGuiScene)
        << "TerminalItem::TerminalItem:"
        << "type=" << terminalType
        << "region=" << region
        << "pixmap=" << pixmap.size();

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
    qCInfo(lcGuiScene)
        << "TerminalItem::~TerminalItem:"
        << "type=" << m_terminalType
        << "name=" << m_properties.value("Name").toString();
}

void TerminalItem::initializeDefaultProperties()
{
    qCDebug(lcGuiScene)
        << "TerminalItem::initializeDefaultProperties:"
        << "type=" << m_terminalType
        << "region=" << m_region;

    // Per-type defaults are owned by Backend's single source of truth; we
    // only override the per-instance Name (suffixed with the type-counter
    // ID). Any divergence here would re-create the bag/field split this
    // refactor closes — do not duplicate the per-type branching logic from
    // TerminalTypeDefaults::defaultProperties.
    const QString typeId = getNewTerminalID(m_terminalType);
    m_properties =
        Backend::Scenario::TerminalTypeDefaults::defaultProperties(
            m_terminalType);
    m_properties[QStringLiteral("Name")] =
        QString("%1%2").arg(m_terminalType).arg(typeId);

    qCDebug(lcGuiScene)
        << "TerminalItem::initializeDefaultProperties:"
        << "name=" << m_properties.value("Name").toString();
}

void TerminalItem::setRegion(const QString &newRegion)
{
    if (m_region != newRegion)
    {
        qCDebug(lcGuiScene)
            << "TerminalItem::setRegion:"
            << "old=" << m_region
            << "new=" << newRegion;
        QString oldRegion = m_region;
        m_region          = newRegion;
        emit regionChanged(newRegion);
    }
}

void TerminalItem::setGlobalTerminalItem(
    GlobalTerminalItem *globalTerminalItem)
{
    qCDebug(lcGuiScene)
        << "TerminalItem::setGlobalTerminalItem:"
        << "name=" << m_properties.value("Name").toString()
        << "hasGlobal=" << (globalTerminalItem != nullptr);
    m_globalTerminalItem = globalTerminalItem;
}

GlobalTerminalItem *TerminalItem::getGlobalTerminalItem() const
{
    return m_globalTerminalItem;
}

void TerminalItem::updateProperties(
    const QMap<QString, QVariant> &newProperties)
{
    qCDebug(lcGuiScene)
        << "TerminalItem::updateProperties:"
        << "name=" << m_properties.value("Name").toString()
        << "count=" << newProperties.size();

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
        qCDebug(lcGuiScene)
            << "TerminalItem::setProperty:"
            << "name=" << m_properties.value("Name").toString()
            << "key=" << key << "value=" << value;
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

void TerminalItem::setPlacement(
    Backend::Scenario::TerminalPlacement *placement)
{
    qCDebug(lcGuiScene)
        << "TerminalItem::setPlacement:"
        << "id=" << (placement ? placement->id : "null");

    m_placement = placement;
    if (m_placement)
    {
        // Update icon when type changes.
        if (m_terminalType != m_placement->type)
        {
            const QPixmap p =
                IconFactory::createTerminalIcons().value(
                    m_placement->type);
            if (!p.isNull())
                m_pixmap = p;
        }
        // Snapshot into local state so getters (including legacy
        // readers that grep m_properties directly) see a consistent
        // view between Task-21 observer deliveries. See the cache-
        // staleness contract in the header.
        m_properties   = m_placement->properties;
        m_region       = m_placement->region;
        m_terminalType = m_placement->type;
    }
    update();
}

QString TerminalItem::getTerminalId() const
{
    return m_placement ? m_placement->id : QString();
}

Backend::Scenario::InterfaceConversion::InterfaceMap
TerminalItem::availableInterfaces() const
{
    using InterfaceMap = Backend::Scenario::InterfaceConversion::InterfaceMap;
    using TerminalInterface = Backend::TerminalTypes::TerminalInterface;

    // Preferred path: bound to a placement. Placement::interfaces holds the
    // typed InterfaceSet (Plan 7 migration); reshape into the backend's
    // InterfaceMap directly. No string round-trip.
    if (m_placement && m_placement->interfaces.isSet)
    {
        InterfaceMap map;
        if (!m_placement->interfaces.landSide.isEmpty())
            map.insert(TerminalInterface::LAND_SIDE,
                       m_placement->interfaces.landSide);
        if (!m_placement->interfaces.seaSide.isEmpty())
            map.insert(TerminalInterface::SEA_SIDE,
                       m_placement->interfaces.seaSide);
        return map;
    }

    // Fallback: placement absent or interfaces.isSet == false. Read the
    // property bag — populated by TerminalTypeDefaults at construction
    // time for legacy pre-scenario items, AND for bound items whose
    // InterfaceSet has isSet=false (type-based defaults live only in the
    // bag on that path). Strings → enums at the boundary.
    const QMap<QString, QVariant> iface =
        m_properties.value(QStringLiteral("Available Interfaces")).toMap();
    return Backend::Scenario::InterfaceConversion::toBackendInterfaces(
        iface.value(QStringLiteral("land_side")).toStringList(),
        iface.value(QStringLiteral("sea_side")).toStringList());
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
    QRectF rect = m_boundingRectValue;
    if (m_placement
        && m_placement->role
               != Backend::Scenario::TerminalPlacement::
                   TerminalRole::Transit)
        rect.adjust(0, -4, 4, 0);
    return rect;
}

void TerminalItem::paint(
    QPainter                       *painter,
    const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    qCDebug(lcGuiScene)
        << "TerminalItem::paint:"
        << "name=" << m_properties.value("Name").toString()
        << "pos=" << pos();

    if (!m_pixmap.isNull())
    {
        int pixmapWidth  = m_pixmap.width();
        int pixmapHeight = m_pixmap.height();
        painter->drawPixmap(-pixmapWidth / 2,
                            -pixmapHeight / 2, m_pixmap);

        // Role badge (top-right corner of icon)
        if (m_placement
            && m_placement->role
                   != Backend::Scenario::TerminalPlacement::
                       TerminalRole::Transit)
        {
            qCDebug(lcGuiScene)
                << "TerminalItem::paint: badge"
                << "role="
                << Backend::Scenario::roleToString(
                       m_placement->role);
            const int badgeSize = 12;
            const int bx = pixmapWidth / 2 - badgeSize + 2;
            const int by = -pixmapHeight / 2 - 2;

            QColor badgeColor;
            QString badgeText;
            if (m_placement->role
                == Backend::Scenario::TerminalPlacement::
                    TerminalRole::Origin)
            {
                badgeColor = QColor(0x4C, 0xAF, 0x50); // green
                badgeText  = QStringLiteral("O");
            }
            else
            {
                badgeColor = QColor(0x21, 0x96, 0xF3); // blue
                badgeText  = QStringLiteral("D");
            }

            painter->setBrush(badgeColor);
            painter->setPen(QPen(Qt::black, 1));
            painter->drawEllipse(bx, by, badgeSize, badgeSize);
            painter->setPen(Qt::white);
            QFont badgeFont(QStringLiteral("Arial"), 7,
                            QFont::Bold);
            painter->setFont(badgeFont);
            painter->drawText(
                QRect(bx, by, badgeSize, badgeSize),
                Qt::AlignCenter, badgeText);
        }
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
    qCDebug(lcGuiScene)
        << "TerminalItem::mousePressEvent:"
        << "name=" << m_properties.value("Name").toString()
        << "button=" << event->button()
        << "scenePos=" << event->scenePos();

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
    qCDebug(lcGuiScene)
        << "TerminalItem::itemChange:"
        << "change=" << change;

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

        // Persist position to backend on drag release
        if (m_placement && !m_placement->id.isEmpty())
        {
            auto *view = scene()->views().isEmpty()
                ? nullptr
                : qobject_cast<CargoNetSim::GUI::GraphicsView *>(
                      scene()->views().first());
            if (view)
            {
                const QPointF latLon =
                    view->sceneToWGS84(pos());
                auto *mw = qobject_cast<
                    CargoNetSim::GUI::MainWindow *>(
                    view->window());
                if (mw && mw->runtime())
                {
                    CargoNetSim::GUI::Scenario::
                        ScenarioMutator::
                            updateTerminalPosition(
                                &mw->runtime()->document(),
                                m_placement->id, latLon);
                    qCDebug(lcGuiScene)
                        << "TerminalItem:"
                        << "persisted position"
                        << m_placement->id
                        << "lat=" << latLon.y()
                        << "lon=" << latLon.x();
                }
            }
        }
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
    qCDebug(lcGuiScene)
        << "TerminalItem::hoverEnterEvent:"
        << "name=" << m_properties.value("Name").toString();
    setCursor(QCursor(Qt::PointingHandCursor));
    QGraphicsObject::hoverEnterEvent(event);
}

void TerminalItem::hoverLeaveEvent(
    QGraphicsSceneHoverEvent *event)
{
    qCDebug(lcGuiScene)
        << "TerminalItem::hoverLeaveEvent:"
        << "name=" << m_properties.value("Name").toString();
    unsetCursor();
    QGraphicsObject::hoverLeaveEvent(event);
}

void TerminalItem::contextMenuEvent(
    QGraphicsSceneContextMenuEvent *event)
{
    if (!m_placement || m_placement->id.isEmpty())
    {
        event->ignore();
        return;
    }

    auto *view = (scene() && !scene()->views().isEmpty())
                     ? qobject_cast<GraphicsView *>(
                           scene()->views().first())
                     : nullptr;
    auto *mw   = view
                   ? qobject_cast<MainWindow *>(view->window())
                   : nullptr;
    if (!mw || !mw->runtime())
    {
        event->ignore();
        return;
    }

    using Role = Backend::Scenario::TerminalPlacement::TerminalRole;
    const Role currentRole = m_placement->role;

    QMenu   menu;
    QAction *originAction =
        menu.addAction(tr("Set as Origin"));
    QAction *destinationAction =
        menu.addAction(tr("Set as Destination"));
    QAction *transitAction =
        menu.addAction(tr("Set as Transit"));
    originAction->setEnabled(currentRole != Role::Origin);
    destinationAction->setEnabled(
        currentRole != Role::Destination);
    transitAction->setEnabled(currentRole != Role::Transit);

    menu.addSeparator();

    QMenu        *changeTypeMenu = menu.addMenu(tr("Change Type"));
    const QString currentType    = m_placement->type;
    for (const QString &type :
         Backend::Scenario::TerminalTypeDefaults::allTypes())
    {
        if (type == currentType) continue;
        changeTypeMenu->addAction(type);
    }

    QAction *selected = menu.exec(event->screenPos());
    if (!selected) { event->accept(); return; }

    if (selected == originAction)
        Scenario::ScenarioMutator::setTerminalRole(
            &mw->runtime()->document(),
            m_placement->id,
            Role::Origin);
    else if (selected == destinationAction)
        Scenario::ScenarioMutator::setTerminalRole(
            &mw->runtime()->document(),
            m_placement->id,
            Role::Destination);
    else if (selected == transitAction)
        Scenario::ScenarioMutator::setTerminalRole(
            &mw->runtime()->document(),
            m_placement->id,
            Role::Transit);
    else
        Scenario::ScenarioMutator::setTerminalType(
            &mw->runtime()->document(),
            m_placement->id,
            selected->text());
    event->accept();
}

QMap<QString, QVariant> TerminalItem::toDict() const
{
    qCDebug(lcGuiScene)
        << "TerminalItem::toDict:"
        << "name=" << m_properties.value("Name").toString()
        << "type=" << m_terminalType;

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
    qCInfo(lcGuiScene)
        << "TerminalItem::fromDict:"
        << "type=" << data.value("terminal_type").toString()
        << "region=" << data.value("region").toString();

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
