#include "ShipManagerDialog.h"
#include "Backend/Commons/LogCategories.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QMessageBox>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

// Include the Ship class header
#include "../../Backend/Models/ShipSystem.h"
#include "../Utils/IconCreator.h"

namespace CargoNetSim
{
namespace GUI
{

ShipManagerDialog::ShipManagerDialog(QWidget *parent)
    : QDialog(parent)
    , m_ships()
{
    qCInfo(lcGuiNetwork) << "ShipManagerDialog::ShipManagerDialog: opening";
    setWindowTitle(tr("Ship Manager"));
    setMinimumSize(1000, 700);

    initUI();
}

void ShipManagerDialog::initUI()
{
    qCDebug(lcGuiNetwork) << "ShipManagerDialog::initUI: building UI";
    QVBoxLayout *layout = new QVBoxLayout(this);

    // Create toolbar
    m_toolbar = new QToolBar();
    m_toolbar->setIconSize(QSize(32, 32));
    m_toolbar->setStyleSheet(
        "QToolButton {"
        "    padding: 6px;"
        "    icon-size: 32px;"
        "}"
        "QToolButton:hover {"
        "    background-color: #E5E5E5;"
        "}");

    // Add ship action
    m_loadAction = new QAction(tr("Load Ships"), this);
    m_loadAction->setIcon(
        QIcon(IconFactory::createImportShipsIcon()));
    m_loadAction->setToolTip(
        tr("Load ships from DAT file"));
    connect(m_loadAction, &QAction::triggered, this,
            &ShipManagerDialog::loadShips);

    // Create QToolButton for load action
    QToolButton *loadButton = new QToolButton();
    loadButton->setDefaultAction(m_loadAction);
    loadButton->setToolButtonStyle(
        Qt::ToolButtonTextUnderIcon);
    loadButton->setText(tr("Load\nShips"));
    m_toolbar->addWidget(loadButton);

    // Delete ship action
    m_deleteAction = new QAction(tr("Delete Ship"), this);
    m_deleteAction->setIcon(
        QIcon(IconFactory::createDeleteShipIcon()));
    m_deleteAction->setToolTip(tr("Delete selected ship"));
    m_deleteAction->setEnabled(false);  // Initially disabled
    connect(m_deleteAction, &QAction::triggered, this,
            &ShipManagerDialog::deleteShip);

    // Create QToolButton for delete action
    QToolButton *deleteButton = new QToolButton();
    deleteButton->setDefaultAction(m_deleteAction);
    deleteButton->setToolButtonStyle(
        Qt::ToolButtonTextUnderIcon);
    deleteButton->setText(tr("Delete\nShip"));
    m_toolbar->addWidget(deleteButton);

    layout->addWidget(m_toolbar);

    // Create splitter for table and details
    m_splitter = new QSplitter(Qt::Vertical);

    // Create main table for ships overview
    m_table = new QTableWidget();
    m_table->setColumnCount(8);
    m_table->setHorizontalHeaderLabels(
        {tr("Ship ID"), tr("Max Speed (knots)"),
         tr("Length (m)"), tr("Beam (m)"),
         tr("Draft (F/A) (m)"), tr("Displacement (m³)"),
         tr("Cargo Weight (t)"), tr("Propulsion")});

    // Set selection behavior and editing flags
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // Set column stretch
    QHeaderView *header = m_table->horizontalHeader();
    header->setSectionResizeMode(
        0, QHeaderView::ResizeToContents);
    for (int i = 1; i < 8; ++i)
    {
        header->setSectionResizeMode(i,
                                     QHeaderView::Stretch);
    }

    // Create details view
    m_detailsText = new QTextEdit();
    m_detailsText->setReadOnly(true);
    m_detailsText->setMinimumHeight(300);

    // Add widgets to splitter
    m_splitter->addWidget(m_table);
    m_splitter->addWidget(m_detailsText);

    // Set initial sizes for splitter (60% table, 40%
    // details)
    m_splitter->setSizes({400, 300});

    layout->addWidget(m_splitter);

    // Connect selection change to update details and delete button state
    connect(m_table, &QTableWidget::itemSelectionChanged,
            [this]() {
                updateDetails();
                m_deleteAction->setEnabled(m_table->currentRow() >= 0);
            });

    // Add Accept/Cancel buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, this,
            &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this,
            &QDialog::reject);
    layout->addWidget(buttonBox);
}

void ShipManagerDialog::loadShips()
{
    qCDebug(lcGuiNetwork) << "ShipManagerDialog::loadShips: opening file dialog";
    QString fileName = QFileDialog::getOpenFileName(
        this, tr("Load Ships File"), QString(),
        tr("DAT Files (*.dat);;All Files (*)"));

    if (fileName.isEmpty())
    {
        qCDebug(lcGuiNetwork) << "ShipManagerDialog::loadShips: cancelled";
        return;
    }

    qCDebug(lcGuiNetwork) << "ShipManagerDialog::loadShips: reading" << fileName;
    try
    {
        QList<CargoNetSim::Backend::Ship *> loadedShips =
            Backend::ShipsReader::readShipsFile(fileName);

        if (loadedShips.isEmpty())
        {
            qCWarning(lcGuiNetwork) << "ShipManagerDialog::loadShips: no valid ships in file";
            QMessageBox::warning(
                this, tr("Warning"),
                tr("No valid ships found in the file."));
            return;
        }

        qCInfo(lcGuiNetwork) << "ShipManagerDialog::loadShips: loaded"
                         << loadedShips.size() << "ships from" << fileName;
        // Extend the existing ships list
        m_ships.append(loadedShips);
        m_newlyLoadedFiles.append(fileName);
        updateTable();

        // Emit signal
        emit shipsLoaded(loadedShips.size());

        QMessageBox::information(
            this, tr("Ships Loaded"),
            tr("Successfully loaded %1 ships.")
                .arg(loadedShips.size()));
    }
    catch (const std::exception &e)
    {
        qCWarning(lcGuiNetwork) << "ShipManagerDialog::loadShips: exception" << e.what();
        QMessageBox::critical(
            this, tr("Error"),
            tr("Failed to load ships: %1").arg(e.what()));
    }
}

void ShipManagerDialog::deleteShip()
{
    int currentRow = m_table->currentRow();
    if (currentRow < 0)
    {
        qCWarning(lcGuiNetwork) << "ShipManagerDialog::deleteShip: no row selected";
        QMessageBox::warning(
            this, tr("Warning"),
            tr("Please select a ship to delete."));
        return;
    }

    QString shipId = m_table->item(currentRow, 0)->text();

    QMessageBox::StandardButton reply =
        QMessageBox::question(
            this, tr("Confirm Delete"),
            tr("Are you sure you want to delete ship '%1'?")
                .arg(shipId),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);

    if (reply == QMessageBox::Yes)
    {
        qCInfo(lcGuiNetwork) << "ShipManagerDialog::deleteShip: confirmed deletion of"
                         << shipId;
        // Store the ship ID before removal
        emit shipDeleted(shipId);

        // Remove the ship
        m_ships.removeAt(currentRow);
        updateTable();
        m_detailsText->clear();
    }
}

void ShipManagerDialog::updateTable()
{
    qCDebug(lcGuiNetwork) << "ShipManagerDialog::updateTable: refreshing" << m_ships.size() << "ships";
    // Clear the table
    m_table->setRowCount(0);

    // Populate with ships
    for (const auto &ship : m_ships)
    {
        int row = m_table->rowCount();
        m_table->insertRow(row);

        // Ship ID
        m_table->setItem(
            row, 0,
            new QTableWidgetItem(ship->getUserId()));

        // Max Speed
        m_table->setItem(
            row, 1,
            new QTableWidgetItem(QString::number(
                ship->getMaxSpeed(), 'f', 1)));

        // Length
        m_table->setItem(
            row, 2,
            new QTableWidgetItem(QString::number(
                ship->getWaterlineLength(), 'f', 1)));

        // Beam
        m_table->setItem(
            row, 3,
            new QTableWidgetItem(
                QString::number(ship->getBeam(), 'f', 1)));

        // Draft
        m_table->setItem(
            row, 4,
            new QTableWidgetItem(QString("%1/%2").arg(
                QString::number(ship->getDraftAtForward(),
                                'f', 1),
                QString::number(ship->getDraftAtAft(), 'f',
                                1))));

        // Displacement
        double  disp = ship->getVolumetricDisplacement();
        QString dispText =
            disp > 0 ? QString::number(disp, 'f', 1)
                     : tr("N/A");
        m_table->setItem(row, 5,
                         new QTableWidgetItem(dispText));

        // Cargo Weight
        m_table->setItem(
            row, 6,
            new QTableWidgetItem(QString::number(
                ship->getCargoWeight(), 'f', 1)));

        // Propulsion summary
        QString propText =
            QString("%1x %2m")
                .arg(ship->getPropellerCount())
                .arg(QString::number(
                    ship->getPropellerDiameter(), 'f', 1));
        m_table->setItem(row, 7,
                         new QTableWidgetItem(propText));
    }
}

void ShipManagerDialog::updateDetails()
{
    qCDebug(lcGuiNetwork) << "ShipManagerDialog::updateDetails: row" << m_table->currentRow();
    int currentRow = m_table->currentRow();
    if (currentRow < 0 || currentRow >= m_ships.size())
    {
        m_detailsText->clear();
        return;
    }

    Backend::Ship* ship = m_ships.at(currentRow);
    if (!ship)
    {
        m_detailsText->clear();
        return;
    }

    // Emit signal when a ship is selected
    emit shipSelected(ship->getUserId());

    // Format and display the details
    m_detailsText->setHtml(formatShipDetails(*ship));
}

QString ShipManagerDialog::formatShipDetails(
    const Backend::Ship &ship) const
{
    QString details =
        QString(
            "<h2>Ship Details for ship ID: %1</h2>"

            "<h3>Physical Dimensions:</h3>"
            "<ul>"
            "    <li><b>Waterline Length:</b> %2 m</li>"
            "    <li><b>Length between Perpendiculars:</b> "
            "%3 m</li>"
            "    <li><b>Beam:</b> %4 m</li>"
            "    <li><b>Draft:</b> Forward %5 m, Aft %6 "
            "m</li>"
            "    <li><b>Displacement:</b> %7 m³</li>"
            "</ul>"

            "<h3>Hull Characteristics:</h3>"
            "<ul>"
            "    <li><b>Wetted Hull Surface:</b> %8 m²</li>"
            "    <li><b>Area Above Waterline:</b> %9 "
            "m²</li>"
            "    <li><b>Surface Roughness:</b> %10</li>"
            "    <li><b>Buoyancy Center:</b> %11</li>"
            "</ul>"

            "<h3>Coefficients:</h3>"
            "<ul>"
            "    <li><b>Block Coefficient:</b> %12</li>"
            "    <li><b>Prismatic Coefficient:</b> %13</li>"
            "    <li><b>Midship Section Coefficient:</b> "
            "%14</li>"
            "    <li><b>Waterplane Area Coefficient:</b> "
            "%15</li>"
            "</ul>"

            "<h3>Propulsion System:</h3>"
            "<ul>"
            "    <li><b>Propellers:</b> %16x Ø%17m</li>"
            "    <li><b>Propeller Pitch:</b> %18 m</li>"
            "    <li><b>Blades per Propeller:</b> %19</li>"
            "    <li><b>Engines per Propeller:</b> %20</li>"
            "    <li><b>Gearbox Ratio:</b> %21</li>"
            "    <li><b>System Efficiencies:</b>"
            "        <ul>"
            "            <li><b>Gearbox:</b> %22</li>"
            "            <li><b>Shaft:</b> %23</li>"
            "        </ul>"
            "    </li>"
            "</ul>"

            "<h3>Weights:</h3>"
            "<ul>"
            "    <li><b>Vessel Weight:</b> %24 t</li>"
            "    <li><b>Cargo Weight:</b> %25 t</li>"
            "</ul>"

            "<h3>Operational Parameters:</h3>"
            "<ul>"
            "    <li><b>Maximum Speed:</b> %26 knots</li>"
            "    <li><b>Maximum Rudder Angle:</b> %27°</li>"
            "    <li><b>Stop if No Energy:</b> %28</li>"
            "</ul>")
            .arg(ship.getUserId())
            .arg(QString::number(ship.getWaterlineLength(),
                                 'f', 2))
            .arg(QString::number(
                ship.getLengthBetweenPerpendiculars(), 'f',
                2))
            .arg(QString::number(ship.getBeam(), 'f', 2))
            .arg(QString::number(ship.getDraftAtForward(),
                                 'f', 2))
            .arg(QString::number(ship.getDraftAtAft(), 'f',
                                 2))
            .arg(ship.getVolumetricDisplacement() > 0
                     ? QString::number(
                           ship.getVolumetricDisplacement(),
                           'f', 2)
                     : tr("N/A"))

            .arg(ship.getWettedHullSurface() > 0
                     ? QString::number(
                           ship.getWettedHullSurface(), 'f',
                           2)
                     : tr("N/A"))
            .arg(QString::number(
                ship.getAreaAboveWaterline(), 'f', 2))
            .arg(QString::number(ship.getSurfaceRoughness(),
                                 'f', 4))
            .arg(QString::number(ship.getBuoyancyCenter(),
                                 'f', 2))

            .arg(ship.getBlockCoef() > 0
                     ? QString::number(ship.getBlockCoef(),
                                       'f', 4)
                     : tr("N/A"))
            .arg(ship.getPrismaticCoef() > 0
                     ? QString::number(
                           ship.getPrismaticCoef(), 'f', 4)
                     : tr("N/A"))
            .arg(ship.getMidshipSectionCoef() > 0
                     ? QString::number(
                           ship.getMidshipSectionCoef(),
                           'f', 4)
                     : tr("N/A"))
            .arg(ship.getWaterplaneAreaCoef() > 0
                     ? QString::number(
                           ship.getWaterplaneAreaCoef(),
                           'f', 4)
                     : tr("N/A"))

            .arg(ship.getPropellerCount())
            .arg(QString::number(
                ship.getPropellerDiameter(), 'f', 2))
            .arg(QString::number(ship.getPropellerPitch(),
                                 'f', 2))
            .arg(ship.getPropellerBladesCount())
            .arg(ship.getEnginesPerPropellerCount())
            .arg(QString::number(ship.getGearboxRatio(),
                                 'f', 3))

            .arg(QString::number(
                ship.getGearboxEfficiency(), 'f', 3))
            .arg(QString::number(ship.getShaftEfficiency(),
                                 'f', 3))

            .arg(QString::number(ship.getVesselWeight(),
                                 'f', 2))
            .arg(QString::number(ship.getCargoWeight(), 'f',
                                 2))

            .arg(
                QString::number(ship.getMaxSpeed(), 'f', 1))
            .arg(ship.getMaxRudderAngle() > 0
                     ? QString::number(
                           ship.getMaxRudderAngle(), 'f', 1)
                     : tr("N/A"))
            .arg(ship.shouldStopIfNoEnergy() ? tr("Yes")
                                             : tr("No"));

    return details;
}

QList<Backend::Ship *> ShipManagerDialog::getShips() const
{
    return m_ships;
}

void ShipManagerDialog::setShips(
    const QList<Backend::Ship *> &ships)
{
    qCDebug(lcGuiNetwork) << "ShipManagerDialog::setShips: count" << ships.size();
    m_ships = ships;
    updateTable();
}

} // namespace GUI
} // namespace CargoNetSim
