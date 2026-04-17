#include "SetCoordinatesDialog.h"

#include "Backend/Commons/LogCategories.h"

namespace CargoNetSim
{
namespace GUI
{

SetCoordinatesDialog::SetCoordinatesDialog(
    const QString &terminalName, QPointF geoPoint,
    QWidget *parent)
    : QDialog(parent)
{
    qCDebug(lcGuiUtil)
        << "SetCoordinatesDialog: constructing for"
        << terminalName << "at" << geoPoint;
    // Set window properties
    setWindowTitle(
        tr("Set Global Position for %1").arg(terminalName));
    setMinimumWidth(350);

    // Create main layout
    mainLayout = new QVBoxLayout(this);

    // Create information label
    infoLabel =
        new QLabel(tr("Set the global position (WGS84) for "
                      "this terminal.\n"
                      "This will adjust the region "
                      "center's shared coordinates."));
    infoLabel->setWordWrap(true);
    mainLayout->addWidget(infoLabel);

    // Create form layout for coordinate inputs
    formLayout = new QFormLayout();

    // Latitude input
    latInput = new QDoubleSpinBox();
    latInput->setRange(-90.0, 90.0);
    latInput->setDecimals(6);
    latInput->setValue(geoPoint.y());
    latInput->setSingleStep(0.1);
    latInput->setToolTip(
        tr("Latitude value in degrees (-90° to 90°)"));
    formLayout->addRow(tr("Latitude:"), latInput);

    // Longitude input
    lonInput = new QDoubleSpinBox();
    lonInput->setRange(-180.0, 180.0);
    lonInput->setDecimals(6);
    lonInput->setValue(geoPoint.x());
    lonInput->setSingleStep(0.1);
    lonInput->setToolTip(
        tr("Longitude value in degrees (-180° to 180°)"));
    formLayout->addRow(tr("Longitude:"), lonInput);

    mainLayout->addLayout(formLayout);

    // Add dialog buttons
    buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

    // Configure OK button behavior
    QPushButton *okButton =
        buttonBox->button(QDialogButtonBox::Ok);
    okButton->setDefault(true);
    okButton->setShortcut(Qt::CTRL | Qt::Key_Return);

    // Connect dialog buttons
    connect(buttonBox, &QDialogButtonBox::accepted, this,
            &SetCoordinatesDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this,
            &SetCoordinatesDialog::reject);

    // Connect coordinate changes
    connect(latInput,
            QOverload<double>::of(
                &QDoubleSpinBox::valueChanged),
            this,
            &SetCoordinatesDialog::onCoordinatesChanged);
    connect(lonInput,
            QOverload<double>::of(
                &QDoubleSpinBox::valueChanged),
            this,
            &SetCoordinatesDialog::onCoordinatesChanged);

    mainLayout->addWidget(buttonBox);
}

QPointF SetCoordinatesDialog::getCoordinates() const
{
    const QPointF coords(lonInput->value(), latInput->value());
    qCDebug(lcGuiUtil)
        << "SetCoordinatesDialog::getCoordinates ->"
        << coords;
    return coords;
}

void SetCoordinatesDialog::onCoordinatesChanged()
{
    qCDebug(lcGuiUtil)
        << "SetCoordinatesDialog::onCoordinatesChanged -> lat"
        << latInput->value() << "lon" << lonInput->value();
    // Emit signal with new coordinate values
    emit coordinatesChanged(
        QPointF(lonInput->value(), latInput->value()));
}

} // namespace GUI
} // namespace CargoNetSim
