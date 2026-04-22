/**
 * @file ShortestPathTable.cpp
 * @brief Implementation of the ShortestPathsTable widget
 * for path visualization and comparison
 * @author Ahmed Aredah
 * @date April 5, 2025
 *
 * This file contains the implementation of the
 * ShortestPathsTable class, which provides a UI component
 * for displaying, comparing, and exporting path data in the
 * CargoNetSim system.
 */

#include "ShortestPathTable.h"
#include "GUI/MainWindow.h"
#include "GUI/Utils/IconCreator.h" // For icon creation utilities
#include "GUI/Utils/PathReportExporter.h"
#include "GUI/Widgets/PathComparisonDialog.h"
#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QVariant>
#include <QtWidgets/qscrollarea.h>
#include <stdexcept>
#include "Backend/Commons/LogCategories.h"

// Register meta-type for storing widgets in QVariant
Q_DECLARE_METATYPE(QWidget *)

namespace CargoNetSim
{
namespace GUI
{

//------------------------------------------------------------------------------
// TerminalPathDelegate Implementation
//------------------------------------------------------------------------------

/**
 * @brief Custom paint implementation for the terminal path
 * delegate
 *
 * Renders the custom widget in the terminal path column, or
 * falls back to standard delegate rendering for other
 * columns.
 */
void TerminalPathDelegate::paint(
    QPainter *painter, const QStyleOptionViewItem &option,
    const QModelIndex &index) const
{
    // Check if we're in the terminals column (column index
    // 2)
    if (index.column() == 2)
    {
        // Try to extract the widget from user data
        QWidget *widget = qvariant_cast<QWidget *>(
            index.data(Qt::UserRole));

        if (widget)
        {
            // Calculate paint rect to avoid scrollbars
            // overlapping other cells
            QRect paintRect = option.rect;

            // Check if the widget is a QScrollArea
            QScrollArea *scrollArea =
                qobject_cast<QScrollArea *>(widget);
            if (scrollArea)
            {
                // Render the scroll area widget
                scrollArea->setFixedHeight(
                    option.rect.height());
                QPixmap pixmap(scrollArea->size());
                pixmap.fill(Qt::transparent);
                scrollArea->render(&pixmap);
                painter->drawPixmap(paintRect.topLeft(),
                                    pixmap);
                return;
            }
            else
            {
                // Render the widget directly to the cell
                QPixmap pixmap(widget->size());
                widget->render(&pixmap);
                painter->drawPixmap(paintRect.topLeft(),
                                    pixmap);
                return;
            }
        }
    }

    // Fall back to default rendering for other columns or
    // if widget extraction failed
    QStyledItemDelegate::paint(painter, option, index);
}

/**
 * @brief Size hint implementation for the terminal path
 * delegate
 *
 * Returns the size of the custom widget for the terminal
 * path column, or falls back to standard delegate size for
 * other columns.
 */
QSize TerminalPathDelegate::sizeHint(
    const QStyleOptionViewItem &option,
    const QModelIndex          &index) const
{
    // Check if we're in the terminals column
    if (index.column() == 2)
    {
        // Try to extract the widget from user data
        QWidget *widget = qvariant_cast<QWidget *>(
            index.data(Qt::UserRole));

        if (widget)
        {
            // For QScrollArea, use the widget's viewport
            // width
            QScrollArea *scrollArea =
                qobject_cast<QScrollArea *>(widget);
            if (scrollArea && scrollArea->widget())
            {
                // Return size that accounts for the scroll
                // area's content
                return QSize(scrollArea->widget()
                                 ->sizeHint()
                                 .width(),
                             option.rect.height());
            }

            // Use the widget's size for the cell
            return widget->size();
        }
    }

    // Fall back to default size hint for other columns
    return QStyledItemDelegate::sizeHint(option, index);
}

//------------------------------------------------------------------------------
// ShortestPathsTable Implementation
//------------------------------------------------------------------------------

/**
 * @brief Constructs a ShortestPathsTable widget
 * @param parent The parent widget
 *
 * Initializes the UI components and sets up the table
 * structure.
 */
ShortestPathsTable::ShortestPathsTable(QWidget *parent)
    : QWidget(parent)
    , m_updatingUI(false) // Initialize to prevent recursive
                          // UI updates during setup
    , m_scrollEventFilter(new PathScrollEventFilter())
{
    // Set up the user interface components
    initUI();

    MainWindow *mainWindow =
        qobject_cast<MainWindow *>(parent);

    if (mainWindow)
    {

        connect(this, &ShortestPathsTable::showPathSignal,
                [this, mainWindow](int pathId) {
                    mainWindow->flashPathLines(pathId);
                });
    }
}

ShortestPathsTable::~ShortestPathsTable()
{
    // Clean up all PathData objects
    for (auto &pathDataPtr : m_pathData)
    {
        delete pathDataPtr;
    }
    m_pathData.clear();

    // Clean up event filter
    if (m_scrollEventFilter)
    {
        delete m_scrollEventFilter;
    }
}

/**
 * @brief Initializes the UI components
 *
 * Creates and lays out all UI elements including the table
 * and buttons.
 */
void ShortestPathsTable::initUI()
{
    // Create main layout with minimal margins
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(
        2); // Compact spacing between elements

    // Create and add the table widget
    createTableWidget();
    layout->addWidget(m_table);

    // Create panel for buttons
    auto buttonPanel = new QHBoxLayout();

    // Initialize button groups
    createPathButtonPanel();
    createExportPanel();
    createSelectionPanel();

    // Add selection buttons to panel with left alignment
    buttonPanel->addWidget(m_selectAllButton);
    buttonPanel->addWidget(m_unselectAllButton);

    // Add buttons to panel with right alignment
    buttonPanel->addStretch(); // Push remaining buttons to
                               // the right
    buttonPanel->addWidget(m_compareButton);
    buttonPanel->addWidget(m_exportButton);

    // Add button panel to main layout
    layout->addLayout(buttonPanel);
}

/**
 * @brief Creates and configures the table widget
 *
 * Sets up columns, headers, and behavior for the path
 * table.
 */
void ShortestPathsTable::createTableWidget()
{
    // Create table widget
    m_table = new QTableWidget(this);

    // Configure table structure.
    //
    // Plan 8.2: 16 columns total — the original 5 plus 6 predicted
    // per-vehicle metrics (cols 5..10) and 5 actual per-vehicle
    // metrics (cols 11..15). Actual lacks a dedicated fuel column
    // because SegmentCostMath does not surface a fuel key during
    // post-simulation extraction; its energy value already reflects
    // the actual fuel burn.
    m_table->setColumnCount(23);
    m_table->setHorizontalHeaderLabels({
        tr("Select"),                tr("Path ID"),
        tr("Terminal Path"),         tr("Predicted Cost"),
        tr("Actual Cost"),
        // Predicted per-vehicle (cols 5-10)
        tr("P. Distance (km)"),      tr("P. Time (h)"),
        tr("P. Fuel/Veh"),           tr("P. Energy/Veh (kWh)"),
        tr("P. CO₂/Veh (t)"), tr("P. Risk/Veh"),
        // Actual per-vehicle (cols 11-15 — no fuel key from
        // SegmentCostMath)
        tr("A. Distance (km)"),      tr("A. Time (h)"),
        tr("A. Energy/Veh (kWh)"),   tr("A. CO₂/Veh (t)"),
        tr("A. Risk/Veh"),
        // Allocator counts and per-container metrics (cols 16-22)
        tr("Containers"),            tr("Vehicles"),
        tr("P. Fuel/Cont"),          tr("P. Energy/Cont (kWh)"),
        tr("P. CO₂/Cont (t)"),
        tr("A. Energy/Cont (kWh)"),  tr("A. CO₂/Cont (t)"),
    });

    // Configure selection behavior
    m_table->setSelectionBehavior(QTableWidget::SelectRows);
    m_table->setSelectionMode(
        QTableWidget::SingleSelection);
    m_table->setEditTriggers(
        QTableWidget::NoEditTriggers); // Read-only table
    m_table->verticalHeader()->setDefaultSectionSize(
        50); // 50 pixels high rows

    // Configure header appearance and behavior
    auto header = m_table->horizontalHeader();

    // Set column sizing policies
    header->setSectionResizeMode(
        0, QHeaderView::Fixed); // Fixed width for checkbox
    m_table->setColumnWidth(
        0, 50); // 50 pixels for checkbox column

    header->setSectionResizeMode(
        1, QHeaderView::ResizeToContents); // Auto-size for
                                           // Path ID
    header->setSectionResizeMode(
        2, QHeaderView::Stretch); // Stretch terminal path
                                  // column
    header->setSectionResizeMode(
        3, QHeaderView::ResizeToContents); // Auto-size for
                                           // costs
    header->setSectionResizeMode(
        4, QHeaderView::ResizeToContents); // Auto-size for
                                           // costs

    // Plan 8.2 + Plan 10: auto-size the metric columns (5..22).
    for (int c = 5; c < 23; ++c)
        header->setSectionResizeMode(
            c, QHeaderView::ResizeToContents);

    // Set custom delegate for terminal path visualization
    m_table->setItemDelegate(
        new TerminalPathDelegate(m_table));

    // Connect selection signal to update UI state when
    // selection changes
    connect(m_table, &QTableWidget::itemSelectionChanged,
            this, &ShortestPathsTable::onSelectionChanged);
}

/**
 * @brief Creates the path manipulation button panel
 *
 * Initializes buttons for operations on paths, such as
 * comparison.
 */
void ShortestPathsTable::createPathButtonPanel()
{
    // Create compare button for path comparison
    m_compareButton =
        new QPushButton(tr("View/Compare Paths"), this);
    m_compareButton->setToolTip(
        tr("View or Compare selected paths"));
    m_compareButton->setEnabled(
        false); // Disabled until paths are selected

    // Connect button click to comparison handler
    connect(m_compareButton, &QPushButton::clicked, this,
            &ShortestPathsTable::onCompareButtonClicked);
}

/**
 * @brief Creates the export button panel
 *
 * Initializes buttons for exporting path data.
 */
void ShortestPathsTable::createExportPanel()
{
    // Create button for exporting paths
    m_exportButton = new QPushButton(tr("Export"), this);
    m_exportButton->setToolTip(
        tr("Export selected paths or all paths if none "
           "selected"));
    m_exportButton->setEnabled(
        false); // Disabled until paths are added

    // Connect button click to export handler
    connect(m_exportButton, &QPushButton::clicked, this,
            &ShortestPathsTable::onExportButtonClicked);
}

/**
 * @brief Creates the selection button panel
 *
 * Initializes buttons for selecting and unselecting all
 * paths.
 */
void ShortestPathsTable::createSelectionPanel()
{
    // Create select all button
    m_selectAllButton =
        new QPushButton(tr("Select All"), this);
    m_selectAllButton->setToolTip(tr("Select all paths"));
    m_selectAllButton->setEnabled(
        false); // Disabled until paths are added

    // Connect button click to select all handler
    connect(m_selectAllButton, &QPushButton::clicked, this,
            &ShortestPathsTable::onSelectAllButtonClicked);

    // Create unselect all button
    m_unselectAllButton =
        new QPushButton(tr("Unselect All"), this);
    m_unselectAllButton->setToolTip(
        tr("Unselect all paths"));
    m_unselectAllButton->setEnabled(
        false); // Disabled until paths are checked

    // Connect button click to unselect all handler
    connect(
        m_unselectAllButton, &QPushButton::clicked, this,
        &ShortestPathsTable::onUnselectAllButtonClicked);
}

/**
 * @brief Adds multiple paths to the table
 * @param paths List of Path pointers to add
 *
 * Processes each Path in the list, creating a PathData
 * object and adding it to the internal data structure.
 */
void ShortestPathsTable::addPaths(
    const QList<Backend::Path *>                     &paths,
    const QHash<int, Backend::Scenario::PathMetrics> &predicted,
    const QHash<int, Backend::Scenario::PathMetrics> &actual)
{
    qCDebug(lcGuiPathTable) << "ShortestPathsTable::addPaths:"
                            << "pathCount=" << paths.size();
    // Plan 8.2: merge any supplied per-path metrics. Existing
    // callers that pass only the path list leave these maps empty,
    // so legacy behavior is preserved byte-for-byte.
    for (auto it = predicted.constBegin();
         it != predicted.constEnd(); ++it)
        m_predicted.insert(it.key(), it.value());
    for (auto it = actual.constBegin();
         it != actual.constEnd(); ++it)
        m_actual.insert(it.key(), it.value());

    // Sort the paths by total path cost
    QVector<Backend::Path *> tempPaths(paths.begin(),
                                       paths.end());
    std::sort(tempPaths.begin(), tempPaths.end(),
              [](Backend::Path *a, Backend::Path *b) {
                  return a->getTotalPathCost()
                         < b->getTotalPathCost();
              });

    // Process each path from the list
    for (Backend::Path *path : tempPaths)
    {
        if (!path)
        {
            // Skip null paths
            qCWarning(lcGuiPathTable) << "Skipping null path in addPaths";
            continue;
        }

        try
        {
            // Create a new PathData pointer
            PathData *pathDataPtr = new PathData();
            pathDataPtr->path =
                path; // PathData now owns the Path
            pathDataPtr->m_totalSimulationPathCost  = -1.0;
            pathDataPtr->m_totalSimulationEdgeCosts = -1.0;
            pathDataPtr->m_totalSimulationTerminalCosts =
                -1.0;
            pathDataPtr->isVisible = true;

            // Store the pointer in the map
            m_pathData.insert(path->getPathId(),
                              pathDataPtr);
        }
        catch (const std::exception &ex)
        {
            // Handle any exceptions during path processing
            qCWarning(lcGuiPathTable) << "Error adding path:" << ex.what();
            delete path; // Clean up on error
        }
    }

    // Refresh the table with the new paths
    refreshTable();

    // Enable export all button if we have data
    m_exportButton->setEnabled(!m_pathData.isEmpty());
}

int ShortestPathsTable::pathsSize() const
{
    return m_pathData.size();
}

/**
 * @brief Updates the prediction costs for an existing path
 * @param pathId The ID of the path to update
 * @param totalCost New predicted total cost
 * @param edgeCost New predicted edge cost
 * @param terminalCost New predicted terminal cost
 *
 * Updates the Path object's cost fields if the path exists
 * and refreshes the table display.
 */
void ShortestPathsTable::updatePredictionCosts(
    int pathId, double totalCost, double edgeCost,
    double terminalCost)
{
    qCDebug(lcGuiPathTable) << "ShortestPathTable: updating cost field:" << "pathId=" << pathId;
    // Check if the path exists in our data
    if (!m_pathData.contains(pathId))
    {
        qCWarning(lcGuiPathTable)
            << "Path ID" << pathId
            << "not found for prediction cost update";
        return;
    }

    // Get reference to the path data
    PathData *pathData = m_pathData[pathId];

    // Validate path pointer
    if (!pathData->path)
    {
        qCWarning(lcGuiPathTable) << "Path object is null for path ID"
                                  << pathId;
        return;
    }

    // Update costs if values are valid (>= 0)
    try
    {
        // Use the new setter methods to update the Path
        // object
        if (totalCost >= 0)
        {
            pathData->path->setTotalPathCost(totalCost);
        }

        if (edgeCost >= 0)
        {
            pathData->path->setTotalEdgeCosts(edgeCost);
        }

        if (terminalCost >= 0)
        {
            pathData->path->setTotalTerminalCosts(
                terminalCost);
        }

        // Update the table display
        for (int row = 0; row < m_table->rowCount(); ++row)
        {
            // Get the path ID for this row
            auto idItem = m_table->item(row, 1);
            if (idItem && idItem->text().toInt() == pathId)
            {
                // Update the predicted cost cell if total
                // cost was updated
                if (totalCost >= 0)
                {
                    m_table->item(row, 3)->setText(
                        QString::number(totalCost, 'f', 2));
                }
                break;
            }
        }
    }
    catch (const std::exception &ex)
    {
        qCWarning(lcGuiPathTable)
            << "Error updating prediction costs for path"
            << pathId << ":" << ex.what();
    }
}

/**
 * @brief Updates the simulation costs for an existing path
 * @param pathId The ID of the path to update
 * @param simulationTotalCost New simulation total cost
 * @param simulationEdgeCost New simulation edge cost
 * @param simulationTerminalCost New simulation terminal
 * cost
 *
 * Updates the PathData's simulation cost fields if the path
 * exists and refreshes the table display.
 */
void ShortestPathsTable::updateSimulationCosts(
    int pathId, double simulationTotalCost,
    double simulationEdgeCost,
    double simulationTerminalCost)
{
    // Check if the path exists in our data
    if (!m_pathData.contains(pathId))
    {
        qCWarning(lcGuiPathTable)
            << "Path ID" << pathId
            << "not found for simulation cost update";
        return;
    }

    // Get reference to the path data
    PathData *pathData = m_pathData[pathId];

    // Update simulation costs if values are valid (>= 0)
    if (simulationTotalCost >= 0)
    {
        pathData->m_totalSimulationPathCost =
            simulationTotalCost;
    }

    if (simulationEdgeCost >= 0)
    {
        pathData->m_totalSimulationEdgeCosts =
            simulationEdgeCost;
    }

    if (simulationTerminalCost >= 0)
    {
        pathData->m_totalSimulationTerminalCosts =
            simulationTerminalCost;
    }

    // Update the table display for this path
    for (int row = 0; row < m_table->rowCount(); ++row)
    {
        // Get the path ID for this row
        auto idItem = m_table->item(row, 1);
        if (idItem && idItem->text().toInt() == pathId)
        {
            // Update the actual cost cell if simulation
            // total cost was updated
            if (simulationTotalCost >= 0)
            {
                m_table->item(row, 4)->setText(
                    QString::number(simulationTotalCost,
                                    'f', 2));
            }
            break;
        }
    }
}

/**
 * @brief Creates a visualization widget for a path
 * @param pathId ID of the path to visualize
 * @param pathData PathData object containing path details
 * @return Widget containing visual representation of the
 * path
 *
 * Generates a custom widget showing terminals and
 * transportation modes for a path, including a button to
 * show it on the map.
 */
QWidget *
ShortestPathsTable::createPathRow(int             pathId,
                                  const PathData *pathData)
{
    // Check if path pointer is valid
    if (!pathData->path)
    {
        qCWarning(lcGuiPathTable)
            << "Cannot create path row: path is null for ID"
            << pathId;
        return new QWidget(); // Return empty widget on
                              // error
    }

    // Create container widget for the path visualization
    auto contentWidget = new QWidget();
    auto contentLayout = new QHBoxLayout(contentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(
        4); // Compact spacing between elements

    // Create show button to visualize the path on the map
    auto showButton = new QPushButton();
    showButton->setIcon(IconFactory::createShowEyeIcon());
    showButton->setFixedSize(24, 24);
    showButton->setToolTip(tr("Show this path on the map"));

    // Connect button to show path signal
    connect(
        showButton, &QPushButton::clicked, this,
        [this, pathId]() { emit showPathSignal(pathId); });

    contentLayout->addWidget(showButton);

    // Get terminals and segments from the path
    const QList<Backend::PathTerminal> &terminals =
        pathData->path->getTerminalsInPath();
    const QList<Backend::PathSegment *> segments =
        pathData->path->getSegments();

    // Validate terminal and segment data
    if (terminals.isEmpty())
    {
        qCWarning(lcGuiPathTable) << "No terminals for path ID" << pathId;
        contentLayout->addWidget(
            new QLabel(tr("No terminal data")));
        contentLayout->addStretch();

        // Create and configure scroll area
        auto scrollArea = new QScrollArea();
        scrollArea->setWidgetResizable(true);
        scrollArea->setHorizontalScrollBarPolicy(
            Qt::ScrollBarAsNeeded);
        scrollArea->setVerticalScrollBarPolicy(
            Qt::ScrollBarAlwaysOff);
        scrollArea->setFrameShape(
            QFrame::NoFrame); // Hide the frame
        scrollArea->setWidget(contentWidget);
        scrollArea->viewport()->installEventFilter(
            m_scrollEventFilter);

        return scrollArea;
    }

    // Add terminal names and transportation mode indicators
    for (int i = 0; i < terminals.size(); ++i)
    {
        // Extract terminal name from the snapshot
        QString terminalName = terminals[i].displayName;
        if (terminalName.isEmpty())
        {
            terminalName = tr("Terminal %1")
                               .arg(i + 1); // Fallback name
        }

        // Add terminal name label
        auto nameLabel = new QLabel(terminalName);
        nameLabel->setAlignment(Qt::AlignCenter);
        contentLayout->addWidget(nameLabel);

        // Add transportation mode arrow for all but the
        // last terminal
        if (i < terminals.size() - 1 && i < segments.size())
        {
            auto modeLabel = new QLabel();
            modeLabel->setAlignment(Qt::AlignCenter);

            // Get transportation mode from the segment
            Backend::TransportationTypes::TransportationMode
                    mode = segments[i]->getMode();
            QString modeText =
                Backend::TransportationTypes::toString(
                    mode);

            // Create transportation mode pixmap
            QPixmap modePixmap =
                IconFactory::createTransportationModePixmap(
                    modeText);

            modeLabel->setPixmap(modePixmap);
            modeLabel->setToolTip(
                modeText); // Show mode name on hover
            contentLayout->addWidget(modeLabel);
        }
    }

    // Add stretch to ensure left alignment of path
    // visualization
    contentLayout->addStretch();

    // Create and configure scroll area
    auto scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(
        Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(
        Qt::ScrollBarAlwaysOff);
    scrollArea->setFrameShape(
        QFrame::NoFrame); // Hide the frame
    scrollArea->setWidget(contentWidget);

    return scrollArea;
}

/**
 * @brief Creates a transportation mode arrow pixmap
 * @param mode Transportation mode text
 * @return Pixmap containing the arrow and mode
 * visualization
 *
 * Generates a visual representation of a transportation
 * mode with appropriate coloring and labeling.
 */
QPixmap ShortestPathsTable::createArrowPixmap(
    const QString &mode) const
{
    // Create a pixmap for the arrow with the mode text
    QPixmap pixmap(64, 40); // Fixed size for consistency
    pixmap.fill(Qt::transparent); // Start with transparent
                                  // background

    QPainter painter(&pixmap);
    painter.setRenderHint(
        QPainter::Antialiasing); // Smooth rendering

    // Set color based on transportation mode
    QColor arrowColor = Qt::black; // Default color

    // Choose color based on transportation mode
    if (mode.contains("Truck", Qt::CaseInsensitive))
    {
        arrowColor =
            QColor(Qt::magenta); // Magenta for truck
    }
    else if (mode.contains("Rail", Qt::CaseInsensitive)
             || mode.contains("Train", Qt::CaseInsensitive))
    {
        arrowColor =
            QColor(Qt::darkGray); // Dark gray for rail
    }
    else if (mode.contains("Ship", Qt::CaseInsensitive)
             || mode.contains("Water", Qt::CaseInsensitive))
    {
        arrowColor = QColor(Qt::blue); // Blue for ship
    }

    // Draw the mode text
    painter.setPen(arrowColor);
    QFont font = painter.font();
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(QRect(0, 0, pixmap.width(), 15),
                     Qt::AlignCenter, mode);

    // Draw the arrow
    QPen pen(arrowColor, 2); // 2 pixel width
    painter.setPen(pen);

    // Draw arrow shaft
    painter.drawLine(10, 25, 54, 25);

    // Draw arrow head
    QPolygon arrowHead;
    arrowHead << QPoint(48, 20) << QPoint(54, 25)
              << QPoint(48, 30);
    painter.setBrush(arrowColor);
    painter.drawPolygon(arrowHead);

    return pixmap;
}

/**
 * @brief Refreshes the table display with current path data
 *
 * Rebuilds the table contents based on the current state of
 * the PathData objects.
 */
void ShortestPathsTable::refreshTable()
{
    qCDebug(lcGuiPathTable) << "ShortestPathsTable::refreshTable:"
                            << "pathCount=" << m_pathData.size();
    // Set flag to prevent recursive UI updates during
    // refresh
    m_updatingUI = true;

    // Clear the table while preserving header
    m_table->setRowCount(0);
    // Plan 8.2: row indices are rebuilt below; reset the map.
    m_rowByPathId.clear();

    // Add rows for each visible path
    for (auto it = m_pathData.begin();
         it != m_pathData.end(); ++it)
    {
        int             pathId   = it.key();
        const PathData *pathData = it.value();

        // Skip paths marked as not visible
        if (!pathData->isVisible || !pathData->path)
        {
            continue;
        }

        // Add a new row at the end of the table
        int row = m_table->rowCount();
        m_table->insertRow(row);
        // Plan 8.2: remember row index for refreshRow updates.
        m_rowByPathId.insert(pathId, row);

        // Create checkbox widget for the select column
        auto checkboxWidget = new QWidget();
        auto checkboxLayout =
            new QHBoxLayout(checkboxWidget);
        checkboxLayout->setAlignment(Qt::AlignCenter);
        checkboxLayout->setContentsMargins(0, 0, 0, 0);

        auto checkbox = new QCheckBox();
        checkboxLayout->addWidget(checkbox);
        m_table->setCellWidget(row, 0, checkboxWidget);

        // Connect checkbox state change to update
        // compare button state
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        // Qt 6.7+ version with checkStateChanged signal
        connect(checkbox, &QCheckBox::checkStateChanged,
                this, [this, pathId](Qt::CheckState state) {
                    // Emit signal that checkbox state changed
                    emit checkboxChanged(pathId, state == Qt::Checked);

                           // Get all checked paths
                    QVector<int> checkedPaths = getCheckedPathIds();
                    bool hasCheckedPaths = !checkedPaths.isEmpty();

                           // Enable compare button if at least 1 path is checked
                    m_compareButton->setEnabled(hasCheckedPaths);

                           // Enable export button if there are any paths in the table (checked or not)
                    m_exportButton->setEnabled(!m_pathData.isEmpty());

                           // Update select/unselect buttons state
                    m_unselectAllButton->setEnabled(hasCheckedPaths);

                           // Enable select all button only if not all paths are already selected
                    m_selectAllButton->setEnabled(m_pathData.size() > checkedPaths.size());
                });
#else
      // Qt 6.4-6.6 version with stateChanged signal
        connect(checkbox, &QCheckBox::stateChanged,
                this, [this, pathId](int state) {
                    // Convert int to Qt::CheckState for consistency
                    Qt::CheckState checkState = static_cast<Qt::CheckState>(state);

                           // Emit signal that checkbox state changed
                    emit checkboxChanged(pathId, checkState == Qt::Checked);

                           // Get all checked paths
                    QVector<int> checkedPaths = getCheckedPathIds();
                    bool hasCheckedPaths = !checkedPaths.isEmpty();

                           // Enable compare button if at least 1 path is checked
                    m_compareButton->setEnabled(hasCheckedPaths);

                           // Enable export button if there are any paths in the table (checked or not)
                    m_exportButton->setEnabled(!m_pathData.isEmpty());

                           // Update select/unselect buttons state
                    m_unselectAllButton->setEnabled(hasCheckedPaths);

                           // Enable select all button only if not all paths are already selected
                    m_selectAllButton->setEnabled(m_pathData.size() > checkedPaths.size());
                });
#endif

        // Add Path ID cell
        auto pathItem =
            new QTableWidgetItem(QString::number(pathId));
        m_table->setItem(row, 1, pathItem);

        // Create and add terminal path visualization
        auto pathWidget = createPathRow(pathId, pathData);
        m_table->setCellWidget(row, 2, pathWidget);

        // Store widget pointer in user role for custom
        // delegate
        QVariant widgetPtr;
        widgetPtr.setValue(pathWidget);
        m_table->model()->setData(
            m_table->model()->index(row, 2), widgetPtr,
            Qt::UserRole);

        // Add Predicted Cost cell
        QString predictedCostText;
        if (pathData->path->getTotalPathCost() >= 0)
        {
            predictedCostText = QString::number(
                pathData->path->getTotalPathCost(), 'f', 2);
        }
        else
        {
            predictedCostText = tr("Waiting analysis");
        }
        auto predictedItem =
            new QTableWidgetItem(predictedCostText);
        m_table->setItem(row, 3, predictedItem);

        // Add Actual Cost cell
        QString actualCostText;
        if (pathData->m_totalSimulationPathCost >= 0)
        {
            actualCostText = QString::number(
                pathData->m_totalSimulationPathCost, 'f',
                2);
        }
        else
        {
            actualCostText = tr("Waiting simulation");
        }
        auto actualItem =
            new QTableWidgetItem(actualCostText);
        m_table->setItem(row, 4, actualItem);

        // Plan 8.2: populate per-vehicle metric columns.
        refreshRow(pathId);
    }

    // Clear UI update flag
    m_updatingUI = false;

    // Update button states
    m_exportButton->setEnabled(!m_pathData.isEmpty());

    // Update selection button states
    m_selectAllButton->setEnabled(!m_pathData.isEmpty());
    m_unselectAllButton->setEnabled(
        false); // Initially disable until paths are checked
}

/**
 * @brief Retrieves path data for a specific path ID
 * @param pathId The ID of the path to retrieve
 * @return Pointer to the path data or nullptr if not found
 */
const ShortestPathsTable::PathData *
ShortestPathsTable::getDataByPathId(int pathId) const
{
    // Look up the path in the map
    auto it = m_pathData.find(pathId);
    if (it == m_pathData.end())
    {
        return nullptr; // Path not found
    }

    // Return pointer to the found path data
    return it.value();
}

const QList<const ShortestPathsTable::PathData *>
ShortestPathsTable::getCheckedPathData() const
{
    QList<const ShortestPathsTable::PathData *> result;
    auto checkedpaths = getCheckedPathIds();
    if (checkedpaths.isEmpty())
    {
        return QList<const PathData *>();
    }

    for (auto pathId : std::as_const(checkedpaths))
    {
        auto pathData = getDataByPathId(pathId);
        if (pathData)
        {
            result.append(pathData);
        }
    }
    result.removeAll(nullptr);
    return result;
}

/**
 * @brief Gets the currently selected path ID
 * @return The selected path ID or -1 if none selected
 */
int ShortestPathsTable::getSelectedPathId() const
{
    // Get all selected items
    QList<QTableWidgetItem *> selectedItems =
        m_table->selectedItems();
    if (selectedItems.isEmpty())
    {
        return -1; // No selection
    }

    // Get the row of the first selected item
    int row = selectedItems.first()->row();

    // Get the Path ID from column 1
    auto idItem = m_table->item(row, 1);
    return idItem ? idItem->text().toInt() : -1;
}

/**
 * @brief Gets all path IDs that are currently checked
 * @return Vector of checked path IDs
 */
QVector<int> ShortestPathsTable::getCheckedPathIds() const
{
    QVector<int> checkedPaths;

    // Iterate through all rows in the table
    for (int row = 0; row < m_table->rowCount(); ++row)
    {
        // Get the checkbox widget in the first column
        auto checkboxWidget = m_table->cellWidget(row, 0);
        if (!checkboxWidget)
        {
            continue; // Skip rows without checkbox widget
        }

        // Get the layout from the widget
        auto layout = checkboxWidget->layout();
        if (!layout)
        {
            continue; // Skip if no layout
        }

        // Get the checkbox from the layout
        auto checkBox = qobject_cast<QCheckBox *>(
            layout->itemAt(0)->widget());
        if (checkBox && checkBox->isChecked())
        {
            // If checkbox exists and is checked, add the
            // path ID
            auto idItem = m_table->item(row, 1);
            if (idItem)
            {
                checkedPaths.append(idItem->text().toInt());
            }
        }
    }

    return checkedPaths;
}

/**
 * @brief Clears all data from the table
 *
 * Removes all paths and resets the UI state.
 */
void ShortestPathsTable::clear()
{
    // Clear all path data, which will delete the owned Path
    // pointers
    m_pathData.clear();

    // Clear the table
    m_table->setRowCount(0);

    // Disable buttons that require paths
    m_compareButton->setEnabled(false);
    m_exportButton->setEnabled(false);
}

/**
 * @brief Slot called when selection in the table changes
 *
 * Updates the UI state based on the new selection and
 * emits signals to notify listeners.
 */
void ShortestPathsTable::onSelectionChanged()
{
    // Ignore selection changes during UI updates
    if (m_updatingUI)
    {
        return;
    }

    // Get the currently selected path ID
    int pathId = getSelectedPathId();
    qCDebug(lcGuiPathTable) << "ShortestPathsTable::onSelectionChanged:"
                            << "pathId=" << pathId;

    // Enable export button if either a path is selected or
    // any path is checked
    bool hasCheckedPaths = !getCheckedPathIds().isEmpty();
    m_exportButton->setEnabled(pathId >= 0
                               || hasCheckedPaths);

    // Emit selection signal if a path is selected
    if (pathId >= 0)
    {
        emit pathSelected(pathId);
    }
}

/**
 * @brief Slot called when an item's checked state changes
 * @param row The row of the changed item
 * @param column The column of the changed item
 *
 * Note: This slot is not directly connected but is provided
 * for completeness and potential future use.
 */
void ShortestPathsTable::onItemCheckedChanged(int row,
                                              int column)
{
    // This functionality is handled by the checkbox state
    // change connections established during table refresh
}

/**
 * @brief Slot called when the compare button is clicked
 *
 * Gathers checked paths and emits comparison signal if at
 * least two paths are selected.
 */
void ShortestPathsTable::onCompareButtonClicked()
{
    qCDebug(lcGuiPathTable) << "ShortestPathsTable::onCompareButtonClicked: begin";
    // Get IDs of all checked paths
    QVector<int> checkedPaths = getCheckedPathIds();

    // Get the path data for all checked paths
    QList<const PathData *> pathDataToCompare =
        getCheckedPathData();

    // Create and show the comparison dialog
    PathComparisonDialog *dialog =
        new PathComparisonDialog(pathDataToCompare, this);
    dialog->setAttribute(
        Qt::WA_DeleteOnClose); // Auto-delete when
                               // closed
    dialog->exec();

    // Emit signal to request path comparison (for any
    // other listeners)
    emit pathComparisonRequested(checkedPaths);
}

/**
 * @brief Slot called when the export button is clicked
 *
 * Emits signal to export the currently selected path.
 */
void ShortestPathsTable::onExportButtonClicked()
{
    qCInfo(lcGuiPathTable) << "ShortestPathsTable::onExportButtonClicked: begin";
    // Get IDs of all checked paths
    QVector<int> checkedPaths = getCheckedPathIds();

    if (checkedPaths.isEmpty())
    {
        // No paths checked, export all visible paths
        exportPathsToPdf(QVector<int>());
        emit allPathsExportRequested();
    }
    else
    {
        // Export only checked paths
        exportPathsToPdf(checkedPaths);

        // If only one path is checked, emit the single path
        // export signal
        if (checkedPaths.size() == 1)
        {
            emit pathExportRequested(checkedPaths.first());
        }
        else
        {
            emit allPathsExportRequested();
        }
    }
}

/**
 * @brief Slot called when the export all button is clicked
 *
 * Emits signal to export all paths currently in the table.
 */
void ShortestPathsTable::onExportAllButtonClicked()
{
    qCInfo(lcGuiPathTable) << "ShortestPathsTable::onExportAllButtonClicked: begin";
    // Export all paths to PDF (empty vector means all
    // paths)
    exportPathsToPdf(QVector<int>());

    // Emit signal for other listeners
    emit allPathsExportRequested();
}

/**
 * @brief Slot called when the select all button is clicked
 *
 * Checks all checkboxes in the table.
 */
void ShortestPathsTable::onSelectAllButtonClicked()
{
    // Set flag to prevent recursive UI updates
    m_updatingUI = true;

    // Iterate through all rows in the table
    for (int row = 0; row < m_table->rowCount(); ++row)
    {
        // Get the checkbox widget in the first column
        auto checkboxWidget = m_table->cellWidget(row, 0);
        if (!checkboxWidget)
        {
            continue; // Skip rows without checkbox widget
        }

        // Get the layout from the widget
        auto layout = checkboxWidget->layout();
        if (!layout)
        {
            continue; // Skip if no layout
        }

        // Get the checkbox from the layout
        auto checkBox = qobject_cast<QCheckBox *>(
            layout->itemAt(0)->widget());
        if (checkBox)
        {
            // Check the checkbox
            checkBox->setChecked(true);
        }
    }

    // Clear UI update flag
    m_updatingUI = false;

    // Enable compare button if paths are checked
    m_compareButton->setEnabled(
        !getCheckedPathIds().isEmpty());

    // Enable/disable selection buttons based on state
    m_selectAllButton->setEnabled(false);
    m_unselectAllButton->setEnabled(
        !getCheckedPathIds().isEmpty());
}

/**
 * @brief Slot called when the unselect all button is
 * clicked
 *
 * Unchecks all checkboxes in the table.
 */
void ShortestPathsTable::onUnselectAllButtonClicked()
{
    // Set flag to prevent recursive UI updates
    m_updatingUI = true;

    // Iterate through all rows in the table
    for (int row = 0; row < m_table->rowCount(); ++row)
    {
        // Get the checkbox widget in the first column
        auto checkboxWidget = m_table->cellWidget(row, 0);
        if (!checkboxWidget)
        {
            continue; // Skip rows without checkbox widget
        }

        // Get the layout from the widget
        auto layout = checkboxWidget->layout();
        if (!layout)
        {
            continue; // Skip if no layout
        }

        // Get the checkbox from the layout
        auto checkBox = qobject_cast<QCheckBox *>(
            layout->itemAt(0)->widget());
        if (checkBox)
        {
            // Uncheck the checkbox
            checkBox->setChecked(false);
        }
    }

    // Clear UI update flag
    m_updatingUI = false;

    // Disable compare button as no paths are checked
    m_compareButton->setEnabled(false);

    // Enable/disable selection buttons based on state
    m_selectAllButton->setEnabled(!m_pathData.isEmpty());
    m_unselectAllButton->setEnabled(false);
}

void ShortestPathsTable::exportPathsToPdf(
    const QVector<int> &pathIds)
{
    qCInfo(lcGuiPathTable) << "ShortestPathsTable::exportPathsToPdf:"
                           << "pathIds=" << pathIds.size();
    // Get path data for the specified IDs
    QList<const PathData *> pathsToExport;

    if (pathIds.isEmpty())
    {
        // If no IDs specified, export all visible paths
        for (auto it = m_pathData.begin();
             it != m_pathData.end(); ++it)
        {
            if (it.value()->isVisible)
            {
                pathsToExport.append(it.value());
            }
        }
    }
    else
    {
        // Export only the specified paths
        for (int pathId : pathIds)
        {
            auto it = m_pathData.find(pathId);
            if (it != m_pathData.end()
                && it.value()->isVisible)
            {
                pathsToExport.append(it.value());
            }
        }
    }

    if (pathsToExport.isEmpty())
    {
        QMessageBox::warning(
            this, tr("Export Error"),
            tr("No paths selected for export."));
        return;
    }

    // Create default filename based on path IDs
    QString defaultFilename;
    if (pathsToExport.size() == 1)
    {
        defaultFilename = tr("path_%1_report.pdf")
                              .arg(pathsToExport.first()
                                       ->path->getPathId());
    }
    else
    {
        defaultFilename = tr("paths_report.pdf");
    }

    // Create exporter
    PathReportExporter exporter;

    // Ask user if they want to preview or directly export
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Export PDF Report"));
    msgBox.setText(tr("How would you like to proceed with "
                      "the PDF report?"));
    QPushButton *previewButton = msgBox.addButton(
        tr("Preview Report"), QMessageBox::ActionRole);
    QPushButton *exportButton = msgBox.addButton(
        tr("Save PDF"), QMessageBox::ActionRole);
    msgBox.addButton(QMessageBox::Cancel);
    msgBox.setDefaultButton(previewButton);

    msgBox.exec();

    if (msgBox.clickedButton() == previewButton)
    {
        // Show preview dialog
        exporter.previewReport(pathsToExport, this);
    }
    else if (msgBox.clickedButton() == exportButton)
    {
        // Show file dialog and export
        exporter.exportPathsWithDialog(pathsToExport, this,
                                       defaultFilename);
    }
}

// ---------------------------------------------------------------
// Plan 8.2: per-path predicted/actual metric column support.
// ---------------------------------------------------------------

void ShortestPathsTable::setActualMetrics(
    const QHash<int, Backend::Scenario::PathMetrics> &actual)
{
    for (auto it = actual.constBegin();
         it != actual.constEnd(); ++it)
    {
        m_actual.insert(it.key(), it.value());
        refreshRow(it.key());
    }
}

void ShortestPathsTable::refreshRow(int pathId)
{
    const int row = m_rowByPathId.value(pathId, -1);
    if (row < 0)
        return;

    auto          fmt  = [](double v) {
        return QString::number(v, 'f', 2);
    };
    const QString dash = QStringLiteral("—");
    auto          cell = [&](bool valid, double v) {
        return new QTableWidgetItem(valid ? fmt(v) : dash);
    };

    const auto &p = m_predicted.value(pathId);
    const auto &a = m_actual.value(pathId);

    // Predicted per-vehicle (cols 5..10)
    m_table->setItem(row, 5, cell(p.valid, p.distanceKm));
    m_table->setItem(row, 6, cell(p.valid, p.travelTimeHours));
    m_table->setItem(row, 7, cell(p.valid, p.fuelPerVehicle));
    m_table->setItem(row, 8, cell(p.valid, p.energyPerVehicle));
    m_table->setItem(row, 9, cell(p.valid, p.carbonPerVehicle));
    m_table->setItem(row, 10, cell(p.valid, p.riskPerVehicle));

    // Actual per-vehicle (cols 11..15; no fuel column — see
    // createTableWidget note).
    m_table->setItem(row, 11, cell(a.valid, a.distanceKm));
    m_table->setItem(row, 12, cell(a.valid, a.travelTimeHours));
    m_table->setItem(row, 13, cell(a.valid, a.energyPerVehicle));
    m_table->setItem(row, 14, cell(a.valid, a.carbonPerVehicle));
    m_table->setItem(row, 15, cell(a.valid, a.riskPerVehicle));

    // Allocator counts and per-container metrics (cols 16..22)
    m_table->setItem(row, 16, new QTableWidgetItem(
        p.valid ? QString::number(p.containerCount) : dash));
    m_table->setItem(row, 17, new QTableWidgetItem(
        p.valid ? QString::number(p.vehiclesNeeded) : dash));

    m_table->setItem(row, 18, cell(p.valid, p.fuelPerContainer));
    m_table->setItem(row, 19, cell(p.valid, p.energyPerContainer));
    m_table->setItem(row, 20, cell(p.valid, p.carbonPerContainer));

    m_table->setItem(row, 21, cell(a.valid, a.energyPerContainer));
    m_table->setItem(row, 22, cell(a.valid, a.carbonPerContainer));
}

} // namespace GUI
} // namespace CargoNetSim
