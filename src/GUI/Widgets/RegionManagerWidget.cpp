#include "RegionManagerWidget.h"
#include "../Controllers/RegionController.h"
#include "../Items/RegionCenterPoint.h"
#include "../MainWindow.h"
#include "../Utils/ColorUtils.h"
#include "../Widgets/ColorPickerDialog.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Controllers/CargoNetSimController.h"

#include <QColor>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPixmap>
#include <QStatusBar>
#include <QVBoxLayout>

namespace CargoNetSim
{
namespace GUI
{

RegionManagerWidget::RegionManagerWidget(
    MainWindow *mainWindow, QWidget *parent)
    : QWidget(parent)
    , mainWindow(mainWindow)
{
    setupUI();
}

void RegionManagerWidget::setupUI()
{
    QVBoxLayout *layout = new QVBoxLayout(this);

    // Create region list with color swatches
    regionList = new QListWidget();
    regionList->setIconSize(QSize(24, 24));
    layout->addWidget(new QLabel("Regions:"));
    layout->addWidget(regionList);
    updateRegionList();

    // Create buttons in a grid layout
    QGridLayout *buttonLayout = new QGridLayout();

    addButton    = new QPushButton("Add");
    renameButton = new QPushButton("Rename");
    deleteButton = new QPushButton("Delete");
    colorButton  = new QPushButton("Change Color");

    // Arrange buttons in a 2x2 grid
    buttonLayout->addWidget(addButton, 0, 0);
    buttonLayout->addWidget(renameButton, 0, 1);
    buttonLayout->addWidget(deleteButton, 1, 0);
    buttonLayout->addWidget(colorButton, 1, 1);

    // Make buttons expand horizontally
    for (QPushButton *button : {addButton, renameButton,
                                deleteButton, colorButton})
    {
        button->setSizePolicy(QSizePolicy::Expanding,
                              QSizePolicy::Fixed);
    }

    layout->addLayout(buttonLayout);

    // Connect signals
    connect(addButton, &QPushButton::clicked, this,
            &RegionManagerWidget::addRegion);
    connect(renameButton, &QPushButton::clicked, this,
            &RegionManagerWidget::renameRegion);
    connect(deleteButton, &QPushButton::clicked, this,
            &RegionManagerWidget::deleteRegion);
    connect(colorButton, &QPushButton::clicked, this,
            &RegionManagerWidget::changeRegionColor);

    // Initial button state
    updateButtonStates();
    connect(regionList, &QListWidget::itemSelectionChanged,
            this, &RegionManagerWidget::updateButtonStates);

    // Refresh the list whenever the authoritative region store changes.
    // Without these, the "Open Scenario" path populates
    // RegionDataController (via ScenarioApplier::applyRegions) but this
    // widget keeps showing the pre-load snapshot — regions appear on the
    // canvas but never in the manager.
    auto *rdc = CargoNetSim::CargoNetSimController::getInstance()
                    .getRegionDataController();
    if (rdc)
    {
        connect(rdc, &Backend::RegionDataController::regionAdded,
                this, [this](const QString &) { updateRegionList(); });
        connect(rdc, &Backend::RegionDataController::regionRemoved,
                this, [this](const QString &) { updateRegionList(); });
        connect(rdc, &Backend::RegionDataController::regionRenamed,
                this, [this](const QString &, const QString &) {
                    updateRegionList();
                });
        connect(rdc, &Backend::RegionDataController::regionsCleared,
                this, [this]() { updateRegionList(); });
    }
}

void RegionManagerWidget::updateRegionList()
{
    regionList->clear();

    for (const QString &regionName :
         CargoNetSim::CargoNetSimController::getInstance()
             .getRegionDataController()
             ->getAllRegionNames())
    {

        // get the color assigned to the region
        QColor color =
            CargoNetSim::CargoNetSimController::
                getInstance()
                    .getRegionDataController()
                    ->getRegionData(regionName)
                    ->getVariableAs<QColor>("color");

        // Create color swatch pixmap
        QPixmap pixmap(24, 24);
        pixmap.fill(color);

        // Create list item with color swatch
        QListWidgetItem *item =
            new QListWidgetItem(QIcon(pixmap), regionName);

        regionList->addItem(item);
    }
}

void RegionManagerWidget::updateButtonStates()
{
    bool hasSelection =
        !regionList->selectedItems().isEmpty();
    renameButton->setEnabled(hasSelection);
    colorButton->setEnabled(hasSelection);

    // Only allow deletion if it's not the last region
    deleteButton->setEnabled(hasSelection
                             && regionList->count() > 1);
}

void RegionManagerWidget::changeRegionColor()
{
    QListWidgetItem *currentItem =
        regionList->currentItem();
    if (!currentItem)
    {
        return;
    }

    QString regionName = currentItem->text();
    qCDebug(lcGuiUtil) << "RegionManagerWidget::changeRegionColor:"
                       << "name=" << regionName;
    QColor  currentColor =
        CargoNetSim::CargoNetSimController::getInstance()
            .getRegionDataController()
            ->getRegionData(regionName)
            ->getVariableAs<QColor>("color");

    ColorPickerDialog dialog(currentColor, this);
    if (dialog.exec())
    {
        QColor newColor = dialog.getSelectedColor();
        if (newColor.isValid())
        {
            // Update color in main window
            CargoNetSim::CargoNetSimController::
                getInstance()
                    .getRegionDataController()
                    ->getRegionData(regionName)
                    ->setVariable("color", newColor);

            // Update region center color
            QMap<QString, RegionCenterPoint *>
                regionCenters =
                    CargoNetSim::CargoNetSimController::
                        getInstance()
                            .getRegionDataController()
                            ->getAllRegionVariableAs<
                                RegionCenterPoint *>(
                                "regionCenterPoint");
            if (regionCenters.contains(regionName))
            {
                RegionCenterPoint *center =
                    regionCenters[regionName];
                center->setColor(newColor);
                center->update();
            }

            // Update region list
            updateRegionList();

            // Update visuals
            // mainWindow->sceneVisibility()->updateSceneVisibility();
            // ViewController::updateGlobalMapScene(mainWindow);

            mainWindow->showStatusBarMessage(
                tr("Updated color for region '%1'")
                    .arg(regionName),
                2000);
        }
    }
}

void RegionManagerWidget::addRegion()
{
    qCDebug(lcGuiUtil) << "RegionManagerWidget::addRegion: begin";
    bool    ok;
    QString newRegionName = QInputDialog::getText(
        this, tr("Add Region"),
        tr("Enter new region name:"), QLineEdit::Normal,
        QString(), &ok);

    if (ok && !newRegionName.isEmpty())
    {
        // Check if name already exists
        if (CargoNetSim::CargoNetSimController::
                getInstance()
                    .getRegionDataController()
                    ->getAllRegionNames()
                    .contains(newRegionName))
        {
            QMessageBox::warning(
                this, tr("Error"),
                tr("A region with this name already "
                   "exists."));
            return;
        }

        // Add color for new region
        QColor color = ColorUtils::getRandomColor();

        // Add to main window's region tracking
        CargoNetSim::CargoNetSimController::getInstance()
            .getRegionDataController()
            ->addRegion(newRegionName);
        CargoNetSim::CargoNetSimController::getInstance()
            .getRegionDataController()
            ->setRegionVariable(newRegionName, "color",
                                color);

        // Create region center point
        mainWindow->regionCtrl()->createRegionCenter(
            newRegionName, color, QPointF(0, 0),
            false);

        // // Update UI
        updateRegionList();
        updateButtonStates();
    }
}

void RegionManagerWidget::renameRegion()
{
    QListWidgetItem *currentItem =
        regionList->currentItem();
    if (!currentItem)
    {
        return;
    }

    QString oldName = currentItem->text();
    qCDebug(lcGuiUtil) << "RegionManagerWidget::renameRegion:"
                       << "name=" << oldName;
    bool    ok;
    QString newName = QInputDialog::getText(
        this, tr("Rename Region"), tr("Enter new name:"),
        QLineEdit::Normal, oldName, &ok);

    // TODO

    if (ok && !newName.isEmpty() && newName != oldName)
    {
        // Check if name already exists
        if (CargoNetSim::CargoNetSimController::
                getInstance()
                    .getRegionDataController()
                    ->getAllRegionNames()
                    .contains(newName))
        {
            QMessageBox::warning(
                this, tr("Error"),
                tr("A region with this name already "
                   "exists."));
            return;
        }

        // Update main window's region data.
        // NOTE: RegionDataController::renameRegion emits
        // signals that may rebuild this widget's list,
        // invalidating currentItem. Do NOT access
        // currentItem after this call.
        CargoNetSim::CargoNetSimController::getInstance()
            .getRegionDataController()
            ->renameRegion(oldName, newName);

        mainWindow->regionCtrl()->renameRegion(oldName,
                                              newName);

        // Refresh the list (currentItem is stale after
        // the signals above may have rebuilt it).
        updateRegionList();
    }
}

void RegionManagerWidget::deleteRegion()
{
    QListWidgetItem *currentItem =
        regionList->currentItem();
    if (!currentItem || regionList->count() <= 1)
    {
        return;
    }

    QString regionName = currentItem->text();
    qCDebug(lcGuiUtil) << "RegionManagerWidget::deleteRegion:"
                       << "name=" << regionName;
    int     reply      = QMessageBox::question(
        this, tr("Delete Region"),
        tr("Are you sure you want to delete region '%1'?\n"
                             "All items in this region will be moved to the "
                             "default region.")
            .arg(regionName),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    // TODO
    if (reply == QMessageBox::Yes)
    {
        // Return color to available colors
        CargoNetSim::CargoNetSimController::getInstance()
            .getRegionDataController()
            ->removeRegion(regionName);

        // // Move all items in this region to default
        // region QString defaultRegion =
        // Backend::RegionDataController::getInstance().getAllRegionNames().at(0);
        // for (QGraphicsItem* item :
        // mainWindow->getScene()->items()) {
        //     if (item->data(0).toString() == "Region" &&
        //         item->data(1).toString() == regionName) {
        //         item->setData(1, defaultRegion);
        //         if (item->data(2).isValid()) {
        //             QMap<QString, QVariant> props =
        //             item->data(2).toMap(); if
        //             (props.contains("Region")) {
        //                 props["Region"] = defaultRegion;
        //                 item->setData(2, props);
        //             }
        //         }
        //     }
        // }

        // Update UI
        int row = regionList->row(currentItem);
        delete regionList->takeItem(row);
        updateButtonStates();

        // // Update visuals
        // mainWindow->sceneVisibility()->updateSceneVisibility();
        // ViewController::updateGlobalMapScene(mainWindow);
    }
}

void RegionManagerWidget::clearRegions()
{
    // TODO
    // Get all regions except Default Region
    QStringList regionsToRemove;
    for (int i = 0; i < regionList->count(); i++)
    {
        QString regionName = regionList->item(i)->text();
        if (regionName != "Default Region")
        {
            regionsToRemove.append(regionName);
        }
    }

    // Process each region
    for (const QString &regionName : regionsToRemove)
    {
        // Remove from RegionsData
        CargoNetSim::CargoNetSimController::getInstance()
            .getRegionDataController()
            ->removeRegion(regionName);

        // Remove region center
        // QMap<QString, RegionCenterPoint*>& regionCenters
        // = mainWindow->getRegionCenters(); if
        // (regionCenters.contains(regionName))
        // {
        //     RegionCenterPoint* center =
        //     regionCenters.take(regionName); try {
        //         mainWindow->getScene()->removeItem(center);
        //         delete center;
        //     } catch (const std::exception& e) {
        //         qWarning() << "Exception removing region
        //         center:" << e.what();
        //     }
        // }

        // Remove from region list
        for (int i = 0; i < regionList->count(); i++)
        {
            if (regionList->item(i)->text() == regionName)
            {
                delete regionList->takeItem(i);
                break;
            }
        }
    }

    // // Make sure Default Region exists
    // if
    // (!Backend::RegionDataController::getInstance()->getAllRegionsNames().contains("Default
    // Region")) {
    //     RegionsData::getInstance()->addRegion("Default
    //     Region", QColor(Qt::green));
    // }

    // Update visuals
    // mainWindow->sceneVisibility()->updateSceneVisibility();
    // ViewController::updateGlobalMapScene(mainWindow);
    updateButtonStates();
}

} // namespace GUI
} // namespace CargoNetSim
