#include "NetworkMoveDialog.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Controllers/RegionDataController.h"
#include "GUI/MainWindow.h"
#include <QLocale>

namespace CargoNetSim
{
namespace GUI
{

NetworkMoveDialog::NetworkMoveDialog(
    MainWindow *mainWindow, const QString &regionName,
    bool isProjectedCoords, QWidget *parent)
    : QDialog(parent)
    , isProjected(isProjectedCoords)
    , selectedNetworkType(NetworkType::Train)
    , selectedNetworkName("")
    , mainWindow(mainWindow)
    , regionName(regionName)
{
    qCInfo(lcGuiNetwork) << "NetworkMoveDialog::NetworkMoveDialog: opening for region"
                      << regionName << "projected:" << isProjectedCoords;
    setWindowTitle("Move Network");
    setMinimumWidth(450);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Network selection group
    QGroupBox *networkSelectionGroup =
        new QGroupBox("Select Network to Move");
    QVBoxLayout *networkLayout = new QVBoxLayout();

    // Instruction label
    QLabel *instructionLabel =
        new QLabel("Please select one network to move:");
    instructionLabel->setStyleSheet("font-weight: bold;");
    networkLayout->addWidget(instructionLabel);

    // Train network group
    trainGroupBox = new QGroupBox("Rail Networks");
    trainGroupBox->setCheckable(true);
    trainGroupBox->setChecked(false);
    QVBoxLayout *trainLayout = new QVBoxLayout();
    trainNetworkCombo        = new QComboBox();
    trainLayout->addWidget(trainNetworkCombo);
    trainGroupBox->setLayout(trainLayout);

    // Truck network group
    truckGroupBox = new QGroupBox("Truck Networks");
    truckGroupBox->setCheckable(true);
    truckGroupBox->setChecked(false);
    QVBoxLayout *truckLayout = new QVBoxLayout();
    truckNetworkCombo        = new QComboBox();
    truckLayout->addWidget(truckNetworkCombo);
    truckGroupBox->setLayout(truckLayout);

    // Add network groups to network selection layout
    networkLayout->addWidget(trainGroupBox);
    networkLayout->addWidget(truckGroupBox);
    networkSelectionGroup->setLayout(networkLayout);

    // Connect signals for group boxes
    connect(trainGroupBox, &QGroupBox::toggled, this,
            &NetworkMoveDialog::onTrainGroupToggled);
    connect(truckGroupBox, &QGroupBox::toggled, this,
            &NetworkMoveDialog::onTruckGroupToggled);

    // Connect signals for combo boxes
    connect(
        trainNetworkCombo,
        QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &NetworkMoveDialog::updateNetworkSelectionUI);
    connect(
        truckNetworkCombo,
        QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &NetworkMoveDialog::updateNetworkSelectionUI);

    // Movement offset group
    QGroupBox *offsetGroup =
        new QGroupBox("Movement Offset");
    QGridLayout *offsetLayout = new QGridLayout();

    // Create input widgets for offsets
    QLabel *horizontalLabel =
        new QLabel("Horizontal offset:");
    QLabel *verticalLabel = new QLabel("Vertical offset:");

    horizontalOffsetSpinBox = new QDoubleSpinBox();
    verticalOffsetSpinBox   = new QDoubleSpinBox();

    // Configure spinboxes to use locale-aware number
    // formatting
    horizontalOffsetSpinBox->setGroupSeparatorShown(true);
    verticalOffsetSpinBox->setGroupSeparatorShown(true);

    // Set range and precision based on coordinate system
    if (isProjected)
    {
        // Meters for projected coordinates
        horizontalOffsetSpinBox->setRange(-1'000'000,
                                          1'000'000);
        verticalOffsetSpinBox->setRange(-1'000'000,
                                        1'000'000);
        horizontalOffsetSpinBox->setDecimals(2);
        verticalOffsetSpinBox->setDecimals(2);
        horizontalOffsetSpinBox->setSingleStep(10);
        verticalOffsetSpinBox->setSingleStep(10);
        unitsLabel = new QLabel("Units: meters");
    }
    else
    {
        // Degrees for WGS84 coordinates
        horizontalOffsetSpinBox->setRange(-1, 1);
        verticalOffsetSpinBox->setRange(-1, 1);
        horizontalOffsetSpinBox->setDecimals(6);
        verticalOffsetSpinBox->setDecimals(6);
        horizontalOffsetSpinBox->setSingleStep(0.001);
        verticalOffsetSpinBox->setSingleStep(0.001);
        unitsLabel = new QLabel("Units: degrees");
    }

    // Information label
    QLabel *infoLabel =
        new QLabel("Positive values move East/South, "
                   "negative values move West/North");
    infoLabel->setWordWrap(true);

    // Add widgets to offset layout
    offsetLayout->addWidget(unitsLabel, 0, 0, 1, 2);
    offsetLayout->addWidget(infoLabel, 1, 0, 1, 2);
    offsetLayout->addWidget(horizontalLabel, 2, 0);
    offsetLayout->addWidget(horizontalOffsetSpinBox, 2, 1);
    offsetLayout->addWidget(verticalLabel, 3, 0);
    offsetLayout->addWidget(verticalOffsetSpinBox, 3, 1);
    offsetGroup->setLayout(offsetLayout);

    // Button box
    buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, this,
            &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this,
            &QDialog::reject);

    // Store a reference to the OK button
    okButton = buttonBox->button(QDialogButtonBox::Ok);

    // Initially disable the OK button
    okButton->setEnabled(false);

    // Add widgets to main layout
    mainLayout->addWidget(networkSelectionGroup);
    mainLayout->addWidget(offsetGroup);
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);

    // Populate network lists
    populateNetworkLists();
    updateNetworkSelectionUI();
}

NetworkMoveDialog::~NetworkMoveDialog()
{
    qCInfo(lcGuiNetwork) << "NetworkMoveDialog::~NetworkMoveDialog: closing";
}

void NetworkMoveDialog::populateNetworkLists()
{
    qCDebug(lcGuiNetwork) << "NetworkMoveDialog::populateNetworkLists: loading for region"
                       << regionName;
    // Clear existing items
    trainNetworkCombo->clear();
    truckNetworkCombo->clear();

    // Get region data
    Backend::RegionData *regionData =
        CargoNetSim::CargoNetSimController::getInstance()
            .getRegionDataController()
            ->getRegionData(regionName);

    if (!regionData)
    {
        qCWarning(lcGuiNetwork) << "NetworkMoveDialog::populateNetworkLists: null regionData for"
                             << regionName;
        return;
    }

    // Populate train networks
    QStringList trainNetworks =
        regionData->getTrainNetworks();
    for (const QString &name : trainNetworks)
    {
        trainNetworkCombo->addItem(name);
    }

    // Populate truck networks
    QStringList truckNetworks =
        regionData->getTruckNetworks();
    for (const QString &name : truckNetworks)
    {
        truckNetworkCombo->addItem(name);
    }

    // Update visibility of group boxes
    trainGroupBox->setVisible(!trainNetworks.isEmpty());
    truckGroupBox->setVisible(!truckNetworks.isEmpty());

    // Set enabled state
    trainNetworkCombo->setEnabled(!trainNetworks.isEmpty());
    truckNetworkCombo->setEnabled(!truckNetworks.isEmpty());

    // If only one network type is available, auto-select it
    if (!trainNetworks.isEmpty() && truckNetworks.isEmpty())
    {
        trainGroupBox->setChecked(true);
        selectedNetworkType = NetworkType::Train;
        if (trainNetworkCombo->count() > 0)
        {
            selectedNetworkName =
                trainNetworkCombo->currentText();
            // Enable OK button since a network is selected
            okButton->setEnabled(true);
        }
    }
    else if (trainNetworks.isEmpty()
             && !truckNetworks.isEmpty())
    {
        truckGroupBox->setChecked(true);
        selectedNetworkType = NetworkType::Truck;
        if (truckNetworkCombo->count() > 0)
        {
            selectedNetworkName =
                truckNetworkCombo->currentText();
            // Enable OK button since a network is selected
            okButton->setEnabled(true);
        }
    }
}

void NetworkMoveDialog::onTrainGroupToggled(bool checked)
{
    qCDebug(lcGuiNetwork) << "NetworkMoveDialog::onTrainGroupToggled:" << checked;
    if (checked)
    {
        // Uncheck truck group
        truckGroupBox->blockSignals(true);
        truckGroupBox->setChecked(false);
        truckGroupBox->blockSignals(false);

        // Set train as selected network type
        selectedNetworkType = NetworkType::Train;
        if (trainNetworkCombo->count() > 0
            && trainNetworkCombo->currentIndex() >= 0)
        {
            selectedNetworkName =
                trainNetworkCombo->currentText();
            // Enable OK button since a network is selected
            okButton->setEnabled(true);
        }
        else
        {
            selectedNetworkName = "";
            // Disable OK button since no network is
            // selected
            okButton->setEnabled(false);
        }
    }
    else if (!truckGroupBox->isChecked())
    {
        // If both are unchecked, clear selection
        selectedNetworkName = "";
        // Disable OK button since no network is selected
        okButton->setEnabled(false);
    }

    // Update UI
    trainNetworkCombo->setEnabled(checked);
    updateNetworkSelectionUI();
}

void NetworkMoveDialog::onTruckGroupToggled(bool checked)
{
    qCDebug(lcGuiNetwork) << "NetworkMoveDialog::onTruckGroupToggled:" << checked;
    if (checked)
    {
        // Uncheck train group
        trainGroupBox->blockSignals(true);
        trainGroupBox->setChecked(false);
        trainGroupBox->blockSignals(false);

        // Set truck as selected network type
        selectedNetworkType = NetworkType::Truck;
        if (truckNetworkCombo->count() > 0
            && truckNetworkCombo->currentIndex() >= 0)
        {
            selectedNetworkName =
                truckNetworkCombo->currentText();
            // Enable OK button since a network is selected
            okButton->setEnabled(true);
        }
        else
        {
            selectedNetworkName = "";
            // Disable OK button since no network is
            // selected
            okButton->setEnabled(false);
        }
    }
    else if (!trainGroupBox->isChecked())
    {
        // If both are unchecked, clear selection
        selectedNetworkName = "";
        // Disable OK button since no network is selected
        okButton->setEnabled(false);
    }

    // Update UI
    truckNetworkCombo->setEnabled(checked);
    updateNetworkSelectionUI();
}

void NetworkMoveDialog::updateNetworkSelectionUI()
{
    qCDebug(lcGuiNetwork) << "NetworkMoveDialog::updateNetworkSelectionUI: selected"
                       << selectedNetworkName;
    // Update based on current selections
    if (trainGroupBox->isChecked()
        && trainNetworkCombo->currentIndex() >= 0)
    {
        selectedNetworkType = NetworkType::Train;
        selectedNetworkName =
            trainNetworkCombo->currentText();
        // Enable OK button since a network is selected
        okButton->setEnabled(true);
    }
    else if (truckGroupBox->isChecked()
             && truckNetworkCombo->currentIndex() >= 0)
    {
        selectedNetworkType = NetworkType::Truck;
        selectedNetworkName =
            truckNetworkCombo->currentText();
        // Enable OK button since a network is selected
        okButton->setEnabled(true);
    }
    else
    {
        selectedNetworkName = "";
        // Disable OK button since no network is selected
        okButton->setEnabled(false);
    }
}

QPointF NetworkMoveDialog::getOffset() const
{
    return QPointF(horizontalOffsetSpinBox->value(),
                   verticalOffsetSpinBox->value());
}

NetworkType
NetworkMoveDialog::getSelectedNetworkType() const
{
    return selectedNetworkType;
}

QString NetworkMoveDialog::getSelectedNetworkName() const
{
    return selectedNetworkName;
}

bool NetworkMoveDialog::hasNetworkSelected() const
{
    return !selectedNetworkName.isEmpty();
}

} // namespace GUI
} // namespace CargoNetSim
