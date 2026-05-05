/**
 * @file PathComparisonDialog.cpp
 * @brief Implementation of the PathComparisonDialog for
 * path comparison
 * @author Ahmed Aredah
 * @date April 18, 2025
 *
 * This file contains the implementation of the
 * PathComparisonDialog class, which provides a UI component
 * for displaying and comparing multiple paths side-by-side
 * in the CargoNetSim system.
 */

#include "PathComparisonDialog.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/GuiApi/ScenarioContractsApi.h"
#include "GUI/Utils/IconCreator.h" // For icon utilities
#include "qcoreapplication.h"
#include <QFileDialog>
#include <QHeaderView>
#include <QMessageBox>
#include <QPainter>
#include <QTextStream>
#include <QtWidgets/qsplitter.h>

namespace CargoNetSim
{
namespace GUI
{

namespace PK = CargoNetSim::Backend::Scenario::PropertyKeys;

PathComparisonDialog::PathComparisonDialog(
    const QList<const PathData *> &pathData,
    QWidget *parent)
    : QDialog(parent)
    , m_pathData(pathData)
    , m_viewModel(pathData)
    , m_tabWidget(nullptr)
    , m_exportButton(nullptr)
{
    qCDebug(lcGuiPathTable) << "PathComparisonDialog::PathComparisonDialog:"
                            << "pathCount=" << pathData.size();
    // Set dialog properties
    setWindowTitle(pathData.size() > 1
                       ? tr("Path Comparison")
                       : tr("Path Details"));
    setMinimumSize(800, 600);

    // Initialize the UI
    initUI();
}

PathComparisonDialog::~PathComparisonDialog()
{
    // Qt automatically cleans up child widgets
}

void PathComparisonDialog::initUI()
{
    qCDebug(lcGuiPathTable) << "PathComparisonDialog::initUI";
    // Create main layout
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    // Create tab widget for organizing comparison views
    m_tabWidget = new QTabWidget(this);

    // Add tabs for different comparison aspects
    m_tabWidget->addTab(createSummaryTab(), tr("Summary"));
    m_tabWidget->addTab(createTerminalsTab(),
                        tr("Terminals"));
    m_tabWidget->addTab(createSegmentsTab(),
                        tr("Segments"));
    m_tabWidget->addTab(createCostsTab(), tr("Costs"));

    mainLayout->addWidget(m_tabWidget);

    // Create button panel
    auto buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    // Add export button
    m_exportButton =
        new QPushButton(tr("Export Comparison"), this);
    m_exportButton->setToolTip(
        tr("Export comparison data to CSV"));
    connect(m_exportButton, &QPushButton::clicked, this,
            &PathComparisonDialog::onExportButtonClicked);

    // Add close button
    auto closeButton = new QPushButton(tr("Close"), this);
    connect(closeButton, &QPushButton::clicked, this,
            &QDialog::accept);

    buttonLayout->addWidget(m_exportButton);
    buttonLayout->addWidget(closeButton);

    mainLayout->addLayout(buttonLayout);
}

QWidget *PathComparisonDialog::createSummaryTab()
{
    qCDebug(lcGuiPathTable) << "PathComparisonDialog::createSummaryTab:"
                            << "pathCount=" << m_pathData.size();
    // Create container widget
    auto container = new QWidget(this);
    auto layout    = new QVBoxLayout(container);

    // Add a header with appropriate text based on number of
    // paths
    QString headerText =
        m_pathData.size() > 1
            ? tr("<h2>Path Comparison Summary</h2>")
            : tr("<h2>Path Details Summary</h2>");

    auto headerLabel = new QLabel(headerText, this);

    headerLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(headerLabel);

    // Create column headers (Path ID 1, Path ID 2, etc.)
    QStringList headers;
    for (const auto &path : m_pathData)
    {
        if (path && path->path)
        {
            headers << m_viewModel.pathSummary(path).pathLabel;
        }
        else
        {
            headers << tr("Unknown Path");
        }
    }

    // Create row labels for the summary data
    QStringList rowLabels = {
        tr("Path ID"),        tr("Total Terminals"),
        tr("Total Segments"), tr("Predicted Cost"),
        tr("Actual Cost"),    tr("Start Terminal"),
        tr("End Terminal")};

    // Populate data for each path
    QList<QStringList> data;
    for (const auto &path : m_pathData)
    {
        QStringList pathData;

        if (path && path->path)
        {
            const auto summary = m_viewModel.pathSummary(path);

            // Path ID
            pathData << QString::number(summary.pathId);

            // Total terminals
            pathData << QString::number(summary.terminalCount);

            // Total segments
            pathData << QString::number(summary.segmentCount);

            // Predicted cost
            pathData << QString::number(
                summary.predictedTotalCost, 'f', 2);

            // Actual cost
            if (path->hasSimulationTotalCost())
            {
                pathData << QString::number(
                    path->simulationTotalCost, 'f',
                    2);
            }
            else
            {
                pathData << tr("Not simulated");
            }

            // Start terminal
            QString startTerminal;
            try
            {
                startTerminal = summary.startTerminalName;
            }
            catch (const std::exception &e)
            {
                qCCritical(lcGuiPathTable) << "PathComparisonDialog::createSummaryTab:"
                                           << "start terminal lookup failed:"
                                           << e.what();
                startTerminal = tr("Unknown");
            }
            pathData << startTerminal;

            // End terminal
            QString endTerminal;
            try
            {
                endTerminal = summary.endTerminalName;
            }
            catch (const std::exception &e)
            {
                qCCritical(lcGuiPathTable) << "PathComparisonDialog::createSummaryTab:"
                                           << "end terminal lookup failed:"
                                           << e.what();
                endTerminal = tr("Unknown");
            }
            pathData << endTerminal;
        }
        else
        {
            // Fill with placeholders if path data is
            // invalid
            for (int i = 0; i < rowLabels.size(); ++i)
            {
                pathData << tr("N/A");
            }
        }

        data.append(pathData);
    }

    // Transpose data for display (paths as columns)
    QList<QStringList> transposedData;
    for (int rowIdx = 0; rowIdx < rowLabels.size();
         ++rowIdx)
    {
        QStringList rowData;
        for (const auto &pathData : data)
        {
            if (rowIdx < pathData.size())
            {
                rowData << pathData[rowIdx];
            }
            else
            {
                rowData << tr("N/A");
            }
        }
        transposedData.append(rowData);
    }

    // Create and add table
    auto table = createComparisonTable(headers, rowLabels,
                                       transposedData);
    layout->addWidget(table);

    // Create path visualization
    auto visualizationLabel =
        new QLabel(tr("<h3>Path Visualization</h3>"), this);
    visualizationLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(visualizationLabel);

    auto visualizationContainer = new QWidget(this);
    createPathVisualization(visualizationContainer);

    auto scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setWidget(visualizationContainer);

    layout->addWidget(scrollArea);

    return container;
}

QWidget *PathComparisonDialog::createTerminalsTab()
{
    qCDebug(lcGuiPathTable) << "PathComparisonDialog::createTerminalsTab:"
                            << "pathCount=" << m_pathData.size();
    auto container = new QWidget(this);
    auto layout    = new QVBoxLayout(container);

    auto headerLabel = new QLabel(
        tr("<h2>Terminal Comparison</h2>"), this);
    headerLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(headerLabel);

    auto noteLabel = new QLabel(
        tr("Actual terminal values below come from recorded "
           "arrival-side handling events. Terminals without an "
           "arrival event in the selected run are shown as "
           "\"Not modeled\"."),
        this);
    noteLabel->setWordWrap(true);
    layout->addWidget(noteLabel);

    QStringList headers;
    for (const auto &path : m_pathData)
    {
        if (path && path->path)
        {
            headers << m_viewModel.pathSummary(path).pathLabel;
        }
        else
        {
            headers << tr("Unknown Path");
        }
    }

    const int maxTerminals = m_viewModel.maxTerminals();

    QStringList rowLabels;
    for (int i = 0; i < maxTerminals; ++i)
    {
        rowLabels << tr("Terminal %1").arg(i + 1);
    }

    QList<QStringList> data;
    for (const auto &path : m_pathData)
    {
        QStringList terminalData;

        if (path && path->path)
        {
            const auto terminals =
                m_viewModel.terminalEntries(path);

            // Add terminal information
            for (int i = 0; i < maxTerminals; ++i)
            {
                if (i < terminals.size())
                {
                    terminalData << terminals[i].displayName;
                }
                else
                {
                    terminalData << tr("-");
                }
            }
        }
        else
        {
            // Fill with placeholders if path data is
            // invalid
            for (int i = 0; i < maxTerminals; ++i)
            {
                terminalData << tr("N/A");
            }
        }

        data.append(terminalData);
    }

    QList<QStringList> transposedData;
    for (int rowIdx = 0; rowIdx < maxTerminals; ++rowIdx)
    {
        QStringList rowData;
        for (const auto &terminalData : data)
        {
            if (rowIdx < terminalData.size())
            {
                rowData << terminalData[rowIdx];
            }
            else
            {
                rowData << tr("-");
            }
        }
        transposedData.append(rowData);
    }

    auto terminalTabWidget = new QTabWidget(this);

    auto basicInfoWidget = new QWidget(this);
    auto basicInfoLayout = new QVBoxLayout(basicInfoWidget);
    auto table = createComparisonTable(
        headers, rowLabels, transposedData);
    basicInfoLayout->addWidget(table);
    terminalTabWidget->addTab(basicInfoWidget,
                              tr("Basic Info"));

    for (int terminalIdx = 0; terminalIdx < maxTerminals;
         ++terminalIdx)
    {
        auto attributeWidget = new QWidget(this);
        auto attributeLayout =
            new QVBoxLayout(attributeWidget);
        auto splitter = new QSplitter(Qt::Vertical, this);

        auto terminalInfoWidget = new QWidget(this);
        auto terminalInfoLayout =
            new QVBoxLayout(terminalInfoWidget);

        QString terminalInfoText =
            tr("<h3>Terminal %1 Attributes</h3>")
                .arg(terminalIdx + 1);
        for (const auto &path : m_pathData)
        {
            if (!(path && path->path))
                continue;

            const auto terminals =
                m_viewModel.terminalEntries(path);
            if (terminalIdx >= terminals.size())
                continue;

            const auto &terminal = terminals[terminalIdx];
            QString predictedNote;
            if (terminal.predictedCostsSkipped
                && !terminal.predictedSkipReason.isEmpty())
            {
                predictedNote =
                    tr(" (prediction note: %1)")
                        .arg(terminal.predictedSkipReason.toHtmlEscaped());
            }

            terminalInfoText +=
                QStringLiteral("<p><b>%1:</b> %2%3</p>")
                    .arg(m_viewModel.pathLabel(path),
                         terminal.displayName.toHtmlEscaped(),
                         predictedNote);
        }

        auto infoLabel = new QLabel(terminalInfoText, this);
        infoLabel->setAlignment(Qt::AlignCenter);
        infoLabel->setWordWrap(true);
        terminalInfoLayout->addWidget(infoLabel);

        auto tableWidget = new QWidget(this);
        auto tableLayout = new QVBoxLayout(tableWidget);

        const QStringList attributeRowLabels = {
            tr("Handling Time (Predicted, s)"),
            tr("Handling Time (Actual, s)"),
            tr("Raw Direct Tariff (Predicted, USD)"),
            tr("Direct Cost (Predicted, USD)"),
            tr("Direct Cost (Actual, USD)"),
            tr("Weighted Delay Contribution (Predicted)"),
            tr("Weighted Delay Contribution (Actual)"),
            tr("Weighted Cost Contribution (Predicted)"),
            tr("Weighted Cost Contribution (Actual)"),
            tr("Weighted Total Contribution (Predicted)"),
            tr("Weighted Total Contribution (Actual)"),
            tr("Arrival Mode (Actual)"),
            tr("Dropped Containers (Actual)"),
            tr("Arrival Events (Actual)"),
            tr("Utilization At Arrival (Actual)"),
            tr("Congestion At Arrival (Actual)"),
            tr("Delay Multiplier At Arrival (Actual)")};

        QList<QStringList> attributeData;
        for (const auto &path : m_pathData)
        {
            QStringList pathAttributeData;
            const auto terminals =
                path ? m_viewModel.terminalEntries(path)
                     : QList<PathComparisonViewModel::TerminalEntry>{};
            if (path && terminalIdx < terminals.size())
            {
                const auto &displayValues =
                    terminals[terminalIdx].displayValues;

                pathAttributeData
                    << (displayValues.predictedAvailable
                            ? QString::number(
                                  displayValues
                                      .predictedHandlingSeconds,
                                  'f', 0)
                            : tr("N/A"));
                pathAttributeData
                    << (displayValues.actualAvailable
                            ? QString::number(
                                  displayValues
                                      .actualTotalHandlingSeconds,
                                  'f', 0)
                            : tr("Not modeled"));
                pathAttributeData
                    << (displayValues.predictedAvailable
                            ? QString::number(
                                  displayValues
                                      .predictedRawDirectCostUsd,
                                  'f', 2)
                            : tr("N/A"));
                pathAttributeData
                    << (displayValues.predictedAvailable
                            ? QString::number(
                                  displayValues
                                      .predictedDirectCostUsd,
                                  'f', 2)
                            : tr("N/A"));
                pathAttributeData
                    << (displayValues.actualAvailable
                            ? QString::number(
                                  displayValues
                                      .actualDirectCostUsd,
                                  'f', 2)
                            : tr("Not modeled"));
                pathAttributeData
                    << (displayValues.predictedAvailable
                            ? QString::number(
                                  displayValues
                                      .predictedWeightedDelayContribution,
                                  'f', 2)
                            : tr("N/A"));
                pathAttributeData
                    << (displayValues.actualAvailable
                            ? QString::number(
                                  displayValues
                                      .actualWeightedDelayContribution,
                                  'f', 2)
                            : tr("Not modeled"));
                pathAttributeData
                    << (displayValues.predictedAvailable
                            ? QString::number(
                                  displayValues
                                      .predictedWeightedCostContribution,
                                  'f', 2)
                            : tr("N/A"));
                pathAttributeData
                    << (displayValues.actualAvailable
                            ? QString::number(
                                  displayValues
                                      .actualWeightedCostContribution,
                                  'f', 2)
                            : tr("Not modeled"));
                pathAttributeData
                    << (displayValues.predictedAvailable
                            ? QString::number(
                                  displayValues
                                      .predictedWeightedTotalContribution,
                                  'f', 2)
                            : tr("N/A"));
                pathAttributeData
                    << (displayValues.actualAvailable
                            ? QString::number(
                                  displayValues
                                      .actualWeightedTotalContribution,
                                  'f', 2)
                            : tr("Not modeled"));
                pathAttributeData
                    << (displayValues.actualAvailable
                            ? Backend::TransportationTypes::toString(
                                  displayValues.actualArrivalMode)
                            : tr("Not modeled"));
                pathAttributeData
                    << (displayValues.actualAvailable
                            ? QString::number(
                                  displayValues
                                      .actualDroppedContainers)
                            : tr("Not modeled"));
                pathAttributeData
                    << (displayValues.actualAvailable
                            ? QString::number(
                                  displayValues
                                      .actualArrivalEvents)
                            : tr("Not modeled"));
                pathAttributeData
                    << (displayValues.actualAvailable
                            ? QString::number(
                                  displayValues
                                      .actualUtilizationAtArrival,
                                  'f', 3)
                            : tr("Not modeled"));
                pathAttributeData
                    << (displayValues.actualAvailable
                            ? QString::number(
                                  displayValues
                                      .actualCongestionAtArrival,
                                  'f', 3)
                            : tr("Not modeled"));
                pathAttributeData
                    << (displayValues.actualAvailable
                            ? QString::number(
                                  displayValues
                                      .actualDelayMultiplierAtArrival,
                                  'f', 3)
                            : tr("Not modeled"));
            }
            else
            {
                for (int i = 0;
                     i < attributeRowLabels.size(); ++i)
                {
                    pathAttributeData << tr("N/A");
                }
            }

            attributeData.append(pathAttributeData);
        }

        QList<QStringList> transposedAttributeData;
        for (int rowIdx = 0;
             rowIdx < attributeRowLabels.size(); ++rowIdx)
        {
            QStringList rowData;
            for (const auto &attrData : attributeData)
            {
                rowData << (rowIdx < attrData.size()
                                ? attrData[rowIdx]
                                : tr("N/A"));
            }
            transposedAttributeData.append(rowData);
        }

        auto attributeTable = createComparisonTable(
            headers, attributeRowLabels,
            transposedAttributeData);
        tableLayout->addWidget(attributeTable);

        splitter->addWidget(terminalInfoWidget);
        splitter->addWidget(tableWidget);
        splitter->setSizes({150, 420});
        attributeLayout->addWidget(splitter);

        terminalTabWidget->addTab(
            attributeWidget,
            tr("Terminal %1 Attributes").arg(terminalIdx + 1));
    }

    layout->addWidget(terminalTabWidget);

    return container;
}

QWidget *PathComparisonDialog::createSegmentsTab()
{
    qCDebug(lcGuiPathTable) << "PathComparisonDialog::createSegmentsTab:"
                            << "pathCount=" << m_pathData.size();
    // Create container widget
    auto container = new QWidget(this);
    auto layout    = new QVBoxLayout(container);

    // Add a header
    auto headerLabel =
        new QLabel(tr("<h2>Segment Comparison</h2>"), this);
    headerLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(headerLabel);

    auto noteLabel = new QLabel(
        tr("Predicted physical segment metrics are raw vehicle/segment "
           "totals. Predicted cost rows use the allocated shipment "
           "cost model used by the path summary."),
        this);
    noteLabel->setWordWrap(true);
    layout->addWidget(noteLabel);

    // Create column headers (Path ID 1, Path ID 2, etc.)
    QStringList headers;
    for (const auto &path : m_pathData)
    {
        if (path && path->path)
        {
            headers << m_viewModel.pathSummary(path).pathLabel;
        }
        else
        {
            headers << tr("Unknown Path");
        }
    }

    // Find the maximum number of segments across all paths
    const int maxSegments = m_viewModel.maxSegments();

    // Create tab container for segments with attributes
    auto segmentTabWidget = new QTabWidget(this);

    // Create basic segment info tab
    auto basicInfoWidget = new QWidget(this);
    auto basicInfoLayout = new QVBoxLayout(basicInfoWidget);

    // Create row labels for segment indices
    QStringList rowLabels;
    for (int i = 0; i < maxSegments; ++i)
    {
        rowLabels << tr("Segment %1").arg(i + 1);
    }

    // Populate segment data for each path
    QList<QStringList> data;
    for (const auto &path : m_pathData)
    {
        QStringList segmentData;

        if (path && path->path)
        {
            const auto segments =
                m_viewModel.segmentEntries(path);

            // Add segment information
            for (int i = 0; i < maxSegments; ++i)
            {
                if (i < segments.size())
                {
                    segmentData << segments[i].description;
                }
                else
                {
                    segmentData << tr("-");
                }
            }
        }
        else
        {
            // Fill with placeholders if path data is
            // invalid
            for (int i = 0; i < maxSegments; ++i)
            {
                segmentData << tr("N/A");
            }
        }

        data.append(segmentData);
    }

    // Transpose data for display (paths as columns)
    QList<QStringList> transposedData;
    for (int rowIdx = 0; rowIdx < maxSegments; ++rowIdx)
    {
        QStringList rowData;
        for (const auto &segmentData : data)
        {
            if (rowIdx < segmentData.size())
            {
                rowData << segmentData[rowIdx];
            }
            else
            {
                rowData << tr("-");
            }
        }
        transposedData.append(rowData);
    }

    // Create and add table
    qCDebug(lcGuiPathTable) << "PathComparisonDialog::createSegmentsTab: rows=" << rowLabels.size() << "cols=" << headers.size();
    auto table = createComparisonTable(headers, rowLabels,
                                       transposedData);
    basicInfoLayout->addWidget(table);

    segmentTabWidget->addTab(basicInfoWidget,
                             tr("Basic Info"));

    // Create attribute tabs for each segment
    for (int segmentIdx = 0; segmentIdx < maxSegments;
         ++segmentIdx)
    {
        auto attributeWidget = new QWidget(this);
        auto attributeLayout =
            new QVBoxLayout(attributeWidget);

        // Create a splitter to allow collapsing segment
        // info
        auto splitter = new QSplitter(Qt::Vertical, this);

        // Top part - segment info widget
        auto segmentInfoWidget = new QWidget(this);
        auto segmentInfoLayout =
            new QVBoxLayout(segmentInfoWidget);

        // Add segment info header with details
        QString segmentInfoText =
            tr("<h3>Segment %1 Attributes</h3>")
                .arg(segmentIdx + 1);

        // Add path-specific segment details
        for (const auto &path : m_pathData)
        {
            if (path && path->path)
            {
                const auto segments =
                    m_viewModel.segmentEntries(path);
                if (segmentIdx < segments.size())
                {
                    const auto &segment = segments[segmentIdx];
                    QString segmentInfo =
                        QString("<p><b>Path %1:</b> %2 → "
                                "%3 (%4)</p>")
                            .arg(m_viewModel.pathSummary(path).pathId)
                            .arg(segment.startTerminalName)
                            .arg(segment.endTerminalName)
                            .arg(segment.modeName);

                    segmentInfoText += segmentInfo;
                }
            }
        }

        auto infoLabel = new QLabel(segmentInfoText, this);
        infoLabel->setAlignment(Qt::AlignCenter);
        segmentInfoLayout->addWidget(infoLabel);

        // Bottom part - table widget
        auto tableWidget = new QWidget(this);
        auto tableLayout = new QVBoxLayout(tableWidget);

        // Create attribute headers
        QStringList attributeHeaders = headers;

        // Create row labels for attributes
        QStringList attributeRowLabels = {
            tr("Carbon Emissions (Vehicle Predicted, t)"),
            tr("Carbon Emissions (Allocated Predicted, t)"),
            tr("Carbon Emissions (t, Actual)"),
            tr("Direct Cost (Allocated Predicted, USD)"),
            tr("Direct Cost (Actual)"),
            tr("Distance (Predicted, km)"),
            tr("Distance (km, Actual)"),
            tr("Energy Consumption (Vehicle Predicted, kWh)"),
            tr("Energy Consumption (Allocated Predicted, kWh)"),
            tr("Energy Consumption (kWh, Actual)"),
            tr("Risk (Vehicle Predicted)"),
            tr("Risk (Actual)"),
            tr("Travel Time (Predicted, hr)"),
            tr("Travel Time (hr, Actual)")};

        // Populate data for each attribute
        QList<QStringList> attributeData;

        for (const auto &path : m_pathData)
        {
            QStringList pathAttributeData;

            if (path && path->path)
            {
                const auto segments =
                    m_viewModel.segmentEntries(path);

                if (segmentIdx < segments.size())
                {
                    const auto &segment = segments[segmentIdx];
                    const auto &displayValues =
                        segment.displayValues;
                    const auto &predictedCosts =
                        segment.predictedCosts;
                    const auto &actualCosts =
                        segment.actualCosts;

                    // Carbon Emissions
                    pathAttributeData
                        << (displayValues.predictedAvailable
                                ? QString::number(
                                      displayValues
                                          .predictedCarbonEmissions,
                                      'f', 3)
                                : tr("N/A"));
                    pathAttributeData
                        << (displayValues.predictedAvailable
                                ? QString::number(
                                      displayValues
                                          .predictedAllocatedCarbonEmissions,
                                      'f', 3)
                                : tr("N/A"));
                    pathAttributeData
                        << (displayValues.actualAvailable
                                ? QString::number(
                                      displayValues
                                          .actualCarbonEmissions,
                                      'f', 3)
                                : tr("N/A"));

                    // Direct Cost
                    pathAttributeData
                        << (predictedCosts.available
                                ? QString::number(
                                      predictedCosts.directCost,
                                      'f', 2)
                                : tr("N/A"));
                    pathAttributeData
                        << (actualCosts.available
                                ? QString::number(
                                      actualCosts.directCost,
                                      'f', 2)
                                : tr("N/A"));

                    // Distance
                    pathAttributeData
                        << (displayValues.predictedAvailable
                                ? QString::number(
                                      displayValues.predictedDistanceKm,
                                      'f', 2)
                                : tr("N/A"));
                    pathAttributeData
                        << (displayValues.actualAvailable
                                ? QString::number(
                                      displayValues.actualDistanceKm,
                                      'f', 2)
                                : tr("N/A"));

                    // Energy Consumption
                    pathAttributeData
                        << (displayValues.predictedAvailable
                                ? QString::number(
                                      displayValues
                                          .predictedEnergyConsumption,
                                      'f', 2)
                                : tr("N/A"));
                    pathAttributeData
                        << (displayValues.predictedAvailable
                                ? QString::number(
                                      displayValues
                                          .predictedAllocatedEnergyConsumption,
                                      'f', 2)
                                : tr("N/A"));
                    pathAttributeData
                        << (displayValues.actualAvailable
                                ? QString::number(
                                      displayValues
                                          .actualEnergyConsumption,
                                      'f', 2)
                                : tr("N/A"));

                    // Risk
                    pathAttributeData
                        << (displayValues.predictedAvailable
                                ? QString::number(
                                      displayValues.predictedRisk,
                                      'f', 6)
                                : tr("N/A"));
                    pathAttributeData
                        << (displayValues.actualAvailable
                                ? QString::number(
                                      displayValues.actualRisk,
                                      'f', 6)
                                : tr("N/A"));

                    // Travel Time
                    pathAttributeData
                        << (displayValues.predictedAvailable
                                ? QString::number(
                                      displayValues
                                          .predictedTravelTimeHours,
                                      'f', 2)
                                : tr("N/A"));
                    pathAttributeData
                        << (displayValues.actualAvailable
                                ? QString::number(
                                      displayValues
                                          .actualTravelTimeHours,
                                      'f', 2)
                                : tr("N/A"));
                }
                else
                {
                    // Fill empty data for all attributes
                    for (int i = 0;
                         i < attributeRowLabels.size(); ++i)
                    {
                        pathAttributeData << tr("N/A");
                    }
                }
            }
            else
            {
                // Fill with placeholders for invalid path
                for (int i = 0;
                     i < attributeRowLabels.size(); ++i)
                {
                    pathAttributeData << tr("N/A");
                }
            }

            attributeData.append(pathAttributeData);
        }

        // Transpose attribute data
        QList<QStringList> transposedAttributeData;
        for (int rowIdx = 0;
             rowIdx < attributeRowLabels.size(); ++rowIdx)
        {
            QStringList rowData;
            for (const auto &attrData : attributeData)
            {
                if (rowIdx < attrData.size())
                {
                    rowData << attrData[rowIdx];
                }
                else
                {
                    rowData << tr("N/A");
                }
            }
            transposedAttributeData.append(rowData);
        }

        // Create and add table to the table widget layout
        auto attributeTable = createComparisonTable(
            attributeHeaders, attributeRowLabels,
            transposedAttributeData);
        tableLayout->addWidget(attributeTable);

        // Add widgets to splitter
        splitter->addWidget(segmentInfoWidget);
        splitter->addWidget(tableWidget);

        // Set initial sizes for the splitter
        QList<int> sizes;
        sizes << 150
              << 400; // Adjust these values as needed
        splitter->setSizes(sizes);

        // Add splitter to main attribute layout
        attributeLayout->addWidget(splitter);

        segmentTabWidget->addTab(attributeWidget,
                                 tr("Segment %1 Attributes")
                                     .arg(segmentIdx + 1));
    }

    layout->addWidget(segmentTabWidget);

    return container;
}

QWidget *PathComparisonDialog::createCostsTab()
{
    qCDebug(lcGuiPathTable) << "PathComparisonDialog::createCostsTab:"
                            << "pathCount=" << m_pathData.size();
    // Create container widget
    auto container = new QWidget(this);
    auto layout    = new QVBoxLayout(container);

    // Add a header
    auto headerLabel =
        new QLabel(tr("<h2>Cost Comparison</h2>"), this);
    headerLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(headerLabel);

    auto noteLabel = new QLabel(
        tr("Predicted cost breakdowns are allocated using the selected "
           "path demand, vehicle capacity, and configured cost weights. "
           "They should reconcile with the predicted edge cost in the "
           "summary."),
        this);
    noteLabel->setWordWrap(true);
    layout->addWidget(noteLabel);

    // Create tab widget for different cost breakdowns
    auto costTabWidget = new QTabWidget(this);

    // --- Create summary cost tab ---
    auto summaryWidget = new QWidget(this);
    auto summaryLayout = new QVBoxLayout(summaryWidget);

    // Create column headers (Path ID 1, Path ID 2, etc.)
    QStringList headers;
    for (const auto &path : m_pathData)
    {
        if (path && path->path)
        {
            headers << m_viewModel.pathSummary(path).pathLabel;
        }
        else
        {
            headers << tr("Unknown Path");
        }
    }

    // Create row labels for cost categories
    QStringList rowLabels = {
        tr("Predicted Total Cost"),
        tr("Predicted Edge Cost"),
        tr("Predicted Terminal Cost"),
        tr("Simulated Total Cost"),
        tr("Simulated Edge Cost"),
        tr("Simulated Terminal Cost"),
        tr("Cost Difference (%)") // Percentage difference
                                  // between predicted and
                                  // simulated
    };

    // Populate cost data for each path
    QList<QStringList> data;
    for (const auto &path : m_pathData)
    {
        QStringList costData;

        if (path && path->path)
        {
            const auto summary = m_viewModel.pathSummary(path);
            // Add predicted costs
            costData << QString::number(
                summary.predictedTotalCost, 'f', 2);
            costData << QString::number(
                summary.predictedEdgeCost, 'f', 2);
            costData << QString::number(
                summary.predictedTerminalCost, 'f',
                2);

            // Add simulated costs
            if (path->hasSimulationTotalCost())
            {
                costData << QString::number(
                    path->simulationTotalCost, 'f',
                    2);
            }
            else
            {
                costData << tr("Not simulated");
            }

            if (path->hasSimulationEdgeCosts())
            {
                costData << QString::number(
                    path->simulationEdgeCosts, 'f',
                    2);
            }
            else
            {
                costData << tr("Not simulated");
            }

            if (path->hasSimulationTerminalCosts())
            {
                costData << QString::number(
                    path->simulationTerminalCosts, 'f',
                    2);
            }
            else
            {
                costData << tr("Not simulated");
            }

            // Calculate percentage difference
            if (path->hasSimulationTotalCost()
                && summary.predictedTotalCost > 0)
            {
                double predictedCost =
                    summary.predictedTotalCost;
                double simulatedCost =
                    path->simulationTotalCost;
                double difference =
                    ((simulatedCost - predictedCost)
                     / predictedCost)
                    * 100.0;

                // Format with + sign for positive
                // differences
                if (difference > 0)
                {
                    costData << QString("+%1%").arg(
                        difference, 0, 'f', 2);
                }
                else
                {
                    costData << QString("%1%").arg(
                        difference, 0, 'f', 2);
                }
            }
            else
            {
                costData << tr("N/A");
            }
        }
        else
        {
            // Fill with placeholders if path data is
            // invalid
            for (int i = 0; i < rowLabels.size(); ++i)
            {
                costData << tr("N/A");
            }
        }

        data.append(costData);
    }

    // Transpose data for display (paths as columns)
    QList<QStringList> transposedData;
    for (int rowIdx = 0; rowIdx < rowLabels.size();
         ++rowIdx)
    {
        QStringList rowData;
        for (const auto &costData : data)
        {
            if (rowIdx < costData.size())
            {
                rowData << costData[rowIdx];
            }
            else
            {
                rowData << tr("N/A");
            }
        }
        transposedData.append(rowData);
    }

    // Create and add table
    qCDebug(lcGuiPathTable) << "PathComparisonDialog::createCostsTab: rows=" << rowLabels.size() << "cols=" << headers.size();
    auto table = createComparisonTable(headers, rowLabels,
                                       transposedData);
    summaryLayout->addWidget(table);

    costTabWidget->addTab(summaryWidget, tr("Summary"));

    // --- Create detailed cost breakdown tab ---
    auto detailedWidget = new QWidget(this);
    auto detailedLayout = new QVBoxLayout(detailedWidget);

    // Create row labels for detailed cost breakdown
    QStringList detailedRowLabels = {
        tr("Carbon Emissions Cost (Allocated Predicted)"),
        tr("Carbon Emissions Cost (Actual)"),
        tr("Direct Cost (Allocated Predicted)"),
        tr("Direct Cost (Actual)"),
        tr("Distance-based Cost (Allocated Predicted)"),
        tr("Distance-based Cost (Actual)"),
        tr("Energy Consumption Cost (Allocated Predicted)"),
        tr("Energy Consumption Cost (Actual)"),
        tr("Risk-based Cost (Allocated Predicted)"),
        tr("Risk-based Cost (Actual)"),
        tr("Travel Time Cost (Allocated Predicted)"),
        tr("Travel Time Cost (Actual)")};

    // For each path, accumulate the cost breakdown across
    // all segments
    QList<QStringList> detailedData;

    for (const auto &path : m_pathData)
    {
        QStringList pathDetailedData;

        if (path && path->path)
        {
            const auto totals = m_viewModel.pathCostTotals(path);
            const auto &predicted = totals.predicted;
            const auto &actual = totals.actual;
            const bool hasPredictedData = predicted.available;
            const bool hasActualData = actual.available;

            // Add predicted costs
            pathDetailedData
                << (hasPredictedData
                        ? QString::number(
                              predicted.carbonEmissions, 'f', 2)
                        : tr("N/A"));
            pathDetailedData
                << (hasActualData
                        ? QString::number(
                              actual.carbonEmissions,
                              'f', 2)
                        : tr("Not simulated"));

            pathDetailedData
                << (hasPredictedData
                        ? QString::number(
                              predicted.directCost, 'f', 2)
                        : tr("N/A"));
            pathDetailedData
                << (hasActualData
                        ? QString::number(actual.directCost,
                                          'f', 2)
                        : tr("Not simulated"));

            pathDetailedData
                << (hasPredictedData
                        ? QString::number(predicted.distance,
                                          'f', 2)
                        : tr("N/A"));
            pathDetailedData
                << (hasActualData
                        ? QString::number(
                              actual.distance, 'f', 2)
                        : tr("Not simulated"));

            pathDetailedData
                << (hasPredictedData
                        ? QString::number(
                              predicted.energyConsumption,
                              'f', 2)
                        : tr("N/A"));
            pathDetailedData
                << (hasActualData
                        ? QString::number(actual.energyConsumption,
                                          'f', 2)
                        : tr("Not simulated"));

            pathDetailedData
                << (hasPredictedData
                        ? QString::number(predicted.risk, 'f', 2)
                        : tr("N/A"));
            pathDetailedData
                << (hasActualData
                        ? QString::number(actual.risk,
                                          'f', 2)
                        : tr("Not simulated"));

            pathDetailedData
                << (hasPredictedData
                        ? QString::number(
                              predicted.travelTime, 'f', 2)
                        : tr("N/A"));
            pathDetailedData
                << (hasActualData
                        ? QString::number(actual.travelTime,
                                          'f', 2)
                        : tr("Not simulated"));
        }
        else
        {
            // Fill with placeholders for invalid path
            for (int i = 0; i < detailedRowLabels.size();
                 ++i)
            {
                pathDetailedData << tr("N/A");
            }
        }

        detailedData.append(pathDetailedData);
    }

    // Transpose detailed data
    QList<QStringList> transposedDetailedData;
    for (int rowIdx = 0; rowIdx < detailedRowLabels.size();
         ++rowIdx)
    {
        QStringList rowData;
        for (const auto &detData : detailedData)
        {
            if (rowIdx < detData.size())
            {
                rowData << detData[rowIdx];
            }
            else
            {
                rowData << tr("N/A");
            }
        }
        transposedDetailedData.append(rowData);
    }

    auto detailedTable = createComparisonTable(
        headers, detailedRowLabels, transposedDetailedData);
    detailedLayout->addWidget(detailedTable);

    costTabWidget->addTab(detailedWidget,
                          tr("Cost Breakdown"));

    // --- Create segment-level cost breakdown tabs ---
    // Find the maximum number of segments across all paths
    const int maxSegments = m_viewModel.maxSegments();

    // Create a tab for each segment
    for (int segmentIdx = 0; segmentIdx < maxSegments;
         ++segmentIdx)
    {
        auto segmentWidget = new QWidget(this);
        auto segmentLayout = new QVBoxLayout(segmentWidget);

        // Create a splitter to allow collapsing segment
        // info
        auto splitter = new QSplitter(Qt::Vertical, this);

        // Top part - segment info widget
        auto segmentInfoWidget = new QWidget(this);
        auto segmentInfoLayout =
            new QVBoxLayout(segmentInfoWidget);

        // Add segment info at the top of the tab
        QString segmentInfoText =
            tr("<h3>Segment %1 Costs</h3>")
                .arg(segmentIdx + 1);
        for (const auto &path : m_pathData)
        {
            if (path && path->path)
            {
                const auto summary =
                    m_viewModel.pathSummary(path);
                const auto segments =
                    m_viewModel.segmentEntries(path);
                if (segmentIdx < segments.size())
                {
                    QString segmentInfo =
                        QString("<p><b>Path %1:</b> %2 → "
                                "%3 (%4)</p>")
                            .arg(summary.pathId)
                            .arg(segments[segmentIdx]
                                     .startTerminalName)
                            .arg(segments[segmentIdx]
                                     .endTerminalName)
                            .arg(segments[segmentIdx].modeName);
                    segmentInfoText += segmentInfo;
                }
            }
        }

        auto infoLabel = new QLabel(segmentInfoText, this);
        infoLabel->setAlignment(Qt::AlignCenter);
        segmentInfoLayout->addWidget(infoLabel);

        // Bottom part - table widget
        auto tableWidget = new QWidget(this);
        auto tableLayout = new QVBoxLayout(tableWidget);

        // Row labels for segment costs
        QStringList segmentCostRowLabels = {
            tr("Carbon Emissions Cost (Allocated Predicted)"),
            tr("Carbon Emissions Cost (Actual)"),
            tr("Direct Cost (Allocated Predicted)"),
            tr("Direct Cost (Actual)"),
            tr("Distance-based Cost (Allocated Predicted)"),
            tr("Distance-based Cost (Actual)"),
            tr("Energy Consumption Cost (Allocated Predicted)"),
            tr("Energy Consumption Cost (Actual)"),
            tr("Risk-based Cost (Allocated Predicted)"),
            tr("Risk-based Cost (Actual)"),
            tr("Travel Time Cost (Allocated Predicted)"),
            tr("Travel Time Cost (Actual)")};

        // Populate data for each path's segment
        QList<QStringList> segmentCostData;

        for (const auto &path : m_pathData)
        {
            QStringList pathSegmentData;

            if (path && path->path)
            {
                const auto segments =
                    m_viewModel.segmentEntries(path);

                if (segmentIdx < segments.size())
                {
                    const auto estimatedCostObj =
                        m_viewModel.segmentPredictedCosts(
                            path, segmentIdx);
                    const auto actualCostObj =
                        m_viewModel.segmentActualCosts(
                            path, segmentIdx);

                    // Carbon Emissions Cost
                    pathSegmentData
                        << (estimatedCostObj.available
                                ? QString::number(
                                      estimatedCostObj
                                          .carbonEmissions,
                                      'f', 2)
                                : tr("N/A"));
                    pathSegmentData
                        << (actualCostObj.available
                                ? QString::number(
                                      actualCostObj
                                          .carbonEmissions,
                                      'f', 2)
                                : tr("N/A"));

                    // Direct Cost
                    pathSegmentData
                        << (estimatedCostObj.available
                                ? QString::number(
                                      estimatedCostObj
                                          .directCost,
                                      'f', 2)
                                : tr("N/A"));
                    pathSegmentData
                        << (actualCostObj.available
                                ? QString::number(
                                      actualCostObj.directCost,
                                      'f', 2)
                                : tr("N/A"));

                    // Distance-based Cost
                    pathSegmentData
                        << (estimatedCostObj.available
                                ? QString::number(
                                      estimatedCostObj.distance,
                                      'f', 2)
                                : tr("N/A"));
                    pathSegmentData
                        << (actualCostObj.available
                                ? QString::number(
                                      actualCostObj.distance,
                                      'f', 2)
                                : tr("N/A"));

                    // Energy Consumption Cost
                    pathSegmentData
                        << (estimatedCostObj.available
                                ? QString::number(
                                      estimatedCostObj
                                          .energyConsumption,
                                      'f', 2)
                                : tr("N/A"));
                    pathSegmentData
                        << (actualCostObj.available
                                ? QString::number(
                                      actualCostObj
                                          .energyConsumption,
                                      'f', 2)
                                : tr("N/A"));

                    // Risk Cost
                    pathSegmentData
                        << (estimatedCostObj.available
                                ? QString::number(
                                      estimatedCostObj.risk,
                                      'f', 6)
                                : tr("N/A"));
                    pathSegmentData
                        << (actualCostObj.available
                                ? QString::number(
                                      actualCostObj.risk,
                                      'f', 6)
                                : tr("N/A"));

                    // Travel Time Cost
                    pathSegmentData
                        << (estimatedCostObj.available
                                ? QString::number(
                                      estimatedCostObj.travelTime,
                                      'f', 2)
                                : tr("N/A"));
                    pathSegmentData
                        << (actualCostObj.available
                                ? QString::number(
                                      actualCostObj.travelTime,
                                      'f', 2)
                                : tr("N/A"));
                }
                else
                {
                    // Fill with placeholders for missing
                    // segment
                    for (int i = 0;
                         i < segmentCostRowLabels.size();
                         ++i)
                    {
                        pathSegmentData << tr("-");
                    }
                }
            }
            else
            {
                // Fill with placeholders for invalid path
                for (int i = 0;
                     i < segmentCostRowLabels.size(); ++i)
                {
                    pathSegmentData << tr("N/A");
                }
            }

            segmentCostData.append(pathSegmentData);
        }

        // Transpose data for display
        QList<QStringList> transposedSegmentData;
        for (int rowIdx = 0;
             rowIdx < segmentCostRowLabels.size(); ++rowIdx)
        {
            QStringList rowData;
            for (const auto &segData : segmentCostData)
            {
                if (rowIdx < segData.size())
                {
                    rowData << segData[rowIdx];
                }
                else
                {
                    rowData << tr("N/A");
                }
            }
            transposedSegmentData.append(rowData);
        }

        // Create and add table to the table widget layout
        auto segmentTable = createComparisonTable(
            headers, segmentCostRowLabels,
            transposedSegmentData);
        tableLayout->addWidget(segmentTable);

        // Add widgets to splitter
        splitter->addWidget(segmentInfoWidget);
        splitter->addWidget(tableWidget);

        // Set initial sizes for the splitter
        QList<int> sizes;
        sizes << 150
              << 400; // Adjust these values as needed
        splitter->setSizes(sizes);

        // Add splitter to segment layout
        segmentLayout->addWidget(splitter);

        costTabWidget->addTab(
            segmentWidget,
            tr("Segment %1").arg(segmentIdx + 1));
    }

    layout->addWidget(costTabWidget);

    return container;
}

QTableWidget *PathComparisonDialog::createComparisonTable(
    const QStringList        &headers,
    const QStringList        &rowLabels,
    const QList<QStringList> &data)
{
    qCDebug(lcGuiPathTable) << "PathComparisonDialog::createComparisonTable:"
                            << "cols=" << headers.size()
                            << "rows=" << rowLabels.size();
    // Create table widget
    auto table = new QTableWidget(this);

    // Set dimensions
    table->setRowCount(rowLabels.size());
    table->setColumnCount(headers.size()
                          + 1); // +1 for row labels

    // Set headers
    QStringList allHeaders;
    allHeaders << tr("Property"); // First column header
    allHeaders.append(headers);
    table->setHorizontalHeaderLabels(allHeaders);

    // Populate with data
    for (int row = 0; row < rowLabels.size(); ++row)
    {
        // Set row label
        auto labelItem =
            new QTableWidgetItem(rowLabels[row]);
        labelItem->setFlags(labelItem->flags()
                            & ~Qt::ItemIsEditable);

        // Make the text bold
        QFont font = labelItem->font();
        font.setBold(true);
        labelItem->setFont(font);
        table->setItem(row, 0, labelItem);

        // Set data cells
        if (row < data.size())
        {
            const QStringList &rowData = data[row];

            for (int col = 0; col < rowData.size()
                              && col < headers.size();
                 ++col)
            {
                auto dataItem =
                    new QTableWidgetItem(rowData[col]);
                dataItem->setFlags(dataItem->flags()
                                   & ~Qt::ItemIsEditable);

                // Color-code cost differences in the costs
                // tab
                if (rowLabels[row]
                        == tr("Cost Difference (%)")
                    && rowData[col] != tr("N/A"))
                {

                    // Extract numeric part
                    QString numStr = rowData[col];
                    numStr.remove('%');
                    numStr.remove('+');
                    bool   ok;
                    double value = numStr.toDouble(&ok);

                    if (ok)
                    {
                        // Green for savings (negative
                        // difference), red for overruns
                        // (positive difference)
                        if (value < 0)
                        {
                            // Darker green for bigger
                            // savings
                            int intensity = qMin(
                                255,
                                qMax(0,
                                     255
                                         - qAbs(static_cast<
                                                int>(
                                             value * 2))));
                            dataItem->setBackground(QBrush(
                                QColor(intensity, 255,
                                       intensity)));
                        }
                        else if (value > 0)
                        {
                            // Darker red for bigger
                            // overruns
                            int intensity = qMin(
                                255,
                                qMax(0,
                                     255
                                         - qAbs(static_cast<
                                                int>(
                                             value * 2))));
                            dataItem->setBackground(QBrush(
                                QColor(255, intensity,
                                       intensity)));
                        }
                    }
                }

                table->setItem(row, col + 1, dataItem);
            }
        }
    }

    // Configure table appearance
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setSectionResizeMode(
        QHeaderView::ResizeToContents);
    table->verticalHeader()->setVisible(false);
    table->setAlternatingRowColors(true);
    table->setEditTriggers(QTableWidget::NoEditTriggers);
    table->setSelectionBehavior(QTableWidget::SelectRows);

    return table;
}

void PathComparisonDialog::createPathVisualization(
    QWidget *container)
{
    qCDebug(lcGuiPathTable) << "PathComparisonDialog::createPathVisualization:"
                            << "pathCount=" << m_pathData.size();
    // Create layout for visualization
    auto layout = new QVBoxLayout(container);

    // Create a grid layout to place path visualizations
    // side by side
    auto gridLayout = new QGridLayout();
    gridLayout->setSpacing(20);

    // Add each path visualization
    for (int pathIdx = 0; pathIdx < m_pathData.size();
         ++pathIdx)
    {
        const auto &pathData = m_pathData[pathIdx];

        if (pathData && pathData->path)
        {
            const auto summary = m_viewModel.pathSummary(pathData);
            const auto terminals =
                m_viewModel.terminalEntries(pathData);
            const auto segments =
                m_viewModel.segmentEntries(pathData);
            // Create a container for this path
            auto pathContainer = new QWidget(container);
            auto pathLayout =
                new QVBoxLayout(pathContainer);

            // Add path header
            auto pathHeader = new QLabel(
                tr("<h3>%1</h3>")
                    .arg(summary.pathLabel),
                container);
            pathHeader->setAlignment(Qt::AlignCenter);
            pathLayout->addWidget(pathHeader);

            // Create a widget for terminal visualization
            auto terminalWidget = new QWidget(container);
            auto terminalLayout =
                new QHBoxLayout(terminalWidget);
            terminalLayout->setSpacing(4);

            // Get terminals and segments
            if (!terminals.isEmpty())
            {
                // Add terminals and transportation modes
                for (int i = 0; i < terminals.size(); ++i)
                {
                    // Add terminal name label
                    QString terminalName =
                        terminals[i].displayName;
                    if (terminalName.isEmpty())
                    {
                        terminalName =
                            tr("Terminal %1").arg(i + 1);
                    }

                    auto nameLabel =
                        new QLabel(terminalName, container);
                    nameLabel->setAlignment(
                        Qt::AlignCenter);
                    nameLabel->setMinimumWidth(120);
                    terminalLayout->addWidget(nameLabel);

                    // Add transportation mode arrow for all
                    // but the last terminal
                    if (i < terminals.size() - 1
                        && i < segments.size())
                    {
                        auto modeLabel =
                            new QLabel(container);
                        modeLabel->setAlignment(
                            Qt::AlignCenter);

                        // Get transportation mode
                        QString modeText =
                            segments[i].modeName;

                        // Create mode pixmap
                        QPixmap modePixmap =
                            createTransportModePixmap(
                                modeText);
                        modeLabel->setPixmap(modePixmap);
                        modeLabel->setToolTip(modeText);
                        terminalLayout->addWidget(
                            modeLabel);
                    }
                }
            }
            else
            {
                terminalLayout->addWidget(new QLabel(
                    tr("No terminal data"), container));
            }

            // Add stretch to ensure elements are
            // left-aligned
            terminalLayout->addStretch();

            // Add terminal visualization to path layout
            pathLayout->addWidget(terminalWidget);

            // Add cost information
            QString costInfo =
                tr("Predicted: %1, Simulated: %2")
                    .arg(summary.predictedTotalCost,
                         0, 'f', 2)
                    .arg(
                        pathData->simulationTotalCost
                                >= 0
                            ? QString::number(
                                  pathData
                                      ->simulationTotalCost,
                                  'f', 2)
                            : tr("Not simulated"));

            auto costLabel =
                new QLabel(costInfo, container);
            costLabel->setAlignment(Qt::AlignCenter);
            pathLayout->addWidget(costLabel);

            // Add this path to the grid
            gridLayout->addWidget(pathContainer, 0,
                                  pathIdx);
        }
    }

    // Add grid to main layout
    layout->addLayout(gridLayout);
    layout->addStretch();
}

QPixmap PathComparisonDialog::createTransportModePixmap(
    const QString &mode)
{
    // Create a pixmap for the arrow with the mode text
    QPixmap pixmap(64, 40);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // Set color based on transportation mode
    QColor arrowColor = Qt::black; // Default color

    if (mode.contains("Truck", Qt::CaseInsensitive))
    {
        arrowColor =
            QColor(255, 0, 255); // Magenta for truck
    }
    else if (mode.contains("Rail", Qt::CaseInsensitive)
             || mode.contains("Train", Qt::CaseInsensitive))
    {
        arrowColor =
            QColor(80, 80, 80); // Dark gray for rail
    }
    else if (mode.contains("Ship", Qt::CaseInsensitive)
             || mode.contains("Water", Qt::CaseInsensitive))
    {
        arrowColor = QColor(0, 0, 255); // Blue for ship
    }

    // Draw the mode text
    painter.setPen(arrowColor);
    QFont font = painter.font();
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(QRect(0, 0, pixmap.width(), 15),
                     Qt::AlignCenter, mode);

    // Draw the arrow
    QPen pen(arrowColor, 2);
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

void PathComparisonDialog::onExportButtonClicked()
{
    qCInfo(lcGuiPathTable) << "PathComparisonDialog::onExportButtonClicked: CSV export";
    // Ask user for file location
    QString fileName = QFileDialog::getSaveFileName(
        this, tr("Export Comprehensive Path Data"),
        QDir::homePath(),
        tr("CSV Files (*.csv);;All Files (*)"));

    if (fileName.isEmpty())
    {
        qCDebug(lcGuiPathTable) << "PathComparisonDialog::onExportButtonClicked:"
                                << "user cancelled file dialog";
        return; // User canceled
    }

    // Make sure file has .csv extension
    if (!fileName.endsWith(".csv", Qt::CaseInsensitive))
    {
        fileName += ".csv";
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qCWarning(lcGuiPathTable) << "PathComparisonDialog::onExportButtonClicked:"
                                  << "failed to open" << fileName
                                  << file.errorString();
        QMessageBox::warning(
            this, tr("Export Error"),
            tr("Could not open file for writing: %1")
                .arg(file.errorString()));
        return;
    }

    QTextStream out(&file);
    const auto writeMetricLine =
        [&out](const QString &label,
               double         value,
               int            precision = 4) {
            out << label << ","
                << QString::number(value, 'f', precision)
                << "\n";
        };
    const auto writeCostLine =
        [&out](const QString &label,
               double         value,
               int            precision = 4) {
            out << label << ","
                << QString::number(value, 'f', precision)
                << "\n";
        };
    const auto writeCostSnapshot =
        [&writeCostLine](
            const PathComparisonViewModel::CostSnapshot &snapshot) {
            writeCostLine(PK::Segment::TravelTime,
                          snapshot.travelTime);
            writeCostLine(PK::Segment::Distance,
                          snapshot.distance);
            writeCostLine(PK::Segment::CarbonEmissions,
                          snapshot.carbonEmissions);
            writeCostLine(PK::Segment::EnergyConsumption,
                          snapshot.energyConsumption);
            writeCostLine(PK::Segment::Risk, snapshot.risk,
                          6);
            writeCostLine(PK::Segment::Cost,
                          snapshot.directCost);
        };
    const auto writePercentDifference =
        [&out](double predicted, double actual) {
            if (predicted > 0.0)
            {
                const double difference =
                    ((actual - predicted) / predicted)
                    * 100.0;
                if (difference > 0.0)
                {
                    out << ",+"
                        << QString::number(difference, 'f',
                                           2);
                }
                else
                {
                    out << ","
                        << QString::number(difference, 'f',
                                           2);
                }
            }
            else
            {
                out << ",N/A";
            }
        };
    const auto writeComparisonRow =
        [&out, this](const QString &label,
                     auto valueAccessor,
                     int precision = 2) {
            out << label;
            for (const auto &path : m_pathData)
            {
                if (!(path && path->path))
                    continue;

                const auto totals =
                    m_viewModel.pathCostTotals(path);
                out << ","
                    << QString::number(
                           valueAccessor(totals.predicted),
                           'f', precision);
                if (totals.actual.available)
                {
                    out << ","
                        << QString::number(
                               valueAccessor(totals.actual),
                               'f', precision);
                }
                else
                {
                    out << ",Not simulated";
                }
            }
            out << "\n";
        };

    // --------------- Export File Header ---------------
    // Add report metadata
    out << "Path Report Generated by CargoNetSim\n";
    out << "Date:,"
        << QDateTime::currentDateTime().toString(
               "yyyy-MM-dd HH:mm:ss")
        << "\n\n";

    // --------------- Summary Section ---------------
    out << "SUMMARY SECTION\n";
    out << "================\n\n";

    // Create column headers for paths
    out << "Property";
    for (const auto &path : m_pathData)
    {
        if (path && path->path)
        {
            out << "," << m_viewModel.pathSummary(path).pathLabel;
        }
        else
        {
            out << ",Unknown Path";
        }
    }
    out << "\n";

    // Basic path information
    // Path ID
    out << "Path ID";
    for (const auto &path : m_pathData)
    {
        if (path && path->path)
        {
            out << "," << m_viewModel.pathSummary(path).pathId;
        }
        else
        {
            out << ",N/A";
        }
    }
    out << "\n";

    // Total terminals
    out << "Total Terminals";
    for (const auto &path : m_pathData)
    {
        if (path && path->path)
        {
            out << "," << m_viewModel.pathSummary(path).terminalCount;
        }
        else
        {
            out << ",N/A";
        }
    }
    out << "\n";

    // Total segments
    out << "Total Segments";
    for (const auto &path : m_pathData)
    {
        if (path && path->path)
        {
            out << "," << m_viewModel.pathSummary(path).segmentCount;
        }
        else
        {
            out << ",N/A";
        }
    }
    out << "\n";

    // Predicted cost
    out << "Predicted Total Cost";
    for (const auto &path : m_pathData)
    {
        if (path && path->path)
        {
            out << ","
                << QString::number(
                       m_viewModel.pathSummary(path)
                           .predictedTotalCost,
                       'f', 2);
        }
        else
        {
            out << ",N/A";
        }
    }
    out << "\n";

    // Actual cost
    out << "Actual Total Cost";
    for (const auto &path : m_pathData)
    {
        if (path && path->path
            && path->hasSimulationTotalCost())
        {
            out << ","
                << QString::number(
                       path->simulationTotalCost, 'f',
                       2);
        }
        else
        {
            out << ",Not simulated";
        }
    }
    out << "\n";

    // Start terminal
    out << "Start Terminal";
    for (const auto &path : m_pathData)
    {
        if (path && path->path)
        {
            const auto summary = m_viewModel.pathSummary(path);
            out << ","
                << (summary.startTerminalName.isEmpty()
                        ? tr("Unknown")
                        : summary.startTerminalName);
        }
        else
        {
            out << ",N/A";
        }
    }
    out << "\n";

    // End terminal
    out << "End Terminal";
    for (const auto &path : m_pathData)
    {
        if (path && path->path)
        {
            const auto summary = m_viewModel.pathSummary(path);
            out << ","
                << (summary.endTerminalName.isEmpty()
                        ? tr("Unknown")
                        : summary.endTerminalName);
        }
        else
        {
            out << ",N/A";
        }
    }
    out << "\n\n";

    // --------------- Terminal Details Section
    // ---------------
    out << "TERMINAL DETAILS\n";
    out << "================\n\n";

    // Find the maximum number of terminals across all paths
    const int maxTerminals = m_viewModel.maxTerminals();

    // For each path, list all terminal details
    for (const auto &path : m_pathData)
    {
        if (path && path->path)
        {
            const auto summary = m_viewModel.pathSummary(path);
            const auto terminals =
                m_viewModel.terminalEntries(path);
            out << "Path " << summary.pathId << " Terminals:\n";
            out << "Index,Terminal Name,Terminal ID\n";

            for (int i = 0; i < terminals.size(); ++i)
            {
                out << i + 1 << ","
                    << (terminals[i].displayName.isEmpty()
                            ? terminals[i].canonicalName
                            : terminals[i].displayName)
                    << ","
                    << terminals[i].canonicalName;
                out << "\n";
            }
            out << "\n";
        }
    }

    // Terminal comparison table
    out << "Terminal Comparison Table:\n";
    out << "Terminal Index";
    for (const auto &path : m_pathData)
    {
        if (path && path->path)
        {
            out << "," << m_viewModel.pathSummary(path).pathLabel;
        }
        else
        {
            out << ",Unknown Path";
        }
    }
    out << "\n";

    // Write terminal data for each index in the comparison
    // table
    for (int i = 0; i < maxTerminals; ++i)
    {
        out << "Terminal " << (i + 1);

        for (const auto &path : m_pathData)
        {
            if (path && path->path)
            {
                const auto terminals =
                    m_viewModel.terminalEntries(path);
                if (i < terminals.size())
                {
                    out << ","
                        << (terminals[i].displayName.isEmpty()
                                ? terminals[i].canonicalName
                                : terminals[i].displayName);
                }
                else
                {
                    out << ",-";
                }
            }
            else
            {
                out << ",N/A";
            }
        }
        out << "\n";
    }
    out << "\n";

    // --------------- Segment Details Section
    // ---------------
    out << "SEGMENT DETAILS\n";
    out << "===============\n\n";

    // Find the maximum number of segments across all paths
    const int maxSegments = m_viewModel.maxSegments();

    // For each path, list detailed segment information
    for (const auto &path : m_pathData)
    {
        if (path && path->path)
        {
            const auto summary = m_viewModel.pathSummary(path);
            const auto segments =
                m_viewModel.segmentEntries(path);
            int pathId = summary.pathId;
            out << "Path " << pathId << " Segments:\n";
            for (int i = 0; i < segments.size(); ++i)
            {
                const auto &segment = segments[i];

                out << "Segment " << i + 1 << " Details:\n";
                out << "Start Terminal,"
                    << segment.startTerminalName << "\n";
                out << "End Terminal," << segment.endTerminalName
                    << "\n";
                out << "Transportation Mode,"
                    << segment.modeName
                    << "\n";

                const auto &displayValues =
                    segment.displayValues;
                const auto &predictedCosts =
                    segment.predictedCosts;
                const auto &actualCosts =
                    segment.actualCosts;

                out << "\nEstimated Values:\n";
                out << "Attribute,Value\n";
                if (displayValues.predictedAvailable)
                {
                    writeMetricLine(PK::Segment::Distance,
                                    displayValues
                                            .predictedDistanceKm,
                                    4);
                    writeMetricLine(PK::Segment::TravelTime,
                                    displayValues
                                            .predictedTravelTimeHours,
                                    4);
                    writeMetricLine(
                        QStringLiteral("vehicle_")
                            + PK::Segment::CarbonEmissions,
                        displayValues
                            .predictedCarbonEmissions,
                        4);
                    writeMetricLine(
                        QStringLiteral("allocated_")
                            + PK::Segment::CarbonEmissions,
                        displayValues
                            .predictedAllocatedCarbonEmissions,
                        4);
                    writeMetricLine(
                        QStringLiteral("vehicle_")
                            + PK::Segment::EnergyConsumption,
                        displayValues
                            .predictedEnergyConsumption,
                        4);
                    writeMetricLine(
                        QStringLiteral("allocated_")
                            + PK::Segment::EnergyConsumption,
                        displayValues
                            .predictedAllocatedEnergyConsumption,
                        4);
                    writeMetricLine(PK::Segment::Risk,
                                    displayValues
                                        .predictedRisk,
                                    6);
                }
                else
                {
                    out << "No estimated values available\n";
                }

                out << "\nActual Values:\n";
                out << "Attribute,Value\n";
                if (displayValues.actualAvailable)
                {
                    writeMetricLine(PK::Segment::Distance,
                                    displayValues.actualDistanceKm,
                                    4);
                    writeMetricLine(
                        PK::Segment::TravelTime,
                        displayValues.actualTravelTimeHours,
                        4);
                    writeMetricLine(
                        PK::Segment::CarbonEmissions,
                        displayValues.actualCarbonEmissions,
                        4);
                    writeMetricLine(
                        PK::Segment::EnergyConsumption,
                        displayValues.actualEnergyConsumption,
                        4);
                    writeMetricLine(PK::Segment::Risk,
                                    displayValues.actualRisk,
                                    6);
                }
                else
                {
                    out << "No actual values available\n";
                }

                out << "\nEstimated Costs:\n";
                out << "Cost Type,Value\n";
                if (predictedCosts.available)
                {
                    writeCostSnapshot(predictedCosts);
                }
                else
                {
                    out << "No estimated cost data "
                           "available\n";
                }

                out << "\nActual Costs:\n";
                out << "Cost Type,Value\n";
                if (actualCosts.available)
                {
                    writeCostSnapshot(actualCosts);
                }
                else
                {
                    out << "No actual cost data "
                           "available\n";
                }

                // Add other segment attributes if available
                out << "\nOther Attributes:\n";
                const QJsonObject &attributes =
                    segment.additionalAttributes;
                QStringList allKeys = attributes.keys();
                for (const QString &key : allKeys)
                {
                    if (key != PK::Segment::Estimated
                        && key != PK::Segment::EstimatedAllocated)
                    {
                        QJsonValue value = attributes[key];
                        if (value.isObject())
                        {
                            out << key << ",<Object>\n";
                        }
                        else if (value.isArray())
                        {
                            out << key << ",<Array>\n";
                        }
                        else if (value.isDouble())
                        {
                            out << key << ","
                                << QString::number(
                                       value.toDouble(),
                                       'f', 4)
                                << "\n";
                        }
                        else
                        {
                            out << key << ","
                                << value.toString() << "\n";
                        }
                    }
                }

                out << "\n"; // Space between segments
            }
            out << "\n"; // Space between paths
        }
    }

    // Segment comparison table
    out << "Segment Comparison Table:\n";
    out << "Segment Index";
    for (const auto &path : m_pathData)
    {
        if (path && path->path)
        {
            out << "," << m_viewModel.pathSummary(path).pathLabel;
        }
        else
        {
            out << ",Unknown Path";
        }
    }
    out << "\n";

    // Write segment data for each index in the comparison
    // table
    for (int i = 0; i < maxSegments; ++i)
    {
        out << "Segment " << (i + 1);

        for (const auto &path : m_pathData)
        {
            if (path && path->path)
            {
                const auto segments =
                    m_viewModel.segmentEntries(path);
                if (i < segments.size())
                {
                    QString segmentInfo =
                        QString("%1 → %2 (%3)")
                            .arg(segments[i].startTerminalName)
                            .arg(segments[i].endTerminalName)
                            .arg(segments[i].modeName);

                    out << "," << segmentInfo;
                }
                else
                {
                    out << ",-";
                }
            }
            else
            {
                out << ",N/A";
            }
        }
        out << "\n";
    }
    out << "\n";

    // --------------- Cost Analysis Section ---------------
    out << "COST ANALYSIS\n";
    out << "=============\n\n";

    // Cost Summary Table
    out << "Cost Summary Table:\n";
    out << "Cost Type";
    for (const auto &path : m_pathData)
    {
        if (path && path->path)
        {
            out << "," << m_viewModel.pathSummary(path).pathLabel;
        }
        else
        {
            out << ",Unknown Path";
        }
    }
    out << "\n";

    // Predicted costs
    out << "Predicted Total Cost";
    for (const auto &path : m_pathData)
    {
        if (path && path->path)
        {
            const auto summary = m_viewModel.pathSummary(path);
            out << ","
                << QString::number(summary.predictedTotalCost, 'f',
                                   2);
        }
        else
        {
            out << ",N/A";
        }
    }
    out << "\n";

    out << "Predicted Edge Cost";
    for (const auto &path : m_pathData)
    {
        if (path && path->path)
        {
            const auto summary = m_viewModel.pathSummary(path);
            out << ","
                << QString::number(summary.predictedEdgeCost, 'f',
                                   2);
        }
        else
        {
            out << ",N/A";
        }
    }
    out << "\n";

    out << "Predicted Terminal Cost";
    for (const auto &path : m_pathData)
    {
        if (path && path->path)
        {
            const auto summary = m_viewModel.pathSummary(path);
            out << ","
                << QString::number(summary.predictedTerminalCost,
                                   'f', 2);
        }
        else
        {
            out << ",N/A";
        }
    }
    out << "\n";

    // Simulated costs
    out << "Simulated Total Cost";
    for (const auto &path : m_pathData)
    {
        if (path && path->path
            && path->hasSimulationTotalCost())
        {
            out << ","
                << QString::number(
                       path->simulationTotalCost, 'f',
                       2);
        }
        else
        {
            out << ",Not simulated";
        }
    }
    out << "\n";

    out << "Simulated Edge Cost";
    for (const auto &path : m_pathData)
    {
        if (path && path->path
            && path->hasSimulationEdgeCosts())
        {
            out << ","
                << QString::number(
                       path->simulationEdgeCosts, 'f',
                       2);
        }
        else
        {
            out << ",Not simulated";
        }
    }
    out << "\n";

    out << "Simulated Terminal Cost";
    for (const auto &path : m_pathData)
    {
        if (path && path->path
            && path->hasSimulationTerminalCosts())
        {
            out << ","
                << QString::number(
                       path->simulationTerminalCosts,
                       'f', 2);
        }
        else
        {
            out << ",Not simulated";
        }
    }
    out << "\n";

    // Cost difference percentage
    out << "Cost Difference (%)";
    for (const auto &path : m_pathData)
    {
        if (path && path->path
            && path->hasSimulationTotalCost()
            && m_viewModel.pathSummary(path).predictedTotalCost > 0)
        {
            const auto summary = m_viewModel.pathSummary(path);
            double predictedCost =
                summary.predictedTotalCost;
            double simulatedCost =
                path->simulationTotalCost;
            double difference =
                ((simulatedCost - predictedCost)
                 / predictedCost)
                * 100.0;

            // Format with + sign for positive differences
            if (difference > 0)
            {
                out << ",+"
                    << QString::number(difference, 'f', 2);
            }
            else
            {
                out << ","
                    << QString::number(difference, 'f', 2);
            }
        }
        else
        {
            out << ",N/A";
        }
    }
    out << "\n\n";

    // Detailed Cost Breakdown by Category for each path
    out << "Detailed Cost Breakdown by Category:\n\n";

    for (const auto &path : m_pathData)
    {
        if (path && path->path)
        {
            const auto summary = m_viewModel.pathSummary(path);
            int pathId = summary.pathId;
            out << "Path " << pathId
                << " Detailed Cost Breakdown:\n";
            out << "Cost "
                   "Category,Predicted,Actual,Difference "
                   "(%)\n";

            // Initialize cost accumulators
            const auto totals =
                m_viewModel.pathCostTotals(path);
            const double predictedCarbonEmissionsCost =
                totals.predicted.carbonEmissions;
            const double actualCarbonEmissionsCost =
                totals.actual.carbonEmissions;
            const double predictedDirectCost =
                totals.predicted.directCost;
            const double actualDirectCost =
                totals.actual.directCost;
            const double predictedDistanceCost =
                totals.predicted.distance;
            const double actualDistanceCost =
                totals.actual.distance;
            const double predictedEnergyCost =
                totals.predicted.energyConsumption;
            const double actualEnergyCost =
                totals.actual.energyConsumption;
            const double predictedRiskCost =
                totals.predicted.risk;
            const double actualRiskCost = totals.actual.risk;
            const double predictedTimeCost =
                totals.predicted.travelTime;
            const double actualTimeCost =
                totals.actual.travelTime;
            const bool hasActualData = totals.actual.available;

            // Output carbon emissions costs
            out << "Carbon Emissions,"
                << QString::number(
                       predictedCarbonEmissionsCost, 'f',
                       2);
            if (hasActualData)
            {
                out << ","
                    << QString::number(
                           actualCarbonEmissionsCost, 'f',
                           2);

                // Calculate percentage difference
                writePercentDifference(
                    predictedCarbonEmissionsCost,
                    actualCarbonEmissionsCost);
            }
            else
            {
                out << ",Not simulated,N/A";
            }
            out << "\n";

            // Output direct costs
            out << "Direct Cost,"
                << QString::number(predictedDirectCost, 'f',
                                   2);
            if (hasActualData)
            {
                out << ","
                    << QString::number(actualDirectCost,
                                       'f', 2);

                // Calculate percentage difference
                writePercentDifference(predictedDirectCost,
                                       actualDirectCost);
            }
            else
            {
                out << ",Not simulated,N/A";
            }
            out << "\n";

            // Output distance-based costs
            out << "Distance-based,"
                << QString::number(predictedDistanceCost,
                                   'f', 2);
            if (hasActualData)
            {
                out << ","
                    << QString::number(actualDistanceCost,
                                       'f', 2);

                // Calculate percentage difference
                writePercentDifference(
                    predictedDistanceCost,
                    actualDistanceCost);
            }
            else
            {
                out << ",Not simulated,N/A";
            }
            out << "\n";

            // Output energy consumption costs
            out << "Energy Consumption,"
                << QString::number(predictedEnergyCost, 'f',
                                   2);
            if (hasActualData)
            {
                out << ","
                    << QString::number(actualEnergyCost,
                                       'f', 2);

                // Calculate percentage difference
                writePercentDifference(predictedEnergyCost,
                                       actualEnergyCost);
            }
            else
            {
                out << ",Not simulated,N/A";
            }
            out << "\n";

            // Output risk-based costs
            out << "Risk-based,"
                << QString::number(predictedRiskCost, 'f',
                                   6);
            if (hasActualData)
            {
                out << ","
                    << QString::number(actualRiskCost, 'f',
                                       6);

                // Calculate percentage difference
                writePercentDifference(predictedRiskCost,
                                       actualRiskCost);
            }
            else
            {
                out << ",Not simulated,N/A";
            }
            out << "\n";

            // Output travel time costs
            out << "Travel Time,"
                << QString::number(predictedTimeCost, 'f',
                                   2);
            if (hasActualData)
            {
                out << ","
                    << QString::number(actualTimeCost, 'f',
                                       2);

                // Calculate percentage difference
                writePercentDifference(predictedTimeCost,
                                       actualTimeCost);
            }
            else
            {
                out << ",Not simulated,N/A";
            }
            out << "\n";

            // Add total
            out << "Total,"
                << QString::number(summary.predictedTotalCost, 'f',
                                   2);
            if (path->hasSimulationTotalCost())
            {
                out << ","
                    << QString::number(
                           path->simulationTotalCost,
                           'f', 2);

                // Calculate percentage difference
                if (summary.predictedTotalCost > 0)
                {
                    double difference =
                        ((path->simulationTotalCost
                          - summary.predictedTotalCost)
                         / summary.predictedTotalCost)
                        * 100.0;

                    if (difference > 0)
                    {
                        out << ",+"
                            << QString::number(difference,
                                               'f', 2);
                    }
                    else
                    {
                        out << ","
                            << QString::number(difference,
                                               'f', 2);
                    }
                }
                else
                {
                    out << ",N/A";
                }
            }
            else
            {
                out << ",Not simulated,N/A";
            }
            out << "\n\n";
        }
    }

    // Cost comparison across paths
    out << "Cost Category Comparison Across Paths:\n";

    // Prepare headers for comparison table
    out << "Cost Category";
    for (const auto &path : m_pathData)
    {
        if (path && path->path)
        {
            const auto summary = m_viewModel.pathSummary(path);
            out << "," << summary.pathLabel
                << " Predicted"
                << "," << summary.pathLabel
                << " Actual";
        }
    }
    out << "\n";

    writeComparisonRow(
        "Carbon Emissions",
        [](const PathComparisonViewModel::CostSnapshot
               &snapshot) {
            return snapshot.carbonEmissions;
        });

    // Output direct costs
    writeComparisonRow(
        "Direct Cost",
        [](const PathComparisonViewModel::CostSnapshot
               &snapshot) {
            return snapshot.directCost;
        });

    // Output distance-based costs
    writeComparisonRow(
        "Distance-based",
        [](const PathComparisonViewModel::CostSnapshot
               &snapshot) {
            return snapshot.distance;
        });

    // Output energy consumption costs
    writeComparisonRow(
        "Energy Consumption",
        [](const PathComparisonViewModel::CostSnapshot
               &snapshot) {
            return snapshot.energyConsumption;
        });

    // Output risk-based costs
    writeComparisonRow(
        "Risk-based",
        [](const PathComparisonViewModel::CostSnapshot
               &snapshot) {
            return snapshot.risk;
        },
        6);

    // Output travel time costs
    writeComparisonRow(
        "Travel Time",
        [](const PathComparisonViewModel::CostSnapshot
               &snapshot) {
            return snapshot.travelTime;
        });

    // Output total costs
    out << "Total";
    for (const auto &path : m_pathData)
    {
        if (path && path->path)
        {
            const auto summary = m_viewModel.pathSummary(path);
            out << ","
                << QString::number(summary.predictedTotalCost, 'f',
                                   2);
            if (path->hasSimulationTotalCost())
            {
                out << ","
                    << QString::number(
                           path->simulationTotalCost,
                           'f', 2);
            }
            else
            {
                out << ",Not simulated";
            }
        }
    }
    out << "\n\n";

    // --------------- Metadata Section ---------------
    out << "EXPORT METADATA\n";
    out << "===============\n\n";

    out << "Generated by,CargoNetSim Path Comparison "
           "Dialog\n";
    out << "Export Date,"
        << QDateTime::currentDateTime().toString(
               "yyyy-MM-dd HH:mm:ss")
        << "\n";
    out << "Number of Paths," << m_pathData.size() << "\n";
    out << "CargoNetSim Version,"
        << QCoreApplication::applicationVersion() << "\n";
    out << "File Format,CSV\n";

    // Close the file
    file.close();

    qCInfo(lcGuiPathTable) << "PathComparisonDialog::onExportButtonClicked:"
                           << "export completed to" << fileName;
    QMessageBox::information(this, tr("Export Successful"),
                             tr("Comprehensive path data "
                                "has been exported to:\n%1")
                                 .arg(fileName));
}

} // namespace GUI
} // namespace CargoNetSim
