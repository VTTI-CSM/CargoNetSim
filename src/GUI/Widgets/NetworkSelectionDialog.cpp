// NetworkSelectionDialog.cpp
#include "NetworkSelectionDialog.h"
#include <QLabel>
#include <QMessageBox>
namespace CargoNetSim
{
namespace GUI
{
NetworkSelectionDialog::NetworkSelectionDialog(
    QWidget *parent, Mode mode)
    : QDialog(parent)
    , currentMode(mode)
{
    setWindowTitle(mode == LinkMode
                       ? "Select Network Types to Link"
                       : "Select Network Types to Unlink");
    setMinimumWidth(350);
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    // Add description label
    descriptionLabel = new QLabel(
        mode == LinkMode ? "Select the network type(s) to "
                           "link terminals to:"
                         : "Select the network type(s) to "
                           "unlink terminals from:",
        this);
    descriptionLabel->setWordWrap(true);
    mainLayout->addWidget(descriptionLabel);
    // Add network type checkboxes
    trainNetworkCheckBox =
        new QCheckBox("Train Network", this);
    truckNetworkCheckBox =
        new QCheckBox("Truck Network", this);
    mainLayout->addWidget(trainNetworkCheckBox);
    mainLayout->addWidget(truckNetworkCheckBox);
    // Create button layout
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    // Create custom buttons
    linkSelectedButton   = new QPushButton(this);
    linkAllVisibleButton = new QPushButton(this);
    cancelButton         = new QPushButton("Cancel", this);

    // Update button labels based on mode
    updateButtonLabels();

    // Add buttons to layout
    buttonLayout->addWidget(linkSelectedButton);
    buttonLayout->addWidget(linkAllVisibleButton);
    buttonLayout->addWidget(cancelButton);
    // Add button layout to main layout
    mainLayout->addLayout(buttonLayout);
    // Disable buttons initially
    linkSelectedButton->setEnabled(false);
    linkAllVisibleButton->setEnabled(false);
    // Connect signals
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    connect(
        trainNetworkCheckBox, &QCheckBox::checkStateChanged,
        this,
        &NetworkSelectionDialog::onCheckBoxStateChanged);
    connect(
        truckNetworkCheckBox, &QCheckBox::checkStateChanged,
        this,
        &NetworkSelectionDialog::onCheckBoxStateChanged);
#else
    connect(
        trainNetworkCheckBox, &QCheckBox::stateChanged,
        this,
        &NetworkSelectionDialog::onCheckBoxStateChanged);
    connect(
        truckNetworkCheckBox, &QCheckBox::stateChanged,
        this,
        &NetworkSelectionDialog::onCheckBoxStateChanged);
#endif
    connect(linkSelectedButton, &QPushButton::clicked, this,
            &QDialog::accept);
    connect(linkAllVisibleButton, &QPushButton::clicked,
            [this]() {
                // Set a result code to distinguish between
                // link selected and link all
                this->done(QDialog::Accepted + 1);
            });
    connect(cancelButton, &QPushButton::clicked, this,
            &QDialog::reject);
    setLayout(mainLayout);
}

void NetworkSelectionDialog::setMode(Mode mode)
{
    if (currentMode != mode)
    {
        currentMode = mode;
        setWindowTitle(
            mode == LinkMode
                ? "Select Network Types to Link"
                : "Select Network Types to Unlink");

        descriptionLabel->setText(
            mode == LinkMode ? "Select the network type(s) "
                               "to link terminals to:"
                             : "Select the network type(s) "
                               "to unlink terminals from:");

        updateButtonLabels();
    }
}

void NetworkSelectionDialog::updateButtonLabels()
{
    QString actionVerb =
        currentMode == LinkMode ? "Link" : "Unlink";

    linkSelectedButton->setText(actionVerb
                                + " Selected Terminals");
    linkAllVisibleButton->setText(
        actionVerb + " All Visible Terminals");
}

QList<NetworkType>
NetworkSelectionDialog::getSelectedNetworkTypes() const
{
    QList<NetworkType> types;
    if (trainNetworkCheckBox->isChecked())
    {
        types.append(NetworkType::Train);
    }
    if (truckNetworkCheckBox->isChecked())
    {
        types.append(NetworkType::Truck);
    }
    return types;
}

void NetworkSelectionDialog::onCheckBoxStateChanged()
{
    // Enable buttons only if at least one network type is
    // selected
    bool enable = trainNetworkCheckBox->isChecked()
                  || truckNetworkCheckBox->isChecked();
    linkSelectedButton->setEnabled(enable);
    linkAllVisibleButton->setEnabled(enable);
}
} // namespace GUI
} // namespace CargoNetSim
