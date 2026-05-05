#include "RegionManagerWidget.h"
#include "../Controllers/RegionController.h"
#include "../MainWindow.h"
#include "../Utils/ColorUtils.h"
#include "../Widgets/ColorPickerDialog.h"
#include "Backend/Application/NetworkViewService.h"
#include "Backend/Commons/LogCategories.h"

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
    if (auto *networkView = mainWindow->networkViewService())
    {
        connect(networkView,
                &Backend::Application::NetworkViewService::regionAdded,
                this, [this](const QString &) { updateRegionList(); });
        connect(networkView,
                &Backend::Application::NetworkViewService::regionRemoved,
                this, [this](const QString &) { updateRegionList(); });
        connect(networkView,
                &Backend::Application::NetworkViewService::regionRenamed,
                this, [this](const QString &, const QString &) {
                    updateRegionList();
                });
        connect(networkView,
                &Backend::Application::NetworkViewService::regionsCleared,
                this, [this]() { updateRegionList(); });
        // Startup and scenario-load both route through ScenarioApplier,
        // which adds the region to RDC FIRST and only then writes its
        // "color" variable. The regionAdded refresh above fires with no
        // colour set — we'd render a black swatch. Re-refresh whenever
        // the colour key actually lands so the swatch catches up.
        connect(networkView,
                &Backend::Application::NetworkViewService::regionVariableChanged,
                this,
                [this](const QString &, const QString &key, const QVariant &) {
                    if (key == QStringLiteral("color"))
                        updateRegionList();
                });
    }
}

void RegionManagerWidget::updateRegionList()
{
    regionList->clear();

    const QStringList regionNames =
        mainWindow && mainWindow->networkViewService()
            ? mainWindow->networkViewService()->regionNames()
            : QStringList();

    for (const QString &regionName : regionNames)
    {
        const QVariant rawColor =
            mainWindow && mainWindow->networkViewService()
                ? mainWindow->networkViewService()->regionVariable(
                      regionName, QStringLiteral("color"))
                : QVariant();
        qCInfo(lcGuiUtil)
            << "RegionManagerWidget::updateRegionList:"
            << "region=" << regionName
            << "variantType=" << rawColor.typeName()
            << "userType=" << rawColor.userType()
            << "rawValue=" << rawColor
            << "canConvertQColor=" << rawColor.canConvert<QColor>();

        QColor color =
            rawColor.value<QColor>();
        qCInfo(lcGuiUtil)
            << "  -> QColor via QVariant: valid=" << color.isValid()
            << "name=" << color.name();

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
        mainWindow && mainWindow->networkViewService()
            ? mainWindow->networkViewService()->regionVariable(
                  regionName, QStringLiteral("color"))
                  .value<QColor>()
            : QColor();

    ColorPickerDialog dialog(currentColor, this);
    if (dialog.exec())
    {
        QColor newColor = dialog.getSelectedColor();
        if (newColor.isValid())
        {
            mainWindow->regionCtrl()->updateRegionColor(
                regionName, newColor);
            // No RDC signal fires for color changes — refresh manually.
            updateRegionList();
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
        if (mainWindow && mainWindow->networkViewService()
            && mainWindow->networkViewService()->regionNames()
                   .contains(newRegionName))
        {
            QMessageBox::warning(
                this, tr("Error"),
                tr("A region with this name already "
                   "exists."));
            return;
        }

        QColor color = ColorUtils::getRandomColor();
        mainWindow->regionCtrl()->addRegion(
            newRegionName, color, QPointF(0, 0));
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

    if (ok && !newName.isEmpty() && newName != oldName)
    {
        // Check if name already exists
        if (mainWindow && mainWindow->networkViewService()
            && mainWindow->networkViewService()->regionNames()
                   .contains(newName))
        {
            QMessageBox::warning(
                this, tr("Error"),
                tr("A region with this name already "
                   "exists."));
            return;
        }

        // Update the backend-owned region view store first.
        // NOTE: NetworkViewService::renameRegion emits
        // signals that may rebuild this widget's list,
        // invalidating currentItem. Do NOT access
        // currentItem after this call.
        if (auto *networkView = mainWindow->networkViewService())
            networkView->renameRegion(oldName, newName);

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
           "All items in this region will be permanently deleted.")
            .arg(regionName),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (reply == QMessageBox::Yes)
    {
        mainWindow->regionCtrl()->removeRegion(regionName);
        updateButtonStates();
    }
}

void RegionManagerWidget::clearRegions()
{
    mainWindow->regionCtrl()->clearRegions();
    updateButtonStates();
}

} // namespace GUI
} // namespace CargoNetSim
