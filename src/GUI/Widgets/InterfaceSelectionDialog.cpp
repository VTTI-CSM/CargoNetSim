// In InterfaceSelectionDialog.cpp
#include "InterfaceSelectionDialog.h"
#include "Backend/Commons/LogCategories.h"
#include <QDialogButtonBox>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace CargoNetSim
{
namespace GUI
{
InterfaceSelectionDialog::InterfaceSelectionDialog(
    const QSet<QString> &availableOptions,
    const QSet<QString> &visibleTerminalTypes,
    DialogType dialogType, QWidget *parent)
    : QDialog(parent)
    , m_dialogType(dialogType)
    , m_useCoordinateDistanceCheckbox(nullptr)
{
    qCInfo(lcGuiUtil) << "InterfaceSelectionDialog::InterfaceSelectionDialog: opening"
                      << (dialogType == NetworkSelection ? "NetworkSelection" : "InterfaceSelection")
                      << "options:" << availableOptions.size()
                      << "terminalTypes:" << visibleTerminalTypes.size();
    QString windowTitle, headerText, optionsGroupTitle;

    // Set titles based on dialog type
    if (dialogType == NetworkSelection)
    {
        windowTitle = "Select Networks to Connect";
        headerText =
            "Select which network types to connect:";
        optionsGroupTitle = "Available Network Types:";
    }
    else
    { // InterfaceSelection (default)
        windowTitle = "Select Interfaces to Connect";
        headerText  = "Select which interfaces to connect:";
        optionsGroupTitle = "Available Interfaces:";
    }

    setWindowTitle(windowTitle);
    setMinimumWidth(450);
    setMaximumWidth(500);
    setMinimumHeight(500);
    setMaximumHeight(500);

    // Main layout
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Header with descriptive label
    QLabel *label = new QLabel(headerText, this);
    label->setStyleSheet("font-weight: bold;");
    mainLayout->addWidget(label);

    // Create a scrollable area for option checkboxes
    QScrollArea *optionsScrollArea = new QScrollArea(this);
    optionsScrollArea->setWidgetResizable(true);
    optionsScrollArea->setFrameShape(QFrame::NoFrame);

    QWidget *optionsScrollContent =
        new QWidget(optionsScrollArea);
    QVBoxLayout *checkboxLayout =
        new QVBoxLayout(optionsScrollContent);
    checkboxLayout->setContentsMargins(0, 0, 0, 0);

    // Create a checkbox for each available option
    for (const QString &option : availableOptions)
    {
        QCheckBox *checkbox = new QCheckBox(option, this);
        checkbox->setChecked(true); // Default to checked
        checkboxLayout->addWidget(checkbox);

        if (dialogType == NetworkSelection)
        {
            m_networkTypeCheckboxes[option] = checkbox;
        }
        else
        {
            m_interfaceCheckboxes[option] = checkbox;
        }
    }

    // Add some spacing at the bottom of the checkbox layout
    checkboxLayout->addStretch();

    optionsScrollArea->setWidget(optionsScrollContent);

    // Create button layout for option selection
    QHBoxLayout *optionsButtonLayout = new QHBoxLayout();
    QPushButton *selectAllOptionsBtn =
        new QPushButton("Select All", this);
    QPushButton *deselectAllOptionsBtn =
        new QPushButton("Deselect All", this);

    optionsButtonLayout->addWidget(selectAllOptionsBtn);
    optionsButtonLayout->addWidget(deselectAllOptionsBtn);

    // Create groupbox for options section
    QGroupBox *optionsBox =
        new QGroupBox(optionsGroupTitle, this);
    QVBoxLayout *optionsBoxLayout =
        new QVBoxLayout(optionsBox);
    optionsBoxLayout->addWidget(optionsScrollArea);
    optionsBoxLayout->addLayout(optionsButtonLayout);

    mainLayout->addWidget(optionsBox);

    // Connect option button signals based on dialog type
    if (dialogType == NetworkSelection)
    {
        connect(selectAllOptionsBtn, &QPushButton::clicked,
                this,
                &InterfaceSelectionDialog::
                    selectAllNetworkTypes);
        connect(deselectAllOptionsBtn,
                &QPushButton::clicked, this,
                &InterfaceSelectionDialog::
                    deselectAllNetworkTypes);
    }
    else
    {
        connect(
            selectAllOptionsBtn, &QPushButton::clicked,
            this,
            &InterfaceSelectionDialog::selectAllInterfaces);
        connect(deselectAllOptionsBtn,
                &QPushButton::clicked, this,
                &InterfaceSelectionDialog::
                    deselectAllInterfaces);
    }

    // Only add terminal types section if we have visible
    // terminal types
    if (!visibleTerminalTypes.isEmpty())
    {
        // Create terminal types section
        QGroupBox *terminalTypesBox = new QGroupBox(
            "Terminal Types to Include:", this);
        QVBoxLayout *terminalTypesLayout =
            new QVBoxLayout(terminalTypesBox);

        // Add terminal type checkboxes - only for visible
        // types
        for (const QString &terminalType :
             visibleTerminalTypes)
        {
            QCheckBox *checkbox =
                new QCheckBox(terminalType, this);
            checkbox->setChecked(
                true); // Default to checked
            terminalTypesLayout->addWidget(checkbox);
            m_terminalTypeCheckboxes[terminalType] =
                checkbox;
        }

        // Create button layout for terminal type selection
        QHBoxLayout *terminalTypeButtonLayout =
            new QHBoxLayout();
        QPushButton *selectAllTerminalTypesBtn =
            new QPushButton("Select All", this);
        QPushButton *deselectAllTerminalTypesBtn =
            new QPushButton("Deselect All", this);

        terminalTypeButtonLayout->addWidget(
            selectAllTerminalTypesBtn);
        terminalTypeButtonLayout->addWidget(
            deselectAllTerminalTypesBtn);

        terminalTypesLayout->addLayout(
            terminalTypeButtonLayout);
        mainLayout->addWidget(terminalTypesBox);

        // Connect terminal type button signals
        connect(selectAllTerminalTypesBtn,
                &QPushButton::clicked, this,
                &InterfaceSelectionDialog::
                    selectAllTerminalTypes);
        connect(deselectAllTerminalTypesBtn,
                &QPushButton::clicked, this,
                &InterfaceSelectionDialog::
                    deselectAllTerminalTypes);
    }

    // Add coordinate distance checkbox for
    // InterfaceSelection type
    if (dialogType == InterfaceSelection)
    {
        m_useCoordinateDistanceCheckbox =
            new QCheckBox("Use coordinate distance to "
                          "calculate connection properties",
                          this);
        m_useCoordinateDistanceCheckbox->setChecked(
            true); // Default to checked
        mainLayout->addWidget(
            m_useCoordinateDistanceCheckbox);
    }

    // Create a 2x2 grid layout for dialog buttons
    QGridLayout *buttonGrid = new QGridLayout();
    buttonGrid->setSpacing(10);

    QPushButton *okButton = new QPushButton("OK", this);
    QPushButton *cancelButton =
        new QPushButton("Cancel", this);

    // Style the OK button to make it stand out as the
    // primary action
    okButton->setDefault(true);

    // Add buttons to the grid layout (1x2 for consistency)
    buttonGrid->addWidget(okButton, 0, 0);
    buttonGrid->addWidget(cancelButton, 0, 1);

    // Add the button grid to the main layout
    mainLayout->addLayout(buttonGrid);

    // Connect dialog button signals
    connect(okButton, &QPushButton::clicked, this,
            &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this,
            &QDialog::reject);
}

InterfaceSelectionDialog::~InterfaceSelectionDialog()
{
    qCInfo(lcGuiUtil) << "InterfaceSelectionDialog::~InterfaceSelectionDialog: closing";
    // QObject parent takes care of widget deletion
}

QList<QString>
InterfaceSelectionDialog::getSelectedInterfaces() const
{
    QList<QString> selectedInterfaces;
    for (auto it = m_interfaceCheckboxes.constBegin();
         it != m_interfaceCheckboxes.constEnd(); ++it)
    {
        if (it.value()->isChecked())
        {
            selectedInterfaces.append(it.key());
        }
    }
    qCDebug(lcGuiUtil) << "InterfaceSelectionDialog::getSelectedInterfaces:"
                       << selectedInterfaces.size() << "selected";
    return selectedInterfaces;
}

QList<QString>
InterfaceSelectionDialog::getSelectedNetworkTypes() const
{
    QList<QString> selectedNetworkTypes;
    for (auto it = m_networkTypeCheckboxes.constBegin();
         it != m_networkTypeCheckboxes.constEnd(); ++it)
    {
        if (it.value()->isChecked())
        {
            selectedNetworkTypes.append(it.key());
        }
    }
    qCDebug(lcGuiUtil) << "InterfaceSelectionDialog::getSelectedNetworkTypes:"
                       << selectedNetworkTypes.size() << "selected";
    return selectedNetworkTypes;
}

QMap<QString, bool>
InterfaceSelectionDialog::getIncludedTerminalTypes() const
{
    QMap<QString, bool> includedTypes;
    for (auto it = m_terminalTypeCheckboxes.constBegin();
         it != m_terminalTypeCheckboxes.constEnd(); ++it)
    {
        includedTypes[it.key()] = it.value()->isChecked();
    }
    return includedTypes;
}

bool InterfaceSelectionDialog::useCoordinateDistance() const
{
    if (m_dialogType == InterfaceSelection
        && m_useCoordinateDistanceCheckbox)
    {
        return m_useCoordinateDistanceCheckbox->isChecked();
    }
    return false;
}

void InterfaceSelectionDialog::selectAllInterfaces()
{
    qCDebug(lcGuiUtil) << "InterfaceSelectionDialog::selectAllInterfaces";
    for (auto checkbox : m_interfaceCheckboxes)
    {
        checkbox->setChecked(true);
    }
}

void InterfaceSelectionDialog::deselectAllInterfaces()
{
    qCDebug(lcGuiUtil) << "InterfaceSelectionDialog::deselectAllInterfaces";
    for (auto checkbox : m_interfaceCheckboxes)
    {
        checkbox->setChecked(false);
    }
}

void InterfaceSelectionDialog::selectAllNetworkTypes()
{
    qCDebug(lcGuiUtil) << "InterfaceSelectionDialog::selectAllNetworkTypes";
    for (auto checkbox : m_networkTypeCheckboxes)
    {
        checkbox->setChecked(true);
    }
}

void InterfaceSelectionDialog::deselectAllNetworkTypes()
{
    qCDebug(lcGuiUtil) << "InterfaceSelectionDialog::deselectAllNetworkTypes";
    for (auto checkbox : m_networkTypeCheckboxes)
    {
        checkbox->setChecked(false);
    }
}

void InterfaceSelectionDialog::selectAllTerminalTypes()
{
    qCDebug(lcGuiUtil) << "InterfaceSelectionDialog::selectAllTerminalTypes";
    for (auto checkbox : m_terminalTypeCheckboxes)
    {
        checkbox->setChecked(true);
    }
}

void InterfaceSelectionDialog::deselectAllTerminalTypes()
{
    qCDebug(lcGuiUtil) << "InterfaceSelectionDialog::deselectAllTerminalTypes";
    for (auto checkbox : m_terminalTypeCheckboxes)
    {
        checkbox->setChecked(false);
    }
}
} // namespace GUI
} // namespace CargoNetSim
