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
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Controllers/ConfigController.h"
#include "Backend/Scenario/ScenarioSerializer.h"
#include "GUI/MainWindow.h"
#include "GUI/Utils/IconCreator.h" // For icon creation utilities
#include "GUI/Utils/PathReportExporter.h"
#include "GUI/Widgets/PathComparisonDialog.h"
#include <QApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QSet>
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

namespace
{

using PathIdentity = ShortestPathsTable::PathIdentity;

PathIdentity basePathKey(const Backend::Path *path)
{
    if (!path)
        return QString();

    const QString canonicalKey = path->canonicalPathKey();
    if (!canonicalKey.isEmpty())
        return canonicalKey;

    return QStringLiteral("path_id|%1")
        .arg(path->getPathId());
}

PathIdentity makeUniquePathKey(
    const PathIdentity &baseKey,
    const QMap<PathIdentity, ShortestPathsTable::PathData *> &existing)
{
    const QString normalizedBase =
        baseKey.isEmpty() ? QStringLiteral("path") : baseKey;
    QString candidate = normalizedBase;
    int     suffix    = 2;
    while (existing.contains(candidate))
    {
        candidate = QStringLiteral("%1#%2")
                        .arg(normalizedBase)
                        .arg(suffix++);
    }
    return candidate;
}

PathIdentity snapshotPathKey(const QJsonObject &snapshot,
                             const Backend::Path *path)
{
    const QString pathIdentity =
        snapshot.value(QStringLiteral("path_identity"))
            .toString();
    if (!pathIdentity.isEmpty())
        return pathIdentity;

    const QString topLevelKey =
        snapshot.value(QStringLiteral("canonical_path_key"))
            .toString();
    if (!topLevelKey.isEmpty())
        return topLevelKey;

    const QString pathUid =
        snapshot.value(QStringLiteral("path_uid")).toString();
    if (!pathUid.isEmpty())
        return pathUid;

    return basePathKey(path);
}

QJsonObject metricsToJson(
    const Backend::Scenario::PathMetrics &metrics)
{
    QJsonObject o;
    o[QStringLiteral("valid")] = metrics.valid;
    o[QStringLiteral("distance_km")] = metrics.distanceKm;
    o[QStringLiteral("travel_time_hours")] = metrics.travelTimeHours;
    o[QStringLiteral("risk_per_vehicle")] = metrics.riskPerVehicle;
    o[QStringLiteral("fuel_per_vehicle")] = metrics.fuelPerVehicle;
    o[QStringLiteral("energy_per_vehicle")] = metrics.energyPerVehicle;
    o[QStringLiteral("carbon_per_vehicle")] = metrics.carbonPerVehicle;
    o[QStringLiteral("fuel_type")] = metrics.fuelType;
    o[QStringLiteral("container_count")] = metrics.containerCount;
    o[QStringLiteral("fuel_per_container")] = metrics.fuelPerContainer;
    o[QStringLiteral("energy_per_container")] = metrics.energyPerContainer;
    o[QStringLiteral("carbon_per_container")] = metrics.carbonPerContainer;
    o[QStringLiteral("risk_per_container")] = metrics.riskPerContainer;
    o[QStringLiteral("vehicles_needed")] = metrics.vehiclesNeeded;
    return o;
}

Backend::Scenario::PathMetrics metricsFromJson(
    const QJsonObject &o)
{
    Backend::Scenario::PathMetrics metrics;
    metrics.valid = o.value(QStringLiteral("valid")).toBool(false);
    metrics.distanceKm =
        o.value(QStringLiteral("distance_km")).toDouble(0.0);
    metrics.travelTimeHours =
        o.value(QStringLiteral("travel_time_hours")).toDouble(0.0);
    metrics.riskPerVehicle =
        o.value(QStringLiteral("risk_per_vehicle")).toDouble(0.0);
    metrics.fuelPerVehicle =
        o.value(QStringLiteral("fuel_per_vehicle")).toDouble(0.0);
    metrics.energyPerVehicle =
        o.value(QStringLiteral("energy_per_vehicle")).toDouble(0.0);
    metrics.carbonPerVehicle =
        o.value(QStringLiteral("carbon_per_vehicle")).toDouble(0.0);
    metrics.fuelType =
        o.value(QStringLiteral("fuel_type")).toString();
    metrics.containerCount =
        o.value(QStringLiteral("container_count")).toInt(0);
    metrics.fuelPerContainer =
        o.value(QStringLiteral("fuel_per_container")).toDouble(0.0);
    metrics.energyPerContainer =
        o.value(QStringLiteral("energy_per_container")).toDouble(0.0);
    metrics.carbonPerContainer =
        o.value(QStringLiteral("carbon_per_container")).toDouble(0.0);
    metrics.riskPerContainer =
        o.value(QStringLiteral("risk_per_container")).toDouble(0.0);
    metrics.vehiclesNeeded =
        o.value(QStringLiteral("vehicles_needed")).toInt(0);
    return metrics;
}

QString sha256Hex(const QJsonObject &o)
{
    const QByteArray payload =
        QJsonDocument(o).toJson(QJsonDocument::Compact);
    return QString::fromLatin1(
        QCryptographicHash::hash(
            payload, QCryptographicHash::Sha256)
            .toHex());
}

QJsonObject buildPathSnapshotJson(const ShortestPathsTable::PathData &pathData)
{
    const auto *path = pathData.path.get();
    QJsonObject pathJson;
    if (!path)
        return pathJson;

    pathJson[QStringLiteral("path_id")] = path->getPathId();
    pathJson[QStringLiteral("path_uid")] = path->getPathUid();
    pathJson[QStringLiteral("canonical_path_key")] =
        path->canonicalPathKey();
    pathJson[QStringLiteral("total_path_cost")] =
        path->getTotalPathCost();
    pathJson[QStringLiteral("total_edge_costs")] =
        path->getTotalEdgeCosts();
    pathJson[QStringLiteral("total_terminal_costs")] =
        path->getTotalTerminalCosts();
    pathJson[QStringLiteral("ranking_cost")] =
        path->getRankingCost();
    pathJson[QStringLiteral("start_terminal")] =
        path->getOriginId();
    pathJson[QStringLiteral("end_terminal")] =
        path->getDestinationId();
    pathJson[QStringLiteral("rank")] = path->getRank();
    pathJson[QStringLiteral("requested_mode")] =
        path->getRequestedMode();
    pathJson[QStringLiteral("requested_top_n")] =
        path->getRequestedTopN();
    pathJson[QStringLiteral("skip_same_mode_terminal_delays_and_costs")] =
        path->skipSameModeTerminalDelaysAndCosts();
    pathJson[QStringLiteral("effective_container_count")] =
        path->getEffectiveContainerCount();
    pathJson[QStringLiteral("weighted_terminal_delay_total")] =
        path->getWeightedTerminalDelayTotal();
    pathJson[QStringLiteral("weighted_terminal_direct_cost_total")] =
        path->getWeightedTerminalDirectCostTotal();
    pathJson[QStringLiteral("raw_terminal_delay_total")] =
        path->getRawTerminalDelayTotal();
    pathJson[QStringLiteral("raw_terminal_cost_total")] =
        path->getRawTerminalCostTotal();
    if (!path->getCostBreakdown().isEmpty())
        pathJson[QStringLiteral("cost_breakdown")] =
            path->getCostBreakdown();

    QJsonObject discoveryContext;
    discoveryContext[QStringLiteral("start_terminal")] =
        path->getOriginId();
    discoveryContext[QStringLiteral("end_terminal")] =
        path->getDestinationId();
    discoveryContext[QStringLiteral("requested_mode")] =
        path->getRequestedMode();
    discoveryContext[QStringLiteral("requested_top_n")] =
        path->getRequestedTopN();
    discoveryContext[QStringLiteral("skip_same_mode_terminal_delays_and_costs")] =
        path->skipSameModeTerminalDelaysAndCosts();
    pathJson[QStringLiteral("discovery_context")] =
        discoveryContext;

    QJsonArray terminals;
    for (const auto &terminal : path->getTerminalsInPath())
    {
        QJsonObject o;
        o[QStringLiteral("terminal")] = terminal.id;
        o[QStringLiteral("display_name")] = terminal.displayName;
        o[QStringLiteral("canonical_name")] = terminal.canonicalName;
        o[QStringLiteral("sequence_index")] = terminal.sequenceIndex;
        o[QStringLiteral("handling_time")] = terminal.handlingTime;
        o[QStringLiteral("cost")] = terminal.rawCost;
        o[QStringLiteral("costs_skipped")] = terminal.costsSkipped;
        o[QStringLiteral("weighted_terminal_delay_contribution")] =
            terminal.weightedTerminalDelayContribution;
        o[QStringLiteral("weighted_terminal_cost_contribution")] =
            terminal.weightedTerminalCostContribution;
        o[QStringLiteral("weighted_terminal_total_contribution")] =
            terminal.weightedTerminalTotalContribution;
        if (!terminal.skipReason.isEmpty())
            o[QStringLiteral("skip_reason")] =
                terminal.skipReason;
        terminals.append(o);
    }
    pathJson[QStringLiteral("terminals_in_path")] = terminals;

    QJsonArray segments;
    for (const auto *segment : path->getSegments())
    {
        if (!segment)
            continue;
        QJsonObject o;
        o[QStringLiteral("from")] = segment->getStart();
        o[QStringLiteral("to")] = segment->getEnd();
        o[QStringLiteral("mode")] =
            Backend::TransportationTypes::toInt(
                segment->getMode());
        o[QStringLiteral("sequence_index")] =
            segment->sequenceIndex();
        o[QStringLiteral("ranking_cost_contribution")] =
            segment->rankingCostContribution();
        o[QStringLiteral("weighted_edge_cost")] =
            segment->weightedEdgeCost();
        o[QStringLiteral("weighted_terminal_cost_embedded_in_segment")] =
            segment->weightedTerminalCostEmbeddedInSegment();
        o[QStringLiteral("attributes")] =
            segment->getAttributes();
        segments.append(o);
    }
    pathJson[QStringLiteral("segments")] = segments;
    return pathJson;
}

} // namespace

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
                [this, mainWindow](const PathIdentity &pathKey) {
                    mainWindow->flashPathLines(pathKey);
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
    const QList<Backend::Path *>                              &paths,
    const QHash<PathIdentity, Backend::Scenario::PathMetrics> &predicted,
    const QHash<PathIdentity, Backend::Scenario::PathMetrics> &actual)
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
            const PathIdentity canonicalKey =
                basePathKey(path);
            const PathIdentity resolvedKey =
                makeUniquePathKey(canonicalKey, m_pathData);

            // Create a new PathData pointer
            PathData *pathDataPtr = new PathData(
                std::shared_ptr<Backend::Path>(path), resolvedKey, {},
                -1.0, -1.0, -1.0);
            m_pathData.insert(resolvedKey, pathDataPtr);
            m_displayOrder.append(resolvedKey);

            if (resolvedKey != canonicalKey)
            {
                qCWarning(lcGuiPathTable)
                    << "Duplicate canonical path identity detected;"
                    << "using compatibility key" << resolvedKey
                    << "for canonical key" << canonicalKey;

                if (!canonicalKey.isEmpty())
                {
                    if (m_predicted.contains(canonicalKey)
                        && !m_predicted.contains(resolvedKey))
                    {
                        m_predicted.insert(
                            resolvedKey,
                            m_predicted.value(canonicalKey));
                    }
                    if (m_actual.contains(canonicalKey)
                        && !m_actual.contains(resolvedKey))
                    {
                        m_actual.insert(
                            resolvedKey,
                            m_actual.value(canonicalKey));
                    }
                }
            }
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

void ShortestPathsTable::setPreparedPaths(
    const Backend::Scenario::PreparedPathSet                  &prepared,
    const QHash<PathIdentity, Backend::Scenario::PathMetrics> &actual,
    const QHash<PathIdentity, Backend::Scenario::PreparedPathEligibility>
        &eligibility)
{
    clear();

    for (auto it = prepared.predictedMetricsByPathIdentity().constBegin();
         it != prepared.predictedMetricsByPathIdentity().constEnd(); ++it)
    {
        m_predicted.insert(it.key(), it.value());
    }
    for (auto it = actual.constBegin(); it != actual.constEnd(); ++it)
        m_actual.insert(it.key(), it.value());

    for (const auto &record : prepared.records())
    {
        if (!record.path)
            continue;

        const PathIdentity preferredKey =
            record.pathIdentity.isEmpty()
                ? basePathKey(record.path.get())
                : record.pathIdentity;
        const PathIdentity resolvedKey =
            makeUniquePathKey(preferredKey, m_pathData);

        const auto resolvedEligibility =
            eligibility.value(record.pathIdentity,
                              Backend::Scenario::
                                  PreparedPathEligibility{});
        auto *pathData = new PathData(record.path, resolvedKey,
                                      resolvedEligibility,
                                      -1.0, -1.0, -1.0);
        m_pathData.insert(resolvedKey, pathData);
        m_displayOrder.append(resolvedKey);

        if (resolvedKey != preferredKey)
        {
            qCWarning(lcGuiPathTable)
                << "Prepared path identity collision detected;"
                << "using compatibility key" << resolvedKey
                << "for prepared identity" << preferredKey;

            if (m_predicted.contains(preferredKey)
                && !m_predicted.contains(resolvedKey))
            {
                m_predicted.insert(resolvedKey,
                                   m_predicted.value(preferredKey));
            }
            if (m_actual.contains(preferredKey)
                && !m_actual.contains(resolvedKey))
            {
                m_actual.insert(resolvedKey,
                                m_actual.value(preferredKey));
            }
        }
    }

    refreshTable();
    m_exportButton->setEnabled(!m_pathData.isEmpty());
}

void ShortestPathsTable::setPathEligibility(
    const QHash<PathIdentity, Backend::Scenario::PreparedPathEligibility>
        &eligibility)
{
    for (auto it = m_pathData.begin(); it != m_pathData.end(); ++it)
    {
        if (!it.value())
            continue;
        it.value()->eligibility =
            eligibility.value(it.key(),
                              Backend::Scenario::
                                  PreparedPathEligibility{});
    }

    refreshTable();
}

bool ShortestPathsTable::isSelectable(const PathData *pathData) const
{
    return pathData && pathData->eligibility.selectable;
}

int ShortestPathsTable::selectablePathCount() const
{
    int count = 0;
    for (const auto *pathData : m_pathData)
    {
        if (pathData && pathData->isVisible && pathData->path
            && isSelectable(pathData))
        {
            ++count;
        }
    }
    return count;
}

QString ShortestPathsTable::eligibilityTooltip(
    const PathData *pathData) const
{
    if (!pathData)
        return QString();

    QStringList lines;
    lines.append(pathData->pathKey);
    if (!pathData->eligibility.blockingReason.isEmpty())
    {
        lines.append(
            tr("Blocked: %1")
                .arg(pathData->eligibility.blockingReason));
    }
    if (!pathData->eligibility.warningReason.isEmpty())
    {
        lines.append(
            tr("Warning: %1")
                .arg(pathData->eligibility.warningReason));
    }
    return lines.join(QLatin1Char('\n'));
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
    const PathIdentity &pathKey, double totalCost, double edgeCost,
    double terminalCost)
{
    qCDebug(lcGuiPathTable) << "ShortestPathTable: updating cost field:"
                            << "pathKey=" << pathKey;
    // Check if the path exists in our data
    if (!m_pathData.contains(pathKey))
    {
        qCWarning(lcGuiPathTable)
            << "Path key" << pathKey
            << "not found for prediction cost update";
        return;
    }

    // Get reference to the path data
    PathData *pathData = m_pathData[pathKey];

    // Validate path pointer
    if (!pathData->path)
    {
        qCWarning(lcGuiPathTable)
            << "Path object is null for path"
            << pathKey;
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
            if (idItem
                && idItem->data(Qt::UserRole).toString()
                       == pathKey)
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
            << pathKey << ":" << ex.what();
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
    const PathIdentity &pathKey, double simulationTotalCost,
    double simulationEdgeCost,
    double simulationTerminalCost)
{
    // Check if the path exists in our data
    if (!m_pathData.contains(pathKey))
    {
        qCWarning(lcGuiPathTable)
            << "Path key" << pathKey
            << "not found for simulation cost update";
        return;
    }

    // Get reference to the path data
    PathData *pathData = m_pathData[pathKey];

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
        if (idItem
            && idItem->data(Qt::UserRole).toString()
                   == pathKey)
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
ShortestPathsTable::createPathRow(
    const PathIdentity &pathKey, const PathData *pathData)
{
    // Check if path pointer is valid
    if (!pathData->path)
    {
        qCWarning(lcGuiPathTable)
            << "Cannot create path row: path is null for ID"
            << pathKey;
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
        [this, pathKey]() { emit showPathSignal(pathKey); });

    contentLayout->addWidget(showButton);

    // Get terminals and segments from the path
    const QList<Backend::PathTerminal> &terminals =
        pathData->path->getTerminalsInPath();
    const QList<Backend::PathSegment *> segments =
        pathData->path->getSegments();

    // Validate terminal and segment data
    if (terminals.isEmpty())
    {
        qCWarning(lcGuiPathTable) << "No terminals for path"
                                  << pathKey;
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

    const QVector<PathIdentity> checkedPathKeys =
        getCheckedPathKeys();
    QSet<PathIdentity> checkedSet;
    for (const auto &pathKey : checkedPathKeys)
        checkedSet.insert(pathKey);

    // Clear the table while preserving header
    m_table->setRowCount(0);
    // Plan 8.2: row indices are rebuilt below; reset the map.
    m_rowByPathKey.clear();

    // Add rows for each visible path
    for (const auto &pathKey : std::as_const(m_displayOrder))
    {
        const PathData *pathData =
            m_pathData.value(pathKey, nullptr);

        // Skip paths marked as not visible
        if (!pathData->isVisible || !pathData->path)
        {
            continue;
        }

        // Add a new row at the end of the table
        int row = m_table->rowCount();
        m_table->insertRow(row);
        // Plan 8.2: remember row index for refreshRow updates.
        m_rowByPathKey.insert(pathKey, row);

        // Create checkbox widget for the select column
        auto checkboxWidget = new QWidget();
        auto checkboxLayout =
            new QHBoxLayout(checkboxWidget);
        checkboxLayout->setAlignment(Qt::AlignCenter);
        checkboxLayout->setContentsMargins(0, 0, 0, 0);

        auto checkbox = new QCheckBox();
        checkboxLayout->addWidget(checkbox);
        m_table->setCellWidget(row, 0, checkboxWidget);
        checkbox->setEnabled(isSelectable(pathData));
        const QString rowTooltip = eligibilityTooltip(pathData);
        checkbox->setToolTip(rowTooltip);
        checkboxWidget->setToolTip(rowTooltip);

        // Connect checkbox state change to update
        // compare button state
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        // Qt 6.7+ version with checkStateChanged signal
        connect(checkbox, &QCheckBox::checkStateChanged,
                this, [this, pathKey](Qt::CheckState state) {
                    // Emit signal that checkbox state changed
                    emit checkboxChanged(pathKey, state == Qt::Checked);

                    // Get all checked paths
                    QVector<PathIdentity> checkedPaths =
                        getCheckedPathKeys();
                    bool hasCheckedPaths =
                        !checkedPaths.isEmpty();

                           // Enable compare button if at least 1 path is checked
                    m_compareButton->setEnabled(hasCheckedPaths);

                           // Enable export button if there are any paths in the table (checked or not)
                    m_exportButton->setEnabled(!m_pathData.isEmpty());

                           // Update select/unselect buttons state
                    m_unselectAllButton->setEnabled(hasCheckedPaths);

                           // Enable select all button only if not all paths are already selected
                    m_selectAllButton->setEnabled(
                        selectablePathCount() > checkedPaths.size());
                });
#else
      // Qt 6.4-6.6 version with stateChanged signal
        connect(checkbox, &QCheckBox::stateChanged,
                this, [this, pathKey](int state) {
                    // Convert int to Qt::CheckState for consistency
                    Qt::CheckState checkState = static_cast<Qt::CheckState>(state);

                    // Emit signal that checkbox state changed
                    emit checkboxChanged(pathKey,
                                         checkState == Qt::Checked);

                    // Get all checked paths
                    QVector<PathIdentity> checkedPaths =
                        getCheckedPathKeys();
                    bool hasCheckedPaths =
                        !checkedPaths.isEmpty();

                           // Enable compare button if at least 1 path is checked
                    m_compareButton->setEnabled(hasCheckedPaths);

                           // Enable export button if there are any paths in the table (checked or not)
                    m_exportButton->setEnabled(!m_pathData.isEmpty());

                           // Update select/unselect buttons state
                    m_unselectAllButton->setEnabled(hasCheckedPaths);

                           // Enable select all button only if not all paths are already selected
                    m_selectAllButton->setEnabled(
                        selectablePathCount() > checkedPaths.size());
                });
#endif

        if (isSelectable(pathData)
            && checkedSet.contains(pathKey))
        {
            checkbox->setChecked(true);
        }

        // Add Path ID cell
        auto pathItem = new QTableWidgetItem(
            QString::number(pathData->path->getPathId()));
        pathItem->setData(Qt::UserRole, pathKey);
        pathItem->setToolTip(rowTooltip);
        if (!isSelectable(pathData))
            pathItem->setForeground(QBrush(Qt::gray));
        m_table->setItem(row, 1, pathItem);

        // Create and add terminal path visualization
        auto pathWidget = createPathRow(pathKey, pathData);
        pathWidget->setToolTip(rowTooltip);
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
        predictedItem->setToolTip(rowTooltip);
        if (!isSelectable(pathData))
            predictedItem->setForeground(QBrush(Qt::gray));
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
        actualItem->setToolTip(rowTooltip);
        if (!isSelectable(pathData))
            actualItem->setForeground(QBrush(Qt::gray));
        m_table->setItem(row, 4, actualItem);

        // Plan 8.2: populate per-vehicle metric columns.
        refreshRow(pathKey);
    }

    // Clear UI update flag
    m_updatingUI = false;

    // Update button states
    m_exportButton->setEnabled(!m_pathData.isEmpty());

    // Update selection button states
    m_selectAllButton->setEnabled(
        selectablePathCount() > getCheckedPathKeys().size());
    m_unselectAllButton->setEnabled(
        !getCheckedPathKeys().isEmpty());
}

/**
 * @brief Retrieves path data for a specific path ID
 * @param pathId The ID of the path to retrieve
 * @return Pointer to the path data or nullptr if not found
 */
const ShortestPathsTable::PathData *
ShortestPathsTable::getDataByPathKey(
    const PathIdentity &pathKey) const
{
    // Look up the path in the map
    auto it = m_pathData.find(pathKey);
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
    auto checkedPaths = getCheckedPathKeys();
    if (checkedPaths.isEmpty())
    {
        return QList<const PathData *>();
    }

    for (const auto &pathKey : std::as_const(checkedPaths))
    {
        auto pathData = getDataByPathKey(pathKey);
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
ShortestPathsTable::PathIdentity
ShortestPathsTable::getSelectedPathKey() const
{
    // Get all selected items
    QList<QTableWidgetItem *> selectedItems =
        m_table->selectedItems();
    if (selectedItems.isEmpty())
    {
        return QString(); // No selection
    }

    // Get the row of the first selected item
    int row = selectedItems.first()->row();

    // Get the Path ID from column 1
    auto idItem = m_table->item(row, 1);
    return idItem ? idItem->data(Qt::UserRole).toString()
                  : QString();
}

/**
 * @brief Gets all path IDs that are currently checked
 * @return Vector of checked path IDs
 */
QVector<PathIdentity>
ShortestPathsTable::getCheckedPathKeys() const
{
    QVector<PathIdentity> checkedPaths;

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
        if (checkBox && checkBox->isEnabled()
            && checkBox->isChecked())
        {
            // If checkbox exists and is checked, add the
            // path ID
            auto idItem = m_table->item(row, 1);
            if (idItem)
            {
                checkedPaths.append(
                    idItem->data(Qt::UserRole).toString());
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
    qDeleteAll(m_pathData);
    m_pathData.clear();
    m_displayOrder.clear();

    // Clear the table
    m_table->setRowCount(0);

    // Disable buttons that require paths
    m_compareButton->setEnabled(false);
    m_exportButton->setEnabled(false);
    m_predicted.clear();
    m_actual.clear();
    m_rowByPathKey.clear();
}

QList<QJsonObject> ShortestPathsTable::buildComparisonSnapshots(
    const Backend::Scenario::ScenarioDocument &doc) const
{
    QList<QJsonObject> snapshots;

    QJsonObject topology;
    topology[QStringLiteral("regions")] = doc.regions.size();
    topology[QStringLiteral("terminals")] = doc.terminals.size();
    topology[QStringLiteral("linkages")] = doc.linkages.size();
    topology[QStringLiteral("connections")] = doc.connections.size();
    topology[QStringLiteral("global_links")] = doc.globalLinks.size();

    QVariantMap costWeights;
    auto &ctl = CargoNetSim::CargoNetSimController::getInstance();
    if (auto *cfg = ctl.getConfigController())
        costWeights = cfg->getCostFunctionWeights();

    const QString discoveredAtUtc =
        QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    QJsonObject scenarioSnapshot =
        Backend::Scenario::ScenarioSerializer::toJson(doc);
    scenarioSnapshot.remove(QStringLiteral("comparison_snapshots"));
    const QString scenarioFingerprint =
        sha256Hex(scenarioSnapshot);

    const QString topologyFingerprint = sha256Hex(topology);
    const QString costFingerprint = sha256Hex(
        QJsonObject::fromVariantMap(costWeights));

    const QVector<PathIdentity> checkedPathKeys =
        getCheckedPathKeys();
    QSet<PathIdentity> checkedSet;
    for (const auto &pathKey : checkedPathKeys)
        checkedSet.insert(pathKey);

    for (const auto &pathKey : std::as_const(m_displayOrder))
    {
        const PathData *pathData =
            m_pathData.value(pathKey, nullptr);
        if (!pathData || !pathData->path)
            continue;

        QJsonObject snapshot;
        snapshot[QStringLiteral("schema_version")] = 2;
        snapshot[QStringLiteral("path_identity")] =
            pathData->pathKey;
        snapshot[QStringLiteral("canonical_path_key")] =
            pathData->path->canonicalPathKey();
        snapshot[QStringLiteral("path_uid")] =
            pathData->path->getPathUid();

        QJsonObject selection;
        selection[QStringLiteral("is_visible")] =
            pathData->isVisible;
        selection[QStringLiteral("is_selected")] =
            checkedSet.contains(pathKey);
        snapshot[QStringLiteral("selection")] = selection;

        QJsonObject queryContext;
        queryContext[QStringLiteral("origin")] =
            pathData->path->getOriginId();
        queryContext[QStringLiteral("destination")] =
            pathData->path->getDestinationId();
        queryContext[QStringLiteral("rank")] =
            pathData->path->getRank();
        queryContext[QStringLiteral("requested_mode")] =
            pathData->path->getRequestedMode();
        queryContext[QStringLiteral("requested_top_n")] =
            pathData->path->getRequestedTopN();
        queryContext[QStringLiteral("skip_same_mode_terminal_delays_and_costs")] =
            pathData->path->skipSameModeTerminalDelaysAndCosts();
        snapshot[QStringLiteral("query_context")] =
            queryContext;

        QJsonObject provenance;
        provenance[QStringLiteral("discovered_at")] =
            discoveredAtUtc;
        provenance[QStringLiteral("scenario_fingerprint")] =
            scenarioFingerprint;
        provenance[QStringLiteral("topology_fingerprint")] =
            topologyFingerprint;
        provenance[QStringLiteral("cost_weights_fingerprint")] =
            costFingerprint;
        provenance[QStringLiteral("cost_weights")] =
            QJsonObject::fromVariantMap(costWeights);
        snapshot[QStringLiteral("discovery_provenance")] =
            provenance;

        const auto predicted = m_predicted.value(pathKey);
        const auto actual = m_actual.value(pathKey);
        snapshot[QStringLiteral("predicted_metrics")] =
            metricsToJson(predicted);
        snapshot[QStringLiteral("actual_metrics")] =
            metricsToJson(actual);

        QJsonObject simulation;
        simulation[QStringLiteral("state")] =
            pathData->m_totalSimulationPathCost >= 0.0
                ? QStringLiteral("simulated")
                : QStringLiteral("not_simulated");
        simulation[QStringLiteral("total_cost")] =
            pathData->m_totalSimulationPathCost;
        simulation[QStringLiteral("edge_costs")] =
            pathData->m_totalSimulationEdgeCosts;
        simulation[QStringLiteral("terminal_costs")] =
            pathData->m_totalSimulationTerminalCosts;
        simulation[QStringLiteral("effective_container_count")] =
            pathData->executionResult.has_value()
                ? pathData->executionResult->effectiveContainerCount
                : pathData->path->getEffectiveContainerCount();
        snapshot[QStringLiteral("simulation")] =
            simulation;
        if (pathData->executionResult.has_value())
        {
            snapshot[QStringLiteral("execution_result")] =
                pathData->executionResult->toJson();
        }

        snapshot[QStringLiteral("path")] =
            buildPathSnapshotJson(*pathData);
        snapshots.append(snapshot);
    }

    return snapshots;
}

void ShortestPathsTable::loadComparisonSnapshots(
    const QList<QJsonObject> &snapshots)
{
    clear();

    QSet<PathIdentity> selectedPathKeys;
    for (const QJsonObject &snapshot : snapshots)
    {
        const QJsonObject pathJson =
            snapshot.value(QStringLiteral("path")).toObject();
        if (pathJson.isEmpty())
            continue;

        auto *path = Backend::Path::fromJson(pathJson, {});
        if (!path)
            continue;

        const PathIdentity storedKey = makeUniquePathKey(
            snapshotPathKey(snapshot, path), m_pathData);
        m_predicted.insert(
            storedKey,
            metricsFromJson(
                snapshot.value(QStringLiteral("predicted_metrics"))
                    .toObject()));
        m_actual.insert(
            storedKey,
            metricsFromJson(
                snapshot.value(QStringLiteral("actual_metrics"))
                    .toObject()));

        const QJsonObject simulation =
            snapshot.value(QStringLiteral("simulation")).toObject();
        const QJsonObject selection =
            snapshot.value(QStringLiteral("selection")).toObject();
        auto *pathData = new PathData(
            std::shared_ptr<Backend::Path>(path), storedKey,
            {},
            simulation.value(QStringLiteral("total_cost")).toDouble(-1.0),
            simulation.value(QStringLiteral("edge_costs")).toDouble(-1.0),
            simulation.value(QStringLiteral("terminal_costs")).toDouble(-1.0));
        const QJsonObject executionResult =
            snapshot.value(QStringLiteral("execution_result")).toObject();
        if (!executionResult.isEmpty())
        {
            pathData->executionResult =
                Backend::Scenario::PathExecutionResult::fromJson(
                    executionResult);
        }
        pathData->isVisible =
            selection.value(QStringLiteral("is_visible")).toBool(true);
        m_pathData.insert(storedKey, pathData);
        m_displayOrder.append(storedKey);
        if (selection.value(QStringLiteral("is_selected")).toBool(false))
            selectedPathKeys.insert(storedKey);
    }

    refreshTable();
    m_exportButton->setEnabled(!m_pathData.isEmpty());
    m_selectAllButton->setEnabled(!m_pathData.isEmpty());

    for (int row = 0; row < m_table->rowCount(); ++row)
    {
        auto *idItem = m_table->item(row, 1);
        if (!idItem)
            continue;
        const QString pathKey =
            idItem->data(Qt::UserRole).toString();
        if (!selectedPathKeys.contains(pathKey))
            continue;
        auto *checkboxWidget = m_table->cellWidget(row, 0);
        if (!checkboxWidget || !checkboxWidget->layout())
            continue;
        auto *checkBox = qobject_cast<QCheckBox *>(
            checkboxWidget->layout()->itemAt(0)->widget());
        if (checkBox)
            checkBox->setChecked(true);
    }

    m_compareButton->setEnabled(!selectedPathKeys.isEmpty());
    m_unselectAllButton->setEnabled(!selectedPathKeys.isEmpty());
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
    const PathIdentity pathKey = getSelectedPathKey();
    qCDebug(lcGuiPathTable) << "ShortestPathsTable::onSelectionChanged:"
                            << "pathKey=" << pathKey;

    // Enable export button if either a path is selected or
    // any path is checked
    bool hasCheckedPaths = !getCheckedPathKeys().isEmpty();
    m_exportButton->setEnabled(!pathKey.isEmpty()
                               || hasCheckedPaths);

    // Emit selection signal if a path is selected
    if (!pathKey.isEmpty())
    {
        emit pathSelected(pathKey);
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
    QVector<PathIdentity> checkedPaths =
        getCheckedPathKeys();

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
    QVector<PathIdentity> checkedPaths =
        getCheckedPathKeys();

    if (checkedPaths.isEmpty())
    {
        // No paths checked, export all visible paths
        exportPathsToPdf(QVector<PathIdentity>());
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
    exportPathsToPdf(QVector<PathIdentity>());

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
        if (checkBox && checkBox->isEnabled())
        {
            // Check the checkbox
            checkBox->setChecked(true);
        }
    }

    // Clear UI update flag
    m_updatingUI = false;

    // Enable compare button if paths are checked
    m_compareButton->setEnabled(
        !getCheckedPathKeys().isEmpty());

    // Enable/disable selection buttons based on state
    m_selectAllButton->setEnabled(
        selectablePathCount() > getCheckedPathKeys().size());
    m_unselectAllButton->setEnabled(
        !getCheckedPathKeys().isEmpty());
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
    m_selectAllButton->setEnabled(selectablePathCount() > 0);
    m_unselectAllButton->setEnabled(false);
}

void ShortestPathsTable::exportPathsToPdf(
    const QVector<PathIdentity> &pathKeys)
{
    qCInfo(lcGuiPathTable) << "ShortestPathsTable::exportPathsToPdf:"
                           << "pathKeys=" << pathKeys.size();
    // Get path data for the specified IDs
    QList<const PathData *> pathsToExport;

    if (pathKeys.isEmpty())
    {
        // If no IDs specified, export all visible paths
        for (const auto &pathKey : std::as_const(m_displayOrder))
        {
            const auto *pathData =
                m_pathData.value(pathKey, nullptr);
            if (pathData && pathData->isVisible)
            {
                pathsToExport.append(pathData);
            }
        }
    }
    else
    {
        // Export only the specified paths
        for (const auto &pathKey : pathKeys)
        {
            auto it = m_pathData.find(pathKey);
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
    const QHash<PathIdentity, Backend::Scenario::PathMetrics> &actual)
{
    for (auto it = actual.constBegin();
         it != actual.constEnd(); ++it)
    {
        m_actual.insert(it.key(), it.value());
        refreshRow(it.key());
    }
}

void ShortestPathsTable::setExecutionResults(
    const Backend::Scenario::ScenarioExecutionResultSet &results)
{
    for (const auto &result : results.pathResults())
    {
        PathData *pathData =
            m_pathData.value(result.pathIdentity, nullptr);
        if (!pathData && !result.canonicalPathKey.isEmpty())
        {
            pathData = m_pathData.value(result.canonicalPathKey, nullptr);
        }
        if (!pathData)
        {
            qCWarning(lcGuiPathTable)
                << "Path key" << result.pathIdentity
                << "not found for execution result update";
            continue;
        }

        pathData->executionResult = result;
        updateSimulationCosts(pathData->pathKey,
                              result.totalCost,
                              result.edgeCosts,
                              result.terminalCosts);
    }
}

void ShortestPathsTable::refreshRow(
    const PathIdentity &pathKey)
{
    const int row = m_rowByPathKey.value(pathKey, -1);
    if (row < 0)
        return;

    auto          fmt  = [](double v) {
        return QString::number(v, 'f', 2);
    };
    const QString dash = QStringLiteral("—");
    auto          cell = [&](bool valid, double v) {
        return new QTableWidgetItem(valid ? fmt(v) : dash);
    };

    const auto &p = m_predicted.value(pathKey);
    const auto &a = m_actual.value(pathKey);

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
