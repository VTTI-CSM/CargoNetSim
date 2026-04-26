#include "TerminalSelectionDialog.h"
#include "../MainWindow.h"
#include "Backend/Application/NetworkViewService.h"
#include "Backend/Commons/LogCategories.h"
#include "GUI/Items/ConnectionLine.h"
#include "GUI/Items/GlobalTerminalItem.h"
#include "GUI/Items/TerminalItem.h"
#include "GUI/Widgets/GraphicsScene.h"

#include <QApplication>
#include <QCheckBox>
#include <QFont>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

namespace CargoNetSim
{
namespace GUI
{

TerminalSelectionDialog::TerminalSelectionDialog(
    MainWindow *mainWindow, QWidget *parent)
    : QDialog(parent)
    , mainWindow_(mainWindow)
{
    qCInfo(lcGuiUtil) << "TerminalSelectionDialog::TerminalSelectionDialog: opening";
    setWindowTitle("Filter Connections");
    setMinimumSize(600, 500);

    setupUI();
    createConnections();

    // Populate data
    populateTerminalNames();
    populateConnectionTypes();
}

TerminalSelectionDialog::~TerminalSelectionDialog()
{
    qCInfo(lcGuiUtil) << "TerminalSelectionDialog::~TerminalSelectionDialog: closing";
}

void TerminalSelectionDialog::setupUI()
{
    qCDebug(lcGuiUtil) << "TerminalSelectionDialog::setupUI: building UI";
    // Main layout
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    // Title label
    QLabel *titleLabel = new QLabel("Connection Filter");
    QFont   titleFont  = titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 2);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    mainLayout->addWidget(titleLabel);

    // Description label
    QLabel *descriptionLabel = new QLabel(
        "Show connections between selected terminals and "
        "of selected types. Only connections matching both "
        "criteria will be displayed.");
    descriptionLabel->setWordWrap(true);
    mainLayout->addWidget(descriptionLabel);

    // Create a splitter for the main content
    QSplitter *contentSplitter =
        new QSplitter(Qt::Horizontal);
    contentSplitter->setChildrenCollapsible(false);

    // Terminal selection section
    QWidget     *terminalSelectionWidget = new QWidget();
    QVBoxLayout *terminalLayout =
        new QVBoxLayout(terminalSelectionWidget);
    terminalLayout->setSpacing(6);
    terminalLayout->setContentsMargins(0, 0, 0, 0);

    QLabel *terminalLabel = new QLabel("Select Terminals");
    QFont   sectionFont   = terminalLabel->font();
    sectionFont.setBold(true);
    terminalLabel->setFont(sectionFont);
    terminalLayout->addWidget(terminalLabel);

    // Search field
    searchField_ = new QLineEdit();
    searchField_->setPlaceholderText("Search terminals...");
    searchField_->setClearButtonEnabled(true);
    terminalLayout->addWidget(searchField_);

    // Select all checkbox
    selectAllTerminalsCheckBox_ =
        new QCheckBox("Select All Terminals");
    terminalLayout->addWidget(selectAllTerminalsCheckBox_);

    // Terminal list
    terminalListWidget_ = new QListWidget();
    terminalListWidget_->setSelectionMode(
        QAbstractItemView::MultiSelection);
    terminalListWidget_->setAlternatingRowColors(true);
    terminalLayout->addWidget(terminalListWidget_);

    contentSplitter->addWidget(terminalSelectionWidget);

    // Connection type section
    QWidget     *connectionTypeWidget = new QWidget();
    QVBoxLayout *connectionTypeLayout =
        new QVBoxLayout(connectionTypeWidget);
    connectionTypeLayout->setSpacing(6);
    connectionTypeLayout->setContentsMargins(0, 0, 0, 0);

    QLabel *connectionTypeLabel =
        new QLabel("Select Connection Types");
    connectionTypeLabel->setFont(sectionFont);
    connectionTypeLayout->addWidget(connectionTypeLabel);

    // Select all connection types checkbox
    selectAllConnectionTypesCheckBox_ =
        new QCheckBox("Select All Connection Types");
    connectionTypeLayout->addWidget(
        selectAllConnectionTypesCheckBox_);

    // Scrollable area for connection types
    QScrollArea *connectionTypeScrollArea =
        new QScrollArea();
    connectionTypeScrollArea->setWidgetResizable(true);
    connectionTypeScrollArea->setFrameShape(
        QFrame::NoFrame);

    connectionTypesWidget_ = new QWidget();
    QVBoxLayout *checkboxLayout =
        new QVBoxLayout(connectionTypesWidget_);
    checkboxLayout->setSpacing(6);
    checkboxLayout->setContentsMargins(0, 0, 0, 0);
    checkboxLayout->addStretch();

    connectionTypeScrollArea->setWidget(
        connectionTypesWidget_);
    connectionTypeLayout->addWidget(
        connectionTypeScrollArea);

    contentSplitter->addWidget(connectionTypeWidget);

    // Set initial splitter sizes
    QList<int> sizes;
    sizes << 300 << 300;
    contentSplitter->setSizes(sizes);

    mainLayout->addWidget(contentSplitter);

    // Action buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();

    okButton_ = new QPushButton("Apply Filter");
    okButton_->setIcon(QApplication::style()->standardIcon(
        QStyle::SP_DialogApplyButton));
    okButton_->setDefault(true);

    cancelButton_ = new QPushButton("Cancel");
    cancelButton_->setIcon(
        QApplication::style()->standardIcon(
            QStyle::SP_DialogCancelButton));

    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton_);
    buttonLayout->addWidget(cancelButton_);

    mainLayout->addLayout(buttonLayout);
}

void TerminalSelectionDialog::createConnections()
{
    // Connect signals and slots
    connect(okButton_, &QPushButton::clicked, this,
            &QDialog::accept);
    connect(cancelButton_, &QPushButton::clicked, this,
            &QDialog::reject);

    connect(searchField_, &QLineEdit::textChanged, this,
            &TerminalSelectionDialog::filterTerminalList);
    connect(terminalListWidget_,
            &QListWidget::itemSelectionChanged, this,
            &TerminalSelectionDialog::validateSelections);

    connect(selectAllTerminalsCheckBox_,
            &QCheckBox::toggled, this,
            &TerminalSelectionDialog::selectAllTerminals);
    connect(
        selectAllConnectionTypesCheckBox_,
        &QCheckBox::toggled, this,
        &TerminalSelectionDialog::selectAllConnectionTypes);
}

QStringList
TerminalSelectionDialog::getSelectedTerminalNames() const
{
    qCDebug(lcGuiUtil) << "TerminalSelectionDialog::getSelectedTerminalNames";
    QStringList selectedTerminals;

    for (int i = 0; i < terminalListWidget_->count(); ++i)
    {
        QListWidgetItem *item =
            terminalListWidget_->item(i);
        if (item && item->isSelected())
        {
            selectedTerminals << item->text();
        }
    }

    qCDebug(lcGuiUtil) << "TerminalSelectionDialog::getSelectedTerminalNames:"
                       << selectedTerminals.size() << "selected";
    return selectedTerminals;
}

QStringList
TerminalSelectionDialog::getSelectedConnectionTypes() const
{
    QStringList selectedTypes;

    for (QCheckBox *checkbox : connectionTypeCheckBoxes_)
    {
        if (checkbox && checkbox->isChecked())
        {
            selectedTypes << checkbox->text();
        }
    }

    qCDebug(lcGuiUtil) << "TerminalSelectionDialog::getSelectedConnectionTypes:"
                       << selectedTypes.size() << "selected";
    return selectedTypes;
}

void TerminalSelectionDialog::populateTerminalNames()
{
    qCDebug(lcGuiUtil) << "TerminalSelectionDialog::populateTerminalNames";
    originalTerminalNames_.clear();
    terminalListWidget_->clear();

    // Determine if we're in global or region view
    bool isGlobalView = mainWindow_->isGlobalViewActive();
    GraphicsScene *scene =
        isGlobalView ? mainWindow_->globalMapScene_
                     : mainWindow_->regionScene_;

    if (isGlobalView)
    {
        // Get terminal names from GlobalTerminalItems
        QList<GlobalTerminalItem *> globalTerminals =
            scene->getItemsByType<GlobalTerminalItem>();

        for (GlobalTerminalItem *terminal : globalTerminals)
        {
            if (terminal && terminal->isVisible()
                && terminal->getLinkedTerminalItem())
            {
                QString terminalName =
                    terminal->getLinkedTerminalItem()
                        ->getProperty("Name")
                        .toString();
                if (!terminalName.isEmpty()
                    && !originalTerminalNames_.contains(
                        terminalName))
                {
                    originalTerminalNames_ << terminalName;
                }
            }
        }
    }
    else
    {
        // Get terminal names from TerminalItems in the
        // current region
        QString currentRegion =
            mainWindow_ && mainWindow_->networkViewService()
                ? mainWindow_->networkViewService()->currentRegionName()
                : QString();

        QList<TerminalItem *> terminals =
            scene->getItemsByType<TerminalItem>();

        for (TerminalItem *terminal : terminals)
        {
            if (terminal && terminal->isVisible()
                && terminal->getRegion() == currentRegion)
            {
                QString terminalName =
                    terminal->getProperty("Name")
                        .toString();
                if (!terminalName.isEmpty()
                    && !originalTerminalNames_.contains(
                        terminalName))
                {
                    originalTerminalNames_ << terminalName;
                }
            }
        }
    }

    qCDebug(lcGuiUtil) << "TerminalSelectionDialog::populateTerminalNames: found"
                       << originalTerminalNames_.size() << "terminals";
    // Sort terminal names alphabetically
    originalTerminalNames_.sort();

    // Populate list widget
    for (const QString &name : originalTerminalNames_)
    {
        QListWidgetItem *item = new QListWidgetItem(name);
        terminalListWidget_->addItem(item);
    }

    validateSelections();
}

void TerminalSelectionDialog::populateConnectionTypes()
{
    qCDebug(lcGuiUtil) << "TerminalSelectionDialog::populateConnectionTypes";
    availableConnectionTypes_.clear();

    // Clear previous checkboxes
    while (!connectionTypeCheckBoxes_.isEmpty())
    {
        QCheckBox *checkbox =
            connectionTypeCheckBoxes_.takeLast();
        delete checkbox;
    }

    // Determine if we're in global or region view
    bool isGlobalView = mainWindow_->isGlobalViewActive();
    GraphicsScene *scene =
        isGlobalView ? mainWindow_->globalMapScene_
                     : mainWindow_->regionScene_;

    // Get all connection lines
    QList<ConnectionLine *> connectionLines =
        scene->getItemsByType<ConnectionLine>();

    // Find unique connection types. `connectionType()` returns the
    // strongly-typed enum; the dialog's availableConnectionTypes_ is
    // a user-facing QStringList, so display via TransportationTypes::
    // toString ("Truck"/"Train"/"Ship").
    for (ConnectionLine *line : connectionLines)
    {
        if (line && line->isVisible())
        {
            const QString connectionType =
                Backend::TransportationTypes::toString(
                    line->connectionType());
            if (!connectionType.isEmpty()
                && !availableConnectionTypes_.contains(
                    connectionType))
            {
                availableConnectionTypes_ << connectionType;
            }
        }
    }

    qCDebug(lcGuiUtil) << "TerminalSelectionDialog::populateConnectionTypes: found"
                       << availableConnectionTypes_.size() << "types";
    // Sort connection types alphabetically
    availableConnectionTypes_.sort();

    // Create checkboxes for each connection type
    QVBoxLayout *layout = qobject_cast<QVBoxLayout *>(
        connectionTypesWidget_->layout());

    for (const QString &type : availableConnectionTypes_)
    {
        QCheckBox *checkbox = new QCheckBox(type);
        checkbox->setChecked(true);
        connect(
            checkbox, &QCheckBox::toggled, this,
            &TerminalSelectionDialog::validateSelections);

        layout->insertWidget(layout->count() - 1, checkbox);
        connectionTypeCheckBoxes_ << checkbox;
    }

    // Update the "Select All" checkbox state
    selectAllConnectionTypesCheckBox_->setChecked(
        !connectionTypeCheckBoxes_.isEmpty());
}

void TerminalSelectionDialog::validateSelections()
{
    // Enable OK button if at least one terminal and one
    // connection type are selected
    bool hasSelectedTerminals = false;
    for (int i = 0; i < terminalListWidget_->count(); ++i)
    {
        if (terminalListWidget_->item(i)->isSelected())
        {
            hasSelectedTerminals = true;
            break;
        }
    }

    bool hasSelectedConnectionTypes = false;
    for (QCheckBox *checkbox : connectionTypeCheckBoxes_)
    {
        if (checkbox->isChecked())
        {
            hasSelectedConnectionTypes = true;
            break;
        }
    }

    okButton_->setEnabled(hasSelectedTerminals
                          && hasSelectedConnectionTypes);

    // Update "Select All Terminals" checkbox
    bool allTerminalsSelected =
        (terminalListWidget_->selectedItems().count()
         == terminalListWidget_->count());
    selectAllTerminalsCheckBox_->setChecked(
        allTerminalsSelected
        && terminalListWidget_->count() > 0);

    // Update "Select All Connection Types" checkbox
    bool allTypesSelected = true;
    for (QCheckBox *checkbox : connectionTypeCheckBoxes_)
    {
        if (!checkbox->isChecked())
        {
            allTypesSelected = false;
            break;
        }
    }
    selectAllConnectionTypesCheckBox_->setChecked(
        allTypesSelected
        && !connectionTypeCheckBoxes_.isEmpty());
}

void TerminalSelectionDialog::filterTerminalList(
    const QString &text)
{
    qCDebug(lcGuiUtil) << "TerminalSelectionDialog::filterTerminalList:" << text;
    for (int i = 0; i < terminalListWidget_->count(); ++i)
    {
        QListWidgetItem *item =
            terminalListWidget_->item(i);
        bool matches = text.isEmpty()
                       || item->text().contains(
                           text, Qt::CaseInsensitive);
        item->setHidden(!matches);
    }

    validateSelections();
}

void TerminalSelectionDialog::selectAllTerminals(
    bool checked)
{
    qCDebug(lcGuiUtil) << "TerminalSelectionDialog::selectAllTerminals:" << checked;
    for (int i = 0; i < terminalListWidget_->count(); ++i)
    {
        QListWidgetItem *item =
            terminalListWidget_->item(i);
        if (!item->isHidden())
        {
            item->setSelected(checked);
        }
    }

    validateSelections();
}

void TerminalSelectionDialog::selectAllConnectionTypes(
    bool checked)
{
    qCDebug(lcGuiUtil) << "TerminalSelectionDialog::selectAllConnectionTypes:" << checked;
    for (QCheckBox *checkbox : connectionTypeCheckBoxes_)
    {
        checkbox->setChecked(checked);
    }

    validateSelections();
}

void TerminalSelectionDialog::updateFilter()
{
    // This method will be called when any filter option
    // changes
    validateSelections();
}

} // namespace GUI
} // namespace CargoNetSim
