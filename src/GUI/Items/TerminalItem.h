#pragma once

#include "Backend/GuiApi/ScenarioContractsApi.h"
#include "Backend/GuiApi/ScenarioDocumentApi.h"
#include "GraphicsObjectBase.h"
#include "GUI/Input/Interfaces/IClickable.h"
#include "GUI/Input/Interfaces/IContextMenuProvider.h"
#include "GUI/Input/Interfaces/IDraggable.h"
#include "GUI/Input/Interfaces/IHoverable.h"

#include <QCursor>
#include <QGraphicsObject>
#include <QMap>
#include <QMenu>
#include <QPixmap>
#include <QPointF>
#include <QPointer>
#include <QPropertyAnimation>
#include <QString>
#include <QVariant>

namespace CargoNetSim
{
namespace GUI
{

// Forward declarations
class GlobalTerminalItem;

/**
 * @brief Graphical representation of a terminal in the
 * freight network
 *
 * This class represents terminals such as ports, train
 * depots, and truck stops within the CargoNetSim
 * application. Terminals can be dragged, selected, and
 * connected to form a transportation network.
 */
class TerminalItem : public GraphicsObjectBase,
                     public Input::IClickable,
                     public Input::IContextMenuProvider,
                     public Input::IHoverable,
                     public Input::IDraggable
{
    Q_OBJECT
    Q_PROPERTY(QPointF pos READ pos WRITE setPos)

public:
    /// Unique graphics-item type id. Required for qgraphicsitem_cast to
    /// distinguish this class from sibling QGraphicsObject subclasses —
    /// without it all siblings collide on QGraphicsObject::Type and the
    /// cast degenerates to an unchecked static_cast.
    enum { Type = UserType + 1 };
    int type() const override { return Type; }

    /**
     * @brief Construct a new Terminal Item
     *
     * @param pixmap Visual representation of the terminal
     * @param properties Terminal properties as key-value
     * pairs
     * @param region Region the terminal belongs to
     * @param parent Parent graphics item (if any)
     * @param terminalType Type of terminal (e.g.,
     * "Sea Port Terminal", "Intermodal Land Terminal")
     */
    TerminalItem(const QPixmap                 &pixmap,
                 const QMap<QString, QVariant> &properties =
                     QMap<QString, QVariant>(),
                 const QString &region = "Default Region",
                 QGraphicsItem *parent = nullptr,
                 const QString &terminalType = QString());

    /**
     * @brief Destructor
     */
    virtual ~TerminalItem();

    /**
     * @brief Set the terminal's region
     *
     * @param region New region name
     */
    void setRegion(const QString &region);

    /**
     * @brief Set the global terminal item
     * @param globalTerminalItem Pointer to the global
     * terminal item
     */
    void setGlobalTerminalItem(
        GlobalTerminalItem *globalTerminalItem);

    /**
     * @brief Get the global terminal item
     * @return Pointer to the global terminal item
     */
    GlobalTerminalItem *getGlobalTerminalItem() const;

    /**
     * @brief Get the terminal's m_region
     *
     * @return Current m_region name
     */
    QString getRegion() const
    {
        return m_region;
    }

    /**
     * @brief Get the terminal's m_pixmap
     *
     * @return Current m_pixmap
     */
    QPixmap getPixmap() const
    {
        return m_pixmap;
    }

    /**
     * @brief Get the terminal type
     *
     * @return Terminal type string
     */
    QString getTerminalType() const
    {
        return m_terminalType;
    }

    /**
     * @brief Get terminal m_properties
     *
     * @return Reference to m_properties map
     */
    const QMap<QString, QVariant> &getProperties() const
    {
        return m_properties;
    }

    /**
     * @brief Get the terminal's scenario role.
     *
     * Returns the placement's role when bound, or Transit (default) when
     * unbound. Production mutators require a bound placement; the default is
     * only for safe rendering of isolated view/test items.
     */
    Backend::Scenario::TerminalPlacement::TerminalRole
    getRole() const
    {
        using R = Backend::Scenario::TerminalPlacement::TerminalRole;
        return m_placement ? m_placement->role : R::Transit;
    }

    /**
     * @brief Typed view of the terminal's interface modes.
     *
     * When bound to a placement with explicit interfaces, returns the
     * placement's typed `InterfaceSet` reshaped into the backend's InterfaceMap.
     * Otherwise derives the typed defaults from the terminal type.
     *
     * Single source of truth for GUI-side interface queries. Consumers
     * (PathFindingWorker, UtilityFunctions) read enums, never strings.
     * Plan 7 added this accessor to collapse five duplicated extractions.
     */
    Backend::Scenario::InterfaceConversion::InterfaceMap
    availableInterfaces() const;

    /**
     * @brief Update terminal properties
     *
     * @param newProperties Map of new properties to set
     */
    void updateProperties(
        const QMap<QString, QVariant> &newProperties);

    /**
     * @brief Update a specific property
     *
     * @param key Property key
     * @param value New property value
     */
    void setProperty(const QString  &key,
                     const QVariant &value);

    /**
     * @brief Get a specific property
     *
     * @param key Property key
     * @param defaultValue Value to return if property
     * doesn't exist
     * @return Property value or defaultValue
     */
    QVariant getProperty(
        const QString  &key,
        const QVariant &defaultValue = QVariant()) const;

    template <typename T>
    T getProperty(const QString &key,
                  const T       &defaultValue) const
    {
        return getProperty(key, QVariant(defaultValue))
            .value<T>();
    }

    /**
     * @brief Reset class ID counter to 0
     */
    static void resetClassIDs();

    /**
     * @brief Set class ID counters based on existing
     * terminals
     *
     * @param allTerminalsById Map of terminal IDs to
     * terminal pointers
     */
    static void setClassIDs(
        const QMap<int, TerminalItem *> &allTerminalsById);

    /**
     * @brief Get a new terminal ID
     *
     * @param terminalType Optional terminal type for
     * type-specific IDs
     * @return New unique ID
     */
    static QString
    getNewTerminalID(const QString &terminalType);

    /**
     * @brief Serialize terminal to a dictionary
     *
     * @return Map containing all terminal data
     */
    QMap<QString, QVariant> toDict() const;

    /**
     * @brief Create terminal from serialized data
     *
     * @param data Serialized terminal data
     * @param pixmap Terminal pixmap
     * @param parent Optional parent item
     * @return Pointer to new TerminalItem
     */
    static TerminalItem *
    fromDict(const QMap<QString, QVariant> &data,
             const QPixmap                 &pixmap,
             QGraphicsItem *parent = nullptr);

    /**
     * @brief Bind this item to a TerminalPlacement (non-owning).
     *
     * When bound, getID() returns the placement's id instead of the
     * auto-generated UUID. The item's internal property snapshot
     * (m_properties, m_region, m_terminalType) is refreshed from the
     * placement at setPlacement-time.
     *
     * Passing nullptr unbinds — useful on documentReset.
     *
     * Cache-staleness contract: m_properties is a snapshot, not a live
     * view. All mutations of the placement are expected to round-trip
     * through ScenarioEditService → ScenarioDocument::terminalChanged →
     * (Task 21) observer → setPlacement() again, which refreshes the
     * snapshot in one place.
     */
    void setPlacement(Backend::Scenario::TerminalPlacement *placement);

    /// Non-owning placement pointer, or nullptr for isolated view tests or
    /// transient items that have not been committed to a ScenarioDocument.
    Backend::Scenario::TerminalPlacement *placement() const
    {
        return m_placement;
    }

    /// Domain id: the bound TerminalPlacement::id, or empty when unbound.
    /// Callers that need a document-addressable terminal id (mutator args,
    /// backend server requests) read this; no fallback to UUID — empty is a
    /// meaningful "unbound" signal.
    /// Implementation in .cpp (needs TerminalPlacement's full definition).
    QString getTerminalId() const;

    /// Emit a command that removes this terminal from the document. Returns
    /// nullptr when the terminal is unbound (no domain id to address) or the
    /// document pointer is absent.
    std::unique_ptr<QUndoCommand> createDeleteCommand(
        Backend::Scenario::ScenarioDocument* doc) const override;

    /// Emit a CreateConnection command between this terminal and @p other,
    /// which must also be a TerminalItem (region Connections are
    /// TerminalItem↔TerminalItem). Returns nullptr for any mismatch.
    std::unique_ptr<QUndoCommand> createConnectCommandTo(
        const GraphicsObjectBase*                        other,
        Backend::TransportationTypes::TransportationMode mode,
        Backend::Scenario::ScenarioDocument*             doc) const override;

protected:
    /// Scene-registration hook: terminal id when bound, else empty so the
    /// base's sceneRegistryKey() falls back to the auto-UUID.
    QString domainKey() const override { return getTerminalId(); }

public:

signals:
    /**
     * @brief Emitted when terminal position changes
     *
     * @param newPos New position in scene coordinates
     */
    void positionChanged(const QPointF &newPos);

    /**
     * @brief Emitted when terminal region changes
     *
     * @param newRegion New region name
     */
    void regionChanged(const QString &newRegion);

    /**
     * @brief Emitted when a terminal property changes
     *
     * @param key Property that changed
     * @param value New value
     */
    void propertyChanged(const QString  &key,
                         const QVariant &value);

    /**
     * @brief Emitted when all properties change
     */
    void propertiesChanged();

    /**
     * @brief Emitted when terminal selection changes
     *
     * @param selected New selection state
     */
    void selectionChanged(bool selected);

protected:
    /**
     * @brief Define the bounding rectangle for painting and
     * interaction
     *
     * @return Rectangle in local coordinates
     */
    QRectF boundingRect() const override;

    /**
     * @brief Paint the terminal on the canvas
     *
     * @param painter Painter to use
     * @param option Style options
     * @param widget Widget being painted on
     */
    void paint(QPainter                       *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = nullptr) override;

public:
    // --- Input interface implementations (Plan 3) ---
    Input::Handled onLeftClick(const Input::ClickContext&) override;
    void           buildContextMenu(QMenu* menu, const Input::ClickContext&) override;
    void           onHoverEnter(const Input::ClickContext&) override;
    void           onHoverLeave(const Input::ClickContext&) override;
    QCursor        hoverCursor() const override { return QCursor(Qt::PointingHandCursor); }
    void           onDragEnd(const QPointF& finalPos, const Input::ClickContext&) override;

private:
    QPixmap m_pixmap; ///< Visual representation
    QString m_region; ///< Region this terminal belongs to
    QString m_terminalType; ///< Type of terminal
    QMap<QString, QVariant>
        m_properties; ///< Terminal properties
    QRectF
        m_boundingRectValue; ///< Cached bounding rectangle
    QPointer<GlobalTerminalItem>
        m_globalTerminalItem; ///< Linked global terminal (QPointer auto-nulls on deletion)

    /// Non-owning pointer into ScenarioDocument's terminals map. When
    /// non-null, this item is a VIEW of the placement; when null, it is an
    /// unbound graphics item and derives structural defaults from its type.
    Backend::Scenario::TerminalPlacement *m_placement = nullptr;

    // Static ID management
    static QMap<QString, int>
        TERMINAL_TYPES_IDs; ///< Next ID by terminal type

    /**
     * @brief Initialize default properties based on
     * terminal type
     */
    void initializeDefaultProperties();
};

} // namespace GUI
} // namespace CargoNetSim

// Register the type for QVariant
Q_DECLARE_METATYPE(CargoNetSim::GUI::TerminalItem)
Q_DECLARE_METATYPE(CargoNetSim::GUI::TerminalItem *)
