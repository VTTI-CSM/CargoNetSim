#include "TrainManagerDialog.h"
#include "Backend/Commons/LogCategories.h"

#include <QAction>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QMessageBox>
#include <QSplitter>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

#include "../../Backend/Models/TrainSystem.h"
#include "../Utils/IconCreator.h"

namespace CargoNetSim
{
namespace GUI
{

TrainManagerDialog::TrainManagerDialog(QWidget *parent)
    : QDialog(parent)
    , m_table(nullptr)
    , m_detailsText(nullptr)
    , m_loadButton(nullptr)
    , m_deleteButton(nullptr)
{
    qCInfo(lcGuiNetwork) << "TrainManagerDialog::TrainManagerDialog: opening";
    initUI();
}

void TrainManagerDialog::initUI()
{
    qCDebug(lcGuiNetwork) << "TrainManagerDialog::initUI: building UI";
    setWindowTitle(tr("Train Manager"));
    setMinimumSize(800, 600);

    QVBoxLayout *layout = new QVBoxLayout(this);

    // Create toolbar
    QToolBar *toolbar = new QToolBar();
    toolbar->setIconSize(QSize(32, 32));
    toolbar->setStyleSheet("QToolButton { "
                           "   padding: 6px; "
                           "   icon-size: 32px; "
                           "} "
                           "QToolButton:hover { "
                           "   background-color: #E5E5E5; "
                           "}");

    // Load trains action
    QAction *loadAction =
        new QAction(tr("Load Trains"), this);
    loadAction->setIcon(
        QIcon(IconFactory::createImportTrainsIcon()));
    loadAction->setToolTip(tr("Load trains from DAT file"));

    m_loadButton = new QToolButton();
    m_loadButton->setDefaultAction(loadAction);
    m_loadButton->setToolButtonStyle(
        Qt::ToolButtonTextUnderIcon);
    m_loadButton->setText(tr("Load\nTrains"));
    connect(m_loadButton, &QToolButton::clicked, this,
            &TrainManagerDialog::loadTrains);
    toolbar->addWidget(m_loadButton);

    // Delete train action
    QAction *deleteAction =
        new QAction(tr("Delete Train"), this);
    deleteAction->setIcon(
        QIcon(IconFactory::createDeleteTrainIcon()));
    deleteAction->setToolTip(tr("Delete selected train"));

    m_deleteButton = new QToolButton();
    m_deleteButton->setDefaultAction(deleteAction);
    m_deleteButton->setToolButtonStyle(
        Qt::ToolButtonTextUnderIcon);
    m_deleteButton->setText(tr("Delete\nTrain"));
    m_deleteButton->setEnabled(false); // Initially disabled
    connect(m_deleteButton, &QToolButton::clicked, this,
            &TrainManagerDialog::deleteTrain);
    toolbar->addWidget(m_deleteButton);

    layout->addWidget(toolbar);

    // Create splitter for table and details
    QSplitter *splitter = new QSplitter(Qt::Vertical);

    // Create table for trains overview
    m_table = new QTableWidget();
    m_table->setColumnCount(3);
    m_table->setHorizontalHeaderLabels(
        {tr("Train ID"), tr("Locomotives"), tr("Cars")});
    m_table->setSelectionBehavior(QTableWidget::SelectRows);
    m_table->setSelectionMode(
        QTableWidget::SingleSelection);
    m_table->setEditTriggers(QTableWidget::NoEditTriggers);

    // Set column stretch
    QHeaderView *header = m_table->horizontalHeader();
    header->setSectionResizeMode(
        0, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(1, QHeaderView::Stretch);
    header->setSectionResizeMode(2, QHeaderView::Stretch);

    // Connect selection change to update details and enable
    // delete button
    connect(m_table, &QTableWidget::itemSelectionChanged,
            [this]() {
                bool hasSelection =
                    !m_table->selectedItems().isEmpty();
                m_deleteButton->setEnabled(hasSelection);
                if (hasSelection)
                {
                    updateTrainDetails();
                }
                else
                {
                    m_detailsText->clear();
                }
            });

    // Create details view
    m_detailsText = new QTextEdit();
    m_detailsText->setReadOnly(true);
    m_detailsText->setMinimumHeight(300);

    // Add widgets to splitter
    splitter->addWidget(m_table);
    splitter->addWidget(m_detailsText);

    // Set initial sizes (60% table, 40% details)
    splitter->setSizes({400, 300});

    layout->addWidget(splitter);

    // Add Accept/Cancel buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, this,
            &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this,
            &QDialog::reject);
    layout->addWidget(buttonBox);
}

void TrainManagerDialog::setTrains(
    const QVector<Backend::Train *> trains)
{
    qCDebug(lcGuiNetwork) << "TrainManagerDialog::setTrains: count" << trains.size();
    m_trains = trains;
    updateTable();
    emit trainListChanged();
}

QVector<Backend::Train *>
TrainManagerDialog::getTrains() const
{
    return m_trains;
}

void TrainManagerDialog::loadTrains()
{
    qCDebug(lcGuiNetwork) << "TrainManagerDialog::loadTrains: opening file dialog";
    QString fileName = QFileDialog::getOpenFileName(
        this, tr("Load Trains File"), QString(),
        tr("DAT Files (*.dat);;All Files (*)"));

    if (fileName.isEmpty())
    {
        qCDebug(lcGuiNetwork) << "TrainManagerDialog::loadTrains: cancelled";
        return;
    }

    qCDebug(lcGuiNetwork) << "TrainManagerDialog::loadTrains: reading" << fileName;
    try
    {
        QVector<Backend::Train *> newTrains =
            Backend::TrainsReader::readTrainsFile(fileName);

        if (newTrains.isEmpty())
        {
            qCWarning(lcGuiNetwork) << "TrainManagerDialog::loadTrains: no trains in file";
            QMessageBox::warning(
                this, tr("Warning"),
                tr("No trains were found in the selected "
                   "file."));
            emit trainsLoaded(0, false);
            return;
        }

        qCInfo(lcGuiNetwork) << "TrainManagerDialog::loadTrains: loaded"
                         << newTrains.size() << "trains from" << fileName;
        m_trains.append(newTrains);
        m_newlyLoadedFiles.append(fileName);
        updateTable();

        QMessageBox::information(
            this, tr("Success"),
            tr("Successfully loaded %1 trains.")
                .arg(newTrains.size()));

        emit trainsLoaded(newTrains.size(), true);
        emit trainListChanged();
    }
    catch (const std::exception &e)
    {
        qCWarning(lcGuiNetwork) << "TrainManagerDialog::loadTrains: exception" << e.what();
        QMessageBox::critical(
            this, tr("Error"),
            tr("Failed to load trains: %1").arg(e.what()));
        emit trainsLoaded(0, false);
    }
}

void TrainManagerDialog::deleteTrain()
{
    int currentRow = m_table->currentRow();
    if (currentRow < 0 || currentRow >= m_trains.size())
    {
        qCWarning(lcGuiNetwork) << "TrainManagerDialog::deleteTrain: invalid row" << currentRow;
        return;
    }

    QString trainId = m_trains[currentRow]->getUserId();
    qCDebug(lcGuiNetwork) << "TrainManagerDialog::deleteTrain: confirming deletion of" << trainId;

    // Confirm deletion
    QMessageBox::StandardButton reply =
        QMessageBox::question(this, tr("Confirm Deletion"),
                              tr("Are you sure you want to "
                                 "delete train '%1'?")
                                  .arg(trainId),
                              QMessageBox::Yes
                                  | QMessageBox::No);

    if (reply != QMessageBox::Yes)
    {
        return;
    }

    qCInfo(lcGuiNetwork) << "TrainManagerDialog::deleteTrain: confirmed deletion of" << trainId;
    // Delete train and update UI
    m_trains.removeAt(currentRow);
    updateTable();
    m_detailsText->clear();

    emit trainDeleted(trainId);
    emit trainListChanged();
}

void TrainManagerDialog::updateTable()
{
    qCDebug(lcGuiNetwork) << "TrainManagerDialog::updateTable: refreshing" << m_trains.size() << "trains";
    m_table->setRowCount(0);

    for (const auto &train : m_trains)
    {
        int row = m_table->rowCount();
        m_table->insertRow(row);

        // Train ID
        QTableWidgetItem *idItem =
            new QTableWidgetItem(train->getUserId());
        m_table->setItem(row, 0, idItem);

        // Locomotives summary
        QString     locoStr;
        const auto &locomotives = train->getLocomotives();
        for (int i = 0; i < locomotives.size(); ++i)
        {
            const auto &loco = locomotives[i];
            locoStr +=
                QString("%1x Type %2 (%3kW)")
                    .arg(loco->getCount())
                    .arg(loco->getLocoType())
                    .arg(loco->getPower(), 0, 'f', 0);

            if (i < locomotives.size() - 1)
            {
                locoStr += "; ";
            }
        }
        QTableWidgetItem *locoItem =
            new QTableWidgetItem(locoStr);
        m_table->setItem(row, 1, locoItem);

        // Cars summary
        QString     carsStr;
        const auto &cars = train->getCars();
        for (int i = 0; i < cars.size(); ++i)
        {
            const auto &car = cars[i];
            carsStr += QString("%1x Type %2")
                           .arg(car->getCount())
                           .arg(car->getCarType());

            if (i < cars.size() - 1)
            {
                carsStr += "; ";
            }
        }
        QTableWidgetItem *carsItem =
            new QTableWidgetItem(carsStr);
        m_table->setItem(row, 2, carsItem);
    }
}

void TrainManagerDialog::updateTrainDetails()
{
    qCDebug(lcGuiNetwork) << "TrainManagerDialog::updateTrainDetails: row" << m_table->currentRow();
    int currentRow = m_table->currentRow();
    if (currentRow < 0 || currentRow >= m_trains.size())
    {
        m_detailsText->clear();
        return;
    }

    const auto &train = m_trains[currentRow];
    m_detailsText->setHtml(formatTrainDetails(train));
}

QString TrainManagerDialog::formatTrainDetails(
    const Backend::Train *train) const
{
    QString details =
        QString("<h2>Train Details for train ID: %1</h2>"
                "<h3>Locomotives:</h3>"
                "<ul>")
            .arg(train->getUserId());

    // Locomotive details
    for (const auto &loco : train->getLocomotives())
    {
        details +=
            QString("<li><b>Type %1:</b> %2 units<ul>"
                    "    <li>Power: %3 kW</li>"
                    "    <li>Gross Weight: %4 tons</li>"
                    "    <li>Length: %5 m</li>"
                    "</ul></li>")
                .arg(loco->getLocoType())
                .arg(loco->getCount())
                .arg(loco->getPower(), 0, 'f', 1)
                .arg(loco->getGrossWeight(), 0, 'f', 1)
                .arg(loco->getLength(), 0, 'f', 2);
    }

    details += "</ul><h3>Cars:</h3><ul>";

    // Car details
    for (const auto &car : train->getCars())
    {
        details +=
            QString("<li><b>Type %1:</b> %2 units<ul>"
                    "    <li>Count: %3 tons</li>"
                    "    <li>Tare Weight: %4 tons</li>"
                    "    <li>Length: %5 m</li>"
                    "</ul></li>")
                .arg(car->getCarType())
                .arg(car->getCount())
                .arg(car->getTareWeight(), 0, 'f', 1)
                .arg(car->getTareWeight(), 0, 'f', 1)
                .arg(car->getLength(), 0, 'f', 2);
    }

    details += "</ul>";

    // Train path
    details += "<h3>Train Path:</h3>";
    const auto &pathNodes = train->getTrainPathOnNodeIds();
    if (pathNodes.isEmpty())
    {
        details += "<p>No path assigned</p>";
    }
    else
    {
        details += "<ul>";
        for (const auto &nodeId : pathNodes)
        {
            details +=
                QString("<li>Node %1</li>").arg(nodeId);
        }
        details += "</ul>";
    }

    // Train operational parameters
    details +=
        QString("<h3>Operational Parameters:</h3>"
                "<ul>"
                "    <li><b>Load Time:</b> %1 minutes</li>"
                "</ul>")
            .arg(train->getLoadTime());

    return details;
}

} // namespace GUI
} // namespace CargoNetSim
