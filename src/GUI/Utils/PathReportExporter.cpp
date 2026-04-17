/**
 * @file PathReportExporter.cpp
 * @brief Implementation of the PathReportExporter class
 * @author [Your Name]
 * @date April 27, 2025
 *
 * This file contains the implementation of the
 * PathReportExporter class, which provides an interface for
 * creating and exporting PDF reports for path data in the
 * CargoNetSim system.
 */

#include "PathReportExporter.h"
#include <KDReportsPreviewDialog.h>
#include <QApplication>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QtCore/QtCore>
#include "Backend/Commons/LogCategories.h"

namespace CargoNetSim
{
namespace GUI
{

PathReportExporter::PathReportExporter(QObject *parent)
    : QObject(parent)
{
}

bool PathReportExporter::exportSinglePath(
    const ShortestPathsTable::PathData *pathData,
    const QString                      &filePath)
{
    if (!pathData || !pathData->path)
    {
        qCWarning(lcGuiUtil) << "PathReportExporter::exportSinglePath:"
                             << "null pathData or path";
        return false;
    }

    // Create a list with just the one path
    QList<const ShortestPathsTable::PathData *> pathList;
    pathList.append(pathData);

    // Create the report generator
    PathReportGenerator generator(pathList);

    // Generate the report
    auto report = generator.generateReport();

    if (report)
    {
        // Export to PDF
        try
        {
            report->exportToFile(filePath);
            return true;
        }
        catch (const std::exception &e)
        {
            qCWarning(lcGuiUtil)
                << "Failed to export report:" << e.what();
            return false;
        }
    }
    else
    {
        qCWarning(lcGuiUtil) << "Failed to export report:";
        return false;
    }
}

bool PathReportExporter::exportMultiplePaths(
    const QList<const ShortestPathsTable::PathData *>
                  &pathData,
    const QString &filePath)
{
    if (pathData.isEmpty())
    {
        qCWarning(lcGuiUtil) << "PathReportExporter::exportMultiplePaths:"
                             << "empty pathData list";
        return false;
    }

    // Create the report generator
    PathReportGenerator generator(pathData);

    // Generate the report
    auto report = generator.generateReport();

    if (report)
    {
        // Export to PDF
        try
        {
            report->exportToFile(filePath);
            return true;
        }
        catch (const std::exception &e)
        {
            qCWarning(lcGuiUtil)
                << "Failed to export report:" << e.what();
            return false;
        }
    }
    else
    {
        qCWarning(lcGuiUtil) << "Failed to export report:";
        return false;
    }
}

bool PathReportExporter::exportPathsWithDialog(
    const QList<const ShortestPathsTable::PathData *>
            &pathData,
    QWidget *parent, const QString &defaultName)
{
    if (pathData.isEmpty())
    {
        QMessageBox::warning(
            parent, tr("Export Error"),
            tr("No path data available to export."));
        return false;
    }

    // Show file dialog to get save location
    QString filePath = QFileDialog::getSaveFileName(
        parent, tr("Save Path Report"),
        QDir::homePath() + QDir::separator() + defaultName,
        tr("PDF Files (*.pdf);;All Files (*)"));

    if (filePath.isEmpty())
    {
        // User canceled
        qCDebug(lcGuiUtil) << "PathReportExporter::exportPathsWithDialog:"
                           << "user cancelled file dialog";
        return false;
    }

    // Ensure the file has .pdf extension
    if (!filePath.endsWith(".pdf", Qt::CaseInsensitive))
    {
        filePath += ".pdf";
    }

    // Export the report
    bool result = exportMultiplePaths(pathData, filePath);

    if (result)
    {
        QMessageBox::information(
            parent, tr("Export Successful"),
            tr("Path report was successfully exported "
               "to:\n%1")
                .arg(filePath));
    }
    else
    {
        QMessageBox::critical(
            parent, tr("Export Error"),
            tr("Failed to export path report to:\n%1")
                .arg(filePath));
    }

    return result;
}

bool PathReportExporter::previewReport(
    const QList<const ShortestPathsTable::PathData *>
            &pathData,
    QWidget *parent)
{
    if (pathData.isEmpty())
    {
        qCWarning(lcGuiUtil) << "PathReportExporter::previewReport:"
                             << "empty pathData list";
        QMessageBox::warning(
            parent, tr("Preview Error"),
            tr("No path data available to preview."));
        return false;
    }

    try
    {
        // Create a temporary file
        QTemporaryFile tempFile;
        tempFile.setAutoRemove(
            false); // Keep the file until we're done with
                    // it

        if (!tempFile.open())
        {
            qCWarning(lcGuiUtil) << "PathReportExporter::previewReport:"
                                 << "failed to create temp file";
            QMessageBox::critical(
                parent, tr("Preview Error"),
                tr("Failed to create temporary file for "
                   "report preview."));
            return false;
        }

        // Generate the report
        PathReportGenerator generator(pathData);
        auto report = generator.generateReport();
        if (!report)
        {
            qCWarning(lcGuiUtil) << "PathReportExporter::previewReport:"
                                 << "report generation returned null";
            QMessageBox::critical(
                parent, tr("Preview Error"),
                tr("Failed to generate report for "
                   "preview."));
            return false;
        }

        // Create and show the preview dialog
        KDReports::PreviewDialog previewDialog(report,
                                               parent);
        previewDialog.setWindowTitle(
            tr("Path Report Preview"));
        previewDialog.resize(800, 600);
        previewDialog.exec();

        return true;
    }
    catch (const std::exception &e)
    {
        QMessageBox::critical(
            parent, tr("Preview Error"),
            tr("Failed to preview report: %1")
                .arg(e.what()));
        return false;
    }
}

} // namespace GUI
} // namespace CargoNetSim
