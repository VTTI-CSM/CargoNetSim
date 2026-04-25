/**
 * @file PathReportGenerator.cpp
 * @brief Implementation of the PathReportGenerator class
 * using KDReports
 * @author Ahmed Aredah
 * @date April 27, 2025
 *
 * This file contains the implementation of the
 * PathReportGenerator class, which creates comprehensive
 * PDF reports for path data visualization and comparison in
 * the CargoNetSim system using KDReports.
 */

#include "PathReportGenerator.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Scenario/MetricDisplayUnits.h"
#include "Backend/Scenario/PropertyKeys.h"
#include <KDReportsPreviewDialog.h>
#include <KDReportsPreviewWidget.h>
#include <QApplication>
#include <QDir>
#include <QFont>
#include <QFontMetrics>
#include <QMessageBox>
#include <QPainter>
#include <QPrinter>

namespace CargoNetSim
{
namespace GUI
{

namespace PK = CargoNetSim::Backend::Scenario::PropertyKeys;

PathReportGenerator::PathReportGenerator(
    const QList<const ShortestPathsTable::PathData *>
            &pathData,
    QObject *parent)
    : QObject(parent)
    , m_pathData(pathData)
    , m_viewModel(pathData)
{
    qCDebug(lcGuiUtil) << "PathReportGenerator::PathReportGenerator: pathData count"
                       << pathData.size();
    // Initialize fonts
    m_pageTitleFont     = QFont("Arial", 18, QFont::Bold);
    m_sectionTitleFont  = QFont("Arial", 14, QFont::Bold);
    m_normalTextFont    = QFont("Arial", 10);
    m_smallTextFont     = QFont("Arial", 8);
    m_tableHeaderFont   = QFont("Arial", 10, QFont::Bold);
    m_tableRowLabelFont = QFont("Arial", 10, QFont::Bold);

    // Initialize colors
    m_titleColor = QColor(44, 62, 80); // Dark blue-gray
    m_subtitleColor =
        QColor(52, 73, 94); // Lighter blue-gray
    m_tableHeaderBgColor =
        QColor(236, 240, 241); // Light gray
    m_tableBorderColor =
        QColor(189, 195, 199); // Medium gray
    m_positiveValueColor =
        QColor(231, 76, 60); // Red for cost increases
    m_negativeValueColor =
        QColor(46, 204, 113); // Green for cost savings
}

PathReportGenerator::~PathReportGenerator()
{
    qCDebug(lcGuiUtil) << "PathReportGenerator::~PathReportGenerator: destroyed";
    // Clean up if needed
}

KDReports::Report *PathReportGenerator::generateReport()
{
    qCInfo(lcGuiUtil) << "PathReportGenerator::generateReport: generating for"
                      << m_pathData.size() << "paths";
    // Create the report
    KDReports::Report *report = new KDReports::Report();

    // Set up the report
    report->setMargins(10, 10, 10, 10); // 10mm margins
    report->setPageSize(QPageSize::A4);
    report->setPageOrientation(QPageLayout::Landscape);

    // Set up headers and footers
    KDReports::Header &header = report->header();
    header.setDefaultFont(m_smallTextFont);
    // header.addVariable(KDReports::DefaultReportTitle);
    header.addVerticalSpacing(2);
    KDReports::HLineElement headerLine;
    headerLine.setThickness(1);
    headerLine.setColor(m_tableBorderColor);
    header.addElement(headerLine);

    KDReports::Footer &footer = report->footer();
    footer.setDefaultFont(m_smallTextFont);
    KDReports::HLineElement footerLine;
    footerLine.setThickness(1);
    footerLine.setColor(m_tableBorderColor);
    footer.addElement(footerLine);
    footer.addInlineElement(KDReports::TextElement(
        tr("CargoNetSim Path Report")));
    footer.addInlineElement(KDReports::TextElement(" - "));
    footer.addVariable(KDReports::PageNumber);
    footer.addInlineElement(KDReports::TextElement(" / "));
    footer.addVariable(KDReports::PageCount);

    // Set default title
    // report.setReportTitle(tr("Path Analysis Report"));

    // Add report content
    addReportHeader(report);
    addTableOfContents(report);

    // Start a new page for path details
    report->addPageBreak();

    addComparativeAnalysis(report);

    // Start a new page for comparative analysis
    report->addPageBreak();

    addIndividualPathSections(report);

    return report;
}

void PathReportGenerator::addReportHeader(
    KDReports::Report *report)
{
    qCDebug(lcGuiUtil) << "PathReportGenerator::addReportHeader: --- header section ---";
    // Title
    KDReports::TextElement titleElement(
        tr("Path Analysis Report"));
    titleElement.setFont(m_pageTitleFont);
    titleElement.setTextColor(m_titleColor);
    report->addElement(titleElement, Qt::AlignCenter);

    // Subtitle
    KDReports::TextElement subtitleElement(
        tr("CargoNetSim Path Comparison"));
    subtitleElement.setFont(m_sectionTitleFont);
    subtitleElement.setTextColor(m_subtitleColor);
    report->addElement(subtitleElement, Qt::AlignCenter);

    // Date and time
    QDateTime now = QDateTime::currentDateTime();
    KDReports::TextElement dateElement(
        tr("Generated on: %1")
            .arg(now.toString("yyyy-MM-dd hh:mm:ss")));
    dateElement.setFont(m_normalTextFont);
    report->addElement(dateElement, Qt::AlignCenter);

    // Add separator line
    report->addVerticalSpacing(5);
    KDReports::HLineElement line;
    line.setThickness(1);
    line.setColor(m_tableBorderColor);
    report->addElement(line);
    report->addVerticalSpacing(5);
}

void PathReportGenerator::addTableOfContents(
    KDReports::Report *report)
{
    qCDebug(lcGuiUtil) << "PathReportGenerator::addTableOfContents: paths" << m_pathData.size();
    // Section title
    KDReports::TextElement tocTitle(
        tr("Table of Contents"));
    tocTitle.setFont(m_sectionTitleFont);
    tocTitle.setTextColor(m_titleColor);
    report->addElement(tocTitle);
    report->addVerticalSpacing(5);

    QFont TOCLeavel1Font = QFont(m_normalTextFont);
    TOCLeavel1Font.setBold(true);

    // Add entry for comparative analysis
    KDReports::TextElement compHeader(
        tr("    Comparative Analysis"));
    compHeader.setFont(TOCLeavel1Font);
    compHeader.setTextColor(Qt::black);
    report->addElement(compHeader);
    report->addVerticalSpacing(2);

    // Sub-sections of comparative analysis
    QString sections[] = {
        tr("        Summary Comparison"),
        tr("        Terminal Comparison"),
        tr("        Segment Comparison"),
        tr("        Cost Comparison"),
        tr("        Segment-by-Segment Attribute "
           "Comparison"),
        tr("        Segment-by-Segment Cost Comparison")};

    for (const QString &section : sections)
    {
        KDReports::TextElement entry(section);
        entry.setFont(m_normalTextFont);
        report->addElement(entry);
    }

    report->addVerticalSpacing(5);

    // Add entries for individual paths
    KDReports::TextElement pathsHeader(
        tr("    Individual Path Analysis"));
    pathsHeader.setFont(TOCLeavel1Font);
    pathsHeader.setTextColor(Qt::black);
    report->addElement(pathsHeader);
    report->addVerticalSpacing(2);

    // Store bookmark references for each path
    for (const auto &pathData : m_pathData)
    {
        if (pathData && pathData->path)
        {
            int     pathId = pathData->path->getPathId();
            QString bookmarkName =
                QString("path_%1").arg(pathId);

            // Create TOC entry with link to the bookmark
            QString pathEntry =
                tr("        Path %1").arg(pathId);
            KDReports::TextElement entry(pathEntry);
            entry.setFont(m_normalTextFont);
            report->addElement(entry);
        }
    }
}

void PathReportGenerator::addIndividualPathSections(
    KDReports::Report *report)
{
    qCDebug(lcGuiUtil) << "PathReportGenerator::addIndividualPathSections: paths"
                       << m_pathData.size();
    // Section title
    KDReports::TextElement sectionTitle(
        tr("Individual Path Analysis"));
    sectionTitle.setFont(m_sectionTitleFont);
    sectionTitle.setTextColor(m_titleColor);
    report->addElement(sectionTitle);

    report->addVerticalSpacing(10);

    // Add each path's details
    for (const auto &pathData : m_pathData)
    {
        if (pathData && pathData->path)
        {
            // Start a new page for each path (except the
            // first one)
            if (pathData != m_pathData.first())
            {
                report->addPageBreak();
            }

            // Add path details
            addPathDetails(report, pathData);
        }
    }
}

void PathReportGenerator::addPathDetails(
    KDReports::Report                  *report,
    const ShortestPathsTable::PathData *pathData)
{
    qCDebug(lcGuiUtil) << "PathReportGenerator::addPathDetails:"
                       << (pathData && pathData->path ? "valid path" : "null");
    if (!pathData || !pathData->path)
        return;

    int pathId = pathData->path->getPathId();

    // Path title
    KDReports::TextElement pathTitle(
        tr("Path %1 Details").arg(pathId));
    pathTitle.setFont(m_pageTitleFont);
    pathTitle.setTextColor(m_titleColor);
    report->addElement(pathTitle, Qt::AlignCenter);

    report->addVerticalSpacing(10);

    // Add path visualization
    addPathVisualization(report, pathData);

    // Add path summary
    addPathSummary(report, pathData);

    // Add path terminals
    addPathTerminals(report, pathData);

    // Add path segments
    addPathSegments(report, pathData);

    // Add path costs
    addPathCosts(report, pathData);
}

void PathReportGenerator::addPathVisualization(
    KDReports::Report                  *report,
    const ShortestPathsTable::PathData *pathData)
{
    qCDebug(lcGuiUtil) << "PathReportGenerator::addPathVisualization: --- visualization ---";
    // Section title
    KDReports::TextElement sectionTitle(
        tr("Path Visualization"));
    sectionTitle.setFont(m_sectionTitleFont);
    sectionTitle.setTextColor(m_subtitleColor);
    report->addElement(sectionTitle);

    report->addVerticalSpacing(5);

    // Create the visualization image
    QImage visualizationImage =
        createPathVisualizationImage(pathData);

    if (!visualizationImage.isNull())
    {
        // Add the image to the report
        KDReports::ImageElement imageElement(
            visualizationImage);
        report->addElement(imageElement, Qt::AlignCenter);
    }
    else
    {
        // If visualization creation failed, display a
        // message
        KDReports::TextElement errorText(
            tr("Path visualization not available"));
        errorText.setFont(m_normalTextFont);
        errorText.setItalic(true);
        report->addElement(errorText, Qt::AlignCenter);
    }

    report->addVerticalSpacing(10);
}

QImage PathReportGenerator::createPathVisualizationImage(
    const ShortestPathsTable::PathData *pathData)
{
    if (!pathData || !pathData->path)
        return QImage();

    // Get terminals and segments from the path
    const QList<Backend::PathTerminal> &terminals =
        pathData->path->getTerminalsInPath();
    const QList<Backend::PathSegment *> segments =
        pathData->path->getSegments();

    const int terminalCount = terminals.size();
    const int segmentCount = segments.size();
    qCDebug(lcGuiUtil) << "PathReportGenerator::createPathVisualizationImage: terminals=" << terminalCount << "segments=" << segmentCount;

    if (terminals.isEmpty())
        return QImage();

    // Create an image with appropriate dimensions
    // Make it wider to accommodate more terminals if needed
    int width =
        qMax(800, terminals.size()
                      * 120); // Adjust width based on
                              // number of terminals
    QImage image(width, 100, QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    // Calculate spacing
    int numTerminals = terminals.size();
    int contentWidth =
        image.width() - 40; // 20px margin on each side
    int terminalSpacing =
        contentWidth
        / (numTerminals > 1 ? (numTerminals - 1) : 1);

    // Draw terminals and transportation modes
    int xPos = 20; // Start from left margin

    for (int i = 0; i < terminals.size(); ++i)
    {
        // Draw terminal circle
        QColor terminalColor = QColor(52, 152, 219); // Blue
        painter.setPen(QPen(terminalColor.darker(), 2));
        painter.setBrush(terminalColor);
        painter.drawEllipse(QPointF(xPos, 50), 10, 10);

        // Draw terminal name
        QString terminalName = terminals[i].displayName;
        if (terminalName.isEmpty())
            terminalName = tr("Terminal %1").arg(i + 1);

        // Limit name length to prevent overlaps
        if (terminalName.length() > 15)
        {
            terminalName = terminalName.left(12) + "...";
        }

        QFont terminalFont = painter.font();
        terminalFont.setPointSize(8);
        painter.setFont(terminalFont);

        // Draw text above circle
        QRectF textRect(xPos - 50, 10, 100, 30);
        painter.setPen(Qt::black);
        painter.drawText(textRect, Qt::AlignCenter,
                         terminalName);

        // Draw transportation mode line to next terminal
        if (i < terminals.size() - 1 && i < segments.size()
            && segments[i])
        {
            // Transportation mode
            Backend::TransportationTypes::TransportationMode
                    mode = segments[i]->getMode();
            QString modeText =
                Backend::TransportationTypes::toString(
                    mode);

            // Abbreviate very long mode names
            if (modeText.length() > 10)
            {
                modeText = modeText.left(8) + "...";
            }

            // Draw line to next terminal
            QColor lineColor;
            if (modeText.contains("Truck",
                                  Qt::CaseInsensitive))
                lineColor = QColor(
                    255, 0, 255); // Magenta for truck
            else if (modeText.contains("Rail",
                                       Qt::CaseInsensitive)
                     || modeText.contains(
                         "Train", Qt::CaseInsensitive))
                lineColor = QColor(
                    80, 80, 80); // Dark gray for rail
            else if (modeText.contains("Ship",
                                       Qt::CaseInsensitive)
                     || modeText.contains(
                         "Water", Qt::CaseInsensitive))
                lineColor =
                    QColor(0, 0, 255); // Blue for ship
            else
                lineColor = Qt::black; // Default

            painter.setPen(
                QPen(lineColor, 2, Qt::SolidLine));
            painter.drawLine(xPos + 10, 50,
                             xPos + terminalSpacing - 10,
                             50);

            // Draw mode text
            QFont modeFont = painter.font();
            modeFont.setPointSize(8);
            painter.setFont(modeFont);

            QRectF modeRect(xPos + 10, 60,
                            terminalSpacing - 20, 30);
            painter.drawText(modeRect, Qt::AlignCenter,
                             modeText);
        }

        // Move to next terminal position
        xPos += terminalSpacing;
    }

    image =
        image.scaled(QSize(400, 400), Qt::KeepAspectRatio,
                     Qt::SmoothTransformation);

    return image;
}

void PathReportGenerator::addPathSummary(
    KDReports::Report                  *report,
    const ShortestPathsTable::PathData *pathData)
{
    qCDebug(lcGuiUtil) << "PathReportGenerator::addPathSummary: --- summary section ---";
    // Section title
    KDReports::TextElement sectionTitle(tr("Path Summary"));
    sectionTitle.setFont(m_sectionTitleFont);
    sectionTitle.setTextColor(m_subtitleColor);
    report->addElement(sectionTitle);

    report->addVerticalSpacing(5);

    // Create table for summary data
    KDReports::TableElement table;
    table.setHeaderColumnCount(2);
    table.setHeaderRowCount(1);
    table.setBorder(1);
    table.setBorderBrush(QBrush(m_tableBorderColor));

    // Set header
    styleTableCell(table, 0, 0, tr("Property"), true);
    styleTableCell(table, 0, 1, tr("Value"), true);

    // Add data rows
    int row = 1;

    // Path ID
    styleTableCell(table, row, 0, tr("Path ID"), false,
                   true);
    styleTableCell(
        table, row, 1,
        QString::number(pathData->path->getPathId()));
    row++;

    // Total terminals
    styleTableCell(table, row, 0, tr("Total Terminals"),
                   false, true);
    styleTableCell(
        table, row, 1,
        QString::number(
            pathData->path->getTerminalsInPath().size()));
    row++;

    // Total segments
    styleTableCell(table, row, 0, tr("Total Segments"),
                   false, true);
    styleTableCell(
        table, row, 1,
        QString::number(
            pathData->path->getSegments().size()));
    row++;

    // Predicted cost
    styleTableCell(table, row, 0, tr("Predicted Cost"),
                   false, true);
    styleTableCell(
        table, row, 1,
        QString::number(pathData->path->getTotalPathCost(),
                        'f', 2));
    row++;

    // Actual cost
    styleTableCell(table, row, 0, tr("Actual Cost"), false,
                   true);
    if (pathData->m_totalSimulationPathCost >= 0)
    {
        styleTableCell(
            table, row, 1,
            QString::number(
                pathData->m_totalSimulationPathCost, 'f',
                2));
    }
    else
    {
        styleTableCell(table, row, 1, tr("Not simulated"));
    }
    row++;

    // Start terminal
    styleTableCell(table, row, 0, tr("Start Terminal"),
                   false, true);
    const auto &pathTerminals =
        pathData->path->getTerminalsInPath();
    QString startTerminal = pathTerminals.isEmpty()
                                ? tr("Unknown")
                                : pathTerminals.first().displayName;
    styleTableCell(table, row, 1, startTerminal);
    row++;

    // End terminal
    styleTableCell(table, row, 0, tr("End Terminal"), false,
                   true);
    QString endTerminal = pathTerminals.isEmpty()
                              ? tr("Unknown")
                              : pathTerminals.last().displayName;
    styleTableCell(table, row, 1, endTerminal);

    // Add table to report
    report->addElement(table);

    report->addVerticalSpacing(10);
}

void PathReportGenerator::addPathTerminals(
    KDReports::Report                  *report,
    const ShortestPathsTable::PathData *pathData)
{
    qCDebug(lcGuiUtil) << "PathReportGenerator::addPathTerminals: --- terminals section ---";
    // Section title
    KDReports::TextElement sectionTitle(tr("Terminals"));
    sectionTitle.setFont(m_sectionTitleFont);
    sectionTitle.setTextColor(m_subtitleColor);
    report->addElement(sectionTitle);

    report->addVerticalSpacing(5);

    // Get the terminals from the path
    const QList<Backend::PathTerminal> &terminals =
        pathData->path->getTerminalsInPath();

    if (terminals.isEmpty())
    {
        // If no terminals, show a message
        KDReports::TextElement noData(
            tr("No terminal data available."));
        noData.setFont(m_normalTextFont);
        report->addElement(noData);
        report->addVerticalSpacing(10);
        return;
    }

    // Create table for terminals data
    KDReports::TableElement table;
    table.setHeaderColumnCount(3);
    table.setHeaderRowCount(1);
    table.setBorder(1);
    table.setBorderBrush(QBrush(m_tableBorderColor));

    // Set header
    styleTableCell(table, 0, 0, tr("Index"), true);
    styleTableCell(table, 0, 1, tr("Terminal Name"), true);
    styleTableCell(table, 0, 2, tr("ID"), true);

    // Add data rows
    for (int i = 0; i < terminals.size(); ++i)
    {
        int row = i + 1;

        // Terminal index
        styleTableCell(table, row, 0,
                       QString::number(i + 1), false, true);

        // Terminal name
        styleTableCell(table, row, 1,
                       terminals[i].displayName);

        // Terminal ID
        styleTableCell(table, row, 2,
                       terminals[i].canonicalName);
    }

    // Add table to report
    report->addElement(table);

    report->addVerticalSpacing(10);
}

void PathReportGenerator::addPathSegments(
    KDReports::Report                  *report,
    const ShortestPathsTable::PathData *pathData)
{
    qCDebug(lcGuiUtil) << "PathReportGenerator::addPathSegments: --- segments section ---";
    // Section title
    KDReports::TextElement sectionTitle(tr("Segments"));
    sectionTitle.setFont(m_sectionTitleFont);
    sectionTitle.setTextColor(m_subtitleColor);
    report->addElement(sectionTitle);

    report->addVerticalSpacing(5);

    // Get the segments from the path
    const QList<Backend::PathSegment *> &segments =
        pathData->path->getSegments();

    if (segments.isEmpty())
    {
        // If no segments, show a message
        KDReports::TextElement noData(
            tr("No segment data available."));
        noData.setFont(m_normalTextFont);
        report->addElement(noData);
        report->addVerticalSpacing(10);
        return;
    }

    // Create table for segments data
    KDReports::TableElement table;
    table.setHeaderColumnCount(4);
    table.setHeaderRowCount(1);
    table.setBorder(1);
    table.setBorderBrush(QBrush(m_tableBorderColor));

    // Set header
    styleTableCell(table, 0, 0, tr("Index"), true);
    styleTableCell(table, 0, 1, tr("Start"), true);
    styleTableCell(table, 0, 2, tr("End"), true);
    styleTableCell(table, 0, 3, tr("Mode"), true);

    // Add data rows
    for (int i = 0; i < segments.size(); ++i)
    {
        int row = i + 1;

        // Segment index
        styleTableCell(table, row, 0,
                       QString::number(i + 1), false, true);

        if (segments[i])
        {
            // Start terminal
            styleTableCell(table, row, 1,
                           getTerminalDisplayNameByID(
                               pathData->path.get(),
                               segments[i]->getStart()));

            // End terminal
            styleTableCell(
                table, row, 2,
                getTerminalDisplayNameByID(
                    pathData->path.get(), segments[i]->getEnd()));

            // Transportation mode
            styleTableCell(
                table, row, 3,
                Backend::TransportationTypes::toString(
                    segments[i]->getMode()));
        }
        else
        {
            // Unknown segment
            styleTableCell(table, row, 1, tr("Unknown"));
            styleTableCell(table, row, 2, tr("Unknown"));
            styleTableCell(table, row, 3, tr("Unknown"));
        }
    }

    // Add table to report
    report->addElement(table);

    report->addVerticalSpacing(10);

    // Add detailed segment attributes for each segment
    for (int i = 0; i < segments.size(); ++i)
    {
        if (!segments[i])
            continue;

        // Section title for the segment
        KDReports::TextElement segmentTitle(
            tr("Segment %1 Attributes").arg(i + 1));
        segmentTitle.setFont(m_normalTextFont);
        segmentTitle.setBold(true);
        segmentTitle.setTextColor(m_subtitleColor);
        report->addElement(segmentTitle);

        report->addVerticalSpacing(5);

        const auto values = m_viewModel.segmentValues(pathData, i);
        const auto predictedValues = segments[i]->estimatedValues();
        const auto predictedCosts =
            m_viewModel.segmentPredictedCosts(pathData, i);
        const auto actualCosts =
            m_viewModel.segmentActualCosts(pathData, i);

        // Create table for segment attributes
        KDReports::TableElement attrTable;
        attrTable.setHeaderColumnCount(3);
        attrTable.setHeaderRowCount(1);
        attrTable.setBorder(1);
        attrTable.setBorderBrush(
            QBrush(m_tableBorderColor));

        // Set header
        styleTableCell(attrTable, 0, 0, tr("Attribute"),
                       true);
        styleTableCell(attrTable, 0, 1, tr("Predicted"),
                       true);
        styleTableCell(attrTable, 0, 2, tr("Actual"), true);

        // Add attribute rows
        int rowIndex = 1;

        // Carbon Emissions
        styleTableCell(attrTable, rowIndex, 0,
                       tr("Carbon Emissions"), false, true);
        styleTableCell(
            attrTable, rowIndex, 1,
            predictedValues.available
                ? QString::number(
                      predictedValues.carbonEmissions, 'f', 3)
                : tr("N/A"));
        styleTableCell(
            attrTable, rowIndex, 2,
            values.actualAvailable
                ? QString::number(
                      values.actualCarbonEmissions,
                      'f', 3)
                : tr("N/A"));
        rowIndex++;

        // Direct Cost
        styleTableCell(attrTable, rowIndex, 0, tr("Direct Cost"),
                       false, true);
        styleTableCell(
            attrTable, rowIndex, 1,
            predictedCosts.available
                ? QString::number(predictedCosts.directCost, 'f', 2)
                : tr("N/A"));
        styleTableCell(
            attrTable, rowIndex, 2,
            actualCosts.available
                ? QString::number(actualCosts.directCost, 'f', 2)
                : tr("N/A"));
        rowIndex++;

        // Distance
        styleTableCell(attrTable, rowIndex, 0,
                       tr("Distance"), false, true);
        styleTableCell(
            attrTable, rowIndex, 1,
            predictedValues.available
                ? QString::number(
                      Backend::Scenario::MetricDisplayUnits::
                          distanceKmFromMetres(
                              predictedValues.distance),
                      'f', 2)
                : tr("N/A"));
        styleTableCell(attrTable, rowIndex, 2,
                       values.actualAvailable
                           ? QString::number(
                                 values.actualDistanceKm,
                                 'f', 2)
                           : tr("N/A"));
        rowIndex++;

        // Energy Consumption
        styleTableCell(attrTable, rowIndex, 0,
                       tr("Energy Consumption"), false,
                       true);
        styleTableCell(
            attrTable, rowIndex, 1,
            predictedValues.available
                ? QString::number(
                      predictedValues.energyConsumption, 'f', 2)
                : tr("N/A"));
        styleTableCell(
            attrTable, rowIndex, 2,
            values.actualAvailable
                ? QString::number(
                      values.actualEnergyConsumption,
                      'f', 2)
                : tr("N/A"));
        rowIndex++;

        // Risk
        styleTableCell(attrTable, rowIndex, 0, tr("Risk"),
                       false, true);
        styleTableCell(
            attrTable, rowIndex, 1,
            predictedValues.available
                ? QString::number(predictedValues.risk, 'f', 6)
                : tr("N/A"));
        styleTableCell(
            attrTable, rowIndex, 2,
            values.actualAvailable
                ? QString::number(values.actualRisk, 'f', 6)
                : tr("N/A"));
        rowIndex++;

        // Travel Time
        styleTableCell(attrTable, rowIndex, 0,
                       tr("Travel Time"), false, true);
        styleTableCell(
            attrTable, rowIndex, 1,
            predictedValues.available
                ? QString::number(
                      Backend::Scenario::MetricDisplayUnits::
                          travelTimeHoursFromSeconds(
                              predictedValues.travelTime),
                      'f', 2)
                : tr("N/A"));
        styleTableCell(
            attrTable, rowIndex, 2,
            values.actualAvailable
                ? QString::number(values.actualTravelTimeHours,
                                  'f', 2)
                : tr("N/A"));

        // Add table to report
        report->addElement(attrTable);

        report->addVerticalSpacing(10);
    }
}

void PathReportGenerator::addPathCosts(
    KDReports::Report                  *report,
    const ShortestPathsTable::PathData *pathData)
{
    qCDebug(lcGuiUtil) << "PathReportGenerator::addPathCosts: --- costs section ---";
    // Section title
    KDReports::TextElement sectionTitle(
        tr("Cost Analysis"));
    sectionTitle.setFont(m_sectionTitleFont);
    sectionTitle.setTextColor(m_subtitleColor);
    report->addElement(sectionTitle);

    report->addVerticalSpacing(5);

    // Create table for costs summary
    KDReports::TableElement table;
    table.setHeaderColumnCount(4);
    table.setHeaderRowCount(1);
    table.setBorder(1);
    table.setBorderBrush(QBrush(m_tableBorderColor));

    // Set table title
    KDReports::TextElement tableTitle(tr("Cost Summary"));
    tableTitle.setFont(m_normalTextFont);
    tableTitle.setBold(true);
    report->addElement(tableTitle, Qt::AlignCenter);
    report->addVerticalSpacing(3);

    // Set header
    styleTableCell(table, 0, 0, tr("Cost Type"), true);
    styleTableCell(table, 0, 1, tr("Predicted"), true);
    styleTableCell(table, 0, 2, tr("Actual"), true);
    styleTableCell(table, 0, 3, tr("Difference (%)"), true);

    // Add data rows
    int rowIndex = 1;

    // Total costs
    styleTableCell(table, rowIndex, 0, tr("Total Cost"),
                   false, true);

    double predictedTotalCost =
        pathData->path->getTotalPathCost();
    double actualTotalCost =
        pathData->m_totalSimulationPathCost;

    styleTableCell(
        table, rowIndex, 1,
        QString::number(predictedTotalCost, 'f', 2));

    if (actualTotalCost >= 0)
    {
        styleTableCell(
            table, rowIndex, 2,
            QString::number(actualTotalCost, 'f', 2));

        // Calculate percentage difference
        if (predictedTotalCost > 0)
        {
            double difference =
                ((actualTotalCost - predictedTotalCost)
                 / predictedTotalCost)
                * 100.0;

            // Format with + sign for positive differences
            QString diffText;
            if (difference > 0)
            {
                diffText = QString("+%1%").arg(difference,
                                               0, 'f', 2);
            }
            else
            {
                diffText = QString("%1%").arg(difference, 0,
                                              'f', 2);
            }

            // Color-code the difference cell
            KDReports::Cell &cell = table.cell(rowIndex, 3);
            KDReports::TextElement element(diffText);

            if (difference > 0)
            {
                // Cost increase (red)
                element.setTextColor(m_positiveValueColor);
            }
            else if (difference < 0)
            {
                // Cost savings (green)
                element.setTextColor(m_negativeValueColor);
            }

            cell.addElement(element);
        }
        else
        {
            styleTableCell(table, rowIndex, 3, tr("N/A"));
        }
    }
    else
    {
        styleTableCell(table, rowIndex, 2,
                       tr("Not simulated"));
        styleTableCell(table, rowIndex, 3, tr("N/A"));
    }
    rowIndex++;

    // Edge costs
    styleTableCell(table, rowIndex, 0, tr("Edge Cost"),
                   false, true);

    double predictedEdgeCost =
        pathData->path->getTotalEdgeCosts();
    double actualEdgeCost =
        pathData->m_totalSimulationEdgeCosts;

    styleTableCell(
        table, rowIndex, 1,
        QString::number(predictedEdgeCost, 'f', 2));

    if (actualEdgeCost >= 0)
    {
        styleTableCell(
            table, rowIndex, 2,
            QString::number(actualEdgeCost, 'f', 2));

        // Calculate percentage difference
        if (predictedEdgeCost > 0)
        {
            double difference =
                ((actualEdgeCost - predictedEdgeCost)
                 / predictedEdgeCost)
                * 100.0;

            // Format with + sign for positive differences
            QString diffText;
            if (difference > 0)
            {
                diffText = QString("+%1%").arg(difference,
                                               0, 'f', 2);
            }
            else
            {
                diffText = QString("%1%").arg(difference, 0,
                                              'f', 2);
            }

            // Color-code the difference cell
            KDReports::Cell &cell = table.cell(rowIndex, 3);
            KDReports::TextElement element(diffText);

            if (difference > 0)
            {
                // Cost increase (red)
                element.setTextColor(m_positiveValueColor);
            }
            else if (difference < 0)
            {
                // Cost savings (green)
                element.setTextColor(m_negativeValueColor);
            }

            cell.addElement(element);
        }
        else
        {
            styleTableCell(table, rowIndex, 3, tr("N/A"));
        }
    }
    else
    {
        styleTableCell(table, rowIndex, 2,
                       tr("Not simulated"));
        styleTableCell(table, rowIndex, 3, tr("N/A"));
    }
    rowIndex++;

    // Terminal costs
    styleTableCell(table, rowIndex, 0, tr("Terminal Cost"),
                   false, true);

    double predictedTerminalCost =
        pathData->path->getTotalTerminalCosts();
    double actualTerminalCost =
        pathData->m_totalSimulationTerminalCosts;

    styleTableCell(
        table, rowIndex, 1,
        QString::number(predictedTerminalCost, 'f', 2));

    if (actualTerminalCost >= 0)
    {
        styleTableCell(
            table, rowIndex, 2,
            QString::number(actualTerminalCost, 'f', 2));

        // Calculate percentage difference
        if (predictedTerminalCost > 0)
        {
            double difference = ((actualTerminalCost
                                  - predictedTerminalCost)
                                 / predictedTerminalCost)
                                * 100.0;

            // Format with + sign for positive differences
            QString diffText;
            if (difference > 0)
            {
                diffText = QString("+%1%").arg(difference,
                                               0, 'f', 2);
            }
            else
            {
                diffText = QString("%1%").arg(difference, 0,
                                              'f', 2);
            }

            // Color-code the difference cell
            KDReports::Cell &cell = table.cell(rowIndex, 3);
            KDReports::TextElement element(diffText);

            if (difference > 0)
            {
                // Cost increase (red)
                element.setTextColor(m_positiveValueColor);
            }
            else if (difference < 0)
            {
                // Cost savings (green)
                element.setTextColor(m_negativeValueColor);
            }

            cell.addElement(element);
        }
        else
        {
            styleTableCell(table, rowIndex, 3, tr("N/A"));
        }
    }
    else
    {
        styleTableCell(table, rowIndex, 2,
                       tr("Not simulated"));
        styleTableCell(table, rowIndex, 3, tr("N/A"));
    }

    // Add table to report
    report->addElement(table);

    report->addVerticalSpacing(10);

    // Add detailed cost breakdown
    KDReports::TextElement detailedTitle(
        tr("Detailed Cost Breakdown"));
    detailedTitle.setFont(m_normalTextFont);
    detailedTitle.setBold(true);
    detailedTitle.setTextColor(m_subtitleColor);
    report->addElement(detailedTitle);

    report->addVerticalSpacing(5);

    // Get segments for the detailed breakdown
    const QList<Backend::PathSegment *> &segments =
        pathData->path->getSegments();

    if (segments.isEmpty())
    {
        // If no segments, show a message
        KDReports::TextElement noData(
            tr("No segment data available for detailed "
               "cost breakdown."));
        noData.setFont(m_normalTextFont);
        report->addElement(noData);
        report->addVerticalSpacing(10);
        return;
    }

    // Initialize cost accumulators for each category
    double predictedCarbonEmissionsCost = 0.0;
    double actualCarbonEmissionsCost    = 0.0;
    double predictedDirectCost          = 0.0;
    double actualDirectCost             = 0.0;
    double predictedDistanceCost        = 0.0;
    double actualDistanceCost           = 0.0;
    double predictedEnergyCost          = 0.0;
    double actualEnergyCost             = 0.0;
    double predictedRiskCost            = 0.0;
    double actualRiskCost               = 0.0;
    double predictedTimeCost            = 0.0;
    double actualTimeCost               = 0.0;

    bool hasActualData = false;

    // Sum up costs across all segments
    for (int i = 0; i < segments.size(); ++i)
    {
        const auto *segment = segments[i];
        if (segment)
        {
            const auto estimatedCostObj = segment->estimatedCosts();
            if (estimatedCostObj.available)
            {
                predictedCarbonEmissionsCost +=
                    estimatedCostObj.carbonEmissions;
                predictedDirectCost += estimatedCostObj.directCost;
                predictedDistanceCost += estimatedCostObj.distance;
                predictedEnergyCost += estimatedCostObj.energyConsumption;
                predictedRiskCost += estimatedCostObj.risk;
                predictedTimeCost += estimatedCostObj.travelTime;
            }

            const auto actualCostObj =
                m_viewModel.segmentActualCosts(pathData, i);
            if (actualCostObj.available)
            {
                actualCarbonEmissionsCost +=
                    actualCostObj.carbonEmissions;
                actualDirectCost += actualCostObj.directCost;
                actualDistanceCost += actualCostObj.distance;
                actualEnergyCost += actualCostObj.energyConsumption;
                actualRiskCost += actualCostObj.risk;
                actualTimeCost += actualCostObj.travelTime;
                hasActualData = true;
            }
        }
    }

    // Create table for detailed cost breakdown
    KDReports::TableElement detailedTable;
    detailedTable.setHeaderColumnCount(4);
    detailedTable.setHeaderRowCount(1);
    detailedTable.setBorder(1);
    detailedTable.setBorderBrush(
        QBrush(m_tableBorderColor));

    // Set header
    styleTableCell(detailedTable, 0, 0, tr("Cost Category"),
                   true);
    styleTableCell(detailedTable, 0, 1, tr("Predicted"),
                   true);
    styleTableCell(detailedTable, 0, 2, tr("Actual"), true);
    styleTableCell(detailedTable, 0, 3,
                   tr("Difference (%)"), true);

    // Add data rows
    rowIndex = 1;

    // Carbon emissions cost
    styleTableCell(detailedTable, rowIndex, 0,
                   tr("Carbon Emissions"), false, true);
    styleTableCell(
        detailedTable, rowIndex, 1,
        QString::number(predictedCarbonEmissionsCost, 'f',
                        2));

    if (hasActualData)
    {
        styleTableCell(
            detailedTable, rowIndex, 2,
            QString::number(actualCarbonEmissionsCost, 'f',
                            2));

        // Calculate percentage difference
        if (predictedCarbonEmissionsCost > 0)
        {
            double difference =
                ((actualCarbonEmissionsCost
                  - predictedCarbonEmissionsCost)
                 / predictedCarbonEmissionsCost)
                * 100.0;

            // Format with + sign for positive differences
            QString diffText;
            if (difference > 0)
            {
                diffText = QString("+%1%").arg(difference,
                                               0, 'f', 2);
            }
            else
            {
                diffText = QString("%1%").arg(difference, 0,
                                              'f', 2);
            }

            // Color-code the difference cell
            KDReports::Cell &cell =
                detailedTable.cell(rowIndex, 3);
            KDReports::TextElement element(diffText);

            if (difference > 0)
            {
                // Cost increase (red)
                element.setTextColor(m_positiveValueColor);
            }
            else if (difference < 0)
            {
                // Cost savings (green)
                element.setTextColor(m_negativeValueColor);
            }

            cell.addElement(element);
        }
        else
        {
            styleTableCell(detailedTable, rowIndex, 3,
                           tr("N/A"));
        }
    }
    else
    {
        styleTableCell(detailedTable, rowIndex, 2,
                       tr("Not simulated"));
        styleTableCell(detailedTable, rowIndex, 3,
                       tr("N/A"));
    }
    rowIndex++;

    // Direct cost
    styleTableCell(detailedTable, rowIndex, 0,
                   tr("Direct Cost"), false, true);
    styleTableCell(
        detailedTable, rowIndex, 1,
        QString::number(predictedDirectCost, 'f', 2));

    if (hasActualData)
    {
        styleTableCell(
            detailedTable, rowIndex, 2,
            QString::number(actualDirectCost, 'f', 2));

        // Calculate percentage difference
        if (predictedDirectCost > 0)
        {
            double difference =
                ((actualDirectCost - predictedDirectCost)
                 / predictedDirectCost)
                * 100.0;

            // Format with + sign for positive differences
            QString diffText;
            if (difference > 0)
            {
                diffText = QString("+%1%").arg(difference,
                                               0, 'f', 2);
            }
            else
            {
                diffText = QString("%1%").arg(difference, 0,
                                              'f', 2);
            }

            // Color-code the difference cell
            KDReports::Cell &cell =
                detailedTable.cell(rowIndex, 3);
            KDReports::TextElement element(diffText);

            if (difference > 0)
            {
                // Cost increase (red)
                element.setTextColor(m_positiveValueColor);
            }
            else if (difference < 0)
            {
                // Cost savings (green)
                element.setTextColor(m_negativeValueColor);
            }

            cell.addElement(element);
        }
        else
        {
            styleTableCell(detailedTable, rowIndex, 3,
                           tr("N/A"));
        }
    }
    else
    {
        styleTableCell(detailedTable, rowIndex, 2,
                       tr("Not simulated"));
        styleTableCell(detailedTable, rowIndex, 3,
                       tr("N/A"));
    }
    rowIndex++;

    // Distance-based cost
    styleTableCell(detailedTable, rowIndex, 0,
                   tr("Distance-based"), false, true);
    styleTableCell(
        detailedTable, rowIndex, 1,
        QString::number(predictedDistanceCost, 'f', 2));

    if (hasActualData)
    {
        styleTableCell(
            detailedTable, rowIndex, 2,
            QString::number(actualDistanceCost, 'f', 2));

        // Calculate percentage difference
        if (predictedDistanceCost > 0)
        {
            double difference = ((actualDistanceCost
                                  - predictedDistanceCost)
                                 / predictedDistanceCost)
                                * 100.0;

            // Format with + sign for positive differences
            QString diffText;
            if (difference > 0)
            {
                diffText = QString("+%1%").arg(difference,
                                               0, 'f', 2);
            }
            else
            {
                diffText = QString("%1%").arg(difference, 0,
                                              'f', 2);
            }

            // Color-code the difference cell
            KDReports::Cell &cell =
                detailedTable.cell(rowIndex, 3);
            KDReports::TextElement element(diffText);

            if (difference > 0)
            {
                // Cost increase (red)
                element.setTextColor(m_positiveValueColor);
            }
            else if (difference < 0)
            {
                // Cost savings (green)
                element.setTextColor(m_negativeValueColor);
            }

            cell.addElement(element);
        }
        else
        {
            styleTableCell(detailedTable, rowIndex, 3,
                           tr("N/A"));
        }
    }
    else
    {
        styleTableCell(detailedTable, rowIndex, 2,
                       tr("Not simulated"));
        styleTableCell(detailedTable, rowIndex, 3,
                       tr("N/A"));
    }
    rowIndex++;

    // Energy consumption cost
    styleTableCell(detailedTable, rowIndex, 0,
                   tr("Energy Consumption"), false, true);
    styleTableCell(
        detailedTable, rowIndex, 1,
        QString::number(predictedEnergyCost, 'f', 2));

    if (hasActualData)
    {
        styleTableCell(
            detailedTable, rowIndex, 2,
            QString::number(actualEnergyCost, 'f', 2));

        // Calculate percentage difference
        if (predictedEnergyCost > 0)
        {
            double difference =
                ((actualEnergyCost - predictedEnergyCost)
                 / predictedEnergyCost)
                * 100.0;

            // Format with + sign for positive differences
            QString diffText;
            if (difference > 0)
            {
                diffText = QString("+%1%").arg(difference,
                                               0, 'f', 2);
            }
            else
            {
                diffText = QString("%1%").arg(difference, 0,
                                              'f', 2);
            }

            // Color-code the difference cell
            KDReports::Cell &cell =
                detailedTable.cell(rowIndex, 3);
            KDReports::TextElement element(diffText);

            if (difference > 0)
            {
                // Cost increase (red)
                element.setTextColor(m_positiveValueColor);
            }
            else if (difference < 0)
            {
                // Cost savings (green)
                element.setTextColor(m_negativeValueColor);
            }

            cell.addElement(element);
        }
        else
        {
            styleTableCell(detailedTable, rowIndex, 3,
                           tr("N/A"));
        }
    }
    else
    {
        styleTableCell(detailedTable, rowIndex, 2,
                       tr("Not simulated"));
        styleTableCell(detailedTable, rowIndex, 3,
                       tr("N/A"));
    }
    rowIndex++;

    // Risk-based cost
    styleTableCell(detailedTable, rowIndex, 0,
                   tr("Risk-based"), false, true);
    styleTableCell(
        detailedTable, rowIndex, 1,
        QString::number(predictedRiskCost, 'f', 6));

    if (hasActualData)
    {
        styleTableCell(
            detailedTable, rowIndex, 2,
            QString::number(actualRiskCost, 'f', 6));

        // Calculate percentage difference
        if (predictedRiskCost > 0)
        {
            double difference =
                ((actualRiskCost - predictedRiskCost)
                 / predictedRiskCost)
                * 100.0;

            // Format with + sign for positive differences
            QString diffText;
            if (difference > 0)
            {
                diffText = QString("+%1%").arg(difference,
                                               0, 'f', 2);
            }
            else
            {
                diffText = QString("%1%").arg(difference, 0,
                                              'f', 2);
            }

            // Color-code the difference cell
            KDReports::Cell &cell =
                detailedTable.cell(rowIndex, 3);
            KDReports::TextElement element(diffText);

            if (difference > 0)
            {
                // Cost increase (red)
                element.setTextColor(m_positiveValueColor);
            }
            else if (difference < 0)
            {
                // Cost savings (green)
                element.setTextColor(m_negativeValueColor);
            }

            cell.addElement(element);
        }
        else
        {
            styleTableCell(detailedTable, rowIndex, 3,
                           tr("N/A"));
        }
    }
    else
    {
        styleTableCell(detailedTable, rowIndex, 2,
                       tr("Not simulated"));
        styleTableCell(detailedTable, rowIndex, 3,
                       tr("N/A"));
    }
    rowIndex++;

    // Travel time cost
    styleTableCell(detailedTable, rowIndex, 0,
                   tr("Travel Time"), false, true);
    styleTableCell(
        detailedTable, rowIndex, 1,
        QString::number(predictedTimeCost, 'f', 2));

    if (hasActualData)
    {
        styleTableCell(
            detailedTable, rowIndex, 2,
            QString::number(actualTimeCost, 'f', 2));

        // Calculate percentage difference
        if (predictedTimeCost > 0)
        {
            double difference =
                ((actualTimeCost - predictedTimeCost)
                 / predictedTimeCost)
                * 100.0;

            // Format with + sign for positive differences
            QString diffText;
            if (difference > 0)
            {
                diffText = QString("+%1%").arg(difference,
                                               0, 'f', 2);
            }
            else
            {
                diffText = QString("%1%").arg(difference, 0,
                                              'f', 2);
            }

            // Color-code the difference cell
            KDReports::Cell &cell =
                detailedTable.cell(rowIndex, 3);
            KDReports::TextElement element(diffText);

            if (difference > 0)
            {
                // Cost increase (red)
                element.setTextColor(m_positiveValueColor);
            }
            else if (difference < 0)
            {
                // Cost savings (green)
                element.setTextColor(m_negativeValueColor);
            }

            cell.addElement(element);
        }
        else
        {
            styleTableCell(detailedTable, rowIndex, 3,
                           tr("N/A"));
        }
    }
    else
    {
        styleTableCell(detailedTable, rowIndex, 2,
                       tr("Not simulated"));
        styleTableCell(detailedTable, rowIndex, 3,
                       tr("N/A"));
    }

    // Add table to report
    report->addElement(detailedTable);

    report->addVerticalSpacing(10);

    // Add section for segment-specific cost analysis if
    // needed
    if (segments.size() > 1)
    {
        // Add section title for segment-specific costs
        KDReports::TextElement segmentTitle(
            tr("Segment-Specific Cost Analysis"));
        segmentTitle.setFont(m_normalTextFont);
        segmentTitle.setBold(true);
        segmentTitle.setTextColor(m_subtitleColor);
        report->addElement(segmentTitle);

        report->addVerticalSpacing(5);

        // Create a chart element if KDReports supports it
        // Note: This would require KDChart integration
        if (pathData->m_totalSimulationPathCost >= 0)
        {
            // Add informational text about segment costs
            KDReports::TextElement chartInfo(
                tr("The following table breaks down costs "
                   "by individual segments. "
                   "This allows you to identify which "
                   "segments contribute most to "
                   "the overall cost differential."));
            chartInfo.setFont(m_normalTextFont);
            report->addElement(chartInfo);

            report->addVerticalSpacing(5);

            // Create table for segment breakdown
            KDReports::TableElement segmentTable;
            segmentTable.setHeaderColumnCount(5);
            segmentTable.setHeaderRowCount(1);
            segmentTable.setBorder(1);
            segmentTable.setBorderBrush(
                QBrush(m_tableBorderColor));

            // Set header
            styleTableCell(segmentTable, 0, 0,
                           tr("Segment"), true);
            styleTableCell(segmentTable, 0, 1, tr("Route"),
                           true);
            styleTableCell(segmentTable, 0, 2,
                           tr("Predicted Cost"), true);
            styleTableCell(segmentTable, 0, 3,
                           tr("Actual Cost"), true);
            styleTableCell(segmentTable, 0, 4,
                           tr("Difference (%)"), true);

            // Add row for each segment
            for (int i = 0; i < segments.size(); ++i)
            {
                int segRowIndex = i + 1;

                // Segment number
                styleTableCell(segmentTable, segRowIndex, 0,
                               tr("Segment %1").arg(i + 1),
                               false, true);

                if (segments[i])
                {
                    // Route info
                    QString routeInfo =
                        QString("%1 → %2 (%3)")
                            .arg(getTerminalDisplayNameByID(
                                pathData->path.get(),
                                segments[i]->getStart()))
                            .arg(getTerminalDisplayNameByID(
                                pathData->path.get(),
                                segments[i]->getEnd()))
                            .arg(
                                Backend::TransportationTypes::
                                    toString(
                                        segments[i]
                                            ->getMode()));
                    styleTableCell(segmentTable,
                                   segRowIndex, 1,
                                   routeInfo);

                    const auto estimatedCostObj =
                        segments[i]->estimatedCosts();
                    const auto actualCostObj =
                        m_viewModel.segmentActualCosts(pathData, i);
                    const double segPredictedCost =
                        estimatedCostObj.available
                            ? estimatedCostObj.directCost
                            : 0.0;
                    const double segActualCost =
                        actualCostObj.available
                            ? actualCostObj.directCost
                            : 0.0;

                    // Add cost data
                    styleTableCell(
                        segmentTable, segRowIndex, 2,
                        QString::number(segPredictedCost,
                                        'f', 2));

                    if (segActualCost > 0)
                    {
                        styleTableCell(
                            segmentTable, segRowIndex, 3,
                            QString::number(segActualCost,
                                            'f', 2));

                        // Calculate percentage difference
                        if (segPredictedCost > 0)
                        {
                            double difference =
                                ((segActualCost
                                  - segPredictedCost)
                                 / segPredictedCost)
                                * 100.0;

                            // Format with + sign for
                            // positive differences
                            QString diffText;
                            if (difference > 0)
                            {
                                diffText =
                                    QString("+%1%").arg(
                                        difference, 0, 'f',
                                        2);
                            }
                            else
                            {
                                diffText =
                                    QString("%1%").arg(
                                        difference, 0, 'f',
                                        2);
                            }

                            // Color-code the difference
                            // cell
                            KDReports::Cell &cell =
                                segmentTable.cell(
                                    segRowIndex, 4);
                            KDReports::TextElement element(
                                diffText);

                            if (difference > 0)
                            {
                                // Cost increase (red)
                                element.setTextColor(
                                    m_positiveValueColor);
                            }
                            else if (difference < 0)
                            {
                                // Cost savings (green)
                                element.setTextColor(
                                    m_negativeValueColor);
                            }

                            cell.addElement(element);
                        }
                        else
                        {
                            styleTableCell(segmentTable,
                                           segRowIndex, 4,
                                           tr("N/A"));
                        }
                    }
                    else
                    {
                        styleTableCell(segmentTable,
                                       segRowIndex, 3,
                                       tr("Not simulated"));
                        styleTableCell(segmentTable,
                                       segRowIndex, 4,
                                       tr("N/A"));
                    }
                }
                else
                {
                    // Handle null segment
                    styleTableCell(segmentTable,
                                   segRowIndex, 1,
                                   tr("N/A"));
                    styleTableCell(segmentTable,
                                   segRowIndex, 2,
                                   tr("N/A"));
                    styleTableCell(segmentTable,
                                   segRowIndex, 3,
                                   tr("N/A"));
                    styleTableCell(segmentTable,
                                   segRowIndex, 4,
                                   tr("N/A"));
                }
            }

            // Add segment table to report
            report->addElement(segmentTable);
        }
        else
        {
            // Message for non-simulated paths
            KDReports::TextElement noSimData(
                tr("No simulation data available for "
                   "segment-specific cost analysis."));
            noSimData.setFont(m_normalTextFont);
            noSimData.setItalic(true);
            report->addElement(noSimData);
        }
    }
}

void PathReportGenerator::addComparativeAnalysis(
    KDReports::Report *report)
{
    qCDebug(lcGuiUtil) << "PathReportGenerator::addComparativeAnalysis: paths"
                       << m_pathData.size();
    // Add a bookmark for the comparative analysis section
    // report.addBookmark("comparative_analysis");

    // Main title
    KDReports::TextElement title(
        tr("Comparative Analysis"));
    title.setFont(m_pageTitleFont);
    title.setTextColor(m_titleColor);
    report->addElement(title, Qt::AlignCenter);

    report->addVerticalSpacing(10);

    // Add each comparison section
    addSummaryComparisonTable(report);
    report->addPageBreak();

    addTerminalComparisonTable(report);
    report->addPageBreak();

    addSegmentComparisonTable(report);
    report->addPageBreak();

    addCostComparisonTable(report);
    report->addPageBreak();

    addSegmentAttributeComparisonTables(report);
    report->addPageBreak();

    addSegmentCostComparisonTables(report);
    report->addPageBreak();
}

void PathReportGenerator::addSummaryComparisonTable(
    KDReports::Report *report)
{
    qCDebug(lcGuiUtil) << "PathReportGenerator::addSummaryComparisonTable";
    // Section title
    KDReports::TextElement sectionTitle(
        tr("Summary Comparison"));
    sectionTitle.setFont(m_sectionTitleFont);
    sectionTitle.setTextColor(m_subtitleColor);
    report->addElement(sectionTitle);

    report->addVerticalSpacing(5);

    // Create column headers (Path ID 1, Path ID 2, etc.)
    QStringList headers;
    headers.append(tr("Property")); // First column header

    for (const auto &path : m_pathData)
    {
        if (path && path->path)
        {
            headers.append(
                tr("Path %1").arg(path->path->getPathId()));
        }
        else
        {
            headers.append(tr("Unknown Path"));
        }
    }

    // Create table
    KDReports::TableElement table;
    table.setHeaderRowCount(1);
    table.setBorder(1);
    table.setBorderBrush(QBrush(m_tableBorderColor));

    // Set headers
    for (int col = 0; col < headers.size(); ++col)
    {
        styleTableCell(table, 0, col, headers[col], true);
    }

    // Create row labels for the summary data
    QStringList rowLabels = {
        tr("Path ID"),        tr("Total Terminals"),
        tr("Total Segments"), tr("Predicted Cost"),
        tr("Actual Cost"),    tr("Start Terminal"),
        tr("End Terminal")};

    // Add data rows
    for (int row = 0; row < rowLabels.size(); ++row)
    {
        int tableRow = row + 1;

        // Set row label
        styleTableCell(table, tableRow, 0, rowLabels[row],
                       false, true);

        // Set data cells
        for (int col = 0; col < m_pathData.size(); ++col)
        {
            const auto &path     = m_pathData[col];
            int         tableCol = col + 1;

            if (path && path->path)
            {
                switch (row)
                {
                case 0: // Path ID
                    styleTableCell(
                        table, tableRow, tableCol,
                        QString::number(
                            path->path->getPathId()));
                    break;
                case 1: // Total terminals
                    styleTableCell(
                        table, tableRow, tableCol,
                        QString::number(
                            path->path->getTerminalsInPath()
                                .size()));
                    break;
                case 2: // Total segments
                    styleTableCell(
                        table, tableRow, tableCol,
                        QString::number(
                            path->path->getSegments()
                                .size()));
                    break;
                case 3: // Predicted Cost
                    styleTableCell(
                        table, tableRow, tableCol,
                        QString::number(
                            path->path->getTotalPathCost(),
                            'f', 2));
                    break;
                case 4: // Actual Cost
                    if (path->m_totalSimulationPathCost
                        >= 0)
                    {
                        styleTableCell(
                            table, tableRow, tableCol,
                            QString::number(
                                path->m_totalSimulationPathCost,
                                'f', 2));
                    }
                    else
                    {
                        styleTableCell(table, tableRow,
                                       tableCol,
                                       tr("Not simulated"));
                    }
                    break;
                case 5: // Start Terminal
                {
                    const auto &t =
                        path->path->getTerminalsInPath();
                    styleTableCell(
                        table, tableRow, tableCol,
                        t.isEmpty() ? tr("Unknown")
                                    : t.first().displayName);
                    break;
                }
                case 6: // End Terminal
                {
                    const auto &t =
                        path->path->getTerminalsInPath();
                    styleTableCell(
                        table, tableRow, tableCol,
                        t.isEmpty() ? tr("Unknown")
                                    : t.last().displayName);
                    break;
                }
                }
            }
            else
            {
                styleTableCell(table, tableRow, tableCol,
                               tr("N/A"));
            }
        }
    }

    // Add table to report
    report->addElement(table);
}

void PathReportGenerator::addTerminalComparisonTable(
    KDReports::Report *report)
{
    qCDebug(lcGuiUtil) << "PathReportGenerator::addTerminalComparisonTable";
    // Section title
    KDReports::TextElement sectionTitle(
        tr("Terminal Comparison"));
    sectionTitle.setFont(m_sectionTitleFont);
    sectionTitle.setTextColor(m_subtitleColor);
    report->addElement(sectionTitle);

    report->addVerticalSpacing(5);

    // Create column headers (Path ID 1, Path ID 2, etc.)
    QStringList headers;
    headers.append(tr("Terminal")); // First column header

    for (const auto &path : m_pathData)
    {
        if (path && path->path)
        {
            headers.append(
                tr("Path %1").arg(path->path->getPathId()));
        }
        else
        {
            headers.append(tr("Unknown Path"));
        }
    }

    // Find the maximum number of terminals across all paths
    int maxTerminals = 0;
    for (const auto &path : m_pathData)
    {
        if (path && path->path)
        {
            maxTerminals = qMax(
                maxTerminals,
                path->path->getTerminalsInPath().size());
        }
    }

    if (maxTerminals == 0)
    {
        // No terminals to display
        KDReports::TextElement noData(tr(
            "No terminal data available for comparison."));
        noData.setFont(m_normalTextFont);
        report->addElement(noData);
        return;
    }

    // Create table
    KDReports::TableElement table;
    table.setHeaderRowCount(1);
    table.setBorder(1);
    table.setBorderBrush(QBrush(m_tableBorderColor));

    // Set headers
    for (int col = 0; col < headers.size(); ++col)
    {
        styleTableCell(table, 0, col, headers[col], true);
    }

    // Create row labels and add data
    for (int i = 0; i < maxTerminals; ++i)
    {
        int tableRow = i + 1;

        // Terminal index as row label
        styleTableCell(table, tableRow, 0,
                       tr("Terminal %1").arg(i + 1), false,
                       true);

        // Add terminal data for each path
        for (int col = 0; col < m_pathData.size(); ++col)
        {
            const auto &path     = m_pathData[col];
            int         tableCol = col + 1;

            if (path && path->path)
            {
                const auto &terminals =
                    path->path->getTerminalsInPath();

                if (i < terminals.size())
                {
                    styleTableCell(
                        table, tableRow, tableCol,
                        terminals[i].displayName);
                }
                else
                {
                    styleTableCell(table, tableRow,
                                   tableCol, tr("-"));
                }
            }
            else
            {
                styleTableCell(table, tableRow, tableCol,
                               tr("N/A"));
            }
        }
    }

    // Add table to report
    report->addElement(table);
}

void PathReportGenerator::addSegmentComparisonTable(
    KDReports::Report *report)
{
    qCDebug(lcGuiUtil) << "PathReportGenerator::addSegmentComparisonTable";
    // Section title
    KDReports::TextElement sectionTitle(
        tr("Segment Comparison"));
    sectionTitle.setFont(m_sectionTitleFont);
    sectionTitle.setTextColor(m_subtitleColor);
    report->addElement(sectionTitle);

    report->addVerticalSpacing(5);

    // Create column headers (Path ID 1, Path ID 2, etc.)
    QStringList headers;
    headers.append(tr("Segment")); // First column header

    for (const auto &path : m_pathData)
    {
        if (path && path->path)
        {
            headers.append(
                tr("Path %1").arg(path->path->getPathId()));
        }
        else
        {
            headers.append(tr("Unknown Path"));
        }
    }

    // Find the maximum number of segments across all paths
    int maxSegments = 0;
    for (const auto &path : m_pathData)
    {
        if (path && path->path)
        {
            maxSegments =
                qMax(maxSegments,
                     path->path->getSegments().size());
        }
    }

    if (maxSegments == 0)
    {
        // No segments to display
        KDReports::TextElement noData(tr(
            "No segment data available for comparison."));
        noData.setFont(m_normalTextFont);
        report->addElement(noData);
        return;
    }

    // Create table
    KDReports::TableElement table;
    table.setHeaderRowCount(1);
    table.setBorder(1);
    table.setBorderBrush(QBrush(m_tableBorderColor));

    // Set headers
    for (int col = 0; col < headers.size(); ++col)
    {
        styleTableCell(table, 0, col, headers[col], true);
    }

    // Create row labels and add data
    for (int i = 0; i < maxSegments; ++i)
    {
        int tableRow = i + 1;

        // Segment index as row label
        styleTableCell(table, tableRow, 0,
                       tr("Segment %1").arg(i + 1), false,
                       true);

        // Add segment data for each path
        for (int col = 0; col < m_pathData.size(); ++col)
        {
            const auto &path     = m_pathData[col];
            int         tableCol = col + 1;

            if (path && path->path)
            {
                const auto &segments =
                    path->path->getSegments();

                if (i < segments.size() && segments[i])
                {
                    // Format: "Start → End (Mode)"
                    QString segmentInfo =
                        QString("%1 → %2 (%3)")
                            .arg(getTerminalDisplayNameByID(
                                path->path.get(),
                                segments[i]->getStart()))
                            .arg(getTerminalDisplayNameByID(
                                path->path.get(),
                                segments[i]->getEnd()))
                            .arg(
                                Backend::TransportationTypes::
                                    toString(
                                        segments[i]
                                            ->getMode()));

                    styleTableCell(table, tableRow,
                                   tableCol, segmentInfo);
                }
                else
                {
                    styleTableCell(table, tableRow,
                                   tableCol, tr("-"));
                }
            }
            else
            {
                styleTableCell(table, tableRow, tableCol,
                               tr("N/A"));
            }
        }
    }

    // Add table to report
    report->addElement(table);
}

void PathReportGenerator::addCostComparisonTable(
    KDReports::Report *report)
{
    qCDebug(lcGuiUtil) << "PathReportGenerator::addCostComparisonTable";
    // Section title
    KDReports::TextElement sectionTitle(
        tr("Cost Comparison"));
    sectionTitle.setFont(m_sectionTitleFont);
    sectionTitle.setTextColor(m_subtitleColor);
    report->addElement(sectionTitle);

    report->addVerticalSpacing(5);

    // Create column headers (Path ID 1, Path ID 2, etc.)
    QStringList headers;
    headers.append(tr("Cost Type")); // First column header

    for (const auto &path : m_pathData)
    {
        if (path && path->path)
        {
            headers.append(
                tr("Path %1").arg(path->path->getPathId()));
        }
        else
        {
            headers.append(tr("Unknown Path"));
        }
    }

    // Create row labels for cost categories
    QStringList rowLabels = {
        tr("Predicted Total"),    tr("Predicted Edge"),
        tr("Predicted Terminal"), tr("Simulated Total"),
        tr("Simulated Edge"),     tr("Simulated Terminal"),
        tr("Difference (%)")};

    // Create table
    KDReports::TableElement table;
    table.setHeaderRowCount(1);
    table.setBorder(1);
    table.setBorderBrush(QBrush(m_tableBorderColor));

    // Set headers
    for (int col = 0; col < headers.size(); ++col)
    {
        styleTableCell(table, 0, col, headers[col], true);
    }

    // Add data rows
    for (int row = 0; row < rowLabels.size(); ++row)
    {
        int tableRow = row + 1;

        // Cost type as row label
        styleTableCell(table, tableRow, 0, rowLabels[row],
                       false, true);

        // Add cost data for each path
        for (int col = 0; col < m_pathData.size(); ++col)
        {
            const auto &path     = m_pathData[col];
            int         tableCol = col + 1;

            if (path && path->path)
            {
                // Handle different cost categories
                switch (row)
                {
                case 0: // Predicted Total
                    styleTableCell(
                        table, tableRow, tableCol,
                        QString::number(
                            path->path->getTotalPathCost(),
                            'f', 2));
                    break;
                case 1: // Predicted Edge
                    styleTableCell(
                        table, tableRow, tableCol,
                        QString::number(
                            path->path->getTotalEdgeCosts(),
                            'f', 2));
                    break;
                case 2: // Predicted Terminal
                    styleTableCell(
                        table, tableRow, tableCol,
                        QString::number(
                            path->path
                                ->getTotalTerminalCosts(),
                            'f', 2));
                    break;
                case 3: // Simulated Total
                    if (path->m_totalSimulationPathCost
                        >= 0)
                    {
                        styleTableCell(
                            table, tableRow, tableCol,
                            QString::number(
                                path->m_totalSimulationPathCost,
                                'f', 2));
                    }
                    else
                    {
                        styleTableCell(table, tableRow,
                                       tableCol,
                                       tr("Not simulated"));
                    }
                    break;
                case 4: // Simulated Edge
                    if (path->m_totalSimulationEdgeCosts
                        >= 0)
                    {
                        styleTableCell(
                            table, tableRow, tableCol,
                            QString::number(
                                path->m_totalSimulationEdgeCosts,
                                'f', 2));
                    }
                    else
                    {
                        styleTableCell(table, tableRow,
                                       tableCol,
                                       tr("Not simulated"));
                    }
                    break;
                case 5: // Simulated Terminal
                    if (path->m_totalSimulationTerminalCosts
                        >= 0)
                    {
                        styleTableCell(
                            table, tableRow, tableCol,
                            QString::number(
                                path->m_totalSimulationTerminalCosts,
                                'f', 2));
                    }
                    else
                    {
                        styleTableCell(table, tableRow,
                                       tableCol,
                                       tr("Not simulated"));
                    }
                    break;
                case 6: // Difference (%)
                    if (path->m_totalSimulationPathCost >= 0
                        && path->path->getTotalPathCost()
                               > 0)
                    {
                        double predictedCost =
                            path->path->getTotalPathCost();
                        double simulatedCost =
                            path->m_totalSimulationPathCost;
                        double difference =
                            ((simulatedCost - predictedCost)
                             / predictedCost)
                            * 100.0;

                        // Format with + sign for positive
                        // differences
                        QString diffText;
                        if (difference > 0)
                        {
                            diffText = QString("+%1%").arg(
                                difference, 0, 'f', 2);
                        }
                        else
                        {
                            diffText = QString("%1%").arg(
                                difference, 0, 'f', 2);
                        }

                        // Color-code the difference cell
                        KDReports::Cell &cell =
                            table.cell(tableRow, tableCol);
                        KDReports::TextElement element(
                            diffText);

                        if (difference > 0)
                        {
                            // Cost increase (red)
                            element.setTextColor(
                                m_positiveValueColor);
                        }
                        else if (difference < 0)
                        {
                            // Cost savings (green)
                            element.setTextColor(
                                m_negativeValueColor);
                        }

                        cell.addElement(element);
                    }
                    else
                    {
                        styleTableCell(table, tableRow,
                                       tableCol, tr("N/A"));
                    }
                    break;
                }
            }
            else
            {
                styleTableCell(table, tableRow, tableCol,
                               tr("N/A"));
            }
        }
    }

    // Add table to report
    report->addElement(table);
}

void PathReportGenerator::
    addSegmentAttributeComparisonTables(
        KDReports::Report *report)
{
    // Section title
    KDReports::TextElement sectionTitle(
        tr("Segment-by-Segment Attribute Comparison"));
    sectionTitle.setFont(m_sectionTitleFont);
    sectionTitle.setTextColor(m_subtitleColor);
    report->addElement(sectionTitle);

    report->addVerticalSpacing(5);

    // Find the maximum number of segments across all paths
    int maxSegments = 0;
    for (const auto &path : m_pathData)
    {
        if (path && path->path)
        {
            maxSegments =
                qMax(maxSegments,
                     path->path->getSegments().size());
        }
    }

    if (maxSegments == 0)
    {
        // No segments to display
        KDReports::TextElement noData(tr(
            "No segment data available for comparison."));
        noData.setFont(m_normalTextFont);
        report->addElement(noData);
        return;
    }

    // For each segment position (first, second, etc.),
    // create a comparison table
    for (int segmentIdx = 0; segmentIdx < maxSegments;
         ++segmentIdx)
    {
        // Create section for this segment position
        KDReports::TextElement segmentSectionTitle(
            tr("Segment %1 Comparison")
                .arg(segmentIdx + 1));
        segmentSectionTitle.setFont(m_normalTextFont);
        segmentSectionTitle.setBold(true);
        report->addElement(segmentSectionTitle);

        report->addVerticalSpacing(3);

        // First, show a descriptive text about these
        // segments
        QString segmentDescText;
        for (const auto &path : m_pathData)
        {
            if (path && path->path)
            {
                const auto &segments =
                    path->path->getSegments();
                if (segmentIdx < segments.size()
                    && segments[segmentIdx])
                {
                    if (!segmentDescText.isEmpty())
                        segmentDescText += tr("\n");

                    segmentDescText +=
                        tr("Path %1: %2 → %3 (%4)")
                            .arg(path->path->getPathId())
                            .arg(getTerminalDisplayNameByID(
                                path->path.get(),
                                segments[segmentIdx]
                                    ->getStart()))
                            .arg(getTerminalDisplayNameByID(
                                path->path.get(),
                                segments[segmentIdx]
                                    ->getEnd()))
                            .arg(
                                Backend::TransportationTypes::
                                    toString(
                                        segments[segmentIdx]
                                            ->getMode()));
                }
            }
        }

        if (!segmentDescText.isEmpty())
        {
            KDReports::TextElement descElement(
                segmentDescText);
            descElement.setFont(m_smallTextFont);
            descElement.setItalic(true);
            report->addElement(descElement);
            report->addVerticalSpacing(5);
        }

        // Create attribution comparison table for this
        // segment position
        addSegmentPositionAttributeTable(report,
                                         segmentIdx);

        // Add space between segment sections
        report->addPageBreak();

        // // Add a separator line between segment
        // // comparisons
        // if (segmentIdx < maxSegments - 1)
        // {
        //     KDReports::HLineElement line;
        //     line.setThickness(1);
        //     line.setColor(m_tableBorderColor);
        //     report->addElement(line, Qt::AlignCenter);
        //     report->addVerticalSpacing(10);
        // }
    }
}

void PathReportGenerator::addSegmentPositionAttributeTable(
    KDReports::Report *report, int segmentIdx)
{
    qCDebug(lcGuiUtil) << "PathReportGenerator::addSegmentPositionAttributeTable: segment"
                       << segmentIdx;
    // Create column headers (Path ID 1, Path ID 2, etc.)
    QStringList headers;
    headers.append(tr("Attribute")); // First column header

    for (const auto &path : m_pathData)
    {
        if (path && path->path)
        {
            headers.append(
                tr("Path %1").arg(path->path->getPathId()));
        }
    }

    // Create table for attribute comparison
    KDReports::TableElement table;
    table.setHeaderRowCount(1);
    table.setBorder(1);
    table.setBorderBrush(QBrush(m_tableBorderColor));

    // Set headers
    for (int col = 0; col < headers.size(); ++col)
    {
        styleTableCell(table, 0, col, headers[col], true);
    }

    // Row labels for attributes (with both predicted and
    // actual values)
    QStringList attributeLabels = {
        tr("Carbon Emissions (Predicted)"),
        tr("Carbon Emissions (Actual)"),
        tr("Cost (Predicted)"),
        tr("Cost (Actual)"),
        tr("Distance (Predicted)"),
        tr("Distance (Actual)"),
        tr("Energy Consumption (Predicted)"),
        tr("Energy Consumption (Actual)"),
        tr("Risk (Predicted)"),
        tr("Risk (Actual)"),
        tr("Travel Time (Predicted)"),
        tr("Travel Time (Actual)")};

    // For each attribute row
    for (int rowIdx = 0; rowIdx < attributeLabels.size();
         ++rowIdx)
    {
        int tableRow = rowIdx + 1;

        // Attribute name
        styleTableCell(table, tableRow, 0,
                       attributeLabels[rowIdx], false,
                       true);

        // For each path, add the attribute value
        for (int col = 0; col < m_pathData.size(); ++col)
        {
            const auto &path     = m_pathData[col];
            int         tableCol = col + 1;

            if (path && path->path)
            {
                const auto &segments =
                    path->path->getSegments();

                if (segmentIdx < segments.size()
                    && segments[segmentIdx])
                {
                    const auto values =
                        m_viewModel.segmentValues(path, segmentIdx);

                    // Handle different attribute types
                    if (rowIdx % 2 == 0) // Even rows are
                                         // predicted values
                    {
                        QString valueText = tr("N/A");
                        switch (rowIdx)
                        {
                        case 0:
                            if (values.predictedAvailable)
                                valueText = QString::number(
                                    values.predictedCarbonEmissions,
                                    'g', 3);
                            break;
                        case 2:
                            valueText = tr("N/A");
                            break;
                        case 4:
                            if (values.predictedAvailable)
                                valueText = QString::number(
                                    values.predictedDistanceKm,
                                    'g', 2);
                            break;
                        case 6:
                            if (values.predictedAvailable)
                                valueText = QString::number(
                                    values.predictedEnergyConsumption,
                                    'g', 2);
                            break;
                        case 8:
                            if (values.predictedAvailable)
                                valueText = QString::number(
                                    values.predictedRisk, 'g', 6);
                            break;
                        case 10:
                            if (values.predictedAvailable)
                                valueText = QString::number(
                                    values.predictedTravelTimeHours,
                                    'g', 2);
                            break;
                        }
                        styleTableCell(table, tableRow,
                                       tableCol, valueText);
                    }
                    else // Odd rows are actual values
                    {
                        QString valueText = tr("N/A");
                        switch (rowIdx)
                        {
                        case 1:
                            if (values.actualAvailable)
                                valueText = QString::number(
                                    values.actualCarbonEmissions,
                                    'g', 3);
                            break;
                        case 3:
                            valueText = tr("N/A");
                            break;
                        case 5:
                            if (values.actualAvailable)
                                valueText = QString::number(
                                    values.actualDistanceKm, 'g', 2);
                            break;
                        case 7:
                            if (values.actualAvailable)
                                valueText = QString::number(
                                    values.actualEnergyConsumption,
                                    'g', 2);
                            break;
                        case 9:
                            if (values.actualAvailable)
                                valueText = QString::number(
                                    values.actualRisk, 'g', 6);
                            break;
                        case 11:
                            if (values.actualAvailable)
                                valueText = QString::number(
                                    values.actualTravelTimeHours,
                                    'g', 2);
                            break;
                        }
                        styleTableCell(table, tableRow,
                                       tableCol, valueText);
                    }
                }
                else
                {
                    styleTableCell(table, tableRow,
                                   tableCol, tr("-"));
                }
            }
            else
            {
                styleTableCell(table, tableRow, tableCol,
                               tr("N/A"));
            }
        }
    }

    // Add table to report
    report->addElement(table);
}

void PathReportGenerator::addSegmentCostComparisonTables(
    KDReports::Report *report)
{
    qCDebug(lcGuiUtil) << "PathReportGenerator::addSegmentCostComparisonTables";
    // Section title
    KDReports::TextElement sectionTitle(
        tr("Segment-by-Segment Cost Comparison"));
    sectionTitle.setFont(m_sectionTitleFont);
    sectionTitle.setTextColor(m_subtitleColor);
    report->addElement(sectionTitle);

    report->addVerticalSpacing(5);

    // Find the maximum number of segments across all paths
    int maxSegments = 0;
    for (const auto &path : m_pathData)
    {
        if (path && path->path)
        {
            maxSegments =
                qMax(maxSegments,
                     path->path->getSegments().size());
        }
    }

    if (maxSegments == 0)
    {
        // No segments to display
        KDReports::TextElement noData(
            tr("No segment data available for cost "
               "comparison."));
        noData.setFont(m_normalTextFont);
        report->addElement(noData);
        return;
    }

    // For each segment position (first, second, etc.),
    // create a cost comparison table
    for (int segmentIdx = 0; segmentIdx < maxSegments;
         ++segmentIdx)
    {
        // Create section for this segment position
        KDReports::TextElement segmentSectionTitle(
            tr("Segment %1 Cost Comparison")
                .arg(segmentIdx + 1));
        segmentSectionTitle.setFont(m_normalTextFont);
        segmentSectionTitle.setBold(true);
        report->addElement(segmentSectionTitle);

        report->addVerticalSpacing(3);

        // First, show a descriptive text about these
        // segments
        QString segmentDescText;
        for (const auto &path : m_pathData)
        {
            if (path && path->path)
            {
                const auto &segments =
                    path->path->getSegments();
                if (segmentIdx < segments.size()
                    && segments[segmentIdx])
                {
                    if (!segmentDescText.isEmpty())
                        segmentDescText += tr("\n");

                    segmentDescText +=
                        tr("Path %1: %2 → %3 (%4)")
                            .arg(path->path->getPathId())
                            .arg(getTerminalDisplayNameByID(
                                path->path.get(),
                                segments[segmentIdx]
                                    ->getStart()))
                            .arg(getTerminalDisplayNameByID(
                                path->path.get(),
                                segments[segmentIdx]
                                    ->getEnd()))
                            .arg(
                                Backend::TransportationTypes::
                                    toString(
                                        segments[segmentIdx]
                                            ->getMode()));
                }
            }
        }

        if (!segmentDescText.isEmpty())
        {
            KDReports::TextElement descElement(
                segmentDescText);
            descElement.setFont(m_smallTextFont);
            descElement.setItalic(true);
            report->addElement(descElement);
            report->addVerticalSpacing(5);
        }

        // Create cost comparison table for this segment
        // position
        addSegmentPositionCostTable(report, segmentIdx);

        // Add space between segment sections
        report->addPageBreak();

        // // Add a separator line between segment
        // comparisons if (segmentIdx < maxSegments - 1)
        // {
        //     KDReports::HLineElement line;
        //     line.setThickness(1);
        //     line.setColor(m_tableBorderColor);
        //     report->addElement(line, Qt::AlignCenter);
        //     report->addVerticalSpacing(10);
        // }
    }
}

void PathReportGenerator::addSegmentPositionCostTable(
    KDReports::Report *report, int segmentIdx)
{
    qCDebug(lcGuiUtil) << "PathReportGenerator::addSegmentPositionCostTable: segment"
                       << segmentIdx;
    // Create column headers (Path ID 1, Path ID 2, etc.)
    QStringList headers;
    headers.append(
        tr("Cost Category")); // First column header

    for (const auto &path : m_pathData)
    {
        if (path && path->path)
        {
            headers.append(
                tr("Path %1").arg(path->path->getPathId()));
        }
    }

    // Create table for cost comparison
    KDReports::TableElement table;
    table.setHeaderRowCount(1);
    table.setBorder(1);
    table.setBorderBrush(QBrush(m_tableBorderColor));

    // Set headers
    for (int col = 0; col < headers.size(); ++col)
    {
        styleTableCell(table, 0, col, headers[col], true);
    }

    // Row labels for cost categories (with both predicted
    // and actual values)
    QStringList costLabels = {
        tr("Carbon Emissions Cost (Predicted)"),
        tr("Carbon Emissions Cost (Actual)"),
        tr("Direct Cost (Predicted)"),
        tr("Direct Cost (Actual)"),
        tr("Distance-based Cost (Predicted)"),
        tr("Distance-based Cost (Actual)"),
        tr("Energy Consumption Cost (Predicted)"),
        tr("Energy Consumption Cost (Actual)"),
        tr("Risk-based Cost (Predicted)"),
        tr("Risk-based Cost (Actual)"),
        tr("Travel Time Cost (Predicted)"),
        tr("Travel Time Cost (Actual)")};

    // For each cost category row
    for (int rowIdx = 0; rowIdx < costLabels.size();
         ++rowIdx)
    {
        int tableRow = rowIdx + 1;

        // Cost category name
        styleTableCell(table, tableRow, 0,
                       costLabels[rowIdx], false, true);

        // For each path, add the cost value
        for (int col = 0; col < m_pathData.size(); ++col)
        {
            const auto &path     = m_pathData[col];
            int         tableCol = col + 1;

            if (path && path->path)
            {
                const auto &segments =
                    path->path->getSegments();

                if (segmentIdx < segments.size()
                    && segments[segmentIdx])
                {
                    const auto estimatedCostObj =
                        m_viewModel.segmentPredictedCosts(
                            path, segmentIdx);
                    const auto actualCostObj =
                        m_viewModel.segmentActualCosts(
                            path, segmentIdx);

                    // Handle different cost categories
                    if (rowIdx % 2 == 0) // Even rows are
                                         // predicted costs
                    {
                        QString valueText = tr("N/A");
                        switch (rowIdx)
                        {
                        case 0:
                            if (estimatedCostObj.available)
                                valueText = QString::number(
                                    estimatedCostObj.carbonEmissions,
                                    'g', 2);
                            break;
                        case 2:
                            if (estimatedCostObj.available)
                                valueText = QString::number(
                                    estimatedCostObj.directCost,
                                    'g', 2);
                            break;
                        case 4:
                            if (estimatedCostObj.available)
                                valueText = QString::number(
                                    estimatedCostObj.distance,
                                    'g', 2);
                            break;
                        case 6:
                            if (estimatedCostObj.available)
                                valueText = QString::number(
                                    estimatedCostObj.energyConsumption,
                                    'g', 2);
                            break;
                        case 8:
                            if (estimatedCostObj.available)
                                valueText = QString::number(
                                    estimatedCostObj.risk, 'g', 6);
                            break;
                        case 10:
                            if (estimatedCostObj.available)
                                valueText = QString::number(
                                    estimatedCostObj.travelTime,
                                    'g', 2);
                            break;
                        }
                        styleTableCell(table, tableRow,
                                       tableCol, valueText);
                    }
                    else // Odd rows are actual costs
                    {
                        QString valueText = tr("N/A");
                        switch (rowIdx)
                        {
                        case 1:
                            if (actualCostObj.available)
                                valueText = QString::number(
                                    actualCostObj.carbonEmissions,
                                    'g', 2);
                            break;
                        case 3:
                            if (actualCostObj.available)
                                valueText = QString::number(
                                    actualCostObj.directCost,
                                    'g', 2);
                            break;
                        case 5:
                            if (actualCostObj.available)
                                valueText = QString::number(
                                    actualCostObj.distance,
                                    'g', 2);
                            break;
                        case 7:
                            if (actualCostObj.available)
                                valueText = QString::number(
                                    actualCostObj.energyConsumption,
                                    'g', 2);
                            break;
                        case 9:
                            if (actualCostObj.available)
                                valueText = QString::number(
                                    actualCostObj.risk, 'g', 6);
                            break;
                        case 11:
                            if (actualCostObj.available)
                                valueText = QString::number(
                                    actualCostObj.travelTime,
                                    'g', 2);
                            break;
                        }
                        styleTableCell(table, tableRow,
                                       tableCol, valueText);
                    }
                }
                else
                {
                    styleTableCell(table, tableRow,
                                   tableCol, tr("-"));
                }
            }
            else
            {
                styleTableCell(table, tableRow, tableCol,
                               tr("N/A"));
            }
        }
    }

    // Add a total row with a separator above it
    int totalRow = costLabels.size() + 1;

    // Add a total row label
    styleTableCell(table, totalRow, 0,
                   tr("Total Segment Cost"), false, true);

    // Calculate and display total cost for each path's
    // segment
    for (int col = 0; col < m_pathData.size(); ++col)
    {
        const auto &path     = m_pathData[col];
        int         tableCol = col + 1;

        if (path && path->path)
        {
            const auto &segments =
                path->path->getSegments();

            if (segmentIdx < segments.size()
                && segments[segmentIdx])
            {
                const auto estimatedCostObj =
                    m_viewModel.segmentPredictedCosts(
                        path, segmentIdx);
                const auto actualCostObj =
                    m_viewModel.segmentActualCosts(
                        path, segmentIdx);
                const double predictedTotal =
                    estimatedCostObj.available
                        ? estimatedCostObj.directCost
                        : 0.0;
                const double actualTotal =
                    actualCostObj.available
                        ? actualCostObj.directCost
                        : -1.0;

                // Display the costs
                QString costText;
                if (actualTotal >= 0)
                {
                    costText =
                        tr("%1 / %2")
                            .arg(QString::number(
                                predictedTotal, 'f', 2))
                            .arg(QString::number(
                                actualTotal, 'f', 2));
                }
                else
                {
                    costText = QString::number(
                                   predictedTotal, 'f', 2)
                               + " / -";
                }

                // Add cell with predicted/actual totals
                KDReports::Cell &cell =
                    table.cell(totalRow, tableCol);
                KDReports::TextElement element(costText);
                element.setFont(m_tableRowLabelFont);
                cell.addElement(element);
            }
            else
            {
                styleTableCell(table, totalRow, tableCol,
                               tr("-"));
            }
        }
        else
        {
            styleTableCell(table, totalRow, tableCol,
                           tr("N/A"));
        }
    }

    // Add table to report
    report->addElement(table);
}

void PathReportGenerator::styleTableCell(
    KDReports::TableElement &table, int row, int col,
    const QString &text, bool isHeader, bool rowLabel)
{
    KDReports::Cell       &cell = table.cell(row, col);
    KDReports::TextElement element(text);

    if (isHeader)
    {
        // Header cell
        element.setFont(m_tableHeaderFont);
        cell.setBackground(m_tableHeaderBgColor);
    }
    else if (rowLabel)
    {
        // Row label (first column)
        element.setFont(m_tableRowLabelFont);

        // Alternate row background for better readability
        if (row % 2 == 0)
        {
            cell.setBackground(
                QColor(245, 245, 245)); // Light gray
        }
    }
    else
    {
        // Normal data cell
        element.setFont(m_normalTextFont);

        // Alternate row background for better readability
        if (row % 2 == 0)
        {
            cell.setBackground(
                QColor(245, 245, 245)); // Light gray
        }
    }

    cell.addElement(element);
}

QImage PathReportGenerator::createTransportModeImage(
    const QString &mode)
{
    // Create an image for the transportation mode
    QImage image(64, 40, QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    QPainter painter(&image);
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
    painter.drawText(QRect(0, 0, image.width(), 15),
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

    return image;
}

QString PathReportGenerator::getTerminalDisplayNameByID(
    Backend::Path *path, const QString &terminalID)
{
    return m_viewModel.terminalDisplayName(path, terminalID);
}

} // namespace GUI
} // namespace CargoNetSim
