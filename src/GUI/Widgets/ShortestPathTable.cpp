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
#include "Backend/Application/PathPresentationService.h"
#include "Backend/Application/ScenarioPersistenceService.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "GUI/MainWindow.h"
#include "GUI/Utils/IconCreator.h" // For icon creation utilities
#include "GUI/Utils/PathReportExporter.h"
#include "GUI/Widgets/PathComparisonDialog.h"
#include <QApplication>
#include <QFileDialog>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
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

using ExecutionPathKey = ShortestPathsTable::ExecutionPathKey;

enum TableColumn : int
{
    ColumnSelect = 0,
    ColumnPathId,
    ColumnStatus,
    ColumnExecutionProgress,
    ColumnTerminalPath,
    ColumnPredictedCost,
    ColumnActualCost,
    ColumnPredictedDistance,
    ColumnPredictedTime,
    ColumnPredictedFuelPerVehicle,
    ColumnPredictedEnergyPerVehicle,
    ColumnPredictedCarbonPerVehicle,
    ColumnPredictedRiskPerVehicle,
    ColumnActualDistance,
    ColumnActualTime,
    ColumnActualEnergyPerVehicle,
    ColumnActualCarbonPerVehicle,
    ColumnActualRiskPerVehicle,
    ColumnContainers,
    ColumnVehicles,
    ColumnPredictedFuelPerContainer,
    ColumnPredictedEnergyPerContainer,
    ColumnPredictedCarbonPerContainer,
    ColumnActualEnergyPerContainer,
    ColumnActualCarbonPerContainer,
    ColumnCount
};

ExecutionPathKey makeUniquePathKey(
    const ExecutionPathKey &baseKey,
    const QMap<ExecutionPathKey, ShortestPathsTable::PathData *> &existing)
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

QString vehicleBreakdownLabel(
    const Backend::Scenario::PathMetrics &metrics)
{
    if (metrics.previewVehicleBreakdown.isEmpty())
    {
        return metrics.valid
            ? QString::number(metrics.vehiclesNeeded)
            : QStringLiteral("—");
    }

    QStringList parts;
    parts.reserve(metrics.previewVehicleBreakdown.size());
    for (const auto &requirement : metrics.previewVehicleBreakdown)
    {
        parts.append(QStringLiteral("%1 x%2")
                         .arg(Backend::TransportationTypes::toString(
                             requirement.mode))
                         .arg(requirement.vehiclesNeeded));
    }
    return parts.join(QStringLiteral(" | "));
}

QString pathLifecycleLabel(
    Backend::Scenario::PathLifecycleState lifecycle)
{
    using Backend::Scenario::PathLifecycleState;
    switch (lifecycle)
    {
    case PathLifecycleState::Pending:
        return QObject::tr("pending");
    case PathLifecycleState::Running:
        return QObject::tr("running");
    case PathLifecycleState::WaitingForTerminalHandoff:
        return QObject::tr("terminal handoff");
    case PathLifecycleState::WaitingForTerminalProcessing:
        return QObject::tr("terminal processing");
    case PathLifecycleState::ReadyForNextSegment:
        return QObject::tr("ready for next segment");
    case PathLifecycleState::Paused:
        return QObject::tr("paused");
    case PathLifecycleState::Skipped:
        return QObject::tr("skipped");
    case PathLifecycleState::Completed:
        return QObject::tr("completed");
    case PathLifecycleState::Failed:
        return QObject::tr("failed");
    }
    return QObject::tr("unknown");
}

QString segmentLifecycleLabel(
    Backend::Scenario::SegmentLifecycleState lifecycle)
{
    using Backend::Scenario::SegmentLifecycleState;
    switch (lifecycle)
    {
    case SegmentLifecycleState::Pending:
        return QObject::tr("pending");
    case SegmentLifecycleState::Dispatched:
        return QObject::tr("dispatched");
    case SegmentLifecycleState::VehicleRunning:
        return QObject::tr("running");
    case SegmentLifecycleState::VehicleArrived:
        return QObject::tr("arrived");
    case SegmentLifecycleState::UnloadCompleted:
        return QObject::tr("unloaded");
    case SegmentLifecycleState::TerminalHandoffCompleted:
        return QObject::tr("handoff complete");
    case SegmentLifecycleState::TerminalProcessing:
        return QObject::tr("terminal processing");
    case SegmentLifecycleState::ReadyForPickup:
        return QObject::tr("ready");
    case SegmentLifecycleState::Skipped:
        return QObject::tr("skipped");
    case SegmentLifecycleState::Completed:
        return QObject::tr("completed");
    case SegmentLifecycleState::Failed:
        return QObject::tr("failed");
    }
    return QObject::tr("unknown");
}

QString modeLabel(
    Backend::TransportationTypes::TransportationMode mode)
{
    if (mode == Backend::TransportationTypes::TransportationMode::Any)
        return QObject::tr("No mode");
    return Backend::TransportationTypes::toString(mode);
}

const Backend::Scenario::SegmentProgressSnapshot *
activeSegmentFor(
    const Backend::Scenario::PathProgressSnapshot &progress)
{
    for (const auto &segment : progress.segments)
    {
        if (segment.segmentIndex == progress.activeSegmentIndex)
            return &segment;
    }
    return nullptr;
}

QString statusMessage(
    const Backend::Scenario::PreparedPathEligibility &eligibility)
{
    if (!eligibility.simulatable)
        return QObject::tr("Simulation unavailable");

    if (!eligibility.warningReason.isEmpty())
        return QObject::tr("Ready with warning");

    return QObject::tr("Ready");
}

QPixmap statusIcon(
    const Backend::Scenario::PreparedPathEligibility &eligibility)
{
    constexpr int kStatusIconSize = 16;

    if (!eligibility.simulatable)
    {
        return IconFactory::createStatusUnavailableIcon(
            kStatusIconSize);
    }

    if (!eligibility.warningReason.isEmpty())
    {
        return IconFactory::createStatusWarningIcon(
            kStatusIconSize);
    }

    return IconFactory::createStatusReadyIcon(kStatusIconSize);
}

void styleStatusItem(
    QTableWidgetItem                                 *item,
    const Backend::Scenario::PreparedPathEligibility &eligibility)
{
    if (!item)
        return;

    item->setTextAlignment(Qt::AlignCenter);
    item->setText(QString());
    item->setIcon(QIcon(statusIcon(eligibility)));

    if (!eligibility.simulatable)
    {
        item->setBackground(QColor(253, 236, 234));
        item->setForeground(QBrush(QColor(145, 33, 54)));
        QFont font = item->font();
        font.setBold(true);
        item->setFont(font);
        return;
    }

    if (!eligibility.warningReason.isEmpty())
    {
        item->setBackground(QColor(255, 245, 224));
        item->setForeground(QBrush(QColor(133, 87, 0)));
        return;
    }

    item->setBackground(QColor(232, 245, 236));
    item->setForeground(QBrush(QColor(31, 101, 58)));
}

QString compactJsonObject(const QJsonObject &object)
{
    if (object.isEmpty())
        return QStringLiteral("{}");
    return QString::fromUtf8(
        QJsonDocument(object).toJson(QJsonDocument::Compact));
}

double costSnapshotTotal(
    const Backend::Application::PathPresentationCostSnapshot &costs)
{
    return costs.travelTime + costs.distance
           + costs.carbonEmissions + costs.energyConsumption
           + costs.risk + costs.directCost;
}

void logPredictedPathDiagnostics(
    const QMap<ExecutionPathKey, ShortestPathsTable::PathData *> &paths,
    const QVector<ExecutionPathKey>                              &displayOrder,
    const QString                                                &source)
{
    Backend::Application::PathPresentationService presenter;

    qCInfo(lcGuiPathTable)
        << "ShortestPathsTable::predictedDiagnostics: begin"
        << "source=" << source
        << "pathCount=" << displayOrder.size();

    for (const auto &pathKey : displayOrder)
    {
        const auto *record = paths.value(pathKey, nullptr);
        if (!record || !record->path)
        {
            qCWarning(lcGuiPathTable)
                << "ShortestPathsTable::predictedDiagnostics:"
                << "missing path record"
                << "source=" << source
                << "pathKey=" << pathKey;
            continue;
        }

        const auto *path = record->path.get();
        const auto  summary = presenter.summary(*record);
        const auto  totals = presenter.pathCostTotals(*record);
        const auto &metrics = record->predictedMetrics;

        qCInfo(lcGuiPathTable)
            << "ShortestPathsTable::predictedDiagnostics:path"
            << "source=" << source
            << "pathKey=" << pathKey
            << "canonicalPathKey=" << path->canonicalPathKey()
            << "pathId=" << path->getPathId()
            << "rank=" << path->getRank()
            << "effectiveContainers=" << path->getEffectiveContainerCount()
            << "terminalCount=" << summary.terminalCount
            << "segmentCount=" << summary.segmentCount
            << "pathTotalUsd=" << path->getTotalPathCost()
            << "pathEdgeUsd=" << path->getTotalEdgeCosts()
            << "pathTerminalUsd=" << path->getTotalTerminalCosts()
            << "rankingCostUsd=" << path->getRankingCost()
            << "weightedTerminalDelayUsd="
            << path->getWeightedTerminalDelayTotal()
            << "weightedTerminalDirectUsd="
            << path->getWeightedTerminalDirectCostTotal()
            << "rawTerminalDelaySeconds="
            << path->getRawTerminalDelayTotal()
            << "rawTerminalDirectUsd="
            << path->getRawTerminalCostTotal()
            << "presentedPredictedCostAvailable="
            << totals.predicted.available
            << "presentedPredictedCostTotalUsd="
            << costSnapshotTotal(totals.predicted)
            << "costBreakdown="
            << compactJsonObject(path->getCostBreakdown());

        qCInfo(lcGuiPathTable)
            << "ShortestPathsTable::predictedDiagnostics:pathMetrics"
            << "source=" << source
            << "pathKey=" << pathKey
            << "valid=" << metrics.valid
            << "distanceKm=" << metrics.distanceKm
            << "travelTimeHours=" << metrics.travelTimeHours
            << "fuelPerVehicle=" << metrics.fuelPerVehicle
            << "energyPerVehicleKWh=" << metrics.energyPerVehicle
            << "carbonPerVehicleT=" << metrics.carbonPerVehicle
            << "riskPerVehicle=" << metrics.riskPerVehicle
            << "containerCount=" << metrics.containerCount
            << "fuelPerContainer=" << metrics.fuelPerContainer
            << "energyPerContainerKWh="
            << metrics.energyPerContainer
            << "carbonPerContainerT=" << metrics.carbonPerContainer
            << "riskPerContainer=" << metrics.riskPerContainer
            << "vehiclesNeeded=" << metrics.vehiclesNeeded
            << "fuelType=" << metrics.fuelType;

        for (const auto &requirement :
             metrics.previewVehicleBreakdown)
        {
            qCInfo(lcGuiPathTable)
                << "ShortestPathsTable::predictedDiagnostics:vehicleRequirement"
                << "source=" << source
                << "pathKey=" << pathKey
                << "segmentIndex=" << requirement.segmentIndex
                << "mode="
                << Backend::TransportationTypes::toString(
                       requirement.mode)
                << "vehiclesNeeded="
                << requirement.vehiclesNeeded;
        }

        const auto &terminals = path->getTerminalsInPath();
        for (int terminalIndex = 0;
             terminalIndex < terminals.size(); ++terminalIndex)
        {
            const auto &terminal = terminals[terminalIndex];
            const auto  values =
                presenter.terminalValues(*record, terminalIndex);
            qCInfo(lcGuiPathTable)
                << "ShortestPathsTable::predictedDiagnostics:terminal"
                << "source=" << source
                << "pathKey=" << pathKey
                << "index=" << terminalIndex
                << "sequenceIndex=" << terminal.sequenceIndex
                << "terminalId=" << terminal.id
                << "displayName=" << terminal.displayName
                << "canonicalName=" << terminal.canonicalName
                << "predictedAvailable="
                << values.predictedAvailable
                << "handlingSeconds="
                << values.predictedHandlingSeconds
                << "rawDirectCostUsd="
                << values.predictedDirectCostUsd
                << "weightedDelayUsd="
                << values.predictedWeightedDelayContribution
                << "weightedDirectCostUsd="
                << values.predictedWeightedCostContribution
                << "weightedTotalUsd="
                << values.predictedWeightedTotalContribution
                << "costsSkipped="
                << values.predictedCostsSkipped
                << "skipReason=" << values.predictedSkipReason;
        }

        const auto segments = path->getSegments();
        for (int segmentIndex = 0; segmentIndex < segments.size();
             ++segmentIndex)
        {
            const auto *segment = segments[segmentIndex];
            if (!segment)
            {
                qCWarning(lcGuiPathTable)
                    << "ShortestPathsTable::predictedDiagnostics:"
                    << "null segment"
                    << "source=" << source
                    << "pathKey=" << pathKey
                    << "index=" << segmentIndex;
                continue;
            }

            const auto values =
                presenter.segmentValues(*record, segmentIndex);
            const auto costs =
                presenter.segmentPredictedCosts(*record,
                                                segmentIndex);
            const auto rawMetrics = segment->estimatedValues();
            const auto rawCosts = segment->estimatedCosts();

            qCInfo(lcGuiPathTable)
                << "ShortestPathsTable::predictedDiagnostics:segmentMetrics"
                << "source=" << source
                << "pathKey=" << pathKey
                << "index=" << segmentIndex
                << "sequenceIndex=" << segment->sequenceIndex()
                << "segmentId=" << segment->getPathSegmentId()
                << "mode="
                << Backend::TransportationTypes::toString(
                       segment->getMode())
                << "start=" << segment->getStart()
                << "end=" << segment->getEnd()
                << "estimatedAvailable=" << rawMetrics.available
                << "distanceMeters=" << rawMetrics.distance
                << "presentedDistanceKm="
                << values.predictedDistanceKm
                << "travelTimeSeconds=" << rawMetrics.travelTime
                << "presentedTravelTimeHours="
                << values.predictedTravelTimeHours
                << "energyKWh=" << rawMetrics.energyConsumption
                << "presentedEnergyKWh="
                << values.predictedEnergyConsumption
                << "carbonT=" << rawMetrics.carbonEmissions
                << "risk=" << rawMetrics.risk
                << "rankingCostContributionUsd="
                << segment->rankingCostContribution()
                << "weightedEdgeCostUsd="
                << segment->weightedEdgeCost()
                << "weightedTerminalEmbeddedUsd="
                << segment->weightedTerminalCostEmbeddedInSegment();

            qCInfo(lcGuiPathTable)
                << "ShortestPathsTable::predictedDiagnostics:segmentCosts"
                << "source=" << source
                << "pathKey=" << pathKey
                << "index=" << segmentIndex
                << "segmentId=" << segment->getPathSegmentId()
                << "rawCostAvailable=" << rawCosts.available
                << "presentedCostAvailable=" << costs.available
                << "travelTimeUsd=" << costs.travelTime
                << "distanceUsd=" << costs.distance
                << "energyUsd=" << costs.energyConsumption
                << "carbonUsd=" << costs.carbonEmissions
                << "riskUsd=" << costs.risk
                << "directUsd=" << costs.directCost
                << "totalUsd=" << costSnapshotTotal(costs);
        }
    }

    qCInfo(lcGuiPathTable)
        << "ShortestPathsTable::predictedDiagnostics: end"
        << "source=" << source
        << "pathCount=" << displayOrder.size();
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
    if (index.column() == ColumnTerminalPath)
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
    if (index.column() == ColumnTerminalPath)
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
                [this, mainWindow](const ExecutionPathKey &pathKey) {
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

    m_availabilityBanner = new QLabel(this);
    m_availabilityBanner->setObjectName(
        QStringLiteral("pathAvailabilityBanner"));
    m_availabilityBanner->setWordWrap(true);
    m_availabilityBanner->setVisible(false);
    m_availabilityBanner->setStyleSheet(
        QStringLiteral(
            "QLabel#pathAvailabilityBanner {"
            " background-color: #fff3f3;"
            " color: #7f1d2d;"
            " border: 1px solid #e6b7c0;"
            " border-radius: 4px;"
            " padding: 6px 8px;"
            "}"));
    layout->addWidget(m_availabilityBanner);

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
    // Plan 8.2/A2: add a dedicated status column near the front of
    // the table so backend eligibility is visible without drowning the
    // metric columns in warning colors.
    m_table->setColumnCount(ColumnCount);
    m_table->setHorizontalHeaderLabels({
        tr("Select"),                tr("Path ID"),
        tr("Status"),                tr("Progress"),
        tr("Terminal Path"),
        tr("Predicted Cost"),        tr("Actual Cost"),
        // Predicted per-vehicle.
        tr("P. Distance (km)"),      tr("P. Time (h)"),
        tr("P. Fuel/Veh"),           tr("P. Energy/Veh (kWh)"),
        tr("P. CO₂/Veh (t)"),        tr("P. Risk/Veh"),
        // Actual per-vehicle; no fuel key from
        // SegmentCostMath)
        tr("A. Distance (km)"),      tr("A. Time (h)"),
        tr("A. Energy/Veh (kWh)"),   tr("A. CO₂/Veh (t)"),
        tr("A. Risk/Veh"),
        // Preview-demand counts and per-container metrics.
        tr("OD Containers"),         tr("Preview Vehicle Plan"),
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
        ColumnSelect, QHeaderView::Fixed); // Fixed width for checkbox
    m_table->setColumnWidth(
        ColumnSelect, 50); // 50 pixels for checkbox column

    header->setSectionResizeMode(
        ColumnPathId, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(
        ColumnStatus, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(
        ColumnExecutionProgress, QHeaderView::Interactive);
    m_table->setColumnWidth(ColumnExecutionProgress, 260);
    header->setSectionResizeMode(
        ColumnTerminalPath, QHeaderView::Interactive);
    // The terminal sequence is the most scan-heavy field in the table.
    // Give it a wide default so it does not collapse behind the metric
    // columns, while still letting users resize it manually.
    m_table->setColumnWidth(ColumnTerminalPath, 520);
    header->setSectionResizeMode(
        ColumnPredictedCost, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(
        ColumnActualCost, QHeaderView::ResizeToContents);

    // Plan 8.2 + Plan 10: auto-size the metric columns.
    for (int c = ColumnPredictedDistance; c < ColumnCount; ++c)
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
    const QHash<ExecutionPathKey, Backend::Scenario::PathMetrics> &predicted,
    const QHash<ExecutionPathKey, Backend::Scenario::PathMetrics> &actual)
{
    qCDebug(lcGuiPathTable) << "ShortestPathsTable::addPaths:"
                            << "pathCount=" << paths.size();
    const auto records =
        Backend::Application::PathPresentationService()
            .recordsFromRawPaths(paths, predicted, actual);
    for (auto record : records)
    {
        appendPathRecord(std::move(record));
    }
    logPredictedPathDiagnostics(m_pathData, m_displayOrder,
                                QStringLiteral("addPaths"));

    // Refresh the table with the new paths
    refreshTable();

    // Enable export all button if we have data
    m_exportButton->setEnabled(!m_pathData.isEmpty());
}

void ShortestPathsTable::setPreparedPaths(
    const Backend::Scenario::PreparedPathSet                  &prepared,
    const QHash<ExecutionPathKey, Backend::Scenario::PathMetrics> &actual,
    const QHash<ExecutionPathKey, Backend::Scenario::PreparedPathEligibility>
        &eligibility)
{
    clear();

    const auto records =
        Backend::Application::PathPresentationService()
            .recordsFromPreparedPaths(prepared, actual,
                                      eligibility);
    for (auto record : records)
    {
        appendPathRecord(std::move(record));
    }
    logPredictedPathDiagnostics(m_pathData, m_displayOrder,
                                QStringLiteral("setPreparedPaths"));

    refreshTable();
    m_exportButton->setEnabled(!m_pathData.isEmpty());
}

void ShortestPathsTable::setPathEligibility(
    const QHash<ExecutionPathKey, Backend::Scenario::PreparedPathEligibility>
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

    if (!pathData->eligibility.simulatable
        && !pathData->eligibility.blockingReason.isEmpty())
    {
        return tr("Simulation unavailable: %1")
            .arg(pathData->eligibility.blockingReason);
    }

    if (!pathData->eligibility.simulatable)
        return tr("Simulation unavailable.");

    if (!pathData->eligibility.warningReason.isEmpty())
    {
        return tr("Ready for simulation with warning: %1")
            .arg(pathData->eligibility.warningReason);
    }

    return tr("Ready for simulation.");
}

QString ShortestPathsTable::availabilityBannerText() const
{
    int           unavailableCount = 0;
    QSet<QString> affectedDependencies;

    for (const auto *pathData : m_pathData)
    {
        if (!pathData || !pathData->path || !pathData->isVisible
            || pathData->eligibility.simulatable)
        {
            continue;
        }

        ++unavailableCount;
        const QString reason = pathData->eligibility.blockingReason;
        if (reason.contains(QStringLiteral("ShipNetSim")))
            affectedDependencies.insert(QStringLiteral("ShipNetSim"));
        if (reason.contains(QStringLiteral("NeTrainSim")))
            affectedDependencies.insert(QStringLiteral("NeTrainSim"));
        if (reason.contains(QStringLiteral("TerminalSim")))
            affectedDependencies.insert(QStringLiteral("TerminalSim"));
        if (reason.contains(QStringLiteral("train fleet")))
            affectedDependencies.insert(tr("train fleet"));
        if (reason.contains(QStringLiteral("ship fleet")))
            affectedDependencies.insert(tr("ship fleet"));
        if (reason.contains(QStringLiteral("truck-backed execution")))
        {
            affectedDependencies.insert(
                tr("truck-backed execution"));
        }
    }

    if (unavailableCount == 0)
        return QString();

    QString text =
        unavailableCount == 1
            ? tr("One path is currently unavailable for simulation. ")
            : tr("%1 paths are currently unavailable for simulation. ")
                  .arg(unavailableCount);

    text += tr("Rows marked unavailable remain visible, but they cannot be selected for simulation until the required backends and fleet resources are available.");

    if (!affectedDependencies.isEmpty())
    {
        QStringList dependencies = affectedDependencies.values();
        dependencies.sort();
        text += tr(" Affected dependencies: %1.")
                    .arg(dependencies.join(QStringLiteral(", ")));
    }

    text += tr(" Hover the Status column for the exact requirement.");
    return text;
}

void ShortestPathsTable::updateAvailabilityBanner()
{
    if (!m_availabilityBanner)
        return;

    const QString text = availabilityBannerText();
    m_availabilityBanner->setText(text);
    m_availabilityBanner->setVisible(!text.isEmpty());
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
    const ExecutionPathKey &pathKey, double totalCost, double edgeCost,
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
            auto idItem = m_table->item(row, ColumnPathId);
            if (idItem
                && idItem->data(Qt::UserRole).toString()
                       == pathKey)
            {
                // Update the predicted cost cell if total
                // cost was updated
                if (totalCost >= 0)
                {
                    m_table->item(row, ColumnPredictedCost)->setText(
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
    const ExecutionPathKey &pathKey, double simulationTotalCost,
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
        pathData->simulationTotalCost =
            simulationTotalCost;
    }

    if (simulationEdgeCost >= 0)
    {
        pathData->simulationEdgeCosts =
            simulationEdgeCost;
    }

    if (simulationTerminalCost >= 0)
    {
        pathData->simulationTerminalCosts =
            simulationTerminalCost;
    }

    // Update the table display for this path
    for (int row = 0; row < m_table->rowCount(); ++row)
    {
        // Get the path ID for this row
        auto idItem = m_table->item(row, ColumnPathId);
        if (idItem
            && idItem->data(Qt::UserRole).toString()
                   == pathKey)
        {
            // Update the actual cost cell if simulation
            // total cost was updated
            if (simulationTotalCost >= 0)
            {
                m_table->item(row, ColumnActualCost)->setText(
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
    const ExecutionPathKey &pathKey, const PathData *pathData)
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

    const QVector<ExecutionPathKey> checkedPathKeys =
        getCheckedPathKeys();
    QSet<ExecutionPathKey> checkedSet;
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
        m_table->setCellWidget(row, ColumnSelect, checkboxWidget);
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
                    QVector<ExecutionPathKey> checkedPaths =
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
                    QVector<ExecutionPathKey> checkedPaths =
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

        auto *statusItem = new QTableWidgetItem();
        statusItem->setData(Qt::AccessibleTextRole,
                            statusMessage(pathData->eligibility));
        statusItem->setData(Qt::AccessibleDescriptionRole,
                            rowTooltip);
        statusItem->setToolTip(rowTooltip);
        styleStatusItem(statusItem, pathData->eligibility);
        m_table->setItem(row, ColumnStatus, statusItem);

        // Add Path ID cell
        auto pathItem = new QTableWidgetItem(
            QString::number(pathData->path->getPathId()));
        pathItem->setData(Qt::UserRole, pathKey);
        pathItem->setToolTip(rowTooltip);
        if (!isSelectable(pathData))
            pathItem->setForeground(QBrush(Qt::gray));
        m_table->setItem(row, ColumnPathId, pathItem);

        // Create and add terminal path visualization
        auto pathWidget = createPathRow(pathKey, pathData);
        pathWidget->setToolTip(rowTooltip);
        m_table->setCellWidget(row, ColumnTerminalPath, pathWidget);

        // Store widget pointer in user role for custom
        // delegate
        QVariant widgetPtr;
        widgetPtr.setValue(pathWidget);
        m_table->model()->setData(
            m_table->model()->index(row, ColumnTerminalPath), widgetPtr,
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
        m_table->setItem(row, ColumnPredictedCost, predictedItem);

        // Add Actual Cost cell
        QString actualCostText;
        if (pathData->hasSimulationTotalCost())
        {
            actualCostText = QString::number(
                pathData->simulationTotalCost, 'f',
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
        m_table->setItem(row, ColumnActualCost, actualItem);

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
    updateAvailabilityBanner();
}

/**
 * @brief Retrieves path data for a specific path ID
 * @param pathId The ID of the path to retrieve
 * @return Pointer to the path data or nullptr if not found
 */
const ShortestPathsTable::PathData *
ShortestPathsTable::getDataByPathKey(
    const ExecutionPathKey &pathKey) const
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
ShortestPathsTable::ExecutionPathKey
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
    auto idItem = m_table->item(row, ColumnPathId);
    return idItem ? idItem->data(Qt::UserRole).toString()
                  : QString();
}

/**
 * @brief Gets all path IDs that are currently checked
 * @return Vector of checked path IDs
 */
QVector<ExecutionPathKey>
ShortestPathsTable::getCheckedPathKeys() const
{
    QVector<ExecutionPathKey> checkedPaths;

    // Iterate through all rows in the table
    for (int row = 0; row < m_table->rowCount(); ++row)
    {
        // Get the checkbox widget in the first column
        auto checkboxWidget = m_table->cellWidget(row, ColumnSelect);
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
            auto idItem = m_table->item(row, ColumnPathId);
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
    m_progressByPathKey.clear();
    m_rowByPathKey.clear();
    updateAvailabilityBanner();
}

QList<QJsonObject> ShortestPathsTable::buildComparisonSnapshots(
    const Backend::Scenario::ScenarioDocument &doc) const
{
    QVariantMap costWeights;
    auto &ctl = CargoNetSim::CargoNetSimController::getInstance();
    costWeights = ctl.getCostFunctionWeights();

    const QVector<ExecutionPathKey> checkedPathKeys =
        getCheckedPathKeys();
    QSet<ExecutionPathKey> checkedSet;
    for (const auto &pathKey : checkedPathKeys)
        checkedSet.insert(pathKey);

    QList<Backend::Application::PathPresentationRecord>
        records;
    records.reserve(m_displayOrder.size());
    for (const auto &pathKey : std::as_const(m_displayOrder))
    {
        const PathData *pathData =
            m_pathData.value(pathKey, nullptr);
        if (!pathData || !pathData->path)
            continue;

        Backend::Application::PathPresentationRecord
            record = *pathData;
        record.executionPathKey = pathData->pathKey;
        record.pathKey = pathData->pathKey;
        record.isSelected =
            checkedSet.contains(pathKey);
        record.predictedMetrics = m_predicted.value(pathKey);
        record.actualMetrics = m_actual.value(pathKey);
        records.append(std::move(record));
    }

    return Backend::Application::PathPresentationService()
        .buildComparisonSnapshots(doc, records, costWeights);
}

void ShortestPathsTable::loadComparisonSnapshots(
    const QList<QJsonObject> &snapshots)
{
    clear();

    QSet<ExecutionPathKey> selectedPathKeys;
    const auto records =
        Backend::Application::PathPresentationService()
            .loadComparisonSnapshots(snapshots);
    for (const auto &record : records)
    {
        if (!record.path)
            continue;

        const ExecutionPathKey storedKey =
            appendPathRecord(record);
        if (record.isSelected)
            selectedPathKeys.insert(storedKey);
    }
    logPredictedPathDiagnostics(m_pathData, m_displayOrder,
                                QStringLiteral("loadComparisonSnapshots"));

    refreshTable();
    m_exportButton->setEnabled(!m_pathData.isEmpty());
    m_selectAllButton->setEnabled(!m_pathData.isEmpty());

    for (int row = 0; row < m_table->rowCount(); ++row)
    {
        auto *idItem = m_table->item(row, ColumnPathId);
        if (!idItem)
            continue;
        const QString pathKey =
            idItem->data(Qt::UserRole).toString();
        if (!selectedPathKeys.contains(pathKey))
            continue;
        auto *checkboxWidget = m_table->cellWidget(row, ColumnSelect);
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
    const ExecutionPathKey pathKey = getSelectedPathKey();
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
    QVector<ExecutionPathKey> checkedPaths =
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
    QVector<ExecutionPathKey> checkedPaths =
        getCheckedPathKeys();

    if (checkedPaths.isEmpty())
    {
        // No paths checked, export all visible paths
        exportPathsToPdf(QVector<ExecutionPathKey>());
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
    exportPathsToPdf(QVector<ExecutionPathKey>());

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
        auto checkboxWidget = m_table->cellWidget(row, ColumnSelect);
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
        auto checkboxWidget = m_table->cellWidget(row, ColumnSelect);
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
    const QVector<ExecutionPathKey> &pathKeys)
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
    const QHash<ExecutionPathKey, Backend::Scenario::PathMetrics> &actual)
{
    for (auto it = actual.constBegin();
         it != actual.constEnd(); ++it)
    {
        m_actual.insert(it.key(), it.value());
        if (auto *pathData = m_pathData.value(it.key(), nullptr))
            pathData->actualMetrics = it.value();
        refreshRow(it.key());
    }
}

void ShortestPathsTable::setExecutionResults(
    const Backend::Scenario::ScenarioExecutionResultSet &results)
{
    for (const auto &result : results.pathResults())
    {
        PathData *pathData =
            m_pathData.value(result.executionPathKey, nullptr);
        if (!pathData && !result.canonicalPathKey.isEmpty())
        {
            pathData = m_pathData.value(result.canonicalPathKey, nullptr);
        }
        if (!pathData)
        {
            qCWarning(lcGuiPathTable)
                << "Path key" << result.executionPathKey
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

void ShortestPathsTable::setExecutionProgress(
    const Backend::Scenario::ExecutionProgressSnapshot &snapshot)
{
    m_progressByPathKey.clear();

    for (const auto &progress : snapshot.paths)
    {
        ExecutionPathKey pathKey;
        if (m_pathData.contains(progress.executionPathKey))
        {
            pathKey = progress.executionPathKey;
        }
        else if (!progress.canonicalPathKey.isEmpty()
                 && m_pathData.contains(progress.canonicalPathKey))
        {
            pathKey = progress.canonicalPathKey;
        }

        if (pathKey.isEmpty())
        {
            qCDebug(lcGuiPathTable)
                << "Ignoring execution progress for unknown path"
                << progress.executionPathKey
                << "canonicalPathKey=" << progress.canonicalPathKey;
            continue;
        }

        m_progressByPathKey.insert(pathKey, progress);
    }

    for (const auto &pathKey : std::as_const(m_displayOrder))
        refreshProgressCell(pathKey);
}

void ShortestPathsTable::clearExecutionProgress()
{
    if (m_progressByPathKey.isEmpty())
        return;

    m_progressByPathKey.clear();
    for (const auto &pathKey : std::as_const(m_displayOrder))
        refreshProgressCell(pathKey);
}

ShortestPathsTable::ExecutionPathKey
ShortestPathsTable::appendPathRecord(PathData record)
{
    if (!record.path)
    {
        qCWarning(lcGuiPathTable)
            << "Skipping null path presentation record";
        return {};
    }

    const ExecutionPathKey preferredKey =
        record.executionPathKey.isEmpty()
            ? record.path->canonicalPathKey().isEmpty()
                  ? QStringLiteral("path_id|%1")
                        .arg(record.path->getPathId())
                  : record.path->canonicalPathKey()
            : record.executionPathKey;
    const ExecutionPathKey resolvedKey =
        makeUniquePathKey(preferredKey, m_pathData);

    record.executionPathKey = resolvedKey;
    record.pathKey = resolvedKey;

    m_predicted.insert(resolvedKey, record.predictedMetrics);
    m_actual.insert(resolvedKey, record.actualMetrics);

    auto *pathData = new PathData(std::move(record));
    m_pathData.insert(resolvedKey, pathData);
    m_displayOrder.append(resolvedKey);

    if (resolvedKey != preferredKey)
    {
        qCWarning(lcGuiPathTable)
            << "Execution path key collision detected;"
            << "using compatibility key" << resolvedKey
            << "for preferred execution path key" << preferredKey;
    }

    return resolvedKey;
}

void ShortestPathsTable::refreshProgressCell(
    const ExecutionPathKey &pathKey)
{
    const int row = m_rowByPathKey.value(pathKey, -1);
    if (row < 0)
        return;

    auto *progressBar = qobject_cast<QProgressBar *>(
        m_table->cellWidget(row, ColumnExecutionProgress));
    if (!progressBar)
    {
        progressBar = new QProgressBar(m_table);
        progressBar->setRange(0, 100);
        progressBar->setTextVisible(true);
        progressBar->setAlignment(Qt::AlignCenter);
        m_table->setCellWidget(row, ColumnExecutionProgress,
                               progressBar);
    }

    const auto it = m_progressByPathKey.constFind(pathKey);
    if (it == m_progressByPathKey.constEnd())
    {
        progressBar->setValue(0);
        progressBar->setFormat(tr("Waiting simulation"));
        progressBar->setToolTip(
            tr("No execution progress has been reported for this path yet."));
        progressBar->setEnabled(false);
        return;
    }

    const auto &progress = it.value();
    progressBar->setEnabled(progress.executable);
    progressBar->setValue(qBound(0, qRound(progress.percent), 100));

    QString phase = pathLifecycleLabel(progress.lifecycle);
    if (const auto *activeSegment = activeSegmentFor(progress))
    {
        phase = segmentLifecycleLabel(activeSegment->lifecycle);
    }

    QString summary = phase;
    if (progress.activeSegmentIndex >= 0
        && progress.totalSegments > 0)
    {
        summary = tr("%1 seg %2/%3 %4")
                      .arg(modeLabel(progress.activeMode))
                      .arg(progress.activeSegmentIndex + 1)
                      .arg(progress.totalSegments)
                      .arg(phase);
    }
    else if (!progress.message.isEmpty())
    {
        summary = progress.message;
    }

    progressBar->setFormat(QStringLiteral("%p%  %1").arg(summary));

    QStringList tooltip;
    tooltip << tr("Path progress: %1%")
                   .arg(progress.percent, 0, 'f', 1)
            << tr("Path state: %1")
                   .arg(pathLifecycleLabel(progress.lifecycle));
    if (progress.activeSegmentIndex >= 0)
    {
        tooltip << tr("Active segment: %1 of %2")
                       .arg(progress.activeSegmentIndex + 1)
                       .arg(progress.totalSegments)
                << tr("Mode: %1").arg(modeLabel(progress.activeMode))
                << tr("Network: %1").arg(progress.activeNetworkName)
                << tr("From: %1").arg(progress.activeStartTerminalId)
                << tr("To: %1").arg(progress.activeEndTerminalId);
    }
    if (!progress.message.isEmpty())
        tooltip << tr("Message: %1").arg(progress.message);

    if (!progress.segments.isEmpty())
    {
        tooltip << tr("Segments:");
        for (const auto &segment : progress.segments)
        {
            tooltip << tr("%1. %2 %3 -> %4: %5 (%6%)")
                           .arg(segment.segmentIndex + 1)
                           .arg(modeLabel(segment.mode))
                           .arg(segment.startTerminalId,
                                segment.endTerminalId,
                                segmentLifecycleLabel(
                                    segment.lifecycle))
                           .arg(segment.percent, 0, 'f', 1);
        }
    }

    progressBar->setToolTip(tooltip.join(QLatin1Char('\n')));
}

void ShortestPathsTable::refreshRow(
    const ExecutionPathKey &pathKey)
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

    refreshProgressCell(pathKey);

    // Predicted per-vehicle.
    m_table->setItem(row, ColumnPredictedDistance,
                     cell(p.valid, p.distanceKm));
    m_table->setItem(row, ColumnPredictedTime,
                     cell(p.valid, p.travelTimeHours));
    m_table->setItem(row, ColumnPredictedFuelPerVehicle,
                     cell(p.valid, p.fuelPerVehicle));
    m_table->setItem(row, ColumnPredictedEnergyPerVehicle,
                     cell(p.valid, p.energyPerVehicle));
    m_table->setItem(row, ColumnPredictedCarbonPerVehicle,
                     cell(p.valid, p.carbonPerVehicle));
    m_table->setItem(row, ColumnPredictedRiskPerVehicle,
                     cell(p.valid, p.riskPerVehicle));

    // Actual per-vehicle; no fuel column, see createTableWidget note.
    m_table->setItem(row, ColumnActualDistance,
                     cell(a.valid, a.distanceKm));
    m_table->setItem(row, ColumnActualTime,
                     cell(a.valid, a.travelTimeHours));
    m_table->setItem(row, ColumnActualEnergyPerVehicle,
                     cell(a.valid, a.energyPerVehicle));
    m_table->setItem(row, ColumnActualCarbonPerVehicle,
                     cell(a.valid, a.carbonPerVehicle));
    m_table->setItem(row, ColumnActualRiskPerVehicle,
                     cell(a.valid, a.riskPerVehicle));

    // Preview-demand counts and per-container metrics.
    m_table->setItem(row, ColumnContainers, new QTableWidgetItem(
        p.valid ? QString::number(p.containerCount) : dash));
    auto *vehicleItem = new QTableWidgetItem(
        p.valid ? vehicleBreakdownLabel(p) : dash);
    if (p.valid && !p.previewVehicleBreakdown.isEmpty())
    {
        vehicleItem->setToolTip(
            tr("Per-segment preview vehicle requirements for the path: %1")
                .arg(vehicleBreakdownLabel(p)));
    }
    m_table->setItem(row, ColumnVehicles, vehicleItem);

    m_table->setItem(row, ColumnPredictedFuelPerContainer,
                     cell(p.valid, p.fuelPerContainer));
    m_table->setItem(row, ColumnPredictedEnergyPerContainer,
                     cell(p.valid, p.energyPerContainer));
    m_table->setItem(row, ColumnPredictedCarbonPerContainer,
                     cell(p.valid, p.carbonPerContainer));

    m_table->setItem(row, ColumnActualEnergyPerContainer,
                     cell(a.valid, a.energyPerContainer));
    m_table->setItem(row, ColumnActualCarbonPerContainer,
                     cell(a.valid, a.carbonPerContainer));
}

} // namespace GUI
} // namespace CargoNetSim
