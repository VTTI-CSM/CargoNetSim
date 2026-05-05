#include "ColorPickerDialog.h"
#include "../Utils/ColorPalette.h"
#include "Backend/Commons/LogCategories.h"

#include <QColorDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QPixmap>
#include <QVBoxLayout>

namespace CargoNetSim
{
namespace GUI
{

ColorPickerDialog::ColorPickerDialog(
    const QColor &currentColor, QWidget *parent)
    : QDialog(parent)
    , currentColor(currentColor)
    , customColor(currentColor)
{
    qCInfo(lcGuiUtil) << "ColorPickerDialog::ColorPickerDialog: opening with currentColor"
                      << currentColor.name();
    setWindowTitle(tr("Select Color"));
    setModal(true);
    setupUI();
}

QColor ColorPickerDialog::getSelectedColor() const
{
    int currentTabIndex = tabWidget->currentIndex();

    if (currentTabIndex == 0)
    { // Predefined colors tab
        QListWidgetItem *item = colorList->currentItem();
        if (item)
        {
            QString colorName = item->text();
            QColor  c         = ColorPalette::getColor(colorName);
            qCDebug(lcGuiUtil) << "ColorPickerDialog::getSelectedColor: predefined"
                               << colorName << c.name();
            return c;
        }
    }
    else
    { // Custom color tab
        qCDebug(lcGuiUtil) << "ColorPickerDialog::getSelectedColor: custom"
                           << customColor.name();
        return customColor;
    }

    qCWarning(lcGuiUtil) << "ColorPickerDialog::getSelectedColor: no color selected";
    return QColor(); // Invalid color if nothing selected
}

void ColorPickerDialog::openColorDialog()
{
    qCDebug(lcGuiUtil) << "ColorPickerDialog::openColorDialog: opening native color dialog";
    QColor initialColor =
        customColor.isValid() ? customColor : Qt::white;
    QColor color =
        QColorDialog::getColor(initialColor, this);

    if (color.isValid())
    {
        qCDebug(lcGuiUtil) << "ColorPickerDialog::openColorDialog: user picked"
                           << color.name();
        customColor = color;

        // Update custom preview
        QString styleSheet =
            QString("background-color: rgb(%1, %2, %3); "
                    "border: 1px solid black;")
                .arg(color.red())
                .arg(color.green())
                .arg(color.blue());

        customPreview->setStyleSheet(styleSheet);

        // Also update the main preview if on custom tab
        if (tabWidget->currentIndex() == 1)
        {
            previewLabel->setStyleSheet(styleSheet);
        }
    }
}

void ColorPickerDialog::updatePreview(QListWidgetItem *item)
{
    if (!item)
    {
        return;
    }

    QString colorName = item->text();
    qCDebug(lcGuiUtil) << "ColorPickerDialog::updatePreview:" << colorName;
    QColor  qcolor    = ColorPalette::getColor(colorName);

    QString styleSheet =
        QString("background-color: rgb(%1, %2, %3); "
                "border: 1px solid black;")
            .arg(qcolor.red())
            .arg(qcolor.green())
            .arg(qcolor.blue());

    previewLabel->setStyleSheet(styleSheet);
}

void ColorPickerDialog::onTabChanged(int index)
{
    qCDebug(lcGuiUtil) << "ColorPickerDialog::onTabChanged: index" << index;
    if (index == 0)
    { // Predefined colors tab
        if (colorList->currentItem())
        {
            updatePreview(colorList->currentItem());
        }
    }
    else
    { // Custom color tab
        if (customColor.isValid())
        {
            QString styleSheet =
                QString(
                    "background-color: rgb(%1, %2, %3); "
                    "border: 1px solid black;")
                    .arg(customColor.red())
                    .arg(customColor.green())
                    .arg(customColor.blue());

            previewLabel->setStyleSheet(styleSheet);
        }
    }
}

void ColorPickerDialog::setupUI()
{
    qCDebug(lcGuiUtil) << "ColorPickerDialog::setupUI: building UI";
    // Main layout
    QVBoxLayout *layout = new QVBoxLayout(this);

    // Create tab widget
    tabWidget = new QTabWidget();

    // ------- Predefined Colors Tab -------
    QWidget     *predefinedTab = new QWidget();
    QVBoxLayout *predefinedLayout =
        new QVBoxLayout(predefinedTab);

    // Create color list widget
    colorList = new QListWidget();
    colorList->setIconSize(QSize(32, 32));

    // Add color swatches
    QStringList colorNames = ColorPalette::getAllColors();
    for (const QString &colorName : colorNames)
    {
        QColor qcolor = ColorPalette::getColor(colorName);

        // Create color swatch pixmap
        QPixmap pixmap(32, 32);
        pixmap.fill(qcolor);

        // Create list item
        QListWidgetItem *item =
            new QListWidgetItem(QIcon(pixmap), colorName);
        colorList->addItem(item);

        // Select the current color if it matches
        if (currentColor.isValid()
            && qcolor == currentColor)
        {
            colorList->setCurrentItem(item);
        }
    }

    predefinedLayout->addWidget(colorList);

    // ------- Custom Color Tab -------
    QWidget     *customTab    = new QWidget();
    QVBoxLayout *customLayout = new QVBoxLayout(customTab);

    // Add color dialog button
    colorButton =
        new QPushButton(tr("Select Custom Color"));
    connect(colorButton, &QPushButton::clicked, this,
            &ColorPickerDialog::openColorDialog);
    customLayout->addWidget(colorButton);

    // Custom color preview
    customPreview = new QLabel();
    customPreview->setFixedSize(100, 100);
    customPreview->setStyleSheet(
        "border: 1px solid black;");
    customLayout->addWidget(customPreview, 0,
                            Qt::AlignCenter);

    if (currentColor.isValid())
    {
        QString styleSheet =
            QString("background-color: rgb(%1, %2, %3); "
                    "border: 1px solid black;")
                .arg(currentColor.red())
                .arg(currentColor.green())
                .arg(currentColor.blue());

        customPreview->setStyleSheet(styleSheet);
    }

    customLayout->addStretch();

    // ------- Add tabs to tab widget -------
    tabWidget->addTab(predefinedTab,
                      tr("Predefined Colors"));
    tabWidget->addTab(customTab, tr("Custom Color"));

    layout->addWidget(tabWidget);

    // ------- Preview area -------
    QHBoxLayout *previewLayout = new QHBoxLayout();
    previewLayout->addWidget(new QLabel(tr("Preview:")));
    previewLabel = new QLabel();
    previewLabel->setFixedSize(50, 50);
    previewLabel->setStyleSheet("border: 1px solid black;");
    previewLayout->addWidget(previewLabel);
    previewLayout->addStretch();

    layout->addLayout(previewLayout);

    // ------- Buttons -------
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, this,
            &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this,
            &QDialog::reject);
    layout->addWidget(buttonBox);

    // ------- Connect signals -------
    connect(colorList, &QListWidget::currentItemChanged,
            this, &ColorPickerDialog::updatePreview);
    connect(tabWidget, &QTabWidget::currentChanged, this,
            &ColorPickerDialog::onTabChanged);

    // ------- Initial preview update -------
    if (colorList->currentItem())
    {
        updatePreview(colorList->currentItem());
    }

    // Set an appropriate size for the dialog
    resize(400, 500);
}

} // namespace GUI
} // namespace CargoNetSim
