/**
 * @file PathComparisonDialog.h
 * @brief Dialog for comparing multiple path data side by
 * side
 * @author Ahmed Aredah
 * @date April 18, 2025
 *
 * This file contains the declaration of the
 * PathComparisonDialog class, which provides a UI component
 * for displaying and comparing multiple paths side-by-side
 * in the CargoNetSim system.
 */
#pragma once

#include "GUI/Utils/PathComparisonViewModel.h"
#include "GUI/Widgets/ShortestPathTable.h"
#include <QDialog>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace CargoNetSim
{
namespace GUI
{

/**
 * @class PathComparisonDialog
 * @brief Dialog for displaying side-by-side path
 * comparisons
 *
 * This class provides a specialized dialog for comparing
 * multiple paths side-by-side, including their terminals,
 * costs, and transportation modes.
 */
class PathComparisonDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Constructs a PathComparisonDialog
     * @param pathData List of PathData pointers to compare
     * @param parent The parent widget
     *
     * Creates a dialog showing the provided paths for
     * comparison.
     */
    explicit PathComparisonDialog(
        const QList<const ShortestPathsTable::PathData *>
                &pathData,
        QWidget *parent = nullptr);

    /**
     * @brief Destructor
     */
    ~PathComparisonDialog();

private slots:
    /**
     * @brief Exports the comparison data to CSV
     */
    void onExportButtonClicked();

private:
    /**
     * @brief Initializes the UI components
     */
    void initUI();

    /**
     * @brief Creates the summary comparison tab
     * @return Widget containing the summary comparison
     */
    QWidget *createSummaryTab();

    /**
     * @brief Creates the detailed terminals tab
     * @return Widget containing the terminals comparison
     */
    QWidget *createTerminalsTab();

    /**
     * @brief Creates the segments comparison tab
     * @return Widget containing the segments comparison
     */
    QWidget *createSegmentsTab();

    /**
     * @brief Creates the costs comparison tab
     * @return Widget containing the costs comparison
     */
    QWidget *createCostsTab();

    /**
     * @brief Creates a table for comparing path attributes
     * @param headers List of column headers
     * @param rowLabels List of row labels
     * @param data 2D list of cell data
     * @return QTableWidget configured with the data
     */
    QTableWidget *
    createComparisonTable(const QStringList &headers,
                          const QStringList &rowLabels,
                          const QList<QStringList> &data);

    /**
     * @brief Creates a visualization of the path routes
     * @param container Widget to place the visualization in
     */
    void createPathVisualization(QWidget *container);

    /**
     * @brief Creates a pixel map for a transportation mode
     * @param mode Transportation mode as a string
     * @return Pixmap for the mode with appropriate coloring
     */
    QPixmap createTransportModePixmap(const QString &mode);

    QString
    getTerminalDisplayNameByID(Backend::Path *path,
                               const QString &terminalID);

    /**
     * @brief List of PathData objects being compared
     */
    QList<const ShortestPathsTable::PathData *> m_pathData;
    PathComparisonViewModel m_viewModel;

    /**
     * @brief Tab widget for organizing comparison views
     */
    QTabWidget *m_tabWidget;

    /**
     * @brief Export button for saving comparison data
     */
    QPushButton *m_exportButton;
};

} // namespace GUI
} // namespace CargoNetSim
