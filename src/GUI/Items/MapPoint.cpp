#include "MapPoint.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Scenario/LinkageSource.h"
#include "Backend/Scenario/NetworkLookup.h"
#include "Backend/Scenario/NodeLinkage.h"
#include "GUI/Controllers/UtilityFunctions.h"
#include "GUI/Controllers/TerminalController.h"
#include "GUI/Input/ClickContext.h"
#include "GUI/Input/Commands/CommandBus.h"
#include "GUI/Input/Commands/CreateTerminalAtMapPointCommand.h"
#include "GUI/Input/Commands/UnlinkTerminalCommand.h"
#include "GUI/Input/InteractionController.h"
#include "Backend/Scenario/ScenarioDocument.h"
#include "GUI/MainWindow.h"
#include "GUI/Scenario/ScenarioMutator.h"
#include "GUI/Widgets/GraphicsView.h"
#include "TerminalItem.h"

#include <QAction>
#include <QApplication>
#include <QCursor>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QLoggingCategory>
#include <QMenu>
#include <QPainter>
#include <QPointer>
#include <QStyle>
#include <QStyleOption>

namespace CargoNetSim
{
namespace GUI
{

int MapPoint::POINT_ID = 0;
MapPoint::MapPoint(
    const QString &referencedNetworkID,
    QPointF sceneCoordinates, const QString &region,
    const QString &shape, TerminalItem *terminal,
    const QMap<QString, QVariant> &properties)
    : m_id(POINT_ID++)
    , m_sceneCoordinate(sceneCoordinates)
    , m_shape(shape)
    , m_terminal(terminal)
    , m_color(Qt::black)
    , m_properties(properties)
{
    qCInfo(lcGuiScene)
        << "MapPoint::MapPoint:"
        << "id=" << m_id
        << "networkID=" << referencedNetworkID
        << "region=" << region
        << "shape=" << shape
        << "pos=" << sceneCoordinates
        << "hasTerminal=" << (terminal != nullptr);

    // Override properties if they were given differently
    // or initialize them if they are missing
    this->m_properties["x"] = m_sceneCoordinate.x();
    this->m_properties["y"] = m_sceneCoordinate.y();
    this->m_properties["Network_ID"] = referencedNetworkID;
    this->m_properties["region"]     = region;

    // Set higher z-value to ensure points are drawn above
    // lines
    setZValue(10);

    // Update terminal link state
    setLinkedTerminal(terminal);

    // Enable selection and hover events
    setFlags(QGraphicsItem::ItemIsSelectable
             | QGraphicsItem::ItemIgnoresTransformations);
    setAcceptHoverEvents(true);
}

void MapPoint::setLinkedTerminal(TerminalItem *newTerminal)
{
    qCDebug(lcGuiScene)
        << "MapPoint::setLinkedTerminal:"
        << "id=" << m_id
        << "old=" << (m_terminal ? "valid" : "null")
        << "new=" << (newTerminal ? "valid" : "null");

    TerminalItem *oldTerminal = m_terminal;
    m_terminal                  = newTerminal;

    if (m_terminal)
    {
        m_properties["LinkedTerminal"] =
            m_terminal->getProperties()["ID"];
    }
    else
    {
        m_properties.remove("LinkedTerminal");
    }

    // Emit signal about the terminal change
    emit terminalChanged(oldTerminal, m_terminal);

    // Trigger a repaint
    update();
}

void MapPoint::setColor(const QColor &newColor)
{
    if (m_color != newColor)
    {
        qCDebug(lcGuiScene)
            << "MapPoint::setColor:"
            << "id=" << m_id
            << "color=" << newColor.name();
        m_color = newColor;
        emit colorChanged(m_color);
        update();
    }
}

void MapPoint::updateProperties(
    const QMap<QString, QVariant> &newProperties)
{
    qCDebug(lcGuiScene)
        << "MapPoint::updateProperties:"
        << "id=" << m_id
        << "count=" << newProperties.size();

    for (auto it = newProperties.constBegin();
         it != newProperties.constEnd(); ++it)
    {
        m_properties[it.key()] = it.value();
    }
    emit propertiesChanged();
}

void MapPoint::setSceneCoordinate(const QPointF &newPos)
{
    qCDebug(lcGuiScene)
        << "MapPoint::setSceneCoordinate:"
        << "id=" << m_id
        << "pos=" << newPos;
    m_sceneCoordinate = newPos;
    setPos(newPos);
    emit positionChanged(newPos);
}

QRectF MapPoint::boundingRect() const
{
    if (m_terminal)
    {
        const QPixmap &pixmap = m_terminal->getPixmap();
        if (!pixmap.isNull())
        {
            int pixmapWidth  = pixmap.width();
            int pixmapHeight = pixmap.height();
            return QRectF(-pixmapWidth / 2,
                          -pixmapHeight / 2, pixmapWidth,
                          pixmapHeight);
        }
    }

    // Default size for shapes
    return QRectF(-7, -7, 14, 14);
}

void MapPoint::paint(QPainter *painter,
                     const QStyleOptionGraphicsItem *option,
                     QWidget                        *widget)
{
    qCDebug(lcGuiScene)
        << "MapPoint::paint:"
        << "id=" << m_id
        << "shape=" << m_shape
        << "hasTerminal=" << (m_terminal != nullptr);

    if (m_terminal)
    {
        // If linked to a m_terminal, draw m_terminal icon at
        // reduced opacity
        const QPixmap &pixmap = m_terminal->getPixmap();
        if (!pixmap.isNull())
        {
            painter->setOpacity(
                0.7); // Slightly transparent to show it's a
                      // link
            int pixmapWidth  = pixmap.width();
            int pixmapHeight = pixmap.height();
            painter->drawPixmap(-pixmapWidth / 2,
                                -pixmapHeight / 2, pixmap);
            painter->setOpacity(1.0);
        }
    }
    else
    {
        // Draw a shape if no terminal
        painter->setPen(QPen(Qt::black, 2));
        painter->setBrush(QBrush(m_color));

        if (m_shape == "circle")
        {
            painter->drawEllipse(-7.0, -7.0, 14.0, 14.0);
        }
        else if (m_shape == "rectangle")
        {
            painter->drawRect(-7.0, -7.0, 14, 14);
        }
        else if (m_shape == "triangle")
        {
            QPainterPath path;
            path.moveTo(0, -7);
            path.lineTo(7, 7);
            path.lineTo(-7, 7);
            path.lineTo(0, -7);
            painter->drawPath(path);
        }
    }

    setPos(m_sceneCoordinate);

    // Draw selection indicator
    if (option->state & QStyle::State_Selected)
    {
        painter->setPen(QPen(Qt::blue, 2, Qt::DashLine));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(boundingRect());
    }
}

Input::Handled MapPoint::onLeftClick(const Input::ClickContext&)
{
    qCDebug(lcGuiInputItem)
        << "MapPoint::onLeftClick; id=" << m_id
        << "networkNodeID=" << getReferencedNetworkNodeID()
        << "region=" << getRegion();
    return Input::Handled::PassThrough;
}

void MapPoint::onHoverEnter(const Input::ClickContext&)
{
    qCDebug(lcGuiInputItem) << "MapPoint::onHoverEnter; id=" << m_id;
    update();
}

void MapPoint::onHoverLeave(const Input::ClickContext&)
{
    qCDebug(lcGuiInputItem) << "MapPoint::onHoverLeave; id=" << m_id;
    update();
}

void MapPoint::buildContextMenu(QMenu* menu, const Input::ClickContext& ctx)
{
    Q_ASSERT(menu);
    qCDebug(lcGuiInputItem)
        << "MapPoint::buildContextMenu; id=" << m_id
        << "hasTerminal=" << (m_terminal != nullptr);

    QPointer<MapPoint>                              self = this;
    QPointer<MainWindow>                            mw   = ctx.mainWindow;
    QPointer<Backend::Scenario::ScenarioDocument>   doc  = ctx.document;

    using Role = Backend::Scenario::TerminalPlacement::TerminalRole;

    Input::CommandBus *createBus = ctx.commandBus;

    // --- "Create Terminal at Node" submenu ---
    QMenu *createTerminalMenu =
        menu->addMenu(QStringLiteral("Create Terminal at Node"));

    QAction *originAction =
        createTerminalMenu->addAction(QStringLiteral("Origin"));
    QObject::connect(originAction, &QAction::triggered,
                     [self, doc, createBus]() {
        if (!self) return;
        self->createTerminalAtPosition(QStringLiteral("Facility"),
                                       Role::Origin,
                                       doc.data(), createBus);
    });

    QAction *destinationAction =
        createTerminalMenu->addAction(QStringLiteral("Destination"));
    QObject::connect(destinationAction, &QAction::triggered,
                     [self, doc, createBus]() {
        if (!self) return;
        self->createTerminalAtPosition(QStringLiteral("Facility"),
                                       Role::Destination,
                                       doc.data(), createBus);
    });

    createTerminalMenu->addSeparator();

    QAction *seaTerminalAction =
        createTerminalMenu->addAction(QStringLiteral("Sea Terminal"));
    QObject::connect(seaTerminalAction, &QAction::triggered,
                     [self, doc, createBus]() {
        if (!self) return;
        self->createTerminalAtPosition(QStringLiteral("Sea Port Terminal"),
                                       Role::Transit,
                                       doc.data(), createBus);
    });

    QAction *intermodalTerminalAction =
        createTerminalMenu->addAction(QStringLiteral("Intermodal Terminal"));
    QObject::connect(intermodalTerminalAction, &QAction::triggered,
                     [self, doc, createBus]() {
        if (!self) return;
        self->createTerminalAtPosition(
            QStringLiteral("Intermodal Land Terminal"),
            Role::Transit,
            doc.data(), createBus);
    });

    QAction *trainDepotAction =
        createTerminalMenu->addAction(QStringLiteral("Train Depot"));
    QObject::connect(trainDepotAction, &QAction::triggered,
                     [self, doc, createBus]() {
        if (!self) return;
        self->createTerminalAtPosition(QStringLiteral("Train Stop/Depot"),
                                       Role::Transit,
                                       doc.data(), createBus);
    });

    QAction *parkingAction =
        createTerminalMenu->addAction(QStringLiteral("Parking"));
    QObject::connect(parkingAction, &QAction::triggered,
                     [self, doc, createBus]() {
        if (!self) return;
        self->createTerminalAtPosition(QStringLiteral("Truck Parking"),
                                       Role::Transit,
                                       doc.data(), createBus);
    });

    QAction *facilityAction =
        createTerminalMenu->addAction(QStringLiteral("Facility"));
    QObject::connect(facilityAction, &QAction::triggered,
                     [self, doc, createBus]() {
        if (!self) return;
        self->createTerminalAtPosition(QStringLiteral("Facility"),
                                       Role::Transit,
                                       doc.data(), createBus);
    });

    // --- "Configure as Origin…" ---
    QAction *configureOriginAction =
        menu->addAction(QStringLiteral("Configure as Origin…"));
    configureOriginAction->setEnabled(m_terminal != nullptr);
    QObject::connect(configureOriginAction, &QAction::triggered,
                     [self, mw]() {
        if (!self || !mw) return;
        TerminalItem *term = self->getLinkedTerminal();
        if (!term) return;
        UtilitiesFunctions::updatePropertiesPanel(mw.data(), term);
    });

    // --- "Unlink Terminal" ---
    QAction *unlinkAction =
        menu->addAction(QStringLiteral("Unlink Terminal"));
    unlinkAction->setEnabled(m_terminal != nullptr);
    Input::CommandBus *bus = ctx.commandBus;
    QObject::connect(unlinkAction, &QAction::triggered,
                     [self, doc, bus]() {
        if (!self) return;

        // Prefer the bound NodeLinkage which carries the canonical tuple.
        Backend::Scenario::NodeLinkage *linkage = self->linkageModel();
        if (doc && bus && linkage)
        {
            bus->submit(std::make_unique<Input::UnlinkTerminalCommand>(
                doc.data(),
                linkage->terminalId,
                linkage->networkName,
                linkage->nodeId));
        }
        else
        {
            // Legacy path: no linkage bound — no canonical (networkName,
            // nodeId) pair available on MapPoint (only a Network_ID string
            // whose mapping to networkName lives on the owning Network).
            // Fall back to the view-only clear so the UI stays consistent.
            qCWarning(lcGuiInputItem)
                << "MapPoint::buildContextMenu[Unlink]: no linkage model"
                << " or command bus bound; performing view-only unlink."
                << " id=" << self->m_id;
        }

        // View clear so the point visually detaches regardless of path.
        self->setLinkedTerminal(nullptr);
    });
}

void MapPoint::createTerminalAtPosition(
    const QString &terminalType,
    Backend::Scenario::TerminalPlacement::TerminalRole role,
    Backend::Scenario::ScenarioDocument *doc,
    Input::CommandBus                   *bus)
{
    qCInfo(lcGuiScene)
        << "MapPoint::createTerminalAtPosition:"
        << "id=" << m_id
        << "terminalType=" << terminalType
        << "role=" << Backend::Scenario::roleToString(role);

    MainWindow *mainWindow =
        qobject_cast<MainWindow *>(scene()->parent());

    if (!mainWindow)
    {
        qCWarning(lcGuiScene)
            << "MapPoint::createTerminalAtPosition:"
            << "no MainWindow";
        return;
    }

    const QString region =
        m_properties.value("region", "Default Region").toString();
    const QString networkName =
        Backend::Scenario::NetworkLookup::networkNameOf(m_referenceNetwork);
    const int     nodeId      = getReferencedNetworkNodeID().toInt();

    // Preferred path: submit the composite command so creation + linkage
    // are both persisted in the document and reversible via undo.
    if (doc && bus && !networkName.isEmpty())
    {
        // Resolve the local lat/lon the same way TerminalController does
        // when creating terminals at a scene point.
        QPointF latLon;
        if (auto *view = mainWindow->regionView())
        {
            latLon = view->sceneToWGS84(pos());
        }

        bus->submit(std::make_unique<Input::CreateTerminalAtMapPointCommand>(
            doc, networkName, nodeId, terminalType, region, latLon, role));
        qCInfo(lcGuiScene)
            << "MapPoint::createTerminalAtPosition: submitted"
            << "CreateTerminalAtMapPointCommand"
            << "network=" << networkName
            << "node="    << nodeId
            << "type="    << terminalType
            << "region="  << region
            << "latLon="  << latLon;
        return;
    }

    // Fallback: legacy view-only path when the input context could not
    // supply the document/bus or the network name is not resolvable.
    qCWarning(lcGuiScene)
        << "MapPoint::createTerminalAtPosition: legacy view-only path"
        << "(doc=" << (doc ? "ok" : "null")
        << ", bus=" << (bus ? "ok" : "null")
        << ", networkName=" << networkName << ")";

    TerminalItem *newTerminal =
        mainWindow->terminalCtrl()->createTerminalAtPoint(
            region, terminalType, pos(), role);

    // Link the newly created terminal to this map point
    if (newTerminal)
    {
        UtilitiesFunctions::linkMapPointToTerminal(
            mainWindow, this, newTerminal);
    }
}

QMap<QString, QVariant> MapPoint::toDict() const
{
    qCDebug(lcGuiScene)
        << "MapPoint::toDict:"
        << "id=" << m_id;

    QMap<QString, QVariant> data;

    data["referenced_network_ID"] =
        m_properties.value("Network_ID");
    data["x"]          = m_sceneCoordinate.x();
    data["y"]          = m_sceneCoordinate.y();
    data["shape"]      = m_shape;
    data["properties"] = m_properties;
    data["color"]      = m_color.name();
    data["selected"]   = isSelected();
    data["z_value"]    = zValue();

    // Add terminal ID if there's a linked terminal
    if (m_terminal)
    {
        data["terminal_id"] =
            m_terminal->getProperties().value("ID");
    }

    return data;
}

MapPoint *MapPoint::fromDict(
    const QMap<QString, QVariant>   &data,
    const QMap<int, TerminalItem *> &terminalsById)
{
    qCInfo(lcGuiScene)
        << "MapPoint::fromDict:"
        << "networkID=" << data.value("referenced_network_ID").toString()
        << "shape=" << data.value("shape").toString();

    // Find linked terminal by ID if terminals dictionary is
    // provided
    TerminalItem *terminal = nullptr;
    if (data.contains("terminal_id")
        && !terminalsById.isEmpty())
    {
        int terminalId = data["terminal_id"].toInt();
        terminal       = terminalsById.value(terminalId);
    }

    MapPoint *instance = new MapPoint(
        data.value("referenced_network_ID").toString(),
        QPointF(data.value("x").toDouble(),
                data.value("y").toDouble()),
        data.value("shape", "circle").toString(),
        data.value("region", "default").toString(),
        terminal, data.value("properties").toMap());

    // Set color
    instance->setColor(
        QColor(data.value("color", "#000000").toString()));

    // Set other properties
    instance->setSelected(
        data.value("selected", false).toBool());
    instance->setZValue(
        data.value("z_value", 10).toDouble());

    return instance;
}

} // namespace GUI
} // namespace CargoNetSim
