#include "NetworkManagerDialog.h"

#include <QAction>
#include <QColorDialog>
#include <QGridLayout>
#include <QHeaderView>
#include <QIcon>
#include <QInputDialog>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include "Backend/Application/NetworkViewService.h"
#include "Backend/Commons/LogCategories.h"
#include "../Controllers/NetworkController.h"
#include "../MainWindow.h"
#include "../Widgets/ColorPickerDialog.h"
#include "GUI/Controllers/SceneVisibilityController.h"

namespace CargoNetSim
{
namespace GUI
{

NetworkManagerDialog::NetworkManagerDialog(QWidget *parent)
    : QDockWidget("Network Manager", parent)
{
    mainWindow = qobject_cast<MainWindow *>(parent);
    if (!mainWindow)
    {
        // Fallback if not directly parented to MainWindow
        mainWindow = qobject_cast<MainWindow *>(parent->window());
    }

    setObjectName("NetworkManagerDock");

    // Create main widget and layout
    QWidget *mainWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(mainWidget);

    // Create tab widget
    QTabWidget *tabWidget = new QTabWidget(mainWidget);

    // Create NeTrainSim tab
    QWidget *netrainsimTab =
        createNetworkTab("Rail Network");
    tabWidget->addTab(netrainsimTab, "Rail Network");

    // Create INTEGRATION tab
    QWidget *integrationTab = createNetworkTab("Truck Network");
    tabWidget->addTab(integrationTab, "Truck Network");

    mainLayout->addWidget(tabWidget);
    setWidget(mainWidget);

    // Initialize network registries
    updateNetworkList("Rail Network");
    updateNetworkList("Truck Network");

    // Connect to region change signals if MainWindow provides them
    if (mainWindow)
    {
        connect(mainWindow, &MainWindow::regionChanged,
                this, [this](const QString &region) {
                    updateNetworkListForChangedRegion(region);
                });
    }
}

QWidget *NetworkManagerDialog::createNetworkTab(
    const QString &networkType)
{
    QWidget     *tab    = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(tab);

    // Create network list
    QListWidget *listWidget = new QListWidget(tab);
    listWidget->setObjectName(
        networkType.toLower().replace(" ", "_") + "_list");

    // Connect selection changed signal
    connect(listWidget, &QListWidget::itemSelectionChanged,
            this, [this, networkType]() {
                onSelectionChanged(networkType);
            });
    layout->addWidget(listWidget);

    // Create grid layout for buttons (2x2)
    QGridLayout *buttonLayout = new QGridLayout();
    buttonLayout->setColumnStretch(
        0, 1); // Make columns stretch equally
    buttonLayout->setColumnStretch(1, 1);

    // Add network button
    QPushButton *addButton = new QPushButton("Add", tab);
    connect(
        addButton, &QPushButton::clicked, this,
        [this, networkType]() { addNetwork(networkType); });
    buttonLayout->addWidget(addButton, 0,
                            0); // Row 0, Column 0

    // Rename network button
    QPushButton *renameButton =
        new QPushButton("Rename", tab);
    renameButton->setEnabled(false); // Initially disabled
    connect(renameButton, &QPushButton::clicked, this,
            [this, networkType]() {
                renameNetwork(networkType);
            });
    buttonLayout->addWidget(renameButton, 0,
                            1); // Row 0, Column 1

    // Delete network button
    QPushButton *deleteButton =
        new QPushButton("Delete", tab);
    deleteButton->setEnabled(false); // Initially disabled
    connect(deleteButton, &QPushButton::clicked, this,
            [this, networkType]() {
                deleteNetwork(networkType);
            });
    buttonLayout->addWidget(deleteButton, 1,
                            0); // Row 1, Column 0

    // Change color button
    QPushButton *colorButton =
        new QPushButton("Change Color", tab);
    colorButton->setEnabled(false); // Initially disabled
    connect(colorButton, &QPushButton::clicked, this,
            [this, networkType]() {
                changeNetworkColor(networkType);
            });
    buttonLayout->addWidget(colorButton, 1,
                            1); // Row 1, Column 1

    // Store button references
    QMap<QString, QPushButton *> buttons;
    buttons["rename"]           = renameButton;
    buttons["delete"]           = deleteButton;
    buttons["color"]            = colorButton;
    networkButtons[networkType] = buttons;

    layout->addLayout(buttonLayout);
    return tab;
}

void NetworkManagerDialog::onSelectionChanged(
    const QString &networkType)
{
    QListWidget *listWidget = findChild<QListWidget *>(
        networkType.toLower().replace(" ", "_") + "_list");

    if (!listWidget)
        return;

    bool hasSelection =
        listWidget->currentItem() != nullptr;

    // Get the appropriate button dictionary
    QMap<QString, QPushButton *> buttons =
        networkButtons[networkType];

    // Enable/disable buttons based on selection
    for (auto it = buttons.begin(); it != buttons.end();
         ++it)
    {
        it.value()->setEnabled(hasSelection);
    }
}

void NetworkManagerDialog::addNetwork(
    const QString &networkType)
{
    qCDebug(lcGuiNetwork) << "NetworkManagerDialog::addNetwork:"
                       << "type=" << networkType;
    if (!mainWindow)
        return;

    auto *networkView = mainWindow->networkViewService();
    const QString regionName =
        networkView ? networkView->currentRegionName()
                    : QString();

    if (regionName.isEmpty())
        return;

    // Import the new network using NetworkController which
    // now contains the validation logic
    try
    {
        QString networkName;

        if (networkType == "Rail Network")
        {
            networkName = NetworkController::importNetwork(
                mainWindow, NetworkType::Train, regionName);

            // Network list is now updated within
            // importNetwork if successful
        }
        else if (networkType == "Truck Network")
        {
            networkName = NetworkController::importNetwork(
                mainWindow, NetworkType::Truck, regionName);

            // Network list is now updated within
            // importNetwork if successful
        }

        // No explicit updateNetworkList call needed here
        // since it's now handled in importNetwork
    }
    catch (const std::exception &e)
    {
        QMessageBox::critical(
            this, "Error",
            QString("Failed to import %1: %2")
                .arg(networkType.toLower())
                .arg(e.what()));
    }
}

void NetworkManagerDialog::deleteNetwork(
    const QString &networkType)
{
    QListWidget *listWidget = findChild<QListWidget *>(
        networkType.toLower().replace(" ", "_") + "_list");

    if (!listWidget || !mainWindow)
        return;

    QListWidgetItem *currentItem =
        listWidget->currentItem();
    qCDebug(lcGuiNetwork) << "NetworkManagerDialog::deleteNetwork:"
                       << "type=" << networkType
                       << "name=" << (currentItem ? currentItem->text() : "(none)");

    if (!currentItem)
    {
        QMessageBox::warning(
            this, "Warning",
            "Please select a network to delete.");
        return;
    }

    QString networkName = currentItem->text();

    QMessageBox::StandardButton reply =
        QMessageBox::question(
            this, "Confirm Delete",
            QString("Are you sure you want to delete the "
                    "network '%1'?")
                .arg(networkName),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);

    if (reply == QMessageBox::Yes)
    {
        try
        {
            auto *networkView = mainWindow->networkViewService();
            const QString regionName =
                networkView ? networkView->currentRegionName()
                            : QString();

            NetworkType type =
                (networkType == "Rail Network")
                    ? NetworkType::Train
                    : NetworkType::Truck;
            NetworkController::removeNetwork(
                mainWindow, type, networkName, regionName);
        }
        catch (const std::exception &e)
        {
            QMessageBox::critical(
                this, "Error",
                QString("Failed to delete network: %1")
                    .arg(e.what()));
        }
    }
}

void NetworkManagerDialog::renameNetwork(
    const QString &networkType)
{
    qCDebug(lcGuiNetwork) << "NetworkManagerDialog::renameNetwork:"
                       << "type=" << networkType;
    QListWidget *listWidget = findChild<QListWidget *>(
        networkType.toLower().replace(" ", "_") + "_list");

    if (!listWidget || !mainWindow)
        return;

    QListWidgetItem *currentItem =
        listWidget->currentItem();

    if (!currentItem)
    {
        QMessageBox::warning(
            this, "Warning",
            "Please select a network to rename.");
        return;
    }

    QString              oldName = currentItem->text();
    auto *networkView = mainWindow->networkViewService();
    const QString regionName =
        networkView ? networkView->currentRegionName()
                    : QString();
    NetworkType type = (networkType == "Rail Network")
                           ? NetworkType::Train
                           : NetworkType::Truck;

    while (true)
    {
        bool    ok;
        QString newName = QInputDialog::getText(
            this, "Rename Network",
            "Enter new network name:", QLineEdit::Normal,
            oldName, &ok);

        if (!ok || newName.isEmpty())
        {
            return;
        }
        // Call the controller method to rename the network
        if (NetworkController::renameNetwork(
                mainWindow, type, oldName, newName,
                regionName))
            break;
        else
            return; // Break out if rename failed
    }
}

void NetworkManagerDialog::changeNetworkColor(
    const QString &networkType)
{
    qCDebug(lcGuiNetwork) << "NetworkManagerDialog::changeNetworkColor:"
                       << "type=" << networkType;
    QListWidget *listWidget = findChild<QListWidget *>(
        networkType.toLower().replace(" ", "_") + "_list");

    if (!listWidget)
        return;

    QListWidgetItem *currentItem =
        listWidget->currentItem();

    if (!currentItem)
    {
        QMessageBox::warning(
            this, "Warning",
            "Please select a network to change color.");
        return;
    }

    ColorPickerDialog colorDialog(nullptr, this);
    if (!colorDialog.exec())
    {
        return;
    }

    QColor newColor = colorDialog.getSelectedColor();
    if (!newColor.isValid())
    {
        return;
    }

    QString networkName = currentItem->text();
    auto *networkView = mainWindow->networkViewService();
    const QString regionName =
        networkView ? networkView->currentRegionName()
                    : QString();

    NetworkType type = (networkType == "Rail Network")
                           ? NetworkType::Train
                           : NetworkType::Truck;

    // Call the controller method to change the network
    // color
    if (NetworkController::changeNetworkColor(
            mainWindow, type, networkName, newColor,
            regionName))
    {
        // Update the icon in the list widget
        currentItem->setIcon(
            QIcon(createColorPixmap(newColor)));
    }
}

void NetworkManagerDialog::updateNetworkList(
    const QString &networkType)
{
    qCDebug(lcGuiNetwork) << "NetworkManagerDialog::updateNetworkList:"
                       << "type=" << networkType;
    const bool isTrainNetwork =
        (networkType == "Rail Network");
    const QString listWidgetName =
        (isTrainNetwork ? "rail_network" : "truck_network")
        + QString("_list");
    QListWidget *listWidget =
        findChild<QListWidget *>(listWidgetName);

    if (!listWidget)
        return;

    // Store current selection before clearing
    QString selectedItemText;
    if (listWidget->currentItem())
    {
        selectedItemText =
            listWidget->currentItem()->text();
    }

    // Store current checkbox states before clearing
    QMap<QString, Qt::CheckState> checkboxStates;
    for (int i = 0; i < listWidget->count(); ++i)
    {
        QListWidgetItem *item        = listWidget->item(i);
        checkboxStates[item->text()] = item->checkState();
    }

    // Disconnect signals before clearing
    listWidget->disconnect(
        SIGNAL(itemChanged(QListWidgetItem *)));
    listWidget->disconnect(SIGNAL(itemSelectionChanged()));

    listWidget->clear();

    // Get the current region data
    auto *networkView = mainWindow->networkViewService();
    const QString regionName =
        networkView ? networkView->currentRegionName()
                    : QString();
    if (regionName.isEmpty() || !networkView)
        return;

    // Get network names and prefix based on network type
    const QString prefix = "";
    // isTrainNetwork ? "rail_" : "truck_";
    const QStringList networkNames =
        isTrainNetwork
            ? networkView->trainNetworkNames(regionName)
            : networkView->truckNetworkNames(regionName);

    int selectedIndex = -1;

    // Process each network
    for (int i = 0; i < networkNames.size(); i++)
    {
        const auto  &networkName = networkNames[i];
        QColor color =
            networkView->networkVariable(
                           regionName, networkName,
                           QStringLiteral("color"))
                .value<QColor>();

        // Create display name
        QString displayName = networkName;
        if (displayName.startsWith(prefix))
        {
            displayName = displayName.mid(prefix.length());
        }

        // Create and configure list item
        QListWidgetItem *item =
            new QListWidgetItem(displayName);
        item->setFlags(item->flags()
                       | Qt::ItemIsUserCheckable);
        item->setCheckState(
            checkboxStates.value(displayName, Qt::Checked));

        if (color.isValid())
        {
            item->setIcon(QIcon(createColorPixmap(color)));
        }

        listWidget->addItem(item);

        // Check if this was previously selected
        if (displayName == selectedItemText)
        {
            selectedIndex = i;
        }
    }

    // Restore selection if possible, or select first item
    // if available
    if (selectedIndex >= 0)
    {
        listWidget->setCurrentRow(selectedIndex);
    }
    else if (listWidget->count() > 0)
    {
        listWidget->setCurrentRow(0);
    }

    // Reconnect signals
    connect(listWidget, &QListWidget::itemChanged, this,
            [this, networkType](QListWidgetItem *item) {
                onItemCheckedChanged(item, networkType);
            });

    connect(listWidget, &QListWidget::itemSelectionChanged,
            this, [this, networkType]() {
                onSelectionChanged(networkType);
            });

    // Manually call onSelectionChanged to update button
    // states
    onSelectionChanged(networkType);
}

void NetworkManagerDialog::
    updateNetworkListForChangedRegion(
        const QString &regionName)
{
    updateNetworkList("Rail Network");
    updateNetworkList("Truck Network");
}

void NetworkManagerDialog::onItemCheckedChanged(
    QListWidgetItem *item, const QString &networkType)
{
    if (!item || !mainWindow)
        return;

    QString networkName = item->text();
    bool    isVisible   = item->checkState() == Qt::Checked;

    // Update visibility of scene
    // items
    mainWindow->sceneVisibility()->changeNetworkVisibility(
        networkName, isVisible);
}

QPixmap
NetworkManagerDialog::createColorPixmap(const QColor &color,
                                        int           size)
{
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setPen(QPen(Qt::black, 1));
    painter.setBrush(QBrush(color));
    painter.drawRect(0, 0, size - 1, size - 1);
    painter.end();

    return pixmap;
}

void NetworkManagerDialog::clear()
{
    qCDebug(lcGuiNetwork) << "NetworkManagerDialog::clear: begin";
    // Clear both list widgets
    QListWidget *trainList =
        findChild<QListWidget *>("rail_network_list");
    QListWidget *truckList =
        findChild<QListWidget *>("truck_network_list");

    if (trainList)
    {
        trainList->clear();
        // Disconnect any existing signals
        trainList->disconnect(
            SIGNAL(itemChanged(QListWidgetItem *)));
    }

    if (truckList)
    {
        truckList->clear();
        truckList->disconnect(
            SIGNAL(itemChanged(QListWidgetItem *)));
    }

    NetworkController::clearAllNetworks(mainWindow);
}

} // namespace GUI
} // namespace CargoNetSim
