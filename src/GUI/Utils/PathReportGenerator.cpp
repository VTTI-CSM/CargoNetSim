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
#include <QFont>
#include <QLocale>
#include <QPainter>
#include <QPrinter>
#include <limits>

namespace CargoNetSim
{
namespace GUI
{

namespace
{

using CostSnapshot =
    CargoNetSim::Backend::Application::PathPresentationCostSnapshot;
using SegmentEntry =
    CargoNetSim::Backend::Application::PathPresentationSegmentEntry;
using PathMetrics = CargoNetSim::Backend::Scenario::PathMetrics;

QString formatFixed(double value, int precision)
{
    return QLocale().toString(value, 'f', precision);
}

QString formatUsd(double value)
{
    return QObject::tr("$%1").arg(formatFixed(value, 2));
}

QString formatPercent(double value)
{
    const QString sign = value > 0.0 ? QStringLiteral("+")
                                     : QString();
    return QObject::tr("%1%2%")
        .arg(sign, formatFixed(value, 2));
}

QString formatAvailable(bool available, double value,
                        int precision)
{
    return available ? formatFixed(value, precision)
                     : QObject::tr("N/A");
}

QString formatActualAvailable(bool available, double value,
                              int precision)
{
    return available ? formatFixed(value, precision)
                     : QObject::tr("Not modeled");
}

QString formatActualInt(bool available, int value)
{
    return available ? QString::number(value)
                     : QObject::tr("Not modeled");
}

QString formatActualMode(
    bool available,
    CargoNetSim::Backend::TransportationTypes::
        TransportationMode mode)
{
    if (!available)
        return QObject::tr("Not modeled");
    return CargoNetSim::Backend::TransportationTypes::
        toString(mode);
}

double costSnapshotTotal(const CostSnapshot &costs)
{
    if (!costs.available)
        return 0.0;

    return costs.travelTime + costs.distance
           + costs.carbonEmissions
           + costs.energyConsumption + costs.risk
           + costs.directCost;
}

QString formatCostComponent(const CostSnapshot &costs,
                            double component,
                            int    precision = 2)
{
    return costs.available ? formatFixed(component, precision)
                           : QObject::tr("N/A");
}

QString percentDifferenceText(double predicted, double actual)
{
    if (predicted <= 0.0 || actual < 0.0)
        return QObject::tr("N/A");

    return formatPercent(((actual - predicted) / predicted)
                         * 100.0);
}

QString shortened(const QString &text, int maxLength = 90)
{
    if (text.size() <= maxLength)
        return text;
    if (maxLength <= 3)
        return text.left(maxLength);
    return text.left(maxLength - 3) + QStringLiteral("...");
}

QString modeSequenceFor(const QList<SegmentEntry> &segments)
{
    QStringList modes;
    for (const auto &segment : segments)
    {
        modes << (segment.modeName.isEmpty()
                      ? QObject::tr("Unknown")
                      : segment.modeName);
    }
    return modes.isEmpty() ? QObject::tr("N/A")
                           : modes.join(QStringLiteral(" -> "));
}

QString vehiclePlanFor(const PathMetrics &metrics)
{
    if (!metrics.previewVehicleBreakdown.isEmpty())
    {
        QStringList parts;
        for (const auto &entry :
             metrics.previewVehicleBreakdown)
        {
            QString mode =
                CargoNetSim::Backend::TransportationTypes::
                    toString(entry.mode);
            if (mode.isEmpty())
                mode = QObject::tr("Unknown");

            parts << QObject::tr("S%1 %2 x%3")
                         .arg(entry.segmentIndex + 1)
                         .arg(mode)
                         .arg(entry.vehiclesNeeded);
        }
        return parts.join(QStringLiteral(", "));
    }

    if (metrics.vehiclesNeeded > 0)
        return QObject::tr("%1 vehicle(s)")
            .arg(metrics.vehiclesNeeded);

    return QObject::tr("N/A");
}

QString yesNo(bool value)
{
    return value ? QObject::tr("Yes") : QObject::tr("No");
}

} // namespace

PathReportGenerator::PathReportGenerator(
    const QList<const PathData *> &pathData,
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

    qCInfo(lcGuiUtil)
        << "PathReportGenerator::generateReport: adding report header";
    addReportHeader(report);
    qCInfo(lcGuiUtil)
        << "PathReportGenerator::generateReport: adding executive summary";
    addExecutiveSummary(report);
    qCInfo(lcGuiUtil)
        << "PathReportGenerator::generateReport: adding report notes";
    addReportNotes(report);
    qCInfo(lcGuiUtil)
        << "PathReportGenerator::generateReport: adding table of contents";
    addTableOfContents(report);

    // Start a new page for path details
    report->addPageBreak();

    qCInfo(lcGuiUtil)
        << "PathReportGenerator::generateReport: adding comparative analysis";
    addComparativeAnalysis(report);

    // Start a new page for comparative analysis
    report->addPageBreak();

    qCInfo(lcGuiUtil)
        << "PathReportGenerator::generateReport: adding individual path sections";
    addIndividualPathSections(report);

    qCInfo(lcGuiUtil)
        << "PathReportGenerator::generateReport: completed";
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

void PathReportGenerator::addExecutiveSummary(
    KDReports::Report *report)
{
    qCDebug(lcGuiUtil)
        << "PathReportGenerator::addExecutiveSummary: paths"
        << m_pathData.size();

    KDReports::TextElement sectionTitle(
        tr("Executive Summary"));
    sectionTitle.setFont(m_sectionTitleFont);
    sectionTitle.setTextColor(m_subtitleColor);
    report->addElement(sectionTitle);
    report->addVerticalSpacing(5);

    int validPathCount = 0;
    int simulatedCount = 0;
    const PathData *bestPredicted = nullptr;
    const PathData *bestActual = nullptr;
    double bestPredictedCost =
        std::numeric_limits<double>::max();
    double bestActualCost = std::numeric_limits<double>::max();

    for (const auto *pathData : m_pathData)
    {
        if (!(pathData && pathData->path))
            continue;

        ++validPathCount;
        const auto summary = m_viewModel.pathSummary(pathData);
        if (summary.predictedTotalCost < bestPredictedCost)
        {
            bestPredictedCost = summary.predictedTotalCost;
            bestPredicted     = pathData;
        }

        if (pathData->hasSimulationTotalCost())
        {
            ++simulatedCount;
            if (pathData->simulationTotalCost < bestActualCost)
            {
                bestActualCost = pathData->simulationTotalCost;
                bestActual     = pathData;
            }
        }
    }

    KDReports::TableElement keyTable;
    keyTable.setHeaderColumnCount(2);
    keyTable.setHeaderRowCount(1);
    keyTable.setBorder(1);
    keyTable.setBorderBrush(QBrush(m_tableBorderColor));
    styleTableCell(keyTable, 0, 0, tr("Metric"), true);
    styleTableCell(keyTable, 0, 1, tr("Value"), true);

    int row = 1;
    styleTableCell(keyTable, row, 0, tr("Paths Included"),
                   false, true);
    styleTableCell(keyTable, row, 1,
                   QString::number(validPathCount));
    ++row;

    styleTableCell(keyTable, row, 0, tr("Simulated Paths"),
                   false, true);
    styleTableCell(keyTable, row, 1,
                   QString::number(simulatedCount));
    ++row;

    styleTableCell(keyTable, row, 0,
                   tr("Best Predicted Path"), false, true);
    styleTableCell(
        keyTable, row, 1,
        bestPredicted
            ? m_viewModel.pathSummary(bestPredicted).pathLabel
            : tr("N/A"));
    ++row;

    styleTableCell(keyTable, row, 0,
                   tr("Best Predicted Total"), false, true);
    styleTableCell(keyTable, row, 1,
                   bestPredicted ? formatUsd(bestPredictedCost)
                                 : tr("N/A"));
    ++row;

    styleTableCell(keyTable, row, 0,
                   tr("Best Simulated Path"), false, true);
    styleTableCell(
        keyTable, row, 1,
        bestActual ? m_viewModel.pathSummary(bestActual).pathLabel
                   : tr("Not simulated"));
    ++row;

    styleTableCell(keyTable, row, 0,
                   tr("Best Simulated Total"), false, true);
    styleTableCell(keyTable, row, 1,
                   bestActual ? formatUsd(bestActualCost)
                              : tr("Not simulated"));
    report->addElement(keyTable);
    report->addVerticalSpacing(8);

    KDReports::TextElement overviewTitle(
        tr("Path Overview"));
    overviewTitle.setFont(m_normalTextFont);
    overviewTitle.setBold(true);
    overviewTitle.setTextColor(m_subtitleColor);
    report->addElement(overviewTitle);
    report->addVerticalSpacing(3);

    KDReports::TableElement overviewTable;
    overviewTable.setHeaderRowCount(1);
    overviewTable.setBorder(1);
    overviewTable.setBorderBrush(QBrush(m_tableBorderColor));

    const QStringList headers = {
        tr("Path"),       tr("Rank"),
        tr("Containers"), tr("Modes"),
        tr("Predicted"),  tr("Actual"),
        tr("Diff"),       tr("Pred Km"),
        tr("Pred h"),     tr("Actual h"),
        tr("Seg"),        tr("Term")};
    for (int col = 0; col < headers.size(); ++col)
        styleTableCell(overviewTable, 0, col, headers[col],
                       true);

    row = 1;
    for (const auto *pathData : m_pathData)
    {
        if (!(pathData && pathData->path))
            continue;

        const auto summary = m_viewModel.pathSummary(pathData);
        const auto segments =
            m_viewModel.segmentEntries(pathData);
        const auto predictedMetrics = pathData->predictedMetrics;
        const bool hasActual = pathData->actualMetrics.valid;

        styleTableCell(overviewTable, row, 0,
                       summary.pathLabel, false, true);
        styleTableCell(overviewTable, row, 1,
                       QString::number(pathData->path->getRank()));
        styleTableCell(
            overviewTable, row, 2,
            QString::number(
                pathData->path->getEffectiveContainerCount()));
        styleTableCell(overviewTable, row, 3,
                       modeSequenceFor(segments));
        styleTableCell(overviewTable, row, 4,
                       formatUsd(summary.predictedTotalCost));
        styleTableCell(
            overviewTable, row, 5,
            pathData->hasSimulationTotalCost()
                ? formatUsd(pathData->simulationTotalCost)
                : tr("Not simulated"));
        styleTableCell(
            overviewTable, row, 6,
            pathData->hasSimulationTotalCost()
                ? percentDifferenceText(
                      summary.predictedTotalCost,
                      pathData->simulationTotalCost)
                : tr("N/A"));
        styleTableCell(overviewTable, row, 7,
                       formatAvailable(
                           predictedMetrics.valid,
                           predictedMetrics.distanceKm, 1));
        styleTableCell(overviewTable, row, 8,
                       formatAvailable(
                           predictedMetrics.valid,
                           predictedMetrics.travelTimeHours,
                           1));
        styleTableCell(overviewTable, row, 9,
                       formatAvailable(
                           hasActual,
                           pathData->actualMetrics
                               .travelTimeHours,
                           1));
        styleTableCell(overviewTable, row, 10,
                       QString::number(summary.segmentCount));
        styleTableCell(overviewTable, row, 11,
                       QString::number(summary.terminalCount));
        ++row;
    }

    report->addElement(overviewTable);
    report->addVerticalSpacing(10);
}

void PathReportGenerator::addReportNotes(
    KDReports::Report *report)
{
    qCDebug(lcGuiUtil)
        << "PathReportGenerator::addReportNotes";

    KDReports::TextElement sectionTitle(
        tr("Report Notes"));
    sectionTitle.setFont(m_sectionTitleFont);
    sectionTitle.setTextColor(m_subtitleColor);
    report->addElement(sectionTitle);
    report->addVerticalSpacing(4);

    const QStringList notes = {
        tr("Predicted segment physical metrics are raw vehicle or "
           "segment totals. Allocated predicted values are the "
           "shipment share used by the cost model."),
        tr("Actual segment values come from the executed simulator "
           "events captured by CargoNetSim. Non-simulated paths are "
           "reported as Not simulated."),
        tr("Actual terminal values come from recorded arrival-side "
           "handling events. Terminals without an arrival event in "
           "the selected run are reported as Not modeled."),
        tr("Segment total cost rows sum every component: travel "
           "time, distance, carbon emissions, energy consumption, "
           "risk, and direct cost.")};

    for (const auto &note : notes)
    {
        KDReports::TextElement noteElement(
            QStringLiteral("- ") + note);
        noteElement.setFont(m_normalTextFont);
        report->addElement(noteElement);
    }

    report->addVerticalSpacing(10);
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

    const QStringList frontMatter = {
        tr("    Executive Summary"),
        tr("    Report Notes")};
    for (const auto &entryText : frontMatter)
    {
        KDReports::TextElement entry(entryText);
        entry.setFont(TOCLeavel1Font);
        entry.setTextColor(Qt::black);
        report->addElement(entry);
    }
    report->addVerticalSpacing(5);

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
        tr("        Terminal-by-Terminal Attribute "
           "Comparison"),
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
            const auto summary = m_viewModel.pathSummary(pathData);
            int     pathId = summary.pathId;

            // Create TOC entry with link to the bookmark
            QString pathEntry =
                tr("        Path %1 Summary, Terminals, "
                   "Segments, and Costs")
                    .arg(pathId);
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
    KDReports::Report *report, const PathData *pathData)
{
    qCDebug(lcGuiUtil) << "PathReportGenerator::addPathDetails:"
                       << (pathData && pathData->path ? "valid path" : "null");
    if (!pathData || !pathData->path)
        return;

    const auto summary = m_viewModel.pathSummary(pathData);
    int pathId = summary.pathId;

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

    // Add physical metric overview
    addPathMetricsOverview(report, pathData);

    // Add path terminals
    addPathTerminals(report, pathData);

    // Add path segments
    addPathSegments(report, pathData);

    // Add path costs
    addPathCosts(report, pathData);
}

void PathReportGenerator::addPathVisualization(
    KDReports::Report *report, const PathData *pathData)
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
    const PathData *pathData)
{
    if (!pathData || !pathData->path)
        return QImage();

    const auto terminals = m_viewModel.terminalEntries(pathData);
    const auto segments = m_viewModel.segmentEntries(pathData);

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

    // Calculate spacing
    int numTerminals = terminals.size();
    int contentWidth =
        image.width() - 40; // 20px margin on each side
    int terminalSpacing =
        contentWidth
        / (numTerminals > 1 ? (numTerminals - 1) : 1);

    {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);

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
            if (i < terminals.size() - 1 && i < segments.size())
            {
                QString modeText = segments[i].modeName;

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
                else if (modeText.contains(
                             "Rail", Qt::CaseInsensitive)
                         || modeText.contains(
                             "Train", Qt::CaseInsensitive))
                    lineColor = QColor(
                        80, 80, 80); // Dark gray for rail
                else if (modeText.contains(
                             "Ship", Qt::CaseInsensitive)
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
    }

    return image.scaled(QSize(400, 400), Qt::KeepAspectRatio,
                        Qt::SmoothTransformation);
}

void PathReportGenerator::addPathSummary(
    KDReports::Report *report, const PathData *pathData)
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
    const auto summary = m_viewModel.pathSummary(pathData);
    const auto segments = m_viewModel.segmentEntries(pathData);
    styleTableCell(
        table, row, 1, QString::number(summary.pathId));
    row++;

    styleTableCell(table, row, 0, tr("Rank"), false, true);
    styleTableCell(table, row, 1,
                   QString::number(pathData->path->getRank()));
    row++;

    styleTableCell(table, row, 0, tr("Effective Containers"),
                   false, true);
    styleTableCell(
        table, row, 1,
        QString::number(
            pathData->path->getEffectiveContainerCount()));
    row++;

    styleTableCell(table, row, 0, tr("Mode Sequence"),
                   false, true);
    styleTableCell(table, row, 1,
                   modeSequenceFor(segments));
    row++;

    // Total terminals
    styleTableCell(table, row, 0, tr("Total Terminals"),
                   false, true);
    styleTableCell(
        table, row, 1, QString::number(summary.terminalCount));
    row++;

    // Total segments
    styleTableCell(table, row, 0, tr("Total Segments"),
                   false, true);
    styleTableCell(
        table, row, 1, QString::number(summary.segmentCount));
    row++;

    // Predicted cost
    styleTableCell(table, row, 0, tr("Predicted Cost"),
                   false, true);
    styleTableCell(table, row, 1,
                   formatUsd(summary.predictedTotalCost));
    row++;

    // Actual cost
    styleTableCell(table, row, 0, tr("Actual Cost"), false,
                   true);
    if (pathData->hasSimulationTotalCost())
    {
        styleTableCell(table, row, 1,
                       formatUsd(pathData->simulationTotalCost));
    }
    else
    {
        styleTableCell(table, row, 1, tr("Not simulated"));
    }
    row++;

    // Start terminal
    styleTableCell(table, row, 0, tr("Start Terminal"),
                   false, true);
    styleTableCell(table, row, 1,
                   summary.startTerminalName.isEmpty()
                       ? tr("Unknown")
                       : summary.startTerminalName);
    row++;

    // End terminal
    styleTableCell(table, row, 0, tr("End Terminal"), false,
                   true);
    styleTableCell(table, row, 1,
                   summary.endTerminalName.isEmpty()
                       ? tr("Unknown")
                       : summary.endTerminalName);
    row++;

    styleTableCell(table, row, 0,
                   tr("Same-Mode Pass-Through Skip"),
                   false, true);
    styleTableCell(
        table, row, 1,
        yesNo(pathData->path
                  ->skipSameModeTerminalDelaysAndCosts()));
    row++;

    styleTableCell(table, row, 0, tr("Canonical Path Key"),
                   false, true);
    styleTableCell(
        table, row, 1,
        shortened(pathData->path->canonicalPathKey(), 120));

    // Add table to report
    report->addElement(table);

    report->addVerticalSpacing(10);
}

void PathReportGenerator::addPathMetricsOverview(
    KDReports::Report *report, const PathData *pathData)
{
    qCDebug(lcGuiUtil)
        << "PathReportGenerator::addPathMetricsOverview";
    if (!pathData || !pathData->path)
        return;

    KDReports::TextElement sectionTitle(
        tr("Path Physical Metrics"));
    sectionTitle.setFont(m_sectionTitleFont);
    sectionTitle.setTextColor(m_subtitleColor);
    report->addElement(sectionTitle);
    report->addVerticalSpacing(5);

    KDReports::TableElement table;
    table.setHeaderColumnCount(2);
    table.setHeaderRowCount(1);
    table.setBorder(1);
    table.setBorderBrush(QBrush(m_tableBorderColor));

    styleTableCell(table, 0, 0, tr("Metric"), true);
    styleTableCell(table, 0, 1, tr("Value"), true);

    const auto &predicted = pathData->predictedMetrics;
    const auto &actual    = pathData->actualMetrics;
    const auto segments   = m_viewModel.segmentEntries(pathData);
    bool   hasPredictedAllocated = false;
    double predictedAllocatedEnergy = 0.0;
    double predictedAllocatedCarbon = 0.0;
    for (const auto &segment : segments)
    {
        const auto &values = segment.displayValues;
        if (!values.predictedAllocatedAvailable)
            continue;

        hasPredictedAllocated = true;
        predictedAllocatedEnergy +=
            values.predictedAllocatedEnergyConsumption;
        predictedAllocatedCarbon +=
            values.predictedAllocatedCarbonEmissions;
    }

    int row = 1;
    styleTableCell(table, row, 0,
                   tr("Predicted Distance (km)"), false,
                   true);
    styleTableCell(table, row, 1,
                   formatAvailable(predicted.valid,
                                   predicted.distanceKm, 2));
    ++row;

    styleTableCell(table, row, 0,
                   tr("Actual Distance (km)"), false, true);
    styleTableCell(table, row, 1,
                   formatAvailable(actual.valid,
                                   actual.distanceKm, 2));
    ++row;

    styleTableCell(table, row, 0,
                   tr("Predicted Travel Time (hr)"), false,
                   true);
    styleTableCell(
        table, row, 1,
        formatAvailable(predicted.valid,
                        predicted.travelTimeHours, 2));
    ++row;

    styleTableCell(table, row, 0,
                   tr("Actual Travel Time (hr)"), false,
                   true);
    styleTableCell(table, row, 1,
                   formatAvailable(actual.valid,
                                   actual.travelTimeHours, 2));
    ++row;

    styleTableCell(table, row, 0,
                   tr("Predicted Energy (Vehicle, kWh)"),
                   false, true);
    styleTableCell(
        table, row, 1,
        formatAvailable(predicted.valid,
                        predicted.energyPerVehicle, 2));
    ++row;

    styleTableCell(table, row, 0,
                   tr("Predicted Energy (Allocated, kWh)"),
                   false, true);
    styleTableCell(
        table, row, 1,
        formatAvailable(hasPredictedAllocated,
                        predictedAllocatedEnergy, 2));
    ++row;

    styleTableCell(table, row, 0,
                   tr("Actual Energy (kWh)"), false, true);
    styleTableCell(table, row, 1,
                   formatAvailable(actual.valid,
                                   actual.energyPerVehicle,
                                   2));
    ++row;

    styleTableCell(table, row, 0,
                   tr("Predicted Carbon (Vehicle, t)"),
                   false, true);
    styleTableCell(
        table, row, 1,
        formatAvailable(predicted.valid,
                        predicted.carbonPerVehicle, 3));
    ++row;

    styleTableCell(table, row, 0,
                   tr("Predicted Carbon (Allocated, t)"),
                   false, true);
    styleTableCell(
        table, row, 1,
        formatAvailable(hasPredictedAllocated,
                        predictedAllocatedCarbon, 3));
    ++row;

    styleTableCell(table, row, 0, tr("Actual Carbon (t)"),
                   false, true);
    styleTableCell(table, row, 1,
                   formatAvailable(actual.valid,
                                   actual.carbonPerVehicle,
                                   3));
    ++row;

    styleTableCell(table, row, 0,
                   tr("Predicted Vehicle Plan"), false,
                   true);
    styleTableCell(table, row, 1,
                   predicted.valid ? vehiclePlanFor(predicted)
                                   : tr("N/A"));
    ++row;

    styleTableCell(table, row, 0, tr("Fuel Type"), false,
                   true);
    styleTableCell(table, row, 1,
                   predicted.fuelType.isEmpty()
                       ? tr("N/A")
                       : predicted.fuelType);

    report->addElement(table);
    report->addVerticalSpacing(10);
}

void PathReportGenerator::addPathTerminals(
    KDReports::Report *report, const PathData *pathData)
{
    qCDebug(lcGuiUtil) << "PathReportGenerator::addPathTerminals: --- terminals section ---";
    // Section title
    KDReports::TextElement sectionTitle(tr("Terminals"));
    sectionTitle.setFont(m_sectionTitleFont);
    sectionTitle.setTextColor(m_subtitleColor);
    report->addElement(sectionTitle);

    report->addVerticalSpacing(5);

    const auto terminals = m_viewModel.terminalEntries(pathData);

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
    table.setHeaderColumnCount(7);
    table.setHeaderRowCount(1);
    table.setBorder(1);
    table.setBorderBrush(QBrush(m_tableBorderColor));

    // Set header
    styleTableCell(table, 0, 0, tr("Index"), true);
    styleTableCell(table, 0, 1, tr("Terminal Name"), true);
    styleTableCell(table, 0, 2, tr("ID"), true);
    styleTableCell(table, 0, 3, tr("Pred. Handling s"),
                   true);
    styleTableCell(table, 0, 4, tr("Pred. Direct $"),
                   true);
    styleTableCell(table, 0, 5, tr("Raw Tariff $"),
                   true);
    styleTableCell(table, 0, 6, tr("Prediction Note"),
                   true);

    // Add data rows
    for (int i = 0; i < terminals.size(); ++i)
    {
        int row = i + 1;

        // Terminal index
        styleTableCell(table, row, 0,
                       QString::number(i + 1), false, true);

        // Terminal name
        styleTableCell(table, row, 1,
                       terminals[i].displayName.isEmpty()
                           ? terminals[i].canonicalName
                           : terminals[i].displayName);

        // Terminal ID
        styleTableCell(table, row, 2,
                       terminals[i].canonicalName);

        const auto &values = terminals[i].displayValues;
        styleTableCell(
            table, row, 3,
            formatAvailable(values.predictedAvailable,
                            values.predictedHandlingSeconds,
                            0));
        styleTableCell(
            table, row, 4,
            values.predictedAvailable
                ? formatUsd(values.predictedDirectCostUsd)
                : tr("N/A"));
        styleTableCell(
            table, row, 5,
            values.predictedAvailable
                ? formatUsd(values.predictedRawDirectCostUsd)
                : tr("N/A"));
        styleTableCell(
            table, row, 6,
            terminals[i].predictedCostsSkipped
                ? (terminals[i].predictedSkipReason.isEmpty()
                       ? tr("Skipped")
                       : terminals[i].predictedSkipReason)
                : tr("-"));
    }

    // Add table to report
    report->addElement(table);

    report->addVerticalSpacing(10);

    addPathTerminalDetails(report, pathData);
}

void PathReportGenerator::addPathTerminalDetails(
    KDReports::Report *report, const PathData *pathData)
{
    qCDebug(lcGuiUtil)
        << "PathReportGenerator::addPathTerminalDetails";
    const auto terminals = m_viewModel.terminalEntries(pathData);
    if (terminals.isEmpty())
        return;

    KDReports::TextElement detailsTitle(
        tr("Terminal Predicted/Actual Details"));
    detailsTitle.setFont(m_normalTextFont);
    detailsTitle.setBold(true);
    detailsTitle.setTextColor(m_subtitleColor);
    report->addElement(detailsTitle);
    report->addVerticalSpacing(5);

    for (int terminalIdx = 0; terminalIdx < terminals.size();
         ++terminalIdx)
    {
        const auto &terminal = terminals[terminalIdx];
        const auto &values = terminal.displayValues;

        KDReports::TextElement terminalTitle(
            tr("Terminal %1: %2")
                .arg(terminalIdx + 1)
                .arg(terminal.displayName.isEmpty()
                         ? terminal.canonicalName
                         : terminal.displayName));
        terminalTitle.setFont(m_normalTextFont);
        terminalTitle.setBold(true);
        report->addElement(terminalTitle);

        if (terminal.predictedCostsSkipped
            && !terminal.predictedSkipReason.isEmpty())
        {
            KDReports::TextElement note(
                tr("Prediction note: %1")
                    .arg(terminal.predictedSkipReason));
            note.setFont(m_smallTextFont);
            note.setItalic(true);
            report->addElement(note);
        }

        report->addVerticalSpacing(3);

        KDReports::TableElement table;
        table.setHeaderColumnCount(3);
        table.setHeaderRowCount(1);
        table.setBorder(1);
        table.setBorderBrush(QBrush(m_tableBorderColor));
        styleTableCell(table, 0, 0, tr("Metric"), true);
        styleTableCell(table, 0, 1, tr("Predicted"),
                       true);
        styleTableCell(table, 0, 2, tr("Actual"), true);

        int row = 1;
        styleTableCell(table, row, 0,
                       tr("Handling Time (s)"), false,
                       true);
        styleTableCell(
            table, row, 1,
            formatAvailable(values.predictedAvailable,
                            values.predictedHandlingSeconds,
                            0));
        styleTableCell(
            table, row, 2,
            formatActualAvailable(
                values.actualAvailable,
                values.actualTotalHandlingSeconds, 0));
        ++row;

        styleTableCell(table, row, 0,
                       tr("Raw Direct Tariff (USD)"), false,
                       true);
        styleTableCell(
            table, row, 1,
            values.predictedAvailable
                ? formatUsd(values.predictedRawDirectCostUsd)
                : tr("N/A"));
        styleTableCell(table, row, 2, tr("-"));
        ++row;

        styleTableCell(table, row, 0,
                       tr("Direct Cost (USD)"), false,
                       true);
        styleTableCell(
            table, row, 1,
            values.predictedAvailable
                ? formatUsd(values.predictedDirectCostUsd)
                : tr("N/A"));
        styleTableCell(
            table, row, 2,
            values.actualAvailable
                ? formatUsd(values.actualDirectCostUsd)
                : tr("Not modeled"));
        ++row;

        styleTableCell(
            table, row, 0,
            tr("Weighted Delay Contribution"), false, true);
        styleTableCell(
            table, row, 1,
            formatAvailable(
                values.predictedAvailable,
                values.predictedWeightedDelayContribution,
                2));
        styleTableCell(
            table, row, 2,
            formatActualAvailable(
                values.actualAvailable,
                values.actualWeightedDelayContribution, 2));
        ++row;

        styleTableCell(
            table, row, 0,
            tr("Weighted Cost Contribution"), false, true);
        styleTableCell(
            table, row, 1,
            formatAvailable(
                values.predictedAvailable,
                values.predictedWeightedCostContribution,
                2));
        styleTableCell(
            table, row, 2,
            formatActualAvailable(
                values.actualAvailable,
                values.actualWeightedCostContribution, 2));
        ++row;

        styleTableCell(
            table, row, 0,
            tr("Weighted Total Contribution"), false, true);
        styleTableCell(
            table, row, 1,
            formatAvailable(
                values.predictedAvailable,
                values.predictedWeightedTotalContribution,
                2));
        styleTableCell(
            table, row, 2,
            formatActualAvailable(
                values.actualAvailable,
                values.actualWeightedTotalContribution, 2));
        ++row;

        styleTableCell(table, row, 0, tr("Arrival Mode"),
                       false, true);
        styleTableCell(table, row, 1, tr("-"));
        styleTableCell(
            table, row, 2,
            formatActualMode(values.actualAvailable,
                             values.actualArrivalMode));
        ++row;

        styleTableCell(table, row, 0,
                       tr("Dropped Containers"), false,
                       true);
        styleTableCell(table, row, 1, tr("-"));
        styleTableCell(
            table, row, 2,
            formatActualInt(values.actualAvailable,
                            values.actualDroppedContainers));
        ++row;

        styleTableCell(table, row, 0,
                       tr("Arrival Events"), false, true);
        styleTableCell(table, row, 1, tr("-"));
        styleTableCell(
            table, row, 2,
            formatActualInt(values.actualAvailable,
                            values.actualArrivalEvents));
        ++row;

        styleTableCell(table, row, 0,
                       tr("Utilization At Arrival"), false,
                       true);
        styleTableCell(table, row, 1, tr("-"));
        styleTableCell(
            table, row, 2,
            formatActualAvailable(
                values.actualAvailable,
                values.actualUtilizationAtArrival, 3));
        ++row;

        styleTableCell(table, row, 0,
                       tr("Congestion At Arrival"), false,
                       true);
        styleTableCell(table, row, 1, tr("-"));
        styleTableCell(
            table, row, 2,
            formatActualAvailable(
                values.actualAvailable,
                values.actualCongestionAtArrival, 3));
        ++row;

        styleTableCell(table, row, 0,
                       tr("Delay Multiplier At Arrival"),
                       false, true);
        styleTableCell(table, row, 1, tr("-"));
        styleTableCell(
            table, row, 2,
            formatActualAvailable(
                values.actualAvailable,
                values.actualDelayMultiplierAtArrival, 3));

        report->addElement(table);
        report->addVerticalSpacing(8);
    }
}

void PathReportGenerator::addPathSegments(
    KDReports::Report *report, const PathData *pathData)
{
    qCDebug(lcGuiUtil) << "PathReportGenerator::addPathSegments: --- segments section ---";
    // Section title
    KDReports::TextElement sectionTitle(tr("Segments"));
    sectionTitle.setFont(m_sectionTitleFont);
    sectionTitle.setTextColor(m_subtitleColor);
    report->addElement(sectionTitle);

    report->addVerticalSpacing(5);

    const auto segments = m_viewModel.segmentEntries(pathData);

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

        styleTableCell(table, row, 1,
                       segments[i].startTerminalName);
        styleTableCell(table, row, 2,
                       segments[i].endTerminalName);
        styleTableCell(table, row, 3,
                       segments[i].modeName);
    }

    // Add table to report
    report->addElement(table);

    report->addVerticalSpacing(10);

    // Add detailed segment attributes for each segment
    for (int i = 0; i < segments.size(); ++i)
    {
        // Section title for the segment
        KDReports::TextElement segmentTitle(
            tr("Segment %1 Attributes").arg(i + 1));
        segmentTitle.setFont(m_normalTextFont);
        segmentTitle.setBold(true);
        segmentTitle.setTextColor(m_subtitleColor);
        report->addElement(segmentTitle);

        report->addVerticalSpacing(5);

        const auto &segment = segments[i];
        const auto &values = segment.displayValues;
        const auto &predictedCosts = segment.predictedCosts;
        const auto &actualCosts = segment.actualCosts;

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
                       tr("Carbon Emissions (Vehicle)"), false, true);
        styleTableCell(
            attrTable, rowIndex, 1,
            values.predictedAvailable
                ? QString::number(
                      values.predictedCarbonEmissions, 'f', 3)
                : tr("N/A"));
        styleTableCell(
            attrTable, rowIndex, 2, tr("-"));
        rowIndex++;

        styleTableCell(attrTable, rowIndex, 0,
                       tr("Carbon Emissions (Allocated)"),
                       false, true);
        styleTableCell(
            attrTable, rowIndex, 1,
            values.predictedAvailable
                ? QString::number(
                      values.predictedAllocatedCarbonEmissions,
                      'f', 3)
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
            values.predictedAvailable
                ? QString::number(
                      values.predictedDistanceKm, 'f', 2)
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
                       tr("Energy Consumption (Vehicle)"), false,
                       true);
        styleTableCell(
            attrTable, rowIndex, 1,
            values.predictedAvailable
                ? QString::number(
                      values.predictedEnergyConsumption, 'f', 2)
                : tr("N/A"));
        styleTableCell(
            attrTable, rowIndex, 2, tr("-"));
        rowIndex++;

        styleTableCell(attrTable, rowIndex, 0,
                       tr("Energy Consumption (Allocated)"),
                       false, true);
        styleTableCell(
            attrTable, rowIndex, 1,
            values.predictedAvailable
                ? QString::number(
                      values.predictedAllocatedEnergyConsumption,
                      'f', 2)
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
            values.predictedAvailable
                ? QString::number(values.predictedRisk, 'f', 6)
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
            values.predictedAvailable
                ? QString::number(
                      values.predictedTravelTimeHours, 'f', 2)
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
    KDReports::Report *report, const PathData *pathData)
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
    const auto summary = m_viewModel.pathSummary(pathData);

    // Total costs
    styleTableCell(table, rowIndex, 0, tr("Total Cost"),
                   false, true);

    double predictedTotalCost =
        summary.predictedTotalCost;
    double actualTotalCost =
        pathData->simulationTotalCost;

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
        summary.predictedEdgeCost;
    double actualEdgeCost =
        pathData->simulationEdgeCosts;

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
        summary.predictedTerminalCost;
    double actualTerminalCost =
        pathData->simulationTerminalCosts;

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

    const auto segments = m_viewModel.segmentEntries(pathData);

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
        const auto &segment = segments[i];
        if (segment.predictedCosts.available)
        {
            predictedCarbonEmissionsCost +=
                segment.predictedCosts.carbonEmissions;
            predictedDirectCost +=
                segment.predictedCosts.directCost;
            predictedDistanceCost +=
                segment.predictedCosts.distance;
            predictedEnergyCost +=
                segment.predictedCosts.energyConsumption;
            predictedRiskCost += segment.predictedCosts.risk;
            predictedTimeCost +=
                segment.predictedCosts.travelTime;
        }

        if (segment.actualCosts.available)
        {
            actualCarbonEmissionsCost +=
                segment.actualCosts.carbonEmissions;
            actualDirectCost += segment.actualCosts.directCost;
            actualDistanceCost += segment.actualCosts.distance;
            actualEnergyCost +=
                segment.actualCosts.energyConsumption;
            actualRiskCost += segment.actualCosts.risk;
            actualTimeCost += segment.actualCosts.travelTime;
            hasActualData = true;
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
        if (pathData->hasSimulationTotalCost())
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

                if (i < segments.size())
                {
                    const auto &segment = segments[i];
                    QString routeInfo =
                        QString("%1 → %2 (%3)")
                            .arg(segment.startTerminalName)
                            .arg(segment.endTerminalName)
                            .arg(segment.modeName);
                    styleTableCell(segmentTable,
                                   segRowIndex, 1,
                                   routeInfo);

                    const auto &estimatedCostObj =
                        segment.predictedCosts;
                    const auto &actualCostObj =
                        segment.actualCosts;
                    const double segPredictedCost =
                        costSnapshotTotal(estimatedCostObj);
                    const double segActualCost =
                        costSnapshotTotal(actualCostObj);

                    // Add cost data
                    styleTableCell(segmentTable, segRowIndex, 2,
                                   estimatedCostObj.available
                                       ? formatUsd(
                                             segPredictedCost)
                                       : tr("N/A"));

                    if (actualCostObj.available)
                    {
                        styleTableCell(
                            segmentTable, segRowIndex, 3,
                            formatUsd(segActualCost));

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

    addTerminalAttributeComparisonTables(report);
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
            headers.append(m_viewModel.pathSummary(path).pathLabel);
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
        tr("Path ID"),
        tr("Rank"),
        tr("Effective Containers"),
        tr("Mode Sequence"),
        tr("Total Terminals"),
        tr("Total Segments"),
        tr("Predicted Cost"),
        tr("Actual Cost"),
        tr("Predicted Distance (km)"),
        tr("Actual Distance (km)"),
        tr("Predicted Travel Time (hr)"),
        tr("Actual Travel Time (hr)"),
        tr("Start Terminal"),
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
                const auto summary = m_viewModel.pathSummary(path);
                switch (row)
                {
                case 0: // Path ID
                    styleTableCell(
                        table, tableRow, tableCol,
                        QString::number(summary.pathId));
                    break;
                case 1: // Rank
                    styleTableCell(
                        table, tableRow, tableCol,
                        QString::number(path->path->getRank()));
                    break;
                case 2: // Effective containers
                    styleTableCell(
                        table, tableRow, tableCol,
                        QString::number(
                            path->path
                                ->getEffectiveContainerCount()));
                    break;
                case 3: // Mode sequence
                    styleTableCell(
                        table, tableRow, tableCol,
                        modeSequenceFor(
                            m_viewModel.segmentEntries(path)));
                    break;
                case 4: // Total terminals
                    styleTableCell(
                        table, tableRow, tableCol,
                        QString::number(summary.terminalCount));
                    break;
                case 5: // Total segments
                    styleTableCell(
                        table, tableRow, tableCol,
                        QString::number(summary.segmentCount));
                    break;
                case 6: // Predicted Cost
                    styleTableCell(
                        table, tableRow, tableCol,
                        formatUsd(summary.predictedTotalCost));
                    break;
                case 7: // Actual Cost
                    if (path->hasSimulationTotalCost())
                    {
                        styleTableCell(
                            table, tableRow, tableCol,
                            formatUsd(path->simulationTotalCost));
                    }
                    else
                    {
                        styleTableCell(table, tableRow,
                                       tableCol,
                                       tr("Not simulated"));
                    }
                    break;
                case 8: // Predicted Distance
                    styleTableCell(
                        table, tableRow, tableCol,
                        formatAvailable(
                            path->predictedMetrics.valid,
                            path->predictedMetrics.distanceKm,
                            2));
                    break;
                case 9: // Actual Distance
                    styleTableCell(
                        table, tableRow, tableCol,
                        formatAvailable(
                            path->actualMetrics.valid,
                            path->actualMetrics.distanceKm, 2));
                    break;
                case 10: // Predicted Travel Time
                    styleTableCell(
                        table, tableRow, tableCol,
                        formatAvailable(
                            path->predictedMetrics.valid,
                            path->predictedMetrics
                                .travelTimeHours,
                            2));
                    break;
                case 11: // Actual Travel Time
                    styleTableCell(
                        table, tableRow, tableCol,
                        formatAvailable(
                            path->actualMetrics.valid,
                            path->actualMetrics.travelTimeHours,
                            2));
                    break;
                case 12: // Start Terminal
                    styleTableCell(
                        table, tableRow, tableCol,
                        summary.startTerminalName.isEmpty()
                            ? tr("Unknown")
                            : summary.startTerminalName);
                    break;
                case 13: // End Terminal
                    styleTableCell(
                        table, tableRow, tableCol,
                        summary.endTerminalName.isEmpty()
                            ? tr("Unknown")
                            : summary.endTerminalName);
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
            const auto summary = m_viewModel.pathSummary(path);
            headers.append(
                tr("Path %1").arg(summary.pathId));
        }
        else
        {
            headers.append(tr("Unknown Path"));
        }
    }

    // Find the maximum number of terminals across all paths
    const int maxTerminals = m_viewModel.maxTerminals();

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
                const auto terminals =
                    m_viewModel.terminalEntries(path);

                if (i < terminals.size())
                {
                    styleTableCell(
                        table, tableRow, tableCol,
                        terminals[i].displayName.isEmpty()
                            ? terminals[i].canonicalName
                            : terminals[i].displayName);
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

void PathReportGenerator::addTerminalAttributeComparisonTables(
    KDReports::Report *report)
{
    qCDebug(lcGuiUtil)
        << "PathReportGenerator::addTerminalAttributeComparisonTables";

    KDReports::TextElement sectionTitle(
        tr("Terminal-by-Terminal Attribute Comparison"));
    sectionTitle.setFont(m_sectionTitleFont);
    sectionTitle.setTextColor(m_subtitleColor);
    report->addElement(sectionTitle);
    report->addVerticalSpacing(5);

    const int maxTerminals = m_viewModel.maxTerminals();
    if (maxTerminals == 0)
    {
        KDReports::TextElement noData(
            tr("No terminal data available for comparison."));
        noData.setFont(m_normalTextFont);
        report->addElement(noData);
        return;
    }

    KDReports::TextElement note(
        tr("Actual terminal values come from arrival-side "
           "handling events. A start facility or pass-through "
           "terminal without an actual arrival event is shown as "
           "Not modeled."));
    note.setFont(m_normalTextFont);
    note.setItalic(true);
    report->addElement(note);
    report->addVerticalSpacing(5);

    for (int terminalIdx = 0; terminalIdx < maxTerminals;
         ++terminalIdx)
    {
        KDReports::TextElement terminalSectionTitle(
            tr("Terminal %1 Attribute Comparison")
                .arg(terminalIdx + 1));
        terminalSectionTitle.setFont(m_normalTextFont);
        terminalSectionTitle.setBold(true);
        report->addElement(terminalSectionTitle);
        report->addVerticalSpacing(3);

        QString terminalDescText;
        for (const auto *path : m_pathData)
        {
            if (!(path && path->path))
                continue;

            const auto terminals =
                m_viewModel.terminalEntries(path);
            if (terminalIdx >= terminals.size())
                continue;

            const auto summary = m_viewModel.pathSummary(path);
            const auto &terminal = terminals[terminalIdx];
            if (!terminalDescText.isEmpty())
                terminalDescText += tr("\n");

            QString noteText;
            if (terminal.predictedCostsSkipped
                && !terminal.predictedSkipReason.isEmpty())
            {
                noteText =
                    tr(" (prediction note: %1)")
                        .arg(terminal.predictedSkipReason);
            }

            terminalDescText +=
                tr("Path %1: %2%3")
                    .arg(summary.pathId)
                    .arg(terminal.displayName.isEmpty()
                             ? terminal.canonicalName
                             : terminal.displayName)
                    .arg(noteText);
        }

        if (!terminalDescText.isEmpty())
        {
            KDReports::TextElement descElement(
                terminalDescText);
            descElement.setFont(m_smallTextFont);
            descElement.setItalic(true);
            report->addElement(descElement);
            report->addVerticalSpacing(5);
        }

        addTerminalPositionAttributeTable(report,
                                          terminalIdx);

        if (terminalIdx < maxTerminals - 1)
            report->addPageBreak();
    }
}

void PathReportGenerator::addTerminalPositionAttributeTable(
    KDReports::Report *report, int terminalIdx)
{
    qCDebug(lcGuiUtil)
        << "PathReportGenerator::addTerminalPositionAttributeTable:"
        << terminalIdx;

    QStringList headers;
    headers.append(tr("Attribute"));
    for (const auto *path : m_pathData)
    {
        if (path && path->path)
            headers.append(m_viewModel.pathSummary(path)
                               .pathLabel);
        else
            headers.append(tr("Unknown Path"));
    }

    KDReports::TableElement table;
    table.setHeaderRowCount(1);
    table.setBorder(1);
    table.setBorderBrush(QBrush(m_tableBorderColor));

    for (int col = 0; col < headers.size(); ++col)
        styleTableCell(table, 0, col, headers[col], true);

    const QStringList attributeLabels = {
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

    for (int rowIdx = 0; rowIdx < attributeLabels.size();
         ++rowIdx)
    {
        const int tableRow = rowIdx + 1;
        styleTableCell(table, tableRow, 0,
                       attributeLabels[rowIdx], false,
                       true);

        for (int col = 0; col < m_pathData.size(); ++col)
        {
            const auto *path = m_pathData[col];
            const int tableCol = col + 1;
            if (!(path && path->path))
            {
                styleTableCell(table, tableRow, tableCol,
                               tr("N/A"));
                continue;
            }

            const auto terminals =
                m_viewModel.terminalEntries(path);
            if (terminalIdx >= terminals.size())
            {
                styleTableCell(table, tableRow, tableCol,
                               tr("-"));
                continue;
            }

            const auto &values =
                terminals[terminalIdx].displayValues;
            QString valueText = tr("N/A");
            switch (rowIdx)
            {
            case 0:
                valueText = formatAvailable(
                    values.predictedAvailable,
                    values.predictedHandlingSeconds, 0);
                break;
            case 1:
                valueText = formatActualAvailable(
                    values.actualAvailable,
                    values.actualTotalHandlingSeconds, 0);
                break;
            case 2:
                valueText =
                    values.predictedAvailable
                        ? formatUsd(
                              values.predictedRawDirectCostUsd)
                        : tr("N/A");
                break;
            case 3:
                valueText =
                    values.predictedAvailable
                        ? formatUsd(values.predictedDirectCostUsd)
                        : tr("N/A");
                break;
            case 4:
                valueText =
                    values.actualAvailable
                        ? formatUsd(values.actualDirectCostUsd)
                        : tr("Not modeled");
                break;
            case 5:
                valueText = formatAvailable(
                    values.predictedAvailable,
                    values.predictedWeightedDelayContribution,
                    2);
                break;
            case 6:
                valueText = formatActualAvailable(
                    values.actualAvailable,
                    values.actualWeightedDelayContribution, 2);
                break;
            case 7:
                valueText = formatAvailable(
                    values.predictedAvailable,
                    values.predictedWeightedCostContribution,
                    2);
                break;
            case 8:
                valueText = formatActualAvailable(
                    values.actualAvailable,
                    values.actualWeightedCostContribution, 2);
                break;
            case 9:
                valueText = formatAvailable(
                    values.predictedAvailable,
                    values.predictedWeightedTotalContribution,
                    2);
                break;
            case 10:
                valueText = formatActualAvailable(
                    values.actualAvailable,
                    values.actualWeightedTotalContribution, 2);
                break;
            case 11:
                valueText = formatActualMode(
                    values.actualAvailable,
                    values.actualArrivalMode);
                break;
            case 12:
                valueText = formatActualInt(
                    values.actualAvailable,
                    values.actualDroppedContainers);
                break;
            case 13:
                valueText = formatActualInt(
                    values.actualAvailable,
                    values.actualArrivalEvents);
                break;
            case 14:
                valueText = formatActualAvailable(
                    values.actualAvailable,
                    values.actualUtilizationAtArrival, 3);
                break;
            case 15:
                valueText = formatActualAvailable(
                    values.actualAvailable,
                    values.actualCongestionAtArrival, 3);
                break;
            case 16:
                valueText = formatActualAvailable(
                    values.actualAvailable,
                    values.actualDelayMultiplierAtArrival, 3);
                break;
            default:
                break;
            }

            styleTableCell(table, tableRow, tableCol,
                           valueText);
        }
    }

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
            const auto summary = m_viewModel.pathSummary(path);
            headers.append(
                tr("Path %1").arg(summary.pathId));
        }
        else
        {
            headers.append(tr("Unknown Path"));
        }
    }

    // Find the maximum number of segments across all paths
    const int maxSegments = m_viewModel.maxSegments();

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
                const auto segments =
                    m_viewModel.segmentEntries(path);

                if (i < segments.size())
                {
                    QString segmentInfo =
                        QString("%1 → %2 (%3)")
                            .arg(segments[i].startTerminalName)
                            .arg(segments[i].endTerminalName)
                            .arg(segments[i].modeName);

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
            const auto summary = m_viewModel.pathSummary(path);
            headers.append(
                tr("Path %1").arg(summary.pathId));
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
                const auto summary =
                    m_viewModel.pathSummary(path);
                // Handle different cost categories
                switch (row)
                {
                case 0: // Predicted Total
                    styleTableCell(
                        table, tableRow, tableCol,
                        QString::number(
                            summary.predictedTotalCost,
                            'f', 2));
                    break;
                case 1: // Predicted Edge
                    styleTableCell(
                        table, tableRow, tableCol,
                        QString::number(
                            summary.predictedEdgeCost,
                            'f', 2));
                    break;
                case 2: // Predicted Terminal
                    styleTableCell(
                        table, tableRow, tableCol,
                        QString::number(
                            summary.predictedTerminalCost,
                            'f', 2));
                    break;
                case 3: // Simulated Total
                    if (path->hasSimulationTotalCost())
                    {
                        styleTableCell(
                            table, tableRow, tableCol,
                            QString::number(
                                path->simulationTotalCost,
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
                    if (path->hasSimulationEdgeCosts())
                    {
                        styleTableCell(
                            table, tableRow, tableCol,
                            QString::number(
                                path->simulationEdgeCosts,
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
                    if (path->hasSimulationTerminalCosts())
                    {
                        styleTableCell(
                            table, tableRow, tableCol,
                            QString::number(
                                path->simulationTerminalCosts,
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
    const int maxSegments = m_viewModel.maxSegments();

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
                const auto summary =
                    m_viewModel.pathSummary(path);
                const auto segments =
                    m_viewModel.segmentEntries(path);
                if (segmentIdx < segments.size())
                {
                    if (!segmentDescText.isEmpty())
                        segmentDescText += tr("\n");

                    segmentDescText +=
                        tr("Path %1: %2 → %3 (%4)")
                            .arg(summary.pathId)
                            .arg(segments[segmentIdx]
                                     .startTerminalName)
                            .arg(segments[segmentIdx]
                                     .endTerminalName)
                            .arg(segments[segmentIdx].modeName);
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
        if (segmentIdx < maxSegments - 1)
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
            const auto summary = m_viewModel.pathSummary(path);
            headers.append(
                tr("Path %1").arg(summary.pathId));
        }
        else
        {
            headers.append(tr("Unknown Path"));
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
        tr("Carbon Emissions (Vehicle Predicted, t)"),
        tr("Carbon Emissions (Allocated Predicted, t)"),
        tr("Carbon Emissions (Actual)"),
        tr("Direct Cost (Allocated Predicted, USD)"),
        tr("Direct Cost (Actual)"),
        tr("Distance (Predicted, km)"),
        tr("Distance (Actual)"),
        tr("Energy Consumption (Vehicle Predicted, kWh)"),
        tr("Energy Consumption (Allocated Predicted, kWh)"),
        tr("Energy Consumption (Actual)"),
        tr("Risk (Vehicle Predicted)"),
        tr("Risk (Actual)"),
        tr("Travel Time (Predicted, hr)"),
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
                const auto segments =
                    m_viewModel.segmentEntries(path);

                if (segmentIdx < segments.size())
                {
                    const auto values =
                        m_viewModel.segmentValues(path, segmentIdx);
                    const auto predictedCosts =
                        m_viewModel.segmentPredictedCosts(
                            path, segmentIdx);
                    const auto actualCosts =
                        m_viewModel.segmentActualCosts(
                            path, segmentIdx);

                    QString valueText = tr("N/A");
                    switch (rowIdx)
                    {
                    case 0:
                        if (values.predictedAvailable)
                            valueText = QString::number(
                                values.predictedCarbonEmissions,
                                'g', 3);
                        break;
                    case 1:
                        if (values.predictedAvailable)
                            valueText = QString::number(
                                values.predictedAllocatedCarbonEmissions,
                                'g', 3);
                        break;
                    case 2:
                        if (values.actualAvailable)
                            valueText = QString::number(
                                values.actualCarbonEmissions,
                                'g', 3);
                        break;
                    case 3:
                        valueText = formatCostComponent(
                            predictedCosts,
                            predictedCosts.directCost, 2);
                        break;
                    case 4:
                        valueText = formatCostComponent(
                            actualCosts,
                            actualCosts.directCost, 2);
                        break;
                    case 5:
                        if (values.predictedAvailable)
                            valueText = QString::number(
                                values.predictedDistanceKm,
                                'g', 2);
                        break;
                    case 6:
                        if (values.actualAvailable)
                            valueText = QString::number(
                                values.actualDistanceKm, 'g', 2);
                        break;
                    case 7:
                        if (values.predictedAvailable)
                            valueText = QString::number(
                                values.predictedEnergyConsumption,
                                'g', 2);
                        break;
                    case 8:
                        if (values.predictedAvailable)
                            valueText = QString::number(
                                values.predictedAllocatedEnergyConsumption,
                                'g', 2);
                        break;
                    case 9:
                        if (values.actualAvailable)
                            valueText = QString::number(
                                values.actualEnergyConsumption,
                                'g', 2);
                        break;
                    case 10:
                        if (values.predictedAvailable)
                            valueText = QString::number(
                                values.predictedRisk, 'g', 6);
                        break;
                    case 11:
                        if (values.actualAvailable)
                            valueText = QString::number(
                                values.actualRisk, 'g', 6);
                        break;
                    case 12:
                        if (values.predictedAvailable)
                            valueText = QString::number(
                                values.predictedTravelTimeHours,
                                'g', 2);
                        break;
                    case 13:
                        if (values.actualAvailable)
                            valueText = QString::number(
                                values.actualTravelTimeHours,
                                'g', 2);
                        break;
                    }
                    styleTableCell(table, tableRow,
                                   tableCol, valueText);
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
    const int maxSegments = m_viewModel.maxSegments();

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
                const auto summary =
                    m_viewModel.pathSummary(path);
                const auto segments =
                    m_viewModel.segmentEntries(path);
                if (segmentIdx < segments.size())
                {
                    if (!segmentDescText.isEmpty())
                        segmentDescText += tr("\n");

                    segmentDescText +=
                        tr("Path %1: %2 → %3 (%4)")
                            .arg(summary.pathId)
                            .arg(segments[segmentIdx]
                                     .startTerminalName)
                            .arg(segments[segmentIdx]
                                     .endTerminalName)
                            .arg(segments[segmentIdx].modeName);
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
        if (segmentIdx < maxSegments - 1)
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
            const auto summary = m_viewModel.pathSummary(path);
            headers.append(
                tr("Path %1").arg(summary.pathId));
        }
        else
        {
            headers.append(tr("Unknown Path"));
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
                const double predictedTotal =
                    costSnapshotTotal(estimatedCostObj);
                const double actualTotal =
                    costSnapshotTotal(actualCostObj);

                // Display the costs
                QString costText;
                if (estimatedCostObj.available
                    && actualCostObj.available)
                {
                    costText =
                        tr("%1 / %2")
                            .arg(formatUsd(predictedTotal))
                            .arg(formatUsd(actualTotal));
                }
                else if (estimatedCostObj.available)
                {
                    costText =
                        tr("%1 / -").arg(
                            formatUsd(predictedTotal));
                }
                else
                {
                    costText = tr("N/A");
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

    {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);

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
    }

    return image;
}

} // namespace GUI
} // namespace CargoNetSim
