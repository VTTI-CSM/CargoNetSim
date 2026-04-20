#include "PropertiesPanel.h"
#include "../Controllers/TerminalController.h"
#include "../Items/BackgroundPhotoItem.h"
#include "../Items/ConnectionLine.h"
#include "../Items/MapPoint.h"
#include "../Items/RegionCenterPoint.h"
#include "../Items/TerminalItem.h"
#include "../MainWindow.h"
#include "../Scenario/ScenarioMutator.h"
#include "../Utils/IconCreator.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Scenario/RegionSpec.h"
#include "Backend/Scenario/ScenarioDocument.h"
#include "Backend/Scenario/PropertyKeys.h"
#include "Backend/Scenario/TerminalPlacement.h"
#include "Backend/Scenario/ScenarioRuntime.h"
#include "DestinationListEditor.h"
#include "GraphicsScene.h"
#include "GraphicsView.h"

#include <QApplication>
#include <QDebug>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QVBoxLayout>

namespace PK = CargoNetSim::Backend::Scenario::PropertyKeys;

namespace CargoNetSim
{
namespace GUI
{

PropertiesPanel::PropertiesPanel(QWidget *parent)
    : QWidget(parent)
    , currentItem(nullptr)
{
    mainWindow = qobject_cast<MainWindow *>(window());

    // Create main layout for the widget
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // Create a QScrollArea
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);

    // Create container widget for the scroll area
    container = new QWidget();
    layout    = new QFormLayout(container);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);

    layout->setFieldGrowthPolicy(
        QFormLayout::AllNonFixedFieldsGrow);

    // Save button
    saveButton = new QPushButton(tr("Save"));
    connect(saveButton, &QPushButton::clicked, this,
            &PropertiesPanel::saveProperties);
    layout->addRow(saveButton);

    // Set the container as the widget for the scroll area
    scrollArea->setWidget(container);

    // Add the scroll area to the main layout
    mainLayout->addWidget(scrollArea);

    // Set the default height to half the screen height
    QScreen *screen = QApplication::primaryScreen();
    if (screen)
    {
        setMinimumWidth(250);
    }
}

void PropertiesPanel::displayMapProperties()
{
    qCDebug(lcGuiUtil) << "PropertiesPanel::displayMapProperties";
    if (!mainWindow)
    {
        mainWindow = qobject_cast<MainWindow *>(window());
        if (!mainWindow)
        {
            qCWarning(lcGuiUtil) << "PropertiesPanel::displayMapProperties:"
                                 << "no MainWindow";
            return;
        }
    }

    GraphicsView *view = mainWindow->getCurrentView();
    if (!view)
    {
        qCWarning(lcGuiUtil) << "PropertiesPanel::displayMapProperties:"
                             << "no current view";
        return;
    }

    currentItem = nullptr;
    clearLayout();

    // Create coordinate system selection
    QGroupBox *coordGroup =
        new QGroupBox(tr("Coordinate System"));
    QVBoxLayout *coordLayout = new QVBoxLayout();

    QComboBox *coordCombo = new QComboBox();
    coordCombo->addItems(
        {tr("WGS84 (Geodetic)"),
         tr("WGS84 World Mercator (Projected)")});
    coordCombo->setCurrentIndex(
        view->isUsingProjectedCoords() ? 1 : 0);
    connect(
        coordCombo,
        &QComboBox::currentIndexChanged,
        this, &PropertiesPanel::onCoordSystemChanged);

    QLabel *descLabel =
        new QLabel(tr("WGS84: Lat/Lon in degrees\nWGS84 "
                      "World Mercator: X/Y in meters"));
    descLabel->setWordWrap(true);

    coordLayout->addWidget(coordCombo);
    coordLayout->addWidget(descLabel);
    coordGroup->setLayout(coordLayout);

    layout->addRow(coordGroup);
    layout->addRow(saveButton);

    emit requestRefresh();
}

void PropertiesPanel::displayProperties(QGraphicsItem *item)
{
    clearLayout();

    if (!item)
    {
        qCWarning(lcGuiUtil) << "PropertiesPanel::displayProperties:"
                             << "null item";
        return;
    }

    currentItem = item;
    editFields.clear();

    qCDebug(lcGuiUtil) << "PropertiesPanel::displayProperties:"
                       << "itemType=" << item->type();

    // Dispatch to appropriate handler based on item type
    if (MapPoint *mapPoint = dynamic_cast<MapPoint *>(item))
    {
        qCDebug(lcGuiUtil) << "PropertiesPanel::displayProperties: dispatching to" << "MapPoint";
        displayMapPointProperties(mapPoint);
    }
    else if (RegionCenterPoint *regionCenter =
                 dynamic_cast<RegionCenterPoint *>(item))
    {
        qCDebug(lcGuiUtil) << "PropertiesPanel::displayProperties: dispatching to" << "RegionCenterPoint";
        displayRegionCenterProperties(regionCenter);
    }
    else if (ConnectionLine *connection =
                 dynamic_cast<ConnectionLine *>(item))
    {
        qCDebug(lcGuiUtil) << "PropertiesPanel::displayProperties: dispatching to" << "ConnectionLine";
        displayConnectionProperties(connection);
    }
    else if (TerminalItem *terminal =
                 dynamic_cast<TerminalItem *>(item))
    {
        qCDebug(lcGuiUtil) << "PropertiesPanel::displayProperties: dispatching to" << "TerminalItem";
        displayTerminalProperties(terminal);
    }
    else
    {
        qCDebug(lcGuiUtil) << "PropertiesPanel::displayProperties: dispatching to" << "Generic";
        displayGenericProperties(item);
    }

    layout->addRow(saveButton);
}

void PropertiesPanel::clearLayout()
{
    if (m_pickConnection)
    {
        disconnect(m_pickConnection);
        m_pickConnection = {};
    }

    // Clear all widgets except save button from layout
    for (int i = layout->count() - 1; i >= 0; --i)
    {
        QLayoutItem *item = layout->itemAt(i);
        QWidget *widget   = item ? item->widget() : nullptr;
        if (widget && widget != saveButton)
        {
            layout->removeWidget(widget);
            delete widget;
        }
        else if (widget == saveButton)
        {
            layout->removeWidget(widget);
        }
    }
}

void PropertiesPanel::displayMapPointProperties(
    MapPoint *item)
{
    qCDebug(lcGuiUtil) << "PropertiesPanel::displayMapPointProperties: property count:" << item->getProperties().size();
    // Display properties for MapPoint items
    for (auto it = item->getProperties().constBegin();
         it != item->getProperties().constEnd(); ++it)
    {
        if (it.key() == "x" || it.key() == "y")
        {
            // Skip position properties as they're handled
            // by the GraphicsItem position
            continue;
        }
        QLabel *label = new QLabel(it.value().toString());
        layout->addRow(it.key() + ":", label);
    }
}

void PropertiesPanel::displayRegionCenterProperties(
    RegionCenterPoint *item)
{
    qCDebug(lcGuiUtil) << "PropertiesPanel::displayRegionCenterProperties: lat="
                       << item->getProperties().value("Latitude").toDouble()
                       << "lon=" << item->getProperties().value("Longitude").toDouble();
    if (!mainWindow)
    {
        mainWindow = qobject_cast<MainWindow *>(window());
        if (!mainWindow)
        {
            qCWarning(lcGuiUtil) << "PropertiesPanel::displayRegionCenterProperties:"
                                 << "no MainWindow";
            return;
        }
    }

    GraphicsView *view = mainWindow->getCurrentView();
    if (!view)
    {
        qCWarning(lcGuiUtil) << "PropertiesPanel::displayRegionCenterProperties:"
                             << "no current view";
        return;
    }

    const QMap<QString, QVariant> &props =
        item->getProperties();
    for (auto it = props.constBegin();
         it != props.constEnd(); ++it)
    {
        if (it.key() == "Type" || it.key() == "Region")
        {
            continue;
        }

        if (it.key() == "Latitude"
            || it.key() == "Longitude")
        {
            addCoordinateField(it.key(), it.value(), view,
                               item);
        }
        else if (it.key() == "Shared Latitude"
                 || it.key() == "Shared Longitude")
        {
            QLineEdit *lineEdit =
                new QLineEdit(it.value().toString());
            editFields[it.key()] = lineEdit;
            layout->addRow(QString("%1:°").arg(it.key()),
                           lineEdit);
        }
        else
        {
            addGenericField(it.key(), it.value());
        }
    }
}

void PropertiesPanel::addCoordinateField(
    const QString &key, const QVariant &value,
    GraphicsView *view, RegionCenterPoint *item)
{
    qCDebug(lcGuiUtil) << "PropertiesPanel::addCoordinateField: label=" << key;
    QDoubleValidator *validator =
        new QDoubleValidator(this);

    QString label;
    QString valueStr;

    if (view->isUsingProjectedCoords())
    {
        double lat =
            item->getProperties()["Latitude"].toDouble();
        double lon =
            item->getProperties()["Longitude"].toDouble();
        QPointF projected = view->convertCoordinates(
            QPointF(lon, lat), "to_projected");

        label = (key == "Longitude") ? tr("X Position (m)")
                                     : tr("Y Position (m)");
        valueStr =
            (key == "Longitude")
                ? QString::number(projected.x(), 'f', 2)
                : QString::number(projected.y(), 'f', 2);
    }
    else
    {
        label    = QString("%1 (°)").arg(key);
        valueStr = value.toString();
    }

    QLineEdit *lineEdit = new QLineEdit(valueStr);
    lineEdit->setValidator(validator);
    editFields[key] = lineEdit;
    layout->addRow(QString("%1:").arg(label), lineEdit);
}

void PropertiesPanel::displayConnectionProperties(
    ConnectionLine *item)
{
    qCDebug(lcGuiUtil) << "PropertiesPanel::displayConnectionProperties:"
                       << "id=" << item->sceneRegistryKey();
    QDoubleValidator *validator =
        new QDoubleValidator(this);
    validator->setBottom(0.0); // Ensure non-negative values

    static const QMap<QString, QPair<QString, QString>>
        propertiesWithUnits = {
            {"cost", {tr("Cost (USD)"), ""}},
            {"travelTime", {tr("Travel Time (Hours)"), ""}},
            {"distance", {tr("Distance (Km)"), ""}},
            {"carbonEmissions",
             {tr("Carbon Emissions (ton CO₂)"), ""}},
            {"risk", {tr("Risk (%)"), ""}},
            {"energyConsumption",
             {tr("Energy Consumption (kWh)"), ""}}};

    qCDebug(lcGuiUtil) << "PropertiesPanel::displayConnectionProperties: setting" << propertiesWithUnits.size() << "fields";

    const QMap<QString, QVariant> &props =
        item->getProperties();
    for (auto it = propertiesWithUnits.constBegin();
         it != propertiesWithUnits.constEnd(); ++it)
    {
        QLineEdit *lineEdit = new QLineEdit(
            props.value(it.key(), "0.0").toString());
        lineEdit->setValidator(validator);
        editFields[it.key()] = lineEdit;
        layout->addRow(it.value().first + ":", lineEdit);
    }
}

void PropertiesPanel::displayTerminalProperties(
    TerminalItem *item)
{
    qCDebug(lcGuiUtil) << "PropertiesPanel::displayTerminalProperties:"
                       << "id=" << item->getTerminalId();
    {
        QMap<QString, QVariant> ifaces =
            item->getProperties().value("Available Interfaces").toMap();
        int ifaceCount = ifaces.value("land_side").toStringList().size()
                       + ifaces.value("sea_side").toStringList().size();
        qCDebug(lcGuiUtil) << "PropertiesPanel::displayTerminalProperties: type="
                           << item->getTerminalType() << "interfaces=" << ifaceCount;
    }
    // Plan 8: `initial_container_count` and `destination_terminal` are
    // surfaced via the dedicated Origin Configuration group below — skip
    // them in the generic listing so the same key never appears twice.
    // `Containers` is the legacy property-bag slot that no longer exists
    // (typed store only); skipping it stays defensive against any old
    // snapshot that may still carry it.
    // Region is a first-class field on TerminalPlacement, not bag storage.
    // Build the ComboBox here so it always appears regardless of bag contents.
    {
        QComboBox *combo = new QComboBox();
        combo->addItems(
            CargoNetSim::CargoNetSimController::getInstance()
                .getRegionDataController()
                ->getAllRegionNames());
        combo->setCurrentText(item->getRegion());
        editFields[QStringLiteral("Region")] = combo;
        layout->addRow(tr("Region:"), combo);
    }

    displayGenericProperties(
        item,
        {"ID", "Type", "Region", "capacity", "cost", "dwell_time",
         "customs", "Available Interfaces", "Containers",
         PK::Terminal::InitialContainerCount,
         PK::Terminal::DestinationTerminal,
         PK::Terminal::Destinations});

    // Interface-edit gate: of the four remaining physical kinds, only
    // Sea Port lets the user edit the land-side set (sea-side is fixed
    // to Ship). All others lock both sides — interfaces are derived from
    // the kind, not from per-instance overrides.
    QMap<QString, bool> isEditable;
    if (item->getTerminalType() == "Sea Port Terminal")
    {
        isEditable = {{"land_side", true},
                      {"sea_side", false}};
    }
    else if (item->getTerminalType() == "Facility")
    {
        // Facility defaults to Truck-only. Let user add Train
        // via land-side editing. Sea-side stays locked.
        isEditable = {{"land_side", true},
                      {"sea_side", false}};
    }
    else
    {
        isEditable = {{"land_side", false},
                      {"sea_side", false}};
    }

    addRoleSection(item);
    addInterfacesSection(item, isEditable);
    addCapacitySection(item);
    addCostSection(item);
    addDwellTimeSection(item);
    addCustomsSection(item);
    addOriginConfigurationSection(item);
}

void PropertiesPanel::addInterfacesSection(
    TerminalItem              *item,
    const QMap<QString, bool> &isEditable)
{
    QGroupBox *interfacesGroup =
        new QGroupBox(tr("Available Interfaces"));
    QVBoxLayout *interfacesLayout = new QVBoxLayout();

    // Get interfaces from properties
    const QMap<QString, QVariant> &properties =
        item->getProperties();
    QMap<QString, QVariant> interfaces =
        properties["Available Interfaces"].toMap();

    // Land-side interfaces
    QStringList currentLand =
        interfaces["land_side"].toStringList();
    QLayout *landLayout = createInterfaceLayout(
        tr("Land-side:"),
        {{tr("Truck"), "truck"}, {tr("Rail"), "rail"}},
        currentLand, "land",
        isEditable.value("land_side", false));

    // Sea-side interfaces
    QStringList currentSea =
        interfaces["sea_side"].toStringList();
    qCDebug(lcGuiUtil) << "PropertiesPanel::addInterfacesSection: landSide=" << currentLand.size() << "seaSide=" << currentSea.size();
    QLayout *seaLayout = createInterfaceLayout(
        tr("Sea-side:"), {{tr("Ship"), "ship"}}, currentSea,
        "sea", isEditable.value("sea_side", false));

    interfacesLayout->addLayout(landLayout);
    interfacesLayout->addLayout(seaLayout);
    interfacesGroup->setLayout(interfacesLayout);
    layout->addRow(interfacesGroup);
}

QLayout *PropertiesPanel::createInterfaceLayout(
    const QString                        &label,
    const QList<QPair<QString, QString>> &options,
    const QStringList &currentValues, const QString &side,
    bool isEditable)
{
    QVBoxLayout *layout = new QVBoxLayout();
    layout->addWidget(new QLabel(label));

    QHBoxLayout *checkboxLayout = new QHBoxLayout();
    for (const auto &option : options)
    {
        QCheckBox *checkbox = new QCheckBox(option.first);
        checkbox->setChecked(
            currentValues.contains(option.first));
        checkbox->setEnabled(isEditable);
        editFields[QString("interfaces.%1.%2")
                       .arg(side)
                       .arg(option.second)] = checkbox;
        checkboxLayout->addWidget(checkbox);
    }

    layout->addLayout(checkboxLayout);
    return layout;
}

void PropertiesPanel::addNestedPropertiesSection(
    TerminalItem *item, const QString &sectionName,
    const QString &propertiesKey)
{
    const QMap<QString, QVariant> &properties =
        item->getProperties();
    if (!properties.contains(propertiesKey))
    {
        return;
    }

    QGroupBox   *group = new QGroupBox(sectionName);
    QFormLayout *propertyLayout = new QFormLayout();
    propertyLayout->setFieldGrowthPolicy(
        QFormLayout::AllNonFixedFieldsGrow);

    // Define units and validators for different property
    // types
    static const QMap<
        QString,
        QMap<QString, QPair<QString, QDoubleValidator *>>>
        propertyConfigs = {
            {"capacity",
             {{"storage",
               {tr("Storage Capacity (TEU)"),
                new QDoubleValidator()}},
              {"processing",
               {tr("Processing Capacity (TEU/day)"),
                new QDoubleValidator()}}}},
            {"cost",
             {{"fixed",
               {tr("Fixed Cost (USD/year)"),
                new QDoubleValidator()}},
              {"variable",
               {tr("Variable Cost (USD/TEU)"),
                new QDoubleValidator()}},
              {"penalty",
               {tr("Penalty Cost (USD/day)"),
                new QDoubleValidator()}}}},
            {"customs",
             {{"processing_time",
               {tr("Processing Time (hours)"),
                new QDoubleValidator()}},
              {"cost",
               {tr("Cost (USD/TEU)"),
                new QDoubleValidator()}}}}};

    QMap<QString, QVariant> nestedProperties =
        properties[propertiesKey].toMap();
    const QMap<QString, QPair<QString, QDoubleValidator *>>
        &configs = propertyConfigs[propertiesKey];

    for (auto it = nestedProperties.constBegin();
         it != nestedProperties.constEnd(); ++it)
    {
        QLineEdit *lineEdit =
            new QLineEdit(it.value().toString());
        setExpandingWidgetPolicy(lineEdit);

        QString           label     = it.key();
        QDoubleValidator *validator = nullptr;

        if (configs.contains(it.key()))
        {
            label     = configs[it.key()].first;
            validator = configs[it.key()].second;
            if (validator)
            {
                validator->setBottom(
                    0.0); // Ensure non-negative values
                lineEdit->setValidator(validator);
            }
        }

        editFields[QString("%1.%2")
                       .arg(propertiesKey)
                       .arg(it.key())] = lineEdit;
        propertyLayout->addRow(label + ":", lineEdit);
    }

    group->setLayout(propertyLayout);
    layout->addRow(group);
}

void PropertiesPanel::addCapacitySection(TerminalItem *item)
{
    addNestedPropertiesSection(item, tr("Capacity"),
                               "capacity");
}

void PropertiesPanel::addCostSection(TerminalItem *item)
{
    addNestedPropertiesSection(item, tr("Cost"), "cost");
}

void PropertiesPanel::addCustomsSection(TerminalItem *item)
{
    addNestedPropertiesSection(item, tr("Customs"),
                               "customs");
}

void PropertiesPanel::addDwellTimeSection(
    TerminalItem *item)
{
    const QMap<QString, QVariant> &properties =
        item->getProperties();
    if (!properties.contains("dwell_time"))
    {
        return;
    }

    QMap<QString, QVariant> dwellTime =
        properties["dwell_time"].toMap();

    // Create a group box for all dwell time controls
    QGroupBox *dwellGroup = new QGroupBox(tr("Dwell Time"));
    QVBoxLayout *dwellLayout = new QVBoxLayout(dwellGroup);

    // Method selection in a horizontal layout
    QHBoxLayout *methodLayout = new QHBoxLayout();
    QLabel      *methodLabel  = new QLabel(tr("Method:"));
    QComboBox   *methodCombo  = new QComboBox();
    methodCombo->addItems(
        {"normal", "gamma", "exponential", "lognormal"});

    // Get the current method
    QString currentMethod = dwellTime["method"].toString();
    if (currentMethod.isEmpty())
    {
        currentMethod = "normal";
    }
    methodCombo->setCurrentText(currentMethod);
    setExpandingWidgetPolicy(methodCombo);
    editFields["dwell_time.method"] = methodCombo;

    methodLayout->addWidget(methodLabel);
    methodLayout->addWidget(methodCombo);
    dwellLayout->addLayout(methodLayout);

    // Create parameters container that will hold parameter
    // fields
    QWidget *parametersContainer = new QWidget(dwellGroup);
    QFormLayout *parametersLayout =
        new QFormLayout(parametersContainer);
    parametersLayout->setFieldGrowthPolicy(
        QFormLayout::AllNonFixedFieldsGrow);
    dwellLayout->addWidget(parametersContainer);

    // Add the parameters for the current method
    QMap<QString, QVariant> currentParams =
        dwellTime["parameters"].toMap();
    addDwellTimeParameterFields(
        parametersLayout, currentMethod, currentParams);

    // Connect method change signal
    connect(methodCombo, &QComboBox::currentTextChanged,
            [this, parametersContainer,
             parametersLayout](const QString &method) {
                // Clear the existing parameter fields
                while (QLayoutItem *item =
                           parametersLayout->takeAt(0))
                {
                    if (QWidget *widget = item->widget())
                    {
                        // Remove from editFields
                        for (auto it = editFields.begin();
                             it != editFields.end();)
                        {
                            if (it.value() == widget)
                            {
                                it = editFields.erase(it);
                            }
                            else
                            {
                                ++it;
                            }
                        }
                        widget->deleteLater();
                    }
                    delete item;
                }

                // Add new parameter fields for the selected
                // method
                addDwellTimeParameterFields(
                    parametersLayout, method);

                // Force layout update
                parametersContainer->updateGeometry();
                parametersContainer->update();
            });

    // Add the group box to the main layout
    layout->addRow(dwellGroup);
}

void PropertiesPanel::addDwellTimeParameterFields(
    QFormLayout *layout, const QString &method,
    const QMap<QString, QVariant> &currentParams)
{
    if (method == "gamma")
    {
        // Shape parameter
        QLineEdit *shape = new QLineEdit(
            currentParams.value("shape", "2.0").toString());
        setExpandingWidgetPolicy(shape);
        editFields["dwell_time.parameters.shape"] = shape;
        layout->addRow(tr("Shape (k):"), shape);

        // Scale parameter
        QLineEdit *scale = new QLineEdit(
            currentParams.value("scale", "1440")
                .toString());
        setExpandingWidgetPolicy(scale);
        editFields["dwell_time.parameters.scale"] = scale;
        layout->addRow(tr("Scale (θ) minutes:"), scale);
    }
    else if (method == "exponential")
    {
        // Scale parameter
        QLineEdit *scale = new QLineEdit(
            currentParams.value("scale", "2880")
                .toString());
        setExpandingWidgetPolicy(scale);
        editFields["dwell_time.parameters.scale"] = scale;
        layout->addRow(tr("Scale (λ) minutes:"), scale);
    }
    else if (method == "normal")
    {
        // Mean parameter
        QLineEdit *mean = new QLineEdit(
            currentParams.value("mean", "2880").toString());
        setExpandingWidgetPolicy(mean);
        editFields["dwell_time.parameters.mean"] = mean;
        layout->addRow(tr("Mean (minutes):"), mean);

        // Standard deviation parameter
        QLineEdit *stdDev = new QLineEdit(
            currentParams.value("std_dev", "720")
                .toString());
        setExpandingWidgetPolicy(stdDev);
        editFields["dwell_time.parameters.std_dev"] =
            stdDev;
        layout->addRow(tr("Std Dev (minutes):"), stdDev);
    }
    else if (method == "lognormal")
    {
        // Mean parameter (log-scale)
        QLineEdit *mean = new QLineEdit(
            currentParams.value("mean", "3.45").toString());
        setExpandingWidgetPolicy(mean);
        editFields["dwell_time.parameters.mean"] = mean;
        layout->addRow(tr("Mean (log-scale):"), mean);

        // Sigma parameter
        QLineEdit *sigma = new QLineEdit(
            currentParams.value("sigma", "0.25")
                .toString());
        setExpandingWidgetPolicy(sigma);
        editFields["dwell_time.parameters.sigma"] = sigma;
        layout->addRow(tr("Sigma:"), sigma);
    }
}

void PropertiesPanel::addRoleSection(TerminalItem *item)
{
    using TerminalRole =
        Backend::Scenario::TerminalPlacement::TerminalRole;

    qCDebug(lcGuiUtil)
        << "PropertiesPanel::addRoleSection:"
        << "id="
        << (item ? item->getTerminalId() : "null");

    if (!item || !item->placement())
    {
        qCDebug(lcGuiUtil)
            << "PropertiesPanel::addRoleSection:"
            << "item or placement null, skipping";
        return;
    }

    if (!mainWindow)
        mainWindow = qobject_cast<MainWindow *>(window());
    if (!mainWindow || !mainWindow->runtime())
    {
        qCDebug(lcGuiUtil)
            << "PropertiesPanel::addRoleSection:"
            << "no runtime, skipping";
        return;
    }

    auto *doc = &mainWindow->runtime()->document();
    const QString id = item->getTerminalId();
    if (id.isEmpty()) return;

    QGroupBox   *group = new QGroupBox("Role");
    QFormLayout *form  = new QFormLayout(group);

    QComboBox *roleCombo = new QComboBox();
    roleCombo->addItem("Transit",     QStringLiteral("transit"));
    roleCombo->addItem("Origin",      QStringLiteral("origin"));
    roleCombo->addItem("Destination", QStringLiteral("destination"));

    const TerminalRole current = item->placement()->role;
    roleCombo->setCurrentIndex(static_cast<int>(current));

    connect(
        roleCombo,
        QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [doc, id](int index) {
            auto role = static_cast<TerminalRole>(index);
            GUI::Scenario::ScenarioMutator::setTerminalRole(
                doc, id, role);
        });

    form->addRow("Terminal Role:", roleCombo);
    layout->addRow(group);
}

void PropertiesPanel::addOriginConfigurationSection(
    TerminalItem *item)
{
    using namespace CargoNetSim::Backend::Scenario;

    if (!mainWindow)
        mainWindow = qobject_cast<MainWindow *>(window());
    if (!mainWindow || !item) return;
    if (!mainWindow->runtime())
    {
        qCDebug(lcGuiUtil)
            << "PropertiesPanel::addOriginConfigurationSection:"
            << "no scenario loaded, skipping";
        return;
    }

    auto *doc        = &mainWindow->runtime()->document();
    const QString id = item->getTerminalId();
    if (id.isEmpty())
    {
        qCWarning(lcGuiUtil)
            << "PropertiesPanel::addOriginConfigurationSection:"
            << "terminal has no ID (unbound), skipping";
        return;
    }

    auto *placement = item->placement();
    if (!placement)
    {
        qCDebug(lcGuiUtil)
            << "PropertiesPanel::addOriginConfigurationSection:"
            << "no placement, skipping";
        return;
    }
    // Show Origin Configuration when the terminal is an
    // origin by EITHER designation: explicit role=Origin OR
    // initial_container_count > 0 (the runtime source of
    // truth). This ensures terminals loaded from YAML with
    // containers but no role field still show the section.
    const bool isOriginByRole =
        placement->role
        == TerminalPlacement::TerminalRole::Origin;
    const bool isOriginByProperty =
        placement->properties
            .value(PK::Terminal::InitialContainerCount, 0)
            .toInt()
        > 0;
    if (!isOriginByRole && !isOriginByProperty)
    {
        qCDebug(lcGuiUtil)
            << "PropertiesPanel::addOriginConfigurationSection:"
            << "not an origin (role="
            << roleToString(placement->role)
            << ", count="
            << placement->properties.value(
                   PK::Terminal::InitialContainerCount, 0)
                   .toInt()
            << "), skipping";
        return;
    }

    QGroupBox   *group = new QGroupBox(tr("Origin Configuration"));
    QFormLayout *form  = new QFormLayout(group);

    // ---- Count spinbox (unchanged from Plan 8 Commit B) ---------------
    QSpinBox *countSpin = new QSpinBox();
    countSpin->setRange(0, 1'000'000);
    countSpin->setValue(
        item->getProperty(PK::Terminal::InitialContainerCount).toInt());
    connect(countSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [doc, id](int value) {
                GUI::Scenario::ScenarioMutator::setTerminalProperty(
                    doc, id, PK::Terminal::InitialContainerCount,
                    value > 0 ? QVariant(value) : QVariant());
            });
    form->addRow(tr("Initial Container Count:"), countSpin);

    // ---- Mode toggle ---------------------------------------------------
    QComboBox *modeCombo = new QComboBox();
    modeCombo->addItem(tr("Single destination"),    QStringLiteral("single"));
    modeCombo->addItem(tr("Multiple destinations"), QStringLiteral("multi"));

    // ---- Single: pick from canvas -------------------
    QPushButton *pickButton = new QPushButton("Pick");
    pickButton->setToolTip(
        tr("Click a terminal on the canvas"));

    const QString currentDest =
        item->getProperty(PK::Terminal::DestinationTerminal)
            .toString();
    if (!currentDest.isEmpty())
    {
        pickButton->setText(
            QStringLiteral("Picked \u2713"));
        if (doc->terminals.contains(currentDest))
            pickButton->setToolTip(
                doc->terminals[currentDest]
                    .properties.value("Name")
                    .toString());
    }

    auto *scene = mainWindow->getCurrentScene();
    qCDebug(lcGuiUtil)
        << "addOriginConfigurationSection: pickButton"
        << "scene=" << (void *)scene;
    connect(pickButton, &QPushButton::clicked, this,
        [this, scene, pickButton]() {
            qCDebug(lcGuiUtil)
                << "pickButton: clicked, scene="
                << (void *)scene;
            if (!scene)
            {
                qCWarning(lcGuiUtil)
                    << "pickButton: scene is null!";
                return;
            }
            if (scene->isInPickDestinationMode())
            {
                qCDebug(lcGuiUtil)
                    << "pickButton: cancelling pick";
                scene->setIsInPickDestinationMode(false);
                return;
            }
            qCDebug(lcGuiUtil)
                << "pickButton: entering pick mode";
            scene->setIsInPickDestinationMode(true);
            pickButton->setText("Picking...");
        });

    // Receive picked terminal -- lambda captures only
    // long-lived objects. Button state updates via
    // terminalChanged -> panel rebuild.
    if (m_pickConnection)
        disconnect(m_pickConnection);
    m_pickConnection = connect(
        scene, &GraphicsScene::destinationPicked, this,
        [this, doc, id](const QString &destId,
                        const QString &destName) {
            Q_UNUSED(destName);
            if (destId == id)
            {
                qCWarning(lcGuiUtil)
                    << "addOriginConfigurationSection:"
                    << "cannot set self as destination";
                if (mainWindow)
                    mainWindow->showMessage(
                        "Cannot set a terminal as its"
                        " own destination", 2000);
                return;
            }
            qCDebug(lcGuiUtil)
                << "addOriginConfigurationSection:"
                << "destination picked:" << destId;
            GUI::Scenario::ScenarioMutator::
                setTerminalProperty(
                    doc, id,
                    PK::Terminal::DestinationTerminal,
                    destId);
        });

    // ---- Multi: DestinationListEditor --------------------------------
    auto *listEditor = new DestinationListEditor();
    listEditor->setScene(mainWindow->getCurrentScene());
    listEditor->setOriginTerminalId(id);
    listEditor->setRoutes(doc->destinationsFor(id));
    connect(listEditor, &DestinationListEditor::changed,
            this, [doc, id, listEditor]() {
                if (!listEditor->isValid()) return;  // wait for valid state
                QVariantList out;
                for (const auto &r : listEditor->routes())
                {
                    QVariantMap m;
                    m[PK::Terminal::DestTerminal] = r.terminal;
                    m[PK::Terminal::DestFraction] = r.fraction;
                    out.append(m);
                }
                GUI::Scenario::ScenarioMutator::setTerminalProperty(
                    doc, id, PK::Terminal::Destinations, out);
            });

    // ---- Mutex wiring: switching modes clears the opposite key -------
    auto applyMode = [=](const QString &mode) {
        const bool multi = (mode == QStringLiteral("multi"));
        pickButton->setVisible(!multi);
        listEditor->setVisible(multi);
        if (multi)
        {
            GUI::Scenario::ScenarioMutator::setTerminalProperty(
                doc, id, PK::Terminal::DestinationTerminal, QVariant());
        }
        else
        {
            GUI::Scenario::ScenarioMutator::setTerminalProperty(
                doc, id, PK::Terminal::Destinations, QVariant());
        }
    };
    // Detect initial mode from whichever property is already set.
    const bool startMulti =
        item->getProperty(PK::Terminal::Destinations).isValid();
    modeCombo->setCurrentIndex(startMulti ? 1 : 0);
    pickButton->setVisible(!startMulti);
    listEditor->setVisible(startMulti);
    connect(modeCombo, &QComboBox::currentTextChanged,
            this, [modeCombo, applyMode]() {
                applyMode(modeCombo->currentData().toString());
            });

    form->addRow(tr("Destination Mode:"), modeCombo);
    form->addRow(tr("Destination:"),      pickButton);
    form->addRow(tr("Destinations:"),     listEditor);

    layout->addRow(group);
}

void PropertiesPanel::displayGenericProperties(
    QGraphicsItem *item, const QStringList &skipProperties)
{
    QStringList defaultSkip = {"Type"};
    QStringList allSkip     = defaultSkip + skipProperties;

    // Need to convert to proper item type to get properties
    TerminalItem *terminal =
        dynamic_cast<TerminalItem *>(item);
    BackgroundPhotoItem *background =
        dynamic_cast<BackgroundPhotoItem *>(item);
    RegionCenterPoint *regionCenter =
        dynamic_cast<RegionCenterPoint *>(item);
    MapPoint *mapPoint = dynamic_cast<MapPoint *>(item);
    ConnectionLine *connection =
        dynamic_cast<ConnectionLine *>(item);

    QMap<QString, QVariant> properties;

    if (terminal)
    {
        properties = terminal->getProperties();
    }
    else if (background)
    {
        properties = background->getProperties();
    }
    else if (regionCenter)
    {
        properties = regionCenter->getProperties();
    }
    else if (mapPoint)
    {
        properties = mapPoint->getProperties();
    }
    else if (connection)
    {
        properties = connection->getProperties();
    }
    else
    {
        return; // No supported item type
    }

    for (auto it = properties.constBegin();
         it != properties.constEnd(); ++it)
    {
        if (allSkip.contains(it.key()))
        {
            continue;
        }

        // Show on Global Map checkbox
        if (it.key() == "Show on Global Map" && terminal)
        {
            QCheckBox *checkbox = new QCheckBox();
            checkbox->setChecked(it.value().toBool());
            editFields[it.key()] = checkbox;
            layout->addRow(it.key() + ":", checkbox);
        }
        else
        {
            addGenericField(it.key(), it.value());
        }
    }

    // Add spacer
    QWidget *spacer = new QWidget();
    spacer->setMinimumHeight(20);
    layout->addRow("", spacer);
}

void PropertiesPanel::addGenericField(const QString  &key,
                                      const QVariant &value)
{
    QLineEdit *lineEdit = new QLineEdit(value.toString());
    editFields[key]     = lineEdit;
    layout->addRow(key + ":", lineEdit);
}

QPair<QLayout *, QMap<QString, QWidget *>>
PropertiesPanel::createDwellTimeParameters(
    const QString                 &method,
    const QMap<QString, QVariant> &currentParams)
{
    QFormLayout *paramLayout = new QFormLayout();
    QMap<QString, QWidget *> paramFields;

    if (method == "gamma")
    {
        // Shape parameter
        QLineEdit *shape = new QLineEdit(
            currentParams.value("shape", "2.0").toString());
        setExpandingWidgetPolicy(shape);
        paramFields["shape"] = shape;
        paramLayout->addRow(tr("Shape (k):"), shape);

        // Scale parameter
        QLineEdit *scale = new QLineEdit(
            currentParams.value("scale", "1440")
                .toString());
        setExpandingWidgetPolicy(scale);
        paramFields["scale"] = scale;
        paramLayout->addRow(tr("Scale (θ) minutes:"),
                            scale);
    }
    else if (method == "exponential")
    {
        // Scale parameter
        QLineEdit *scale = new QLineEdit(
            currentParams.value("scale", "2880")
                .toString());
        setExpandingWidgetPolicy(scale);
        paramFields["scale"] = scale;
        paramLayout->addRow(tr("Scale (λ) minutes:"),
                            scale);
    }
    else if (method == "normal")
    {
        // Mean parameter
        QLineEdit *mean = new QLineEdit(
            currentParams.value("mean", "2880").toString());
        setExpandingWidgetPolicy(mean);
        paramFields["mean"] = mean;
        paramLayout->addRow(tr("Mean (minutes):"), mean);

        // Standard deviation parameter
        QLineEdit *stdDev = new QLineEdit(
            currentParams.value("std_dev", "720")
                .toString());
        setExpandingWidgetPolicy(stdDev);
        paramFields["std_dev"] = stdDev;
        paramLayout->addRow(tr("Std Dev (minutes):"),
                            stdDev);
    }
    else if (method == "lognormal")
    {
        // Mean parameter (log-scale)
        QLineEdit *mean = new QLineEdit(
            currentParams.value("mean", "3.45").toString());
        setExpandingWidgetPolicy(mean);
        paramFields["mean"] = mean;
        paramLayout->addRow(tr("Mean (log-scale):"), mean);

        // Sigma parameter
        QLineEdit *sigma = new QLineEdit(
            currentParams.value("sigma", "0.25")
                .toString());
        setExpandingWidgetPolicy(sigma);
        paramFields["sigma"] = sigma;
        paramLayout->addRow(tr("Sigma:"), sigma);
    }

    // Make the layout use all available width
    paramLayout->setFieldGrowthPolicy(
        QFormLayout::AllNonFixedFieldsGrow);

    return {paramLayout, paramFields};
}

void PropertiesPanel::onDwellMethodChanged(
    const QString &method, QGroupBox *dwellGroup)
{
    // Remove old parameter fields
    QFormLayout *oldParamLayout = nullptr;
    for (int i = 0; i < dwellGroup->layout()->count(); ++i)
    {
        QLayoutItem *item = dwellGroup->layout()->itemAt(i);
        if (QFormLayout *formLayout =
                qobject_cast<QFormLayout *>(item->layout()))
        {
            oldParamLayout = formLayout;
            break;
        }
    }

    if (oldParamLayout)
    {
        // Get the current parameters if they exist
        QMap<QString, QVariant> currentParams;
        for (auto it = editFields.constBegin();
             it != editFields.constEnd(); ++it)
        {
            if (it.key().startsWith(
                    "dwell_time.parameters."))
            {
                QString paramName = it.key().mid(
                    21); // Remove "dwell_time.parameters."
                if (QLineEdit *lineEdit =
                        qobject_cast<QLineEdit *>(
                            it.value()))
                {
                    currentParams[paramName] =
                        lineEdit->text();
                }
            }
        }

        // Remove old parameter fields from editFields
        for (auto it = editFields.begin();
             it != editFields.end();)
        {
            if (it.key().startsWith(
                    "dwell_time.parameters."))
            {
                it = editFields.erase(it);
            }
            else
            {
                ++it;
            }
        }

        // Remove the old layout
        while (oldParamLayout->count())
        {
            QLayoutItem *item = oldParamLayout->takeAt(0);
            if (item->widget())
            {
                delete item->widget();
            }
            delete item;
        }
        QLayout *groupLayout = dwellGroup->layout();
        groupLayout->removeItem(oldParamLayout);
        delete oldParamLayout;
    }

    // Create and add new parameter fields
    auto [paramLayout, paramFields] =
        createDwellTimeParameters(method);
    dwellGroup->layout()->addItem(paramLayout);

    // Update editFields with new parameter fields
    for (auto it = paramFields.constBegin();
         it != paramFields.constEnd(); ++it)
    {
        editFields[QString("dwell_time.parameters.%1")
                       .arg(it.key())] = it.value();
    }

    // Force layout update to show the new parameters
    // immediately
    dwellGroup->layout()->invalidate();
    dwellGroup->layout()->activate();
    dwellGroup->update();

    // Also make sure the parameters get applied when saved
    if (QComboBox *methodCombo = qobject_cast<QComboBox *>(
            editFields["dwell_time.method"]))
    {
        methodCombo->setCurrentText(
            method); // Ensure method is set
    }
}

void PropertiesPanel::updatePositionFields(
    const QPointF &pos)
{
    if (currentItem && !editFields.isEmpty())
    {
        if (editFields.contains("X Position"))
        {
            QLineEdit *xField = qobject_cast<QLineEdit *>(
                editFields["X Position"]);
            if (xField)
            {
                xField->setText(
                    QString::number(pos.x(), 'f', 2));
            }
        }
        if (editFields.contains("Y Position"))
        {
            QLineEdit *yField = qobject_cast<QLineEdit *>(
                editFields["Y Position"]);
            if (yField)
            {
                yField->setText(
                    QString::number(pos.y(), 'f', 2));
            }
        }
    }
}

void PropertiesPanel::updateCoordinateFields(
    QPointF geoPoint)
{
    if (currentItem && !editFields.isEmpty())
    {
        if (editFields.contains("Latitude"))
        {
            QLineEdit *latField = qobject_cast<QLineEdit *>(
                editFields["Latitude"]);
            if (latField)
            {
                latField->setText(
                    QString::number(geoPoint.y(), 'f', 6));
            }
        }
        if (editFields.contains("Longitude"))
        {
            QLineEdit *lonField = qobject_cast<QLineEdit *>(
                editFields["Longitude"]);
            if (lonField)
            {
                lonField->setText(
                    QString::number(geoPoint.x(), 'f', 6));
            }
        }
    }
}

void PropertiesPanel::saveProperties()
{
    if (!currentItem)
    {
        qCWarning(lcGuiUtil) << "PropertiesPanel::saveProperties:"
                             << "no current item";
        return;
    }

    qCDebug(lcGuiUtil) << "PropertiesPanel::saveProperties:"
                       << "itemType=" << currentItem->type();

    // Get main window reference if needed
    if (!mainWindow)
    {
        mainWindow = qobject_cast<MainWindow *>(window());
    }

    // Dispatch to the appropriate handler based on item
    // type
    if (TerminalItem *terminal =
            dynamic_cast<TerminalItem *>(currentItem))
    {
        saveTerminalProperties(terminal);
    }
    else if (BackgroundPhotoItem *background =
                 dynamic_cast<BackgroundPhotoItem *>(
                     currentItem))
    {
        saveBackgroundPhotoProperties(background);
    }
    else if (RegionCenterPoint *regionCenter =
                 dynamic_cast<RegionCenterPoint *>(
                     currentItem))
    {
        saveRegionCenterProperties(regionCenter);
    }
    else if (MapPoint *mapPoint =
                 dynamic_cast<MapPoint *>(currentItem))
    {
        saveMapPointProperties(mapPoint);
    }
    else if (ConnectionLine *connection =
                 dynamic_cast<ConnectionLine *>(
                     currentItem))
    {
        saveConnectionProperties(connection);
    }

    // Show status bar message
    if (mainWindow)
    {
        mainWindow->showStatusBarMessage(
            tr("Properties updated successfully"), 2000);
    }
}

void PropertiesPanel::saveTerminalProperties(
    TerminalItem *terminal)
{
    qCDebug(lcGuiUtil)
        << "PropertiesPanel::saveTerminalProperties:"
        << "id=" << terminal->getTerminalId();

    QMap<QString, QVariant> oldProperties =
        terminal->getProperties();
    QMap<QString, QVariant> newProperties = oldProperties;

    // Build complete updated properties using the existing
    // processEditFields logic (handles nested dot-path keys
    // like "cost.fixed_fees", "dwell_time.parameters.mean").
    processEditFields(newProperties);

    // Route through backend if bound to ScenarioDocument.
    // Merge only CHANGED keys on top of backend truth —
    // avoids losing properties added by CLI or other
    // mutations. Single updateTerminal call for all
    // changes — no multiple terminalChanged signals.
    if (terminal->placement()
        && !terminal->getTerminalId().isEmpty()
        && mainWindow && mainWindow->runtime())
    {
        auto *doc = &mainWindow->runtime()->document();
        const QString id        = terminal->getTerminalId();
        const QString oldRegion = terminal->getRegion();

        // Start from BACKEND truth, not local snapshot.
        Backend::Scenario::TerminalPlacement p =
            doc->terminals[id];

        // Apply only keys that the user actually changed.
        // "Region" is a first-class field handled separately below.
        int changedCount = 0;
        for (auto it = newProperties.constBegin();
             it != newProperties.constEnd(); ++it)
        {
            if (it.key() == QStringLiteral("Region"))
                continue;
            if (oldProperties.value(it.key()) != it.value())
            {
                p.properties[it.key()] = it.value();
                ++changedCount;
            }
        }

        // Region is a first-class field on TerminalPlacement.
        const QString newRegion =
            newProperties.value(QStringLiteral("Region")).toString();
        const bool regionChanged =
            !newRegion.isEmpty() && newRegion != oldRegion;
        if (regionChanged)
            p.region = newRegion;

        if (changedCount > 0 || regionChanged)
            doc->updateTerminal(id, p);

        qCDebug(lcGuiUtil)
            << "saveTerminalProperties: persisted"
            << changedCount << "bag changes +"
            << (regionChanged ? "region change" : "no region change")
            << "to backend for" << id;

        if (regionChanged)
            handleRegionChange(terminal, newRegion);
    }
    else
    {
        // Legacy fallback — no backend binding
        terminal->updateProperties(newProperties);
    }

    // Update the visibility of the global terminal
    if (mainWindow)
        mainWindow->terminalCtrl()->updateGlobalMapItem(
            terminal);

    emit propertiesChanged(terminal, newProperties);
}

void PropertiesPanel::saveBackgroundPhotoProperties(
    BackgroundPhotoItem *background)
{
    QMap<QString, QVariant> newProperties =
        background->getProperties();

    // Process all edit fields
    processEditFields(newProperties);

    // Handle special background photo properties
    try
    {
        // Get new lat/lon values
        double newLat =
            newProperties.value("Latitude", "0.0")
                .toDouble();
        double newLon =
            newProperties.value("Longitude", "0.0")
                .toDouble();

        // Get and validate new scale value
        double newScale =
            newProperties.value("Scale", "1.0").toDouble();
        if (newScale <= 0)
        {
            throw std::invalid_argument(
                "Scale must be greater than 0");
        }

        // Update position using WGS84 coordinates
        background->setFromWGS84(QPointF(newLon, newLat));

        // Update scale and trigger redraw
        background->getProperties()["Scale"] =
            QString::number(newScale);
        background->updateScale();
    }
    catch (const std::exception &e)
    {
        if (mainWindow)
        {
            mainWindow->showStatusBarMessage(
                tr("Invalid coordinate or scale values: %1")
                    .arg(e.what()),
                3000);
            return;
        }
    }

    // Update the item properties
    background->updateProperties(newProperties);

    // Emit the properties changed signal
    emit propertiesChanged(background, newProperties);
}

void PropertiesPanel::saveRegionCenterProperties(
    RegionCenterPoint *regionCenter)
{
    qCDebug(lcGuiUtil)
        << "PropertiesPanel::saveRegionCenterProperties:";

    QMap<QString, QVariant> oldProperties =
        regionCenter->getProperties();
    QMap<QString, QVariant> newProperties = oldProperties;
    processEditFields(newProperties);

    // Persist coordinates to backend if changed
    if (mainWindow && mainWindow->runtime()
        && !regionCenter->getRegion().isEmpty())
    {
        auto *doc = &mainWindow->runtime()->document();
        const QString region = regionCenter->getRegion();

        const double oldLat =
            oldProperties.value("Latitude").toDouble();
        const double oldLon =
            oldProperties.value("Longitude").toDouble();
        const double newLat =
            newProperties.value("Latitude").toDouble();
        const double newLon =
            newProperties.value("Longitude").toDouble();

        // Use absolute tolerance — qFuzzyCompare fails
        // near zero (e.g., 0.0 vs 0.05 returns true).
        constexpr double kCoordEps = 1e-6;
        if (qAbs(oldLat - newLat) > kCoordEps
            || qAbs(oldLon - newLon) > kCoordEps)
        {
            qCDebug(lcGuiUtil)
                << "saveRegionCenterProperties:"
                << "local origin changed"
                << "lat:" << oldLat << "->" << newLat
                << "lon:" << oldLon << "->" << newLon;
            GUI::Scenario::ScenarioMutator::
                updateRegionLocalOrigin(
                    doc, region,
                    QPointF(newLon, newLat));
        }

        const double oldSLat =
            oldProperties.value("Shared Latitude")
                .toDouble();
        const double oldSLon =
            oldProperties.value("Shared Longitude")
                .toDouble();
        const double newSLat =
            newProperties.value("Shared Latitude")
                .toDouble();
        const double newSLon =
            newProperties.value("Shared Longitude")
                .toDouble();

        if (qAbs(oldSLat - newSLat) > kCoordEps
            || qAbs(oldSLon - newSLon) > kCoordEps)
        {
            qCDebug(lcGuiUtil)
                << "saveRegionCenterProperties:"
                << "global position changed"
                << "lat:" << oldSLat << "->" << newSLat
                << "lon:" << oldSLon << "->" << newSLon;
            GUI::Scenario::ScenarioMutator::
                updateRegionGlobalPosition(
                    doc, region,
                    QPointF(newSLon, newSLat));
        }

        // Refresh local item from updated backend state
        // so propertiesChanged emits fresh data.
        auto it = doc->regions.find(region);
        if (it != doc->regions.end())
            regionCenter->refreshFromSpec(&it.value());
    }
    else
    {
        // Legacy fallback — update item directly
        regionCenter->updateProperties(newProperties);
    }

    emit propertiesChanged(
        regionCenter, regionCenter->getProperties());
}

void PropertiesPanel::saveMapPointProperties(
    MapPoint *mapPoint)
{
    QMap<QString, QVariant> newProperties =
        mapPoint->getProperties();

    // Process all edit fields
    processEditFields(newProperties);

    // Update the item properties
    mapPoint->updateProperties(newProperties);

    // Emit the properties changed signal
    emit propertiesChanged(mapPoint, newProperties);
}

void PropertiesPanel::saveConnectionProperties(
    ConnectionLine *connection)
{
    qCDebug(lcGuiUtil)
        << "PropertiesPanel::saveConnectionProperties:";

    QMap<QString, QVariant> newProperties =
        connection->getProperties();
    processEditFields(newProperties);

    // Route through ScenarioMutator if bound to backend
    // (handles both regional Connections and GlobalLinks)
    QString fromId, toId;
    Backend::TransportationTypes::TransportationMode mode{};
    bool hasBacking = false;

    if (connection->connectionModel())
    {
        fromId     = connection->connectionModel()->fromTerminalId;
        toId       = connection->connectionModel()->toTerminalId;
        mode       = connection->connectionModel()->mode;
        hasBacking = true;
    }
    else if (connection->globalLinkModel())
    {
        fromId     = connection->globalLinkModel()->fromTerminalId;
        toId       = connection->globalLinkModel()->toTerminalId;
        mode       = connection->globalLinkModel()->mode;
        hasBacking = true;
    }

    if (hasBacking && mainWindow && mainWindow->runtime())
    {
        auto *doc = &mainWindow->runtime()->document();
        for (auto it = newProperties.constBegin();
             it != newProperties.constEnd(); ++it)
        {
            if (connection->getProperties().value(it.key())
                != it.value())
            {
                GUI::Scenario::ScenarioMutator::
                    setConnectionProperty(
                        doc, fromId, toId, mode,
                        it.key(), it.value());
            }
        }
    }
    else
    {
        // Legacy fallback — update item directly
        connection->updateProperties(newProperties);
    }

    emit propertiesChanged(connection, newProperties);
}

void PropertiesPanel::processEditFields(
    QMap<QString, QVariant> &properties)
{
    // Handle all properties
    for (auto it = editFields.constBegin();
         it != editFields.constEnd(); ++it)
    {
        if (it.key().contains('.'))
        {
            processNestedProperty(properties, it.key(),
                                  it.value());
        }
        else
        {
            processSimpleProperty(properties, it.key(),
                                  it.value());
        }
    }
}

void PropertiesPanel::processNestedProperty(
    QMap<QString, QVariant> &properties, const QString &key,
    QWidget *widget)
{
    QStringList parts = key.split('.');

    // Special handling for interfaces
    if (parts[0] == "interfaces")
    {
        processInterfaceProperty(properties, parts, widget);
        return;
    }

    // Get the value from the widget
    QVariant value = getWidgetValue(widget);

    // Handle nested properties (like
    // dwell_time.parameters.mean)
    if (parts.size() == 2)
    {
        // Two-level nesting (e.g., "dwell_time.method")
        if (properties.contains(parts[0]))
        {
            QVariantMap nestedMap =
                properties[parts[0]].toMap();
            nestedMap[parts[1]]  = value;
            properties[parts[0]] = nestedMap;
        }
        else
        {
            QVariantMap nestedMap;
            nestedMap[parts[1]]  = value;
            properties[parts[0]] = nestedMap;
        }
    }
    else if (parts.size() == 3)
    {
        // Three-level nesting (e.g.,
        // "dwell_time.parameters.mean")
        if (properties.contains(parts[0]))
        {
            QVariantMap topLevelMap =
                properties[parts[0]].toMap();

            if (topLevelMap.contains(parts[1]))
            {
                QVariantMap secondLevelMap =
                    topLevelMap[parts[1]].toMap();
                secondLevelMap[parts[2]] = value;
                topLevelMap[parts[1]]    = secondLevelMap;
            }
            else
            {
                QVariantMap secondLevelMap;
                secondLevelMap[parts[2]] = value;
                topLevelMap[parts[1]]    = secondLevelMap;
            }

            properties[parts[0]] = topLevelMap;
        }
        else
        {
            QVariantMap secondLevelMap;
            secondLevelMap[parts[2]] = value;

            QVariantMap topLevelMap;
            topLevelMap[parts[1]] = secondLevelMap;

            properties[parts[0]] = topLevelMap;
        }
    }
    // Don't add the dotted key as a new property
}

void PropertiesPanel::processInterfaceProperty(
    QMap<QString, QVariant> &properties,
    const QStringList &parts, QWidget *widget)
{
    if (!properties.contains("Available Interfaces"))
    {
        properties["Available Interfaces"] =
            QVariantMap{{"land_side", QVariantList()},
                        {"sea_side", QVariantList()}};
    }

    // Get the current lists
    QVariantMap interfaces =
        properties["Available Interfaces"].toMap();
    QStringList landSide =
        interfaces["land_side"].toStringList();
    QStringList seaSide =
        interfaces["sea_side"].toStringList();

    // Update interfaces based on checkbox state
    if (QCheckBox *checkbox =
            qobject_cast<QCheckBox *>(widget))
    {
        QString mode = parts[2];
        mode[0] =
            mode[0].toUpper(); // Capitalize first letter

        if (parts[1] == "land")
        {
            // Remove the mode if it exists (to avoid
            // duplicates)
            landSide.removeAll(mode);

            // Add it back if checked
            if (checkbox->isChecked()
                && !landSide.contains(mode))
            {
                landSide.append(mode);
            }
        }
        else if (parts[1] == "sea")
        {
            // Remove the mode if it exists (to avoid
            // duplicates)
            seaSide.removeAll(mode);

            // Add it back if checked
            if (checkbox->isChecked()
                && !seaSide.contains(mode))
            {
                seaSide.append(mode);
            }
        }

        // Update the interfaces in the map
        interfaces["land_side"]            = landSide;
        interfaces["sea_side"]             = seaSide;
        properties["Available Interfaces"] = interfaces;
    }
}

void PropertiesPanel::processSimpleProperty(
    QMap<QString, QVariant> &properties, const QString &key,
    QWidget *widget)
{
    properties[key] = getWidgetValue(widget);
}

QVariant PropertiesPanel::getWidgetValue(QWidget *widget)
{
    if (QCheckBox *checkbox =
            qobject_cast<QCheckBox *>(widget))
    {
        return checkbox->isChecked();
    }
    else if (QComboBox *combo =
                 qobject_cast<QComboBox *>(widget))
    {
        return combo->currentText();
    }
    else if (QLineEdit *lineEdit =
                 qobject_cast<QLineEdit *>(widget))
    {
        return lineEdit->text();
    }
    else if (QSpinBox *spinBox =
                 qobject_cast<QSpinBox *>(widget))
    {
        return spinBox->value();
    }
    else if (QDoubleSpinBox *doubleSpinBox =
                 qobject_cast<QDoubleSpinBox *>(widget))
    {
        return doubleSpinBox->value();
    }

    return QVariant(); // Return empty variant if widget
                       // type not recognized
}

void PropertiesPanel::handleRegionChange(
    TerminalItem *terminal, const QString &newRegionName)
{
    QString oldRegionName = terminal->getRegion();

    // Get the new region's center point
    RegionCenterPoint *newRegionCenter = nullptr;
    RegionCenterPoint *oldRegionCenter = nullptr;

    if (mainWindow)
    {
        const auto &centers =
            CargoNetSim::CargoNetSimController::
                getInstance()
                    .getRegionDataController()
                    ->getAllRegionVariableAs<
                        RegionCenterPoint *>(
                        "regionCenterPoint");
        newRegionCenter = centers.value(newRegionName);
        oldRegionCenter = centers.value(oldRegionName);
    }

    if (newRegionCenter && oldRegionCenter)
    {
        // Calculate item's offset from old region center
        QPointF oldOffset(terminal->pos().x()
                              - oldRegionCenter->pos().x(),
                          terminal->pos().y()
                              - oldRegionCenter->pos().y());

        // Apply offset to new region center
        QPointF newPos(
            newRegionCenter->pos().x() + oldOffset.x(),
            newRegionCenter->pos().y() + oldOffset.y());

        // Update item position
        terminal->setPos(newPos);

        // Update region property
        terminal->setRegion(newRegionName);
    }
}

void PropertiesPanel::onCoordSystemChanged(int index)
{
    qCDebug(lcGuiUtil) << "PropertiesPanel::onCoordSystemChanged:"
                       << "index=" << index;
    if (!mainWindow)
    {
        mainWindow = qobject_cast<MainWindow *>(window());
    }

    if (!mainWindow)
    {
        qCWarning(lcGuiUtil) << "PropertiesPanel::onCoordSystemChanged:"
                             << "no MainWindow";
        return;
    }

    GraphicsView *view = mainWindow->getCurrentView();
    if (!view)
    {
        qCWarning(lcGuiUtil) << "PropertiesPanel::onCoordSystemChanged:"
                             << "no current view";
        return;
    }

    view->setUsingProjectedCoords(index == 1);
    mainWindow->updateAllCoordinates();
}

void PropertiesPanel::setExpandingWidgetPolicy(
    QWidget *widget)
{
    if (widget)
    {
        widget->setSizePolicy(QSizePolicy::Expanding,
                              QSizePolicy::Preferred);
    }
}

void PropertiesPanel::clearIfShowing(QGraphicsItem *item)
{
    if (currentItem == item)
    {
        qCDebug(lcGuiUtil)
            << "PropertiesPanel::clearIfShowing:"
            << "clearing panel for removed item";
        clearLayout();
        currentItem = nullptr;
    }
}

} // namespace GUI
} // namespace CargoNetSim
