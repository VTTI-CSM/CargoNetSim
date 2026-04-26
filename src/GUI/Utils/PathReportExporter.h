/**
 * @file PathReportExporter.h
 * @brief Header for the PathReportExporter class
 * @author [Your Name]
 * @date April 27, 2025
 *
 * This file contains the declaration of the
 * PathReportExporter class, which provides an interface for
 * creating and exporting PDF reports for path data in the
 * CargoNetSim system.
 */

#pragma once

#include "Backend/Application/PathPresentationService.h"
#include "PathReportGenerator.h"
#include <QObject>
#include <QString>

namespace CargoNetSim
{
namespace GUI
{

/**
 * @brief A utility class for exporting path data to PDF
 * reports
 *
 * This class provides methods for creating comprehensive
 * PDF reports of path data, including individual path
 * details and comparisons between multiple paths.
 */
class PathReportExporter : public QObject
{
    Q_OBJECT

public:
    using PathData = Backend::Application::PathPresentationRecord;

    /**
     * @brief Constructor for PathReportExporter
     * @param parent Parent QObject
     */
    explicit PathReportExporter(QObject *parent = nullptr);

    /**
     * @brief Exports a single path to a PDF report
     * @param pathData Pointer to the path data to export
     * @param filePath Path where the PDF file should be
     * saved
     * @return True if export was successful, false
     * otherwise
     */
    bool exportSinglePath(
        const PathData *pathData,
        const QString  &filePath);

    /**
     * @brief Exports multiple paths to a PDF report with
     * comparison
     * @param pathData List of path data pointers to include
     * in the report
     * @param filePath Path where the PDF file should be
     * saved
     * @return True if export was successful, false
     * otherwise
     */
    bool exportMultiplePaths(
        const QList<const PathData *> &pathData,
        const QString &filePath);

    /**
     * @brief Shows a file dialog to select a save location
     * and exports to that location
     * @param pathData List of path data pointers to include
     * in the report
     * @param parent Parent widget for the file dialog
     * @param defaultName Default filename suggestion
     * @return True if export was successful, false
     * otherwise
     */
    bool exportPathsWithDialog(
        const QList<const PathData *> &pathData,
        QWidget       *parent      = nullptr,
        const QString &defaultName = "path_report.pdf");

    /**
     * @brief Shows a preview dialog for the report before
     * saving
     * @param pathData List of path data pointers to include
     * in the report
     * @param parent Parent widget for the preview dialog
     * @return True if preview was displayed successfully,
     * false otherwise
     */
    bool previewReport(
        const QList<const PathData *> &pathData,
        QWidget *parent = nullptr);
};

} // namespace GUI
} // namespace CargoNetSim
