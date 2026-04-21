#pragma once
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QDoubleValidator>
#include <QFormLayout>
#include <QGraphicsItem>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>
#include <QVariant>
#include <QWidget>

#include "../Input/PickSession.h"
namespace CargoNetSim
{
namespace GUI
{
// Forward declarations
class MainWindow;
class GraphicsScene;
class GraphicsView;
class TerminalItem;
class RegionCenterPoint;
class ConnectionLine;
class BackgroundPhotoItem;
class MapPoint;
class DestinationListEditor;
namespace Input { class InteractionController; } // namespace Input
class PropertiesPanel : public QWidget
{
    Q_OBJECT
public:
    explicit PropertiesPanel(QWidget *parent = nullptr);
    ~PropertiesPanel() = default;
    // Display property methods for different items
    void displayMapProperties();
    void displayProperties(QGraphicsItem *item);
    // Update position fields for selected item
    void updatePositionFields(const QPointF &pos);
    void           updateCoordinateFields(QPointF geoPoint);
    QGraphicsItem *getCurrentItem()
    {
        return currentItem;
    }
signals:
    void propertiesChanged(
        QGraphicsItem                 *item,
        const QMap<QString, QVariant> &properties);
    void requestRefresh();
public:
    /// Register the two scenes the panel listens to. After Plan 3 the
    /// panel reacts to `QGraphicsScene::selectionChanged` instead of
    /// per-item `clicked` signals. Call once during construction.
    void setScenes(GraphicsScene *regionScene,
                   GraphicsScene *globalScene);

    /// Called when the user switches tabs so the panel knows which
    /// scene to prefer on `refreshFromSelection`.
    void setActiveScene(GraphicsScene *activeScene);

public slots:
    void clearIfShowing(QGraphicsItem *item);
    void saveProperties();

    /// Rebuild the panel from the current selection on the active
    /// scene (falling back to the other scene if the active one has
    /// nothing selected). Wired to both scenes' `selectionChanged`.
    void refreshFromSelection();
private slots:
    void onCoordSystemChanged(int index);
    void onDwellMethodChanged(const QString &method,
                              QGroupBox     *dwellGroup);

private:
    // Clear the form layout
    void clearLayout();
    // Display methods for specific item types
    void displayMapPointProperties(MapPoint *item);
    void
    displayRegionCenterProperties(RegionCenterPoint *item);
    void displayConnectionProperties(ConnectionLine *item);
    void displayTerminalProperties(TerminalItem *item);
    void displayGenericProperties(
        QGraphicsItem     *item,
        const QStringList &skipProperties = QStringList());
    // Helper methods for terminal properties
    void addInterfacesSection(
        TerminalItem              *item,
        const QMap<QString, bool> &isEditable =
            QMap<QString, bool>());
    QLayout *createInterfaceLayout(
        const QString                        &label,
        const QList<QPair<QString, QString>> &options,
        const QStringList                    &currentValues,
        const QString &side, bool isEditable = false);
    void addNestedPropertiesSection(
        TerminalItem *item, const QString &sectionName,
        const QString &propertiesKey);
    void addCapacitySection(TerminalItem *item);
    void addCostSection(TerminalItem *item);
    void addCustomsSection(TerminalItem *item);
    void addDwellTimeSection(TerminalItem *item);

    /// Plan 8: sole authoring surface for origin role + destination
    /// routing. Always visible on every terminal — any physical kind
    /// can become an origin by setting count > 0. Writes through
    /// ScenarioMutator::setProperty so the typed store stays consistent.
    void addOriginConfigurationSection(TerminalItem *item);
    void addRoleSection(TerminalItem *item);
    // Helper method for coordinate fields
    void addCoordinateField(const QString     &key,
                            const QVariant    &value,
                            GraphicsView      *view,
                            RegionCenterPoint *item);
    // Common helper methods
    void addGenericField(const QString  &key,
                         const QVariant &value);
    QPair<QLayout *, QMap<QString, QWidget *>>
    createDwellTimeParameters(
        const QString                 &method,
        const QMap<QString, QVariant> &currentParams =
            QMap<QString, QVariant>());

    // Save property methods for different item types
    void saveTerminalProperties(TerminalItem *terminal);
    void saveBackgroundPhotoProperties(
        BackgroundPhotoItem *background);
    void saveRegionCenterProperties(
        RegionCenterPoint *regionCenter);
    void saveMapPointProperties(MapPoint *mapPoint);
    void
    saveConnectionProperties(ConnectionLine *connection);

    // Helper methods for property saving
    void
    processEditFields(QMap<QString, QVariant> &properties);
    void processNestedProperty(
        QMap<QString, QVariant> &properties,
        const QString &key, QWidget *widget);
    void processInterfaceProperty(
        QMap<QString, QVariant> &properties,
        const QStringList &parts, QWidget *widget);
    void processSimpleProperty(
        QMap<QString, QVariant> &properties,
        const QString &key, QWidget *widget);
    QVariant getWidgetValue(QWidget *widget);
    void     handleRegionChange(TerminalItem  *terminal,
                                const QString &newRegionName,
                                const QString &oldRegionName);
    void     setExpandingWidgetPolicy(QWidget *widget);
    void     addDwellTimeParameterFields(
            QFormLayout *layout, const QString &method,
            const QMap<QString, QVariant> &currentParams =
                QMap<QString, QVariant>());

    // Member variables
    MainWindow              *mainWindow;
    QFormLayout             *layout;
    QWidget                 *container;
    QPushButton             *saveButton;
    QGraphicsItem           *currentItem;
    QMap<QString, QWidget *> editFields;

    /// When an Origin Configuration section is rendered, these track the
    /// origin and its multi-destination editor so that PickCoordinator
    /// resolutions can be routed to the right place by slot.
    QString                  m_currentOriginId;
    DestinationListEditor   *m_currentListEditor = nullptr;

    /// True once we've subscribed to the coordinator's destinationResolved
    /// signal. Subscription happens lazily (needs a valid MainWindow).
    bool                     m_coordConnected = false;

    QPointer<GraphicsScene>  m_regionScene;
    QPointer<GraphicsScene>  m_globalScene;
    QPointer<GraphicsScene>  m_activeScene;

private slots:
    /// Routes a pick result by PickSlot variant: primary -> direct mutator
    /// write; multi-row -> forward to m_currentListEditor.
    void onDestinationResolved(const Input::PickSession &session,
                               const QString &terminalId,
                               const QString &terminalName);
};
} // namespace GUI
} // namespace CargoNetSim
