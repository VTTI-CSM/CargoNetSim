/**
 * @file PathReportGenerator.h
 * @brief Header for the PathReportGenerator class using
 * KDReports
 * @author Ahmed Aredah
 * @date April 27, 2025
 *
 * This file contains the declaration of the
 * PathReportGenerator class, which creates comprehensive
 * PDF reports for path data visualization and comparison in
 * the CargoNetSim system using KDReports.
 */

#pragma once

#include "Backend/Application/PathPresentationService.h"
#include "GUI/Utils/PathComparisonViewModel.h"
#include <KDReportsAutoTableElement.h>
#include <KDReportsCell.h>
#include <KDReportsChartElement.h>
#include <KDReportsHLineElement.h>
#include <KDReportsHeader.h>
#include <KDReportsImageElement.h>
#include <KDReportsReport.h>
#include <KDReportsTableElement.h>
#include <KDReportsTextElement.h>
#include <QBuffer>
#include <QDateTime>
#include <QFileInfo>

namespace CargoNetSim
{
namespace GUI
{

/**
 * @brief A class for generating PDF reports of path data
 * and comparisons using KDReports
 *
 * This class creates comprehensive PDF reports containing
 * detailed information about individual paths and
 * comparative analysis between multiple paths, similar to
 * the PathComparisonDialog but in a printable/shareable
 * format.
 */
class PathReportGenerator : public QObject
{
    Q_OBJECT

public:
    using PathData = Backend::Application::PathPresentationRecord;

    /**
     * @brief Constructor for PathReportGenerator
     * @param pathData List of path data pointers to include
     * in the report
     * @param parent Parent QObject
     */
    explicit PathReportGenerator(
        const QList<const PathData *> &pathData,
        QObject *parent = nullptr);

    /**
     * @brief Destructor
     */
    ~PathReportGenerator();

    /**
     * @brief Generates and saves the PDF report to the
     * specified file
     * @return Report pointer
     */
    KDReports::Report *generateReport();

private:
    // Report sections
    /**
     * @brief Adds the report header with title and metadata
     * @param report KDReports report object
     */
    void addReportHeader(KDReports::Report *report);

    /**
     * @brief Adds the table of contents
     * @param report KDReports report object
     */
    void addTableOfContents(KDReports::Report *report);

    /**
     * @brief Adds individual path sections
     * @param report KDReports report object
     */
    void
    addIndividualPathSections(KDReports::Report *report);

    /**
     * @brief Adds a single path's detailed information
     * @param report KDReports report object
     * @param pathData Pointer to the path data to add
     */
    void addPathDetails(
        KDReports::Report *report,
        const PathData    *pathData);

    /**
     * @brief Adds path summary information
     * @param report KDReports report object
     * @param pathData Pointer to the path data
     */
    void addPathSummary(
        KDReports::Report *report,
        const PathData    *pathData);

    /**
     * @brief Adds path terminal information
     * @param report KDReports report object
     * @param pathData Pointer to the path data
     */
    void addPathTerminals(
        KDReports::Report *report,
        const PathData    *pathData);

    /**
     * @brief Adds path segment information
     * @param report KDReports report object
     * @param pathData Pointer to the path data
     */
    void addPathSegments(
        KDReports::Report *report,
        const PathData    *pathData);

    /**
     * @brief Adds path cost information
     * @param report KDReports report object
     * @param pathData Pointer to the path data
     */
    void addPathCosts(
        KDReports::Report *report,
        const PathData    *pathData);

    /**
     * @brief Adds path visualization
     * @param report KDReports report object
     * @param pathData Pointer to the path data
     */
    void addPathVisualization(
        KDReports::Report *report,
        const PathData    *pathData);

    /**
     * @brief Adds comparative analysis section
     * @param report KDReports report object
     */
    void addComparativeAnalysis(KDReports::Report *report);

    /**
     * @brief Adds summary comparison table
     * @param report KDReports report object
     */
    void
    addSummaryComparisonTable(KDReports::Report *report);

    /**
     * @brief Adds terminal comparison table
     * @param report KDReports report object
     */
    void
    addTerminalComparisonTable(KDReports::Report *report);

    /**
     * @brief Adds segment comparison table
     * @param report KDReports report object
     */
    void
    addSegmentComparisonTable(KDReports::Report *report);

    /**
     * @brief Adds cost comparison table
     * @param report KDReports report object
     */
    void addCostComparisonTable(KDReports::Report *report);

    void addSegmentAttributeComparisonTables(
        KDReports::Report *report);
    void addSegmentPositionAttributeTable(
        KDReports::Report *report, int segmentIdx);

    void addSegmentCostComparisonTables(
        KDReports::Report *report);
    void
    addSegmentPositionCostTable(KDReports::Report *report,
                                int segmentIdx);

    // Helper methods
    /**
     * @brief Creates a transportation mode image for
     * visualization
     * @param mode Text description of the transportation
     * mode
     * @return Image representing the transportation mode
     */
    QImage createTransportModeImage(const QString &mode);

    /**
     * @brief Creates an image for the path visualization
     * @param pathData Pointer to the path data to visualize
     * @return Image containing the path visualization
     */
    QImage createPathVisualizationImage(
        const PathData *pathData);

    /**
     * @brief Applies styles to table cells based on content
     * @param table Table element to style
     * @param row Row index
     * @param col Column index
     * @param text Cell text
     * @param isHeader Whether the cell is a header cell
     * @param rowLabel Whether the cell is a row label
     * (first column)
     */
    void styleTableCell(KDReports::TableElement &table,
                        int row, int col,
                        const QString &text,
                        bool           isHeader = false,
                        bool           rowLabel = false);

    // Data members
    const QList<const PathData *>
         &m_pathData; ///< Path data to include in report
    PathComparisonViewModel m_viewModel;
    QFont m_pageTitleFont;    ///< Font for page titles
    QFont m_sectionTitleFont; ///< Font for section titles
    QFont m_normalTextFont;   ///< Font for normal text
    QFont m_smallTextFont;    ///< Font for small text
    QFont m_tableHeaderFont;  ///< Font for table headers
    QFont
        m_tableRowLabelFont; ///< Font for table row labels
    QColor m_titleColor;     ///< Color for titles
    QColor m_subtitleColor;  ///< Color for subtitles
    QColor m_tableHeaderBgColor; ///< Background color for
                                 ///< table headers
    QColor m_tableBorderColor; ///< Color for table borders
    QColor
        m_positiveValueColor; ///< Color for positive values
                              ///< (cost increases)
    QColor m_negativeValueColor; ///< Color for negative
                                 ///< values (cost savings)
};

} // namespace GUI
} // namespace CargoNetSim
