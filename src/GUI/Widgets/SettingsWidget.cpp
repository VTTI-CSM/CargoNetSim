#include "SettingsWidget.h"

#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Scenario/PropertyKeys.h"
#include "GUI/MainWindow.h"
#include "GUI/Utils/IconCreator.h"
#include <QDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QTableWidgetItem>
#include <QToolButton>
#include <QVBoxLayout>
#include "Backend/Commons/LogCategories.h"

namespace PK = CargoNetSim::Backend::Scenario::PropertyKeys;

namespace CargoNetSim
{
namespace GUI
{

// Constructor
SettingsWidget::SettingsWidget(QWidget *parent)
    : QWidget(parent)
    , configLoader(nullptr)
{
    // Initialize with default values for fuel types
    fuelTypes = {{"HFO",
                  {{"cost", 0.56},
                   {"calorific", 11.1},
                   {"carbon_content", 3.15},
                   {"unit", "L"}}},
                 {"diesel_1",
                  {{"cost", 1.35},
                   {"calorific", 10.7},
                   {"carbon_content", 2.68},
                   {"unit", "L"}}},
                 {"diesel_2",
                  {{"cost", 1.35},
                   {"calorific", 10.0},
                   {"carbon_content", 2.68},
                   {"unit", "L"}}}};

    initUI();

    // Try to load settings after UI is initialized
    loadSettings();
}

void SettingsWidget::initUI()
{
    qCDebug(lcGuiUtil) << "SettingsWidget::initUI: setting up transport mode parameters";
    // Main layout for the widget
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // Create a QScrollArea that will contain all the
    // settings controls
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);

    // Create a container widget for the scroll area
    QWidget *container = new QWidget(this);
    containerLayout    = new QVBoxLayout(container);
    containerLayout->setContentsMargins(10, 10, 10, 10);
    containerLayout->setSpacing(10);

    // --- Simulation Settings Group ---
    simulationGroup =
        new QGroupBox(tr("Simulation"), container);
    QFormLayout *simLayout =
        new QFormLayout(simulationGroup);
    simLayout->setFieldGrowthPolicy(
        QFormLayout::AllNonFixedFieldsGrow);

    timeStepSpin = new QSpinBox(simulationGroup);
    timeStepSpin->setRange(1, 60);
    timeStepSpin->setValue(15);
    timeStepSpin->setSuffix(tr(" minutes"));
    simLayout->addRow(tr("Time Step:"), timeStepSpin);

    // Time value of money global settings
    useSpecificTimeValues =
        new QCheckBox(tr("Use mode-specific time values"),
                      simulationGroup);
    simLayout->addRow("", useSpecificTimeValues);

    averageTimeValueSpin =
        new QDoubleSpinBox(simulationGroup);
    averageTimeValueSpin->setRange(0, 1000.0);
    averageTimeValueSpin->setValue(20.43);
    averageTimeValueSpin->setSuffix(tr(" USD/h"));
    simLayout->addRow(tr("Average Time Value (all modes):"),
                      averageTimeValueSpin);

    shortestPathsSpin = new QSpinBox(simulationGroup);
    shortestPathsSpin->setRange(1, 20);
    shortestPathsSpin->setValue(3);
    shortestPathsSpin->setSuffix(tr(" paths"));
    simLayout->addRow(tr("Number of Shortest Paths:"),
                      shortestPathsSpin);

    containerLayout->addWidget(simulationGroup);

    // --- Fuel Types Table ---
    fuelTypesGroup =
        new QGroupBox(tr("Fuel Types"), container);
    QVBoxLayout *fuelTypesLayout =
        new QVBoxLayout(fuelTypesGroup);

    // Create table for fuel types
    fuelTable = new QTableWidget(
        0, 5); // 5 columns for fuel type, cost, energy,
               // carbon, unit
    fuelTable->setHorizontalHeaderLabels(
        {tr("Fuel Type"), tr("Cost (USD)"),
         tr("Energy Content (kWh)"),
         tr("Carbon Content (kg CO₂)"), tr("Unit")});

    fuelTable->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::Stretch);
    fuelTable->horizontalHeader()->setSectionResizeMode(
        1, QHeaderView::ResizeToContents);
    fuelTable->horizontalHeader()->setSectionResizeMode(
        2, QHeaderView::ResizeToContents);
    fuelTable->horizontalHeader()->setSectionResizeMode(
        3, QHeaderView::ResizeToContents);
    fuelTable->horizontalHeader()->setSectionResizeMode(
        4, QHeaderView::ResizeToContents);

    // Make table taller with a fixed minimum height
    fuelTable->setMinimumHeight(150);

    // Add fuel types to table
    updateFuelTable();

    // Add "Add Fuel Type" button
    QPushButton *addFuelButton = new QPushButton(
        tr("Add Fuel Type"), fuelTypesGroup);
    connect(addFuelButton, &QPushButton::clicked, this,
            &SettingsWidget::addFuelType);

    fuelTypesLayout->addWidget(fuelTable);
    fuelTypesLayout->addWidget(addFuelButton);
    containerLayout->addWidget(fuelTypesGroup);

    // --- Carbon Emissions Pricing Group ---
    carbonGroup = new QGroupBox(
        tr("Carbon Emissions Pricing"), container);
    QFormLayout *carbonLayout =
        new QFormLayout(carbonGroup);
    carbonLayout->setFieldGrowthPolicy(
        QFormLayout::AllNonFixedFieldsGrow);

    carbonRateSpin = new QDoubleSpinBox(carbonGroup);
    carbonRateSpin->setRange(0, 1000);
    carbonRateSpin->setValue(65);
    carbonRateSpin->setSuffix(tr(" per CO₂ ton"));
    carbonLayout->addRow(tr("Rate:"), carbonRateSpin);

    shipMultiplierSpin = new QDoubleSpinBox(carbonGroup);
    shipMultiplierSpin->setRange(0, 10);
    shipMultiplierSpin->setDecimals(2);
    shipMultiplierSpin->setValue(1.2);
    carbonLayout->addRow(tr("Ship Multiplier:"),
                         shipMultiplierSpin);

    truckMultiplierSpin = new QDoubleSpinBox(carbonGroup);
    truckMultiplierSpin->setRange(0, 10);
    truckMultiplierSpin->setDecimals(2);
    truckMultiplierSpin->setValue(1.1);
    carbonLayout->addRow(tr("Truck Multiplier:"),
                         truckMultiplierSpin);

    trainMultiplierSpin = new QDoubleSpinBox(carbonGroup);
    trainMultiplierSpin->setRange(0, 10);
    trainMultiplierSpin->setDecimals(2);
    trainMultiplierSpin->setValue(1.1);
    carbonLayout->addRow(tr("Train Multiplier:"),
                         trainMultiplierSpin);

    containerLayout->addWidget(carbonGroup);

    // --- Transportation Mode Parameters Group ---
    transportGroup =
        new QGroupBox(tr("Transportation Mode Parameters "
                         "For Predictions"),
                      container);
    QVBoxLayout *transportLayout =
        new QVBoxLayout(transportGroup);

    // -- Ship Settings --
    shipGroup = new QGroupBox(tr("Ship"), transportGroup);
    QFormLayout *shipLayout = new QFormLayout(shipGroup);
    shipLayout->setFieldGrowthPolicy(
        QFormLayout::AllNonFixedFieldsGrow);

    // Ship time value of money
    shipTimeValueSpin = new QDoubleSpinBox(shipGroup);
    shipTimeValueSpin->setRange(0, 1000.0);
    shipTimeValueSpin->setValue(13.43);
    shipTimeValueSpin->setSuffix(tr(" USD/h"));
    shipLayout->addRow(tr("Time Value of Money:"),
                       shipTimeValueSpin);

    shipSpeedSpin = new QDoubleSpinBox(shipGroup);
    shipSpeedSpin->setRange(0, 100);
    shipSpeedSpin->setValue(20);
    shipSpeedSpin->setSuffix(tr(" km/h"));
    shipLayout->addRow(tr("Average Speed:"), shipSpeedSpin);

    // Ship fuel type dropdown
    shipFuelType = new QComboBox(shipGroup);
    shipLayout->addRow(tr("Fuel Type:"), shipFuelType);

    // Ship fuel consumption
    QHBoxLayout *shipFuelLayout = new QHBoxLayout();
    shipFuelSpin = new QDoubleSpinBox(shipGroup);
    shipFuelSpin->setRange(0, 600);
    shipFuelSpin->setValue(50);
    shipFuelSpin->setSuffix(tr(" L/km"));
    shipFuelLayout->addWidget(shipFuelSpin);

    QToolButton *shipCalcButton =
        new QToolButton(shipGroup);
    shipCalcButton->setIcon(
        QIcon(IconFactory::createCalculatorIcon()));
    shipCalcButton->setToolTip(
        tr("Calculate energy from fuel consumption"));
    connect(shipCalcButton, &QToolButton::clicked,
            [this]() { showEnergyCalculator("ship"); });
    shipCalcButton->setMaximumWidth(30);
    shipFuelLayout->addWidget(shipCalcButton);

    shipLayout->addRow(tr("Fuel Consumption:"),
                       shipFuelLayout);

    shipContainers = new QSpinBox(shipGroup);
    shipContainers->setRange(1, 10000000);
    shipContainers->setSingleStep(200);
    shipContainers->setValue(5000);
    shipLayout->addRow(tr("Average Number of Containers:"),
                       shipContainers);

    shipRiskSpin = new QDoubleSpinBox(shipGroup);
    shipRiskSpin->setRange(0, 1);
    shipRiskSpin->setDecimals(3);
    shipRiskSpin->setValue(0.025);
    shipLayout->addRow(tr("Risk Factor:"), shipRiskSpin);

    transportLayout->addWidget(shipGroup);

    // -- Train Settings --
    trainGroup = new QGroupBox(tr("Rail"), transportGroup);
    QFormLayout *trainLayout = new QFormLayout(trainGroup);
    trainLayout->setFieldGrowthPolicy(
        QFormLayout::AllNonFixedFieldsGrow);

    // Train time value of money
    trainTimeValueSpin = new QDoubleSpinBox(trainGroup);
    trainTimeValueSpin->setRange(0, 1000.0);
    trainTimeValueSpin->setValue(16.43);
    trainTimeValueSpin->setSuffix(tr(" USD/h"));
    trainLayout->addRow(tr("Time Value of Money:"),
                        trainTimeValueSpin);

    // Speed settings with checkbox
    QHBoxLayout *trainSpeedLayout = new QHBoxLayout();
    trainSpeedSpin = new QDoubleSpinBox(trainGroup);
    trainSpeedSpin->setRange(0, 200);
    trainSpeedSpin->setValue(40);
    trainSpeedSpin->setSuffix(tr(" km/h"));
    trainSpeedLayout->addWidget(trainSpeedSpin);

    trainUseNetwork =
        new QCheckBox(tr("Use Network"), trainGroup);
    connect(trainUseNetwork, &QCheckBox::toggled,
            trainSpeedSpin, &QDoubleSpinBox::setDisabled);
    trainSpeedLayout->addWidget(trainUseNetwork);

    trainLayout->addRow(tr("Average Speed:"),
                        trainSpeedLayout);

    // Train fuel type dropdown
    trainFuelType = new QComboBox(trainGroup);
    trainLayout->addRow(tr("Fuel Type:"), trainFuelType);

    // Train fuel consumption
    QHBoxLayout *trainFuelLayout = new QHBoxLayout();
    trainFuelSpin = new QDoubleSpinBox(trainGroup);
    trainFuelSpin->setRange(0, 600);
    trainFuelSpin->setValue(20);
    trainFuelSpin->setSuffix(tr(" L/km"));
    trainFuelLayout->addWidget(trainFuelSpin);

    QToolButton *trainCalcButton =
        new QToolButton(trainGroup);
    trainCalcButton->setIcon(
        QIcon(IconFactory::createCalculatorIcon()));
    trainCalcButton->setToolTip(
        tr("Calculate energy from fuel consumption"));
    connect(trainCalcButton, &QToolButton::clicked,
            [this]() { showEnergyCalculator("rail"); });
    trainCalcButton->setMaximumWidth(30);
    trainFuelLayout->addWidget(trainCalcButton);

    trainLayout->addRow(
        tr("Fuel Consumption per Locomotive:"),
        trainFuelLayout);

    trainContainers = new QSpinBox(trainGroup);
    trainContainers->setRange(1, 10000000);
    trainContainers->setSingleStep(10);
    trainContainers->setValue(400);
    trainLayout->addRow(tr("Average Number of Containers:"),
                        trainContainers);

    trainRiskSpin = new QDoubleSpinBox(trainGroup);
    trainRiskSpin->setRange(0, 1);
    trainRiskSpin->setDecimals(3);
    trainRiskSpin->setValue(0.006);
    trainLayout->addRow(tr("Risk Factor:"), trainRiskSpin);

    transportLayout->addWidget(trainGroup);

    // -- Truck Settings --
    truckGroup = new QGroupBox(tr("Truck"), transportGroup);
    QFormLayout *truckLayout = new QFormLayout(truckGroup);
    truckLayout->setFieldGrowthPolicy(
        QFormLayout::AllNonFixedFieldsGrow);

    // Truck time value of money
    truckTimeValueSpin = new QDoubleSpinBox(truckGroup);
    truckTimeValueSpin->setRange(0, 1000.0);
    truckTimeValueSpin->setValue(31.43);
    truckTimeValueSpin->setSuffix(tr(" USD/h"));
    truckLayout->addRow(tr("Time Value of Money:"),
                        truckTimeValueSpin);

    QHBoxLayout *truckSpeedLayout = new QHBoxLayout();
    truckSpeedSpin = new QDoubleSpinBox(truckGroup);
    truckSpeedSpin->setRange(0, 200);
    truckSpeedSpin->setValue(70);
    truckSpeedSpin->setSuffix(tr(" km/h"));
    truckSpeedLayout->addWidget(truckSpeedSpin);

    truckUseNetwork =
        new QCheckBox(tr("Use Network"), truckGroup);
    connect(truckUseNetwork, &QCheckBox::toggled,
            truckSpeedSpin, &QDoubleSpinBox::setDisabled);
    truckSpeedLayout->addWidget(truckUseNetwork);

    truckLayout->addRow(tr("Average Speed:"),
                        truckSpeedLayout);

    // Truck fuel type dropdown
    truckFuelType = new QComboBox(truckGroup);
    truckLayout->addRow(tr("Fuel Type:"), truckFuelType);

    // Truck fuel consumption
    QHBoxLayout *truckFuelLayout = new QHBoxLayout();
    truckFuelSpin = new QDoubleSpinBox(truckGroup);
    truckFuelSpin->setRange(0, 600);
    truckFuelSpin->setValue(15);
    truckFuelSpin->setSuffix(tr(" L/km"));
    truckFuelLayout->addWidget(truckFuelSpin);

    QToolButton *truckCalcButton =
        new QToolButton(truckGroup);
    truckCalcButton->setIcon(
        QIcon(IconFactory::createCalculatorIcon()));
    truckCalcButton->setToolTip(
        tr("Calculate energy from fuel consumption"));
    connect(truckCalcButton, &QToolButton::clicked,
            [this]() { showEnergyCalculator("truck"); });
    truckCalcButton->setMaximumWidth(30);
    truckFuelLayout->addWidget(truckCalcButton);

    truckLayout->addRow(tr("Fuel Consumption:"),
                        truckFuelLayout);

    truckContainers = new QSpinBox(truckGroup);
    truckContainers->setRange(1, 100);
    truckContainers->setValue(1);
    truckLayout->addRow(tr("Average Number of Containers:"),
                        truckContainers);

    truckRiskSpin = new QDoubleSpinBox(truckGroup);
    truckRiskSpin->setRange(0, 1);
    truckRiskSpin->setDecimals(3);
    truckRiskSpin->setValue(0.012);
    truckLayout->addRow(tr("Risk Factor:"), truckRiskSpin);

    transportLayout->addWidget(truckGroup);

    containerLayout->addWidget(transportGroup);

    // Connect checkbox to enable/disable mode-specific time
    // values
    connect(useSpecificTimeValues, &QCheckBox::toggled,
            [this](bool checked) {
                shipTimeValueSpin->setEnabled(checked);
                trainTimeValueSpin->setEnabled(checked);
                truckTimeValueSpin->setEnabled(checked);
            });

    // Initially disable mode-specific time values if not
    // checked
    shipTimeValueSpin->setEnabled(false);
    trainTimeValueSpin->setEnabled(false);
    truckTimeValueSpin->setEnabled(false);

    // Add fuel types to all dropdowns
    updateFuelTypeDropdowns();

    // Spacer in the container
    containerLayout->addStretch();

    // Apply Settings Button inside the scrollable content
    applyButton =
        new QPushButton(tr("Apply Settings"), container);
    connect(applyButton, &QPushButton::clicked, this,
            &SettingsWidget::applySettings);
    containerLayout->addWidget(applyButton);

    // Set the container as the widget for the scroll area
    scrollArea->setWidget(container);

    // Add the scroll area to the main layout
    mainLayout->addWidget(scrollArea);
}

void SettingsWidget::updateFuelTable()
{
    qCDebug(lcGuiUtil) << "SettingsWidget::updateFuelTable: fuel types:" << fuelTypes.size();
    fuelTable->setRowCount(
        fuelTypes.size()); // Set row count to match number
                           // of fuel types

    int row = 0;
    for (auto it = fuelTypes.begin(); it != fuelTypes.end();
         ++it, ++row)
    {
        const QString                 &fuelType = it.key();
        const QMap<QString, QVariant> &data = it.value();

        // Fuel type name
        QTableWidgetItem *nameItem =
            new QTableWidgetItem(fuelType);
        fuelTable->setItem(row, 0, nameItem);

        // Cost
        QDoubleSpinBox *costSpin = new QDoubleSpinBox();
        costSpin->setRange(0, 10000);
        costSpin->setDecimals(2);
        costSpin->setValue(data["cost"].toDouble());

        costSpin->setSuffix(tr(" per ")
                            + data["unit"].toString());

        fuelTable->setCellWidget(row, 1, costSpin);

        // Calorific value
        QDoubleSpinBox *calorificSpin =
            new QDoubleSpinBox();
        calorificSpin->setRange(0, 100);
        calorificSpin->setDecimals(1);
        calorificSpin->setValue(
            data["calorific"].toDouble());
        calorificSpin->setSuffix(tr(" kWh/")
                                 + data["unit"].toString());
        fuelTable->setCellWidget(row, 2, calorificSpin);

        // Carbon content
        QDoubleSpinBox *carbonSpin = new QDoubleSpinBox();
        carbonSpin->setRange(0, 10);
        carbonSpin->setDecimals(2);
        carbonSpin->setValue(
            data["carbon_content"].toDouble());
        carbonSpin->setSuffix(tr(" kg CO₂/")
                              + data["unit"].toString());
        fuelTable->setCellWidget(row, 3, carbonSpin);

        // Unit
        QComboBox *unitCombo = new QComboBox();
        unitCombo->addItems({"L"});
        unitCombo->setCurrentText(data["unit"].toString());
        fuelTable->setCellWidget(row, 4, unitCombo);

        // Connect signals to update fuel data when values
        // change
        connect(costSpin,
                QOverload<double>::of(
                    &QDoubleSpinBox::valueChanged),
                [this, fuelType](double value) {
                    updateFuelData(fuelType, "cost", value);
                });

        connect(calorificSpin,
                QOverload<double>::of(
                    &QDoubleSpinBox::valueChanged),
                [this, fuelType](double value) {
                    updateFuelData(fuelType, "calorific",
                                   value);
                });

        connect(carbonSpin,
                QOverload<double>::of(
                    &QDoubleSpinBox::valueChanged),
                [this, fuelType](double value) {
                    updateFuelData(fuelType,
                                   "carbon_content", value);
                });

        connect(unitCombo, &QComboBox::currentTextChanged,
                [this, fuelType](const QString &value) {
                    updateFuelData(fuelType, "unit", value);
                });
    }
}

void SettingsWidget::updateFuelData(const QString &fuelType,
                                    const QString &key,
                                    const QVariant &value)
{
    qCDebug(lcGuiUtil) << "SettingsWidget::updateFuelData:"
                       << "fuel=" << fuelType << "key=" << key;
    if (fuelTypes.contains(fuelType))
    {
        fuelTypes[fuelType][key] = value;
        qCDebug(lcGuiUtil) << "SettingsWidget: fuel parameter updated:" << key << "=" << value;

        // If unit changes, update the suffixes in the table
        if (key == "unit")
        {
            for (int row = 0; row < fuelTable->rowCount();
                 ++row)
            {
                QTableWidgetItem *nameItem =
                    fuelTable->item(row, 0);
                if (nameItem
                    && nameItem->text() == fuelType)
                {
                    QDoubleSpinBox *costSpin =
                        qobject_cast<QDoubleSpinBox *>(
                            fuelTable->cellWidget(row, 1));
                    QDoubleSpinBox *calorificSpin =
                        qobject_cast<QDoubleSpinBox *>(
                            fuelTable->cellWidget(row, 2));
                    QDoubleSpinBox *carbonSpin =
                        qobject_cast<QDoubleSpinBox *>(
                            fuelTable->cellWidget(row, 3));

                    QString unitStr = value.toString();

                    costSpin->setSuffix(tr(" per ")
                                        + unitStr);

                    calorificSpin->setSuffix(tr(" kWh/")
                                             + unitStr);
                    carbonSpin->setSuffix(tr(" kg CO₂/")
                                          + unitStr);
                    break;
                }
            }
        }
    }
}

void SettingsWidget::addFuelType()
{
    qCDebug(lcGuiUtil) << "SettingsWidget::addFuelType: begin";
    // Create a dialog to get the fuel type name
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Add Fuel Type"));
    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QLabel *nameLabel =
        new QLabel(tr("Fuel Type Name:"), this);
    QLineEdit *nameEdit = new QLineEdit(this);
    layout->addWidget(nameLabel);
    layout->addWidget(nameEdit);

    QHBoxLayout *buttonLayout = new QHBoxLayout(this);
    QPushButton *cancelButton =
        new QPushButton(tr("Cancel"), this);
    connect(cancelButton, &QPushButton::clicked, &dialog,
            &QDialog::reject);

    QPushButton *addButton =
        new QPushButton(tr("Add"), this);
    connect(addButton, &QPushButton::clicked, &dialog,
            &QDialog::accept);

    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(addButton);
    layout->addLayout(buttonLayout);

    if (dialog.exec() == QDialog::Accepted)
    {
        QString fuelName = nameEdit->text().trimmed();
        if (!fuelName.isEmpty()
            && !fuelTypes.contains(fuelName))
        {
            // Add new fuel type with default values
            fuelTypes[fuelName] = {{"cost", 1.0},
                                   {"calorific", 10.0},
                                   {"carbon_content", 2.68},
                                   {"unit", "L"}};

            // Update the table and dropdowns
            updateFuelTable();
            updateFuelTypeDropdowns();
        }
    }
}

void SettingsWidget::updateFuelTypeDropdowns()
{
    // Store current selections
    QString shipFuel  = shipFuelType->count() > 0
                            ? shipFuelType->currentText()
                            : "HFO";
    QString trainFuel = trainFuelType->count() > 0
                            ? trainFuelType->currentText()
                            : "diesel_1";
    QString truckFuel = truckFuelType->count() > 0
                            ? truckFuelType->currentText()
                            : "diesel_2";

    // Clear and refill dropdowns
    shipFuelType->clear();
    trainFuelType->clear();
    truckFuelType->clear();

    for (auto it = fuelTypes.begin(); it != fuelTypes.end();
         ++it)
    {
        const QString &fuelType = it.key();
        shipFuelType->addItem(fuelType);
        trainFuelType->addItem(fuelType);
        truckFuelType->addItem(fuelType);
    }

    // Restore selections if they still exist
    int shipIdx  = shipFuelType->findText(shipFuel);
    int trainIdx = trainFuelType->findText(trainFuel);
    int truckIdx = truckFuelType->findText(truckFuel);

    if (shipIdx >= 0)
        shipFuelType->setCurrentIndex(shipIdx);
    if (trainIdx >= 0)
        trainFuelType->setCurrentIndex(trainIdx);
    if (truckIdx >= 0)
        truckFuelType->setCurrentIndex(truckIdx);
}

bool SettingsWidget::loadSettings()
{
    qCInfo(lcGuiUtil) << "SettingsWidget::loadSettings: begin";
    try
    {
        // load settings from the controller
        CargoNetSim::CargoNetSimController::getInstance()
            .getConfigController()
            ->loadConfig();
        settings = CargoNetSim::CargoNetSimController::
                       getInstance()
                           .getConfigController()
                           ->getAllParams();

        qCDebug(lcGuiUtil) << "SettingsWidget::loadSettings: loaded"
                           << settings.size() << "top-level sections";
        // Apply simulation settings
        if (settings.contains("simulation"))
        {
            QMap<QString, QVariant> simSettings =
                settings["simulation"].toMap();

            if (simSettings.contains("time_step"))
                timeStepSpin->setValue(
                    simSettings["time_step"].toInt());

            // Load the average time value of money
            if (simSettings.contains(PK::Mode::TimeValueOfMoney))
                averageTimeValueSpin->setValue(
                    simSettings[PK::Mode::TimeValueOfMoney]
                        .toDouble());

            // Load the use_mode_specific flag
            if (simSettings.contains("use_mode_specific"))
                useSpecificTimeValues->setChecked(
                    simSettings["use_mode_specific"]
                        .toBool());

            if (simSettings.contains("shortest_paths"))
                shortestPathsSpin->setValue(
                    simSettings["shortest_paths"].toInt());
        }

        // Apply carbon tax settings
        if (settings.contains("carbon_taxes"))
        {
            QMap<QString, QVariant> carbonSettings =
                settings["carbon_taxes"].toMap();

            if (carbonSettings.contains("rate"))
                carbonRateSpin->setValue(
                    carbonSettings["rate"].toDouble());

            if (carbonSettings.contains("ship_multiplier"))
                shipMultiplierSpin->setValue(
                    carbonSettings["ship_multiplier"]
                        .toDouble());

            if (carbonSettings.contains("truck_multiplier"))
                truckMultiplierSpin->setValue(
                    carbonSettings["truck_multiplier"]
                        .toDouble());

            if (carbonSettings.contains("train_multiplier"))
                trainMultiplierSpin->setValue(
                    carbonSettings["train_multiplier"]
                        .toDouble());
        }

        // Load fuel types data from settings
        if (settings.contains("fuel_energy")
            && settings.contains("fuel_prices")
            && settings.contains("fuel_carbon_content"))
        {

            QMap<QString, QVariant> fuelEnergyMap =
                settings["fuel_energy"].toMap();
            QMap<QString, QVariant> fuelPricesMap =
                settings["fuel_prices"].toMap();
            QMap<QString, QVariant> fuelCarbonMap =
                settings["fuel_carbon_content"].toMap();

            // Clear existing fuel types and reload from
            // settings
            fuelTypes.clear();
            // Combine all fuel type keys from all three
            // maps
            QSet<QString> allFuelKeys;
            for (auto it = fuelEnergyMap.begin();
                 it != fuelEnergyMap.end(); ++it)
                allFuelKeys.insert(it.key());
            for (auto it = fuelPricesMap.begin();
                 it != fuelPricesMap.end(); ++it)
                allFuelKeys.insert(it.key());
            for (auto it = fuelCarbonMap.begin();
                 it != fuelCarbonMap.end(); ++it)
                allFuelKeys.insert(it.key());

            // Create fuel type entries
            for (const QString &fuelKey : allFuelKeys)
            {
                QMap<QString, QVariant> fuelData;

                // Set default unit if not already in the
                // data
                fuelData["unit"] = "L";

                // Add energy content (calorific value)
                if (fuelEnergyMap.contains(fuelKey))
                    fuelData["calorific"] =
                        fuelEnergyMap[fuelKey];

                // Add price
                if (fuelPricesMap.contains(fuelKey))
                    fuelData["cost"] =
                        fuelPricesMap[fuelKey];

                // Add carbon content
                if (fuelCarbonMap.contains(fuelKey))
                    fuelData["carbon_content"] =
                        fuelCarbonMap[fuelKey];

                // Store the fuel type data
                fuelTypes[fuelKey] = fuelData;
            }

            // Update the fuel table and dropdowns
            updateFuelTable();
            updateFuelTypeDropdowns();
        }

        // Apply transport mode settings
        if (settings.contains("transport_modes"))
        {
            QMap<QString, QVariant> transportModes =
                settings["transport_modes"].toMap();

            // Ship settings
            if (transportModes.contains("ship"))
            {
                QMap<QString, QVariant> shipSettings =
                    transportModes["ship"].toMap();

                // Load mode-specific time value of money
                if (shipSettings.contains(
                        PK::Mode::TimeValueOfMoney))
                    shipTimeValueSpin->setValue(
                        shipSettings[PK::Mode::TimeValueOfMoney]
                            .toDouble());

                if (shipSettings.contains(PK::Mode::AverageSpeed))
                    shipSpeedSpin->setValue(
                        shipSettings[PK::Mode::AverageSpeed]
                            .toDouble());

                if (shipSettings.contains(
                        PK::Mode::AverageFuelConsumption))
                    shipFuelSpin->setValue(
                        shipSettings
                            [PK::Mode::AverageFuelConsumption]
                                .toDouble());

                if (shipSettings.contains(
                        PK::Mode::AverageContainerNumber))
                    shipContainers->setValue(
                        shipSettings
                            [PK::Mode::AverageContainerNumber]
                                .toInt());

                if (shipSettings.contains(PK::Mode::RiskFactor))
                    shipRiskSpin->setValue(
                        shipSettings[PK::Mode::RiskFactor]
                            .toDouble());

                if (shipSettings.contains(PK::Mode::FuelType))
                {
                    QString shipFuelTypeName =
                        shipSettings[PK::Mode::FuelType]
                            .toString();
                    int idx = shipFuelType->findText(
                        shipFuelTypeName);
                    if (idx >= 0)
                    {
                        shipFuelType->setCurrentIndex(idx);
                    }
                }
            }

            // Rail settings
            if (transportModes.contains("rail"))
            {
                QMap<QString, QVariant> railSettings =
                    transportModes["rail"].toMap();

                // Load mode-specific time value of money
                if (railSettings.contains(
                        PK::Mode::TimeValueOfMoney))
                    trainTimeValueSpin->setValue(
                        railSettings[PK::Mode::TimeValueOfMoney]
                            .toDouble());

                if (railSettings.contains(PK::Mode::AverageSpeed))
                    trainSpeedSpin->setValue(
                        railSettings[PK::Mode::AverageSpeed]
                            .toDouble());

                if (railSettings.contains(PK::Mode::UseNetwork))
                    trainUseNetwork->setChecked(
                        railSettings[PK::Mode::UseNetwork]
                            .toBool());

                if (railSettings.contains(
                        PK::Mode::AverageFuelConsumption))
                    trainFuelSpin->setValue(
                        railSettings
                            [PK::Mode::AverageFuelConsumption]
                                .toDouble());

                if (railSettings.contains(
                        PK::Mode::AverageContainerNumber))
                    trainContainers->setValue(
                        railSettings
                            [PK::Mode::AverageContainerNumber]
                                .toInt());

                if (railSettings.contains(PK::Mode::RiskFactor))
                    trainRiskSpin->setValue(
                        railSettings[PK::Mode::RiskFactor]
                            .toDouble());

                if (railSettings.contains(PK::Mode::FuelType))
                {
                    QString trainFuelTypeName =
                        railSettings[PK::Mode::FuelType]
                            .toString();
                    int idx = trainFuelType->findText(
                        trainFuelTypeName);
                    if (idx >= 0)
                    {
                        trainFuelType->setCurrentIndex(idx);
                    }
                }
            }

            // Truck settings
            if (transportModes.contains("truck"))
            {
                QMap<QString, QVariant> truckSettings =
                    transportModes["truck"].toMap();

                // Load mode-specific time value of money
                if (truckSettings.contains(
                        PK::Mode::TimeValueOfMoney))
                    truckTimeValueSpin->setValue(
                        truckSettings[PK::Mode::TimeValueOfMoney]
                            .toDouble());

                if (truckSettings.contains(PK::Mode::AverageSpeed))
                    truckSpeedSpin->setValue(
                        truckSettings[PK::Mode::AverageSpeed]
                            .toDouble());

                if (truckSettings.contains(PK::Mode::UseNetwork))
                    truckUseNetwork->setChecked(
                        truckSettings[PK::Mode::UseNetwork]
                            .toBool());

                if (truckSettings.contains(
                        PK::Mode::AverageFuelConsumption))
                    truckFuelSpin->setValue(
                        truckSettings
                            [PK::Mode::AverageFuelConsumption]
                                .toDouble());

                if (truckSettings.contains(
                        PK::Mode::AverageContainerNumber))
                    truckContainers->setValue(
                        truckSettings
                            [PK::Mode::AverageContainerNumber]
                                .toInt());

                if (truckSettings.contains(PK::Mode::RiskFactor))
                    truckRiskSpin->setValue(
                        truckSettings[PK::Mode::RiskFactor]
                            .toDouble());

                if (truckSettings.contains(PK::Mode::FuelType))
                {
                    QString truckFuelTypeName =
                        truckSettings[PK::Mode::FuelType]
                            .toString();
                    int idx = truckFuelType->findText(
                        truckFuelTypeName);
                    if (idx >= 0)
                    {
                        truckFuelType->setCurrentIndex(idx);
                    }
                }
            }
        }

        // Enable/disable the mode-specific time value
        // spinners based on checkbox
        shipTimeValueSpin->setEnabled(
            useSpecificTimeValues->isChecked());
        trainTimeValueSpin->setEnabled(
            useSpecificTimeValues->isChecked());
        truckTimeValueSpin->setEnabled(
            useSpecificTimeValues->isChecked());

        return true;
    }
    catch (const std::exception &e)
    {
        qCWarning(lcGuiUtil)
            << "Failed to load settings from config file:"
            << e.what();
        return false;
    }
}

void SettingsWidget::applySettings()
{
    qCInfo(lcGuiUtil) << "SettingsWidget::applySettings: begin";
    // Collect fuel energy and prices data
    QMap<QString, QVariant> fuelEnergy;
    QMap<QString, QVariant> fuelPrices;
    QMap<QString, QVariant> fuelCarbonContent;

    for (auto it = fuelTypes.begin(); it != fuelTypes.end();
         ++it)
    {
        const QString                 &fuelType = it.key();
        const QMap<QString, QVariant> &data = it.value();

        fuelEnergy[fuelType] = data["calorific"];
        fuelPrices[fuelType] = data["cost"];
        fuelCarbonContent[fuelType] =
            data["carbon_content"];
    }

    // Create a new settings map
    QMap<QString, QVariant> newSettings;

    // Simulation settings
    QMap<QString, QVariant> simulation;
    simulation["time_step"] = timeStepSpin->value();
    simulation[PK::Mode::TimeValueOfMoney] =
        averageTimeValueSpin->value();
    simulation["use_mode_specific"] =
        useSpecificTimeValues->isChecked();
    simulation["shortest_paths"] =
        shortestPathsSpin->value();
    newSettings["simulation"] = simulation;

    // Fuel data
    newSettings["fuel_energy"]         = fuelEnergy;
    newSettings["fuel_prices"]         = fuelPrices;
    newSettings["fuel_carbon_content"] = fuelCarbonContent;

    // Carbon taxes
    QMap<QString, QVariant> carbonTaxes;
    carbonTaxes["rate"] = carbonRateSpin->value();
    carbonTaxes["ship_multiplier"] =
        shipMultiplierSpin->value();
    carbonTaxes["truck_multiplier"] =
        truckMultiplierSpin->value();
    carbonTaxes["train_multiplier"] =
        trainMultiplierSpin->value();
    newSettings["carbon_taxes"] = carbonTaxes;

    // Transport modes
    QMap<QString, QVariant> transportModes;

    // Ship settings
    QMap<QString, QVariant> shipSettings;
    shipSettings[PK::Mode::AverageSpeed] = shipSpeedSpin->value();
    shipSettings[PK::Mode::AverageFuelConsumption] =
        shipFuelSpin->value();
    shipSettings[PK::Mode::AverageContainerNumber] =
        shipContainers->value();
    shipSettings[PK::Mode::RiskFactor] = shipRiskSpin->value();
    shipSettings[PK::Mode::FuelType] = shipFuelType->currentText();
    shipSettings[PK::Mode::TimeValueOfMoney] =
        shipTimeValueSpin->value();
    transportModes["ship"] = shipSettings;

    // Train settings
    QMap<QString, QVariant> trainSettings;
    trainSettings[PK::Mode::AverageSpeed] =
        trainSpeedSpin->value();
    trainSettings[PK::Mode::UseNetwork] =
        trainUseNetwork->isChecked();
    trainSettings[PK::Mode::AverageFuelConsumption] =
        trainFuelSpin->value();
    trainSettings[PK::Mode::AverageContainerNumber] =
        trainContainers->value();
    trainSettings[PK::Mode::RiskFactor] = trainRiskSpin->value();
    trainSettings[PK::Mode::FuelType] =
        trainFuelType->currentText();
    trainSettings[PK::Mode::TimeValueOfMoney] =
        trainTimeValueSpin->value();
    transportModes["rail"] = trainSettings;

    // Truck settings
    QMap<QString, QVariant> truckSettings;
    truckSettings[PK::Mode::AverageSpeed] =
        truckSpeedSpin->value();
    truckSettings[PK::Mode::UseNetwork] =
        truckUseNetwork->isChecked();
    truckSettings[PK::Mode::AverageFuelConsumption] =
        truckFuelSpin->value();
    truckSettings[PK::Mode::AverageContainerNumber] =
        truckContainers->value();
    truckSettings[PK::Mode::RiskFactor] = truckRiskSpin->value();
    truckSettings[PK::Mode::FuelType] =
        truckFuelType->currentText();
    truckSettings[PK::Mode::TimeValueOfMoney] =
        truckTimeValueSpin->value();
    transportModes["truck"] = truckSettings;

    newSettings["transport_modes"] = transportModes;

    // Update settings in the controller
    CargoNetSim::CargoNetSimController::getInstance()
        .getConfigController()
        ->updateConfig(newSettings);
    CargoNetSim::CargoNetSimController::getInstance()
        .getConfigController()
        ->saveConfig();

    settings = newSettings;

    // Emit signal that settings have changed
    emit settingsChanged(newSettings);

    // Settings applied message
    if (!parent())
    {
        return;
    }
    MainWindow *mainWindow =
        dynamic_cast<MainWindow *>(parent());
    if (!mainWindow)
    {
        return;
    }
    mainWindow->showStatusBarMessage("Settings applied.",
                                     3000);
}

QMap<QString, QVariant> SettingsWidget::getSettings() const
{
    return settings;
}

void SettingsWidget::showEnergyCalculator(
    const QString &mode)
{
    qCDebug(lcGuiUtil) << "SettingsWidget::showEnergyCalculator:"
                       << "mode=" << mode;
    QDialog dialog(this);
    dialog.setWindowTitle(
        tr("Energy Consumption Calculator"));
    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    // Get current fuel consumption from settings
    double currentFuelConsumption = 0.0;
    if (!settings.isEmpty()
        && settings.contains("transport_modes"))
    {
        auto transportModes =
            settings["transport_modes"].toMap();
        if (transportModes.contains(mode))
        {
            currentFuelConsumption =
                transportModes[mode]
                    .toMap()[PK::Mode::AverageFuelConsumption]
                    .toDouble();
        }
    }

    // Get the fuel type for the selected mode
    QString         fuelType;
    QDoubleSpinBox *modeFuelSpin = nullptr;

    if (mode == "ship")
    {
        fuelType     = shipFuelType->currentText();
        modeFuelSpin = shipFuelSpin;
    }
    else if (mode == "rail")
    {
        fuelType     = trainFuelType->currentText();
        modeFuelSpin = trainFuelSpin;
    }
    else if (mode == "truck")
    {
        fuelType     = truckFuelType->currentText();
        modeFuelSpin = truckFuelSpin;
    }

    // Input for fuel consumption
    QHBoxLayout    *fuelLayout = new QHBoxLayout();
    QDoubleSpinBox *fuelSpin = new QDoubleSpinBox(&dialog);
    fuelSpin->setRange(0, 500);
    fuelSpin->setDecimals(2);
    fuelSpin->setValue(currentFuelConsumption);

    // Set proper suffix based on unit
    QString unit = fuelTypes.value(fuelType)
                       .value("unit", "L")
                       .toString();
    fuelSpin->setSuffix(tr(" %1/km").arg(unit));

    fuelLayout->addWidget(
        new QLabel(tr("Fuel Consumption:"), &dialog));
    fuelLayout->addWidget(fuelSpin);
    layout->addLayout(fuelLayout);

    // Show the selected fuel type
    QLabel *fuelTypeLabel = new QLabel(
        tr("Fuel Type: %1").arg(fuelType), &dialog);
    layout->addWidget(fuelTypeLabel);

    // Get calorific value from table
    double calorificValue = fuelTypes.value(fuelType)
                                .value("calorific", 10.0)
                                .toDouble();

    // Display calorific value
    QLabel *calorificLabel =
        new QLabel(tr("Calorific Value: %1 kWh/%2")
                       .arg(calorificValue)
                       .arg(unit),
                   &dialog);
    layout->addWidget(calorificLabel);

    // Result label
    QLabel *resultLabel = new QLabel(&dialog);
    layout->addWidget(resultLabel);

    // Calculate button
    QPushButton *calcButton =
        new QPushButton(tr("Calculate"), &dialog);
    connect(calcButton, &QPushButton::clicked, [=]() {
        double fuelConsumption = fuelSpin->value();
        double energy = fuelConsumption * calorificValue;
        resultLabel->setText(
            tr("Energy Consumption: %1 kWh/km")
                .arg(energy, 0, 'f', 2));

        // Update the corresponding spin box
        if (modeFuelSpin)
        {
            modeFuelSpin->setValue(fuelConsumption);
        }
    });
    layout->addWidget(calcButton);

    // Close button
    QPushButton *closeButton =
        new QPushButton(tr("Close"), &dialog);
    connect(closeButton, &QPushButton::clicked, &dialog,
            &QDialog::close);
    layout->addWidget(closeButton);

    dialog.exec();
}

QPair<QFormLayout *, QMap<QString, QWidget *>>
SettingsWidget::createDwellTimeParameters(
    const QString                 &method,
    const QMap<QString, QVariant> &currentParams)
{

    QFormLayout *paramLayout = new QFormLayout();
    paramLayout->setFieldGrowthPolicy(
        QFormLayout::AllNonFixedFieldsGrow);
    QMap<QString, QWidget *> paramFields;

    if (method == "gamma")
    {
        // Shape parameter
        QLineEdit *shape = new QLineEdit(
            currentParams.value("shape", "2.0").toString());
        paramFields["shape"] = shape;
        paramLayout->addRow(tr("Shape (k):"), shape);

        // Scale parameter
        QLineEdit *scale = new QLineEdit(
            currentParams.value("scale", "1440")
                .toString());
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
        paramFields["scale"] = scale;
        paramLayout->addRow(tr("Scale (λ) minutes:"),
                            scale);
    }
    else if (method == "normal")
    {
        // Mean parameter
        QLineEdit *mean = new QLineEdit(
            currentParams.value("mean", "2880").toString());
        paramFields["mean"] = mean;
        paramLayout->addRow(tr("Mean (minutes):"), mean);

        // Standard deviation parameter
        QLineEdit *stdDev = new QLineEdit(
            currentParams.value("std_dev", "720")
                .toString());
        paramFields["std_dev"] = stdDev;
        paramLayout->addRow(tr("Std Dev (minutes):"),
                            stdDev);
    }
    else if (method == "lognormal")
    {
        // Mean parameter (log-scale)
        QLineEdit *mean = new QLineEdit(
            currentParams.value("mean", "3.45").toString());
        paramFields["mean"] = mean;
        paramLayout->addRow(tr("Mean (log-scale):"), mean);

        // Sigma parameter
        QLineEdit *sigma = new QLineEdit(
            currentParams.value("sigma", "0.25")
                .toString());
        paramFields["sigma"] = sigma;
        paramLayout->addRow(tr("Sigma:"), sigma);
    }

    return qMakePair(paramLayout, paramFields);
}

void SettingsWidget::onDwellMethodChanged(
    const QString &method, QGroupBox *dwellGroup)
{
    qCDebug(lcGuiUtil) << "SettingsWidget::onDwellMethodChanged:"
                       << "method=" << method;
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
        for (auto it = settings.begin();
             it != settings.end(); ++it)
        {
            if (it.key().startsWith(
                    "dwell_time.parameters."))
            {
                QString paramName =
                    it.key().split(".").last();
                currentParams[paramName] = it.value();
            }
        }

        // Remove the old layout
        while (oldParamLayout->count())
        {
            QLayoutItem *item = oldParamLayout->takeAt(0);
            if (item->widget())
            {
                item->widget()->deleteLater();
            }
            delete item;
        }
        dwellGroup->layout()->removeItem(oldParamLayout);
        delete oldParamLayout;
    }

    // Create and add new parameter fields
    QPair<QFormLayout *, QMap<QString, QWidget *>>
        paramData = createDwellTimeParameters(method);
    QFormLayout             *paramLayout = paramData.first;
    QMap<QString, QWidget *> paramFields = paramData.second;

    // Set parent for widgets created in
    // createDwellTimeParameters
    for (auto it = paramFields.begin();
         it != paramFields.end(); ++it)
    {
        QWidget *widget = it.value();
        widget->setParent(dwellGroup); // FIXED: Set correct
                                       // parent for widgets
    }

    // FIXED: Properly add the layout to the group box
    QVBoxLayout *dwellLayout =
        qobject_cast<QVBoxLayout *>(dwellGroup->layout());
    if (dwellLayout)
    {
        dwellLayout->addLayout(paramLayout);
    }

    // Update settings with new parameter fields
    for (auto it = paramFields.begin();
         it != paramFields.end(); ++it)
    {
        QString  paramName = it.key();
        QWidget *widget    = it.value();

        if (QLineEdit *lineEdit =
                qobject_cast<QLineEdit *>(widget))
        {
            settings[QString("dwell_time.parameters.%1")
                         .arg(paramName)] =
                lineEdit->text();
        }
        else if (QComboBox *comboBox =
                     qobject_cast<QComboBox *>(widget))
        {
            settings[QString("dwell_time.parameters.%1")
                         .arg(paramName)] =
                comboBox->currentText();
        }
    }
}

void SettingsWidget::addNestedPropertiesSection(
    const QMap<QString, QVariant> &item,
    const QString                 &sectionName,
    const QString                 &propertiesKey)
{

    if (!item.contains(propertiesKey))
    {
        return;
    }

    QGroupBox *group = new QGroupBox(
        tr(sectionName.toUtf8().constData()), this);
    QFormLayout *layout = new QFormLayout(group);
    layout->setFieldGrowthPolicy(
        QFormLayout::AllNonFixedFieldsGrow);

    // Define units and validators for different property
    // types
    QMap<QString, QPair<QString, QValidator *>>
        propertyConfigs;

    if (propertiesKey == "capacity")
    {
        propertyConfigs["storage"] =
            qMakePair(tr("Storage Capacity (TEU)"),
                      new QDoubleValidator(group));
        propertyConfigs["processing"] =
            qMakePair(tr("Processing Capacity (TEU/day)"),
                      new QDoubleValidator(group));
    }
    else if (propertiesKey == "cost")
    {
        propertyConfigs["fixed"] =
            qMakePair(tr("Fixed Cost (USD/year)"),
                      new QDoubleValidator(group));
        propertyConfigs["variable"] =
            qMakePair(tr("Variable Cost (USD/TEU)"),
                      new QDoubleValidator(group));
        propertyConfigs["penalty"] =
            qMakePair(tr("Penalty Cost (USD/day)"),
                      new QDoubleValidator(group));
    }
    else if (propertiesKey == "customs")
    {
        propertyConfigs["processing_time"] =
            qMakePair(tr("Processing Time (hours)"),
                      new QDoubleValidator(group));
        propertyConfigs["cost"] =
            qMakePair(tr("Cost (USD/TEU)"),
                      new QDoubleValidator(group));
    }

    QMap<QString, QVariant> properties =
        item[propertiesKey].toMap();

    for (auto it = properties.begin();
         it != properties.end(); ++it)
    {
        const QString  &subkey   = it.key();
        const QVariant &subvalue = it.value();

        QLineEdit *lineEdit =
            new QLineEdit(subvalue.toString(), group);

        if (propertyConfigs.contains(subkey))
        {
            QString label = propertyConfigs[subkey].first;
            QValidator *validator =
                propertyConfigs[subkey].second;

            if (validator)
            {
                if (QDoubleValidator *doubleValidator =
                        qobject_cast<QDoubleValidator *>(
                            validator))
                {
                    doubleValidator->setBottom(
                        0.0); // Ensure non-negative values
                }
                lineEdit->setValidator(validator);
            }

            layout->addRow(label + ":", lineEdit);
        }
        else
        {
            layout->addRow(subkey + ":", lineEdit);
        }
    }

    containerLayout->addWidget(group);
}

} // namespace GUI
} // namespace CargoNetSim
