/**
 * @file ShortestPathTable.h
 * @brief Defines the ShortestPathsTable widget for path
 * data visualization and comparison
 * @author Ahmed Aredah
 * @date April 5, 2025
 *
 * This file contains the declaration of the
 * ShortestPathsTable class, which provides a UI component
 * for displaying, comparing, and exporting path data in the
 * CargoNetSim system. It includes functionality for
 * visualizing transportation networks, including terminals,
 * routes, and associated costs.
 */
#pragma once
#include "Backend/Models/Path.h"
#include "Backend/Scenario/PathMetrics.h"
#include "Backend/Scenario/ScenarioDocument.h"
#include <QBrush>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QLabel>
#include <QMap>
#include <QPainterPath>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>
#include <QtCore/qcoreevent.h>
#include <QtGui/qevent.h>

namespace CargoNetSim
{
namespace GUI
{

/**
 * @class TerminalPathDelegate
 * @brief Custom delegate for rendering complex terminal
 * path visualizations in table cells
 *
 * This delegate class provides specialized rendering for
 * the terminal path column in the ShortestPathsTable,
 * allowing rich visual representations of paths including
 * terminals and transportation modes between them.
 */
class TerminalPathDelegate : public QStyledItemDelegate
{
public:
    /**
     * @brief Constructs a TerminalPathDelegate instance
     * @param parent The parent QObject (default: nullptr)
     */
    explicit TerminalPathDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    /**
     * @brief Renders the custom terminal path visualization
     * @param painter The QPainter to use for drawing
     * @param option The style options for the item
     * @param index The model index of the item to render
     *
     * Overrides the default painting behavior to display a
     * custom widget stored in the item's UserRole data if
     * available.
     */
    void paint(QPainter                   *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

    /**
     * @brief Determines the appropriate size for the custom
     * terminal path visualization
     * @param option The style options for the item
     * @param index The model index of the item
     * @return The preferred size for the item
     *
     * Returns the size of the custom widget if available,
     * otherwise defaults to the standard item delegate
     * size.
     */
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;
};

/**
 * @class ShortestPathsTable
 * @brief Widget for displaying, comparing, and exporting
 * path data
 *
 * This class provides a comprehensive UI component for
 * visualizing transportation paths, including their
 * terminals, transportation modes, and associated costs
 * (both predicted and simulated). It supports selecting,
 * comparing, and exporting path data, and emits signals
 * when the user interacts with paths.
 */
class ShortestPathsTable : public QWidget
{
    Q_OBJECT
public:
    /**
     * @struct PathData
     * @brief Extension structure that holds Path object and
     * additional simulation data
     *
     * This structure contains a pointer to a Path object
     * along with additional simulation cost data and
     * UI-specific properties. It provides a
     * composition-based approach rather than inheritance
     * since QObject-derived classes cannot be copied.
     */
    struct PathData
    {
        /**
         * @brief Constructs a PathData instance
         * @param path Pointer to the base Path object (will
         * be owned by this PathData)
         * @param simulationTotalCost Total cost from
         * simulation (default: -1.0 indicating unavailable)
         * @param simulationEdgeCost Edge costs from
         * simulation (default: -1.0 indicating unavailable)
         * @param simulationTerminalCost Terminal costs from
         * simulation (default: -1.0 indicating unavailable)
         *
         * Creates a PathData object that takes ownership of
         * the provided Path pointer and initializes
         * simulation-specific cost fields.
         */
        PathData(Backend::Path *path = nullptr,
                 double         simulationTotalCost = -1.0,
                 double         simulationEdgeCost  = -1.0,
                 double simulationTerminalCost      = -1.0)
            : path(path)
            , m_totalSimulationPathCost(simulationTotalCost)
            , m_totalSimulationEdgeCosts(simulationEdgeCost)
            , m_totalSimulationTerminalCosts(
                  simulationTerminalCost)
            , isVisible(true)
        {
        }

        /**
         * @brief Destructor that cleans up the owned Path
         * pointer
         */
        ~PathData()
        {
            if (path)
            {
                delete path;
                path = nullptr;
            }
        }

        /**
         * @brief Pointer to the Path object (owned by this
         * PathData)
         */
        Backend::Path *path;

        /**
         * @brief Total cost of the path from simulation
         */
        double m_totalSimulationPathCost;

        /**
         * @brief Total edge costs from simulation
         */
        double m_totalSimulationEdgeCosts;

        /**
         * @brief Total terminal costs from simulation
         */
        double m_totalSimulationTerminalCosts;

        /**
         * @brief Visibility flag for the path in the UI
         */
        bool isVisible;

        // Deleted copy constructor and assignment operator
        // to prevent accidental copying
        PathData(const PathData &)            = delete;
        PathData &operator=(const PathData &) = delete;

        // Allow move operations
        PathData(PathData &&other) noexcept
            : path(other.path)
            , m_totalSimulationPathCost(
                  other.m_totalSimulationPathCost)
            , m_totalSimulationEdgeCosts(
                  other.m_totalSimulationEdgeCosts)
            , m_totalSimulationTerminalCosts(
                  other.m_totalSimulationTerminalCosts)
            , isVisible(other.isVisible)
        {
            other.path = nullptr;
        }

        PathData &operator=(PathData &&other) noexcept
        {
            if (this != &other)
            {
                delete path;
                path = other.path;
                m_totalSimulationPathCost =
                    other.m_totalSimulationPathCost;
                m_totalSimulationEdgeCosts =
                    other.m_totalSimulationEdgeCosts;
                m_totalSimulationTerminalCosts =
                    other.m_totalSimulationTerminalCosts;
                isVisible  = other.isVisible;
                other.path = nullptr;
            }
            return *this;
        }
    };

    /**
     * @brief Constructs a ShortestPathsTable widget
     * @param parent The parent widget (default: nullptr)
     *
     * Initializes the shortest paths table with an empty
     * model and prepares the UI components for displaying
     * path data.
     */
    explicit ShortestPathsTable(QWidget *parent = nullptr);

    /**
     * @brief Virtual destructor
     *
     * Ensures proper cleanup of resources when the table is
     * destroyed.
     */
    ~ShortestPathsTable();

    /**
     * @brief Adds multiple paths to the table
     * @param paths List of Path pointers to add to the
     * table
     *
     * Processes a list of Path objects, converting each to
     * a PathData object and adding them to the table.
     * Simulation costs are initialized as unavailable
     * (-1.0). The table view is automatically refreshed
     * after adding the paths.
     */
    /// Plan 8.2: additive signature extension. Existing callers that
    /// pass only the path list continue to work (maps default to empty).
    /// When non-empty, predicted/actual metric columns populate per path id.
    void addPaths(
        const QList<Backend::Path *>                     &paths,
        const QHash<int, Backend::Scenario::PathMetrics> &predicted = {},
        const QHash<int, Backend::Scenario::PathMetrics> &actual    = {});

    /// Post-run update. Calls refreshRow for each pathId present.
    void setActualMetrics(
        const QHash<int, Backend::Scenario::PathMetrics> &actual);

    /**
     * @brief Gets the size of the paths in the table
     * @return The number of paths in the table.
     */
    int pathsSize() const;

    /**
     * @brief Updates the prediction costs for an existing
     * path
     * @param pathId The ID of the path to update
     * @param totalCost New predicted total cost (default:
     * -1.0 to not update)
     * @param edgeCost New predicted edge cost (default:
     * -1.0 to not update)
     * @param terminalCost New predicted terminal cost
     * (default: -1.0 to not update)
     *
     * Updates the prediction (analysis) cost fields for a
     * specific path and refreshes the display. Parameters
     * set to -1.0 indicate that the corresponding cost
     * should not be updated.
     */
    void updatePredictionCosts(int    pathId,
                               double totalCost    = -1.0,
                               double edgeCost     = -1.0,
                               double terminalCost = -1.0);

    /**
     * @brief Updates the simulation costs for an existing
     * path
     * @param pathId The ID of the path to update
     * @param simulationTotalCost New simulation total cost
     * (default: -1.0 to not update)
     * @param simulationEdgeCost New simulation edge cost
     * (default: -1.0 to not update)
     * @param simulationTerminalCost New simulation terminal
     * cost (default: -1.0 to not update)
     *
     * Updates the simulation cost fields for a specific
     * path and refreshes the display. Parameters set to
     * -1.0 indicate that the corresponding cost should not
     * be updated.
     */
    void updateSimulationCosts(
        int pathId, double simulationTotalCost = -1.0,
        double simulationEdgeCost     = -1.0,
        double simulationTerminalCost = -1.0);

    /**
     * @brief Retrieves path data for a specific path ID
     * @param pathId The ID of the path to retrieve
     * @return Pointer to the path data or nullptr if not
     * found
     *
     * Provides access to the PathData object for a specific
     * path ID, allowing inspection of both prediction and
     * simulation costs.
     */
    const PathData *getDataByPathId(int pathId) const;

    /**
     * @brief Gets all path IDs cooresponding data that are
     * currently checked
     * @return List of checked path data pointers
     */
    const QList<const ShortestPathsTable::PathData *>
    getCheckedPathData() const;

    /**
     * @brief Gets the currently selected path ID
     * @return The selected path ID or -1 if none selected
     *
     * Returns the ID of the path that is currently selected
     * in the table, or -1 if no path is selected.
     */
    int getSelectedPathId() const;

    /**
     * @brief Gets all path IDs that are currently checked
     * @return Vector of checked path IDs
     *
     * Returns a vector containing the IDs of all paths that
     * have their checkbox checked in the table.
     */
    QVector<int> getCheckedPathIds() const;

    /**
     * @brief Clears all data from the table
     *
     * Removes all paths from the table and resets the UI
     * state. This operation cannot be undone.
     */
    void clear();

    QList<QJsonObject> buildComparisonSnapshots(
        const Backend::Scenario::ScenarioDocument &doc) const;
    void loadComparisonSnapshots(
        const QList<QJsonObject> &snapshots);

signals:
    /**
     * @brief Signal emitted when a path is selected in the
     * table
     * @param pathId ID of the selected path
     *
     * Notifies listeners that the user has selected a
     * different path in the table.
     */
    void pathSelected(int pathId);

    /**
     * @brief Signal emitted when the show path button is
     * clicked
     * @param pathId ID of the path to show
     *
     * Notifies listeners that the user has requested to
     * visualize a specific path on the map or in another
     * view.
     */
    void showPathSignal(int pathId);

    /**
     * @brief Signal emitted when a path checkbox state
     * changes
     * @param pathId ID of the path whose checkbox changed
     * @param checked New checkbox state (true = checked,
     * false = unchecked)
     *
     * Notifies listeners that the user has changed the
     * checked state of a path in the table.
     */
    void checkboxChanged(int pathId, bool checked);

    /**
     * @brief Signal emitted when path comparison is
     * requested
     * @param pathIds Vector of path IDs to compare
     *
     * Notifies listeners that the user has requested a
     * comparison of the specified paths.
     */
    void
    pathComparisonRequested(const QVector<int> &pathIds);

    /**
     * @brief Signal emitted when path export is requested
     * @param pathId ID of the path to export
     *
     * Notifies listeners that the user has requested to
     * export data for a specific path.
     */
    void pathExportRequested(int pathId);

    /**
     * @brief Signal emitted when export of all paths is
     * requested
     *
     * Notifies listeners that the user has requested to
     * export data for all paths currently in the table.
     */
    void allPathsExportRequested();

private slots:
    /**
     * @brief Slot called when selection in the table
     * changes
     *
     * Updates the UI state and emits the pathSelected
     * signal when the user selects a different path in the
     * table.
     */
    void onSelectionChanged();

    /**
     * @brief Slot called when an item's checked state
     * changes
     * @param row The row of the changed item
     * @param column The column of the changed item
     *
     * Updates the UI state when a checkbox is toggled in
     * the table.
     */
    void onItemCheckedChanged(int row, int column);

    /**
     * @brief Slot called when the compare button is clicked
     *
     * Gathers the IDs of checked paths and emits the
     * pathComparisonRequested signal if at least two paths
     * are checked.
     */
    void onCompareButtonClicked();

    /**
     * @brief Slot called when the export button is clicked
     *
     * Emits the pathExportRequested signal for the
     * currently selected path.
     */
    void onExportButtonClicked();

    /**
     * @brief Slot called when the export all button is
     * clicked
     *
     * Emits the allPathsExportRequested signal to request
     * export of all paths.
     */
    void onExportAllButtonClicked();

    void onSelectAllButtonClicked();
    void onUnselectAllButtonClicked();

    /**
     * @brief Slot called when the export path to PDF is
     * called
     * @param pathIds IDs of the paths to export
     */
    void exportPathsToPdf(const QVector<int> &pathIds);

private:
    class PathScrollEventFilter : public QObject
    {
    protected:
        bool eventFilter(QObject *watched,
                         QEvent  *event) override
        {
            if (event->type() == QEvent::Wheel)
            {
                // Convert to wheel event
                QWheelEvent *wheelEvent =
                    static_cast<QWheelEvent *>(event);

                // Check if shift is pressed or if this is a
                // horizontal wheel event
                if (wheelEvent->modifiers()
                        & Qt::ShiftModifier
                    || wheelEvent->angleDelta().x() != 0)
                {
                    // Let the scroll area handle horizontal
                    // scrolling
                    return false;
                }

                // Ignore vertical wheel events to allow the
                // table to scroll
                return true;
            }

            return QObject::eventFilter(watched, event);
        }
    };

    /**
     * @brief Initializes the UI components
     *
     * Sets up the layout and creates all UI components
     * needed for the table, including the table widget and
     * control buttons.
     */
    void initUI();

    /**
     * @brief Creates and configures the table widget
     *
     * Initializes the QTableWidget with appropriate
     * columns, headers, and delegate for custom rendering.
     */
    void createTableWidget();

    /**
     * @brief Creates the path button panel
     *
     * Initializes buttons for path manipulation operations,
     * such as comparing paths.
     */
    void createPathButtonPanel();

    /**
     * @brief Creates the export panel
     *
     * Initializes buttons for exporting path data,
     * both for individual paths and for all paths.
     */
    void createExportPanel();

    /**
     * @brief Refreshes the table display with current path
     * data
     *
     * Updates the table view to reflect the current state
     * of path data, applying visibility filters and
     * formatting as needed.
     */
    void refreshTable();

    /**
     * @brief Creates a widget containing the terminal path
     * visualization
     * @param pathId The ID of the path to visualize
     * @param pathData The PathData object containing path
     * details
     * @return A widget representing the path with terminals
     * and transportation modes
     *
     * Generates a custom widget for visualizing a path,
     * including terminals, transportation modes, and a
     * button to show the path on a map.
     */
    QWidget *createPathRow(int             pathId,
                           const PathData *pathData);

    /**
     * @brief Creates an arrow pixmap with a label for a
     * transportation mode
     * @param mode The transportation mode to represent
     * @return A pixmap containing an arrow and the mode
     * label
     *
     * Generates a visual representation of a transportation
     * mode, color-coded and labeled for clear
     * identification.
     */
    QPixmap createArrowPixmap(const QString &mode) const;

    // Function to handle the showPath signal and flash path
    // map lines
    void flashPathMapLines(int pathId);

    void createSelectionPanel();

    /**
     * @brief Table widget for displaying path data
     *
     * The main UI component that shows paths, their
     * terminals, transportation modes, and associated
     * costs.
     */
    QTableWidget *m_table;

    /**
     * @brief Storage for path data indexed by path ID
     *
     * Maps path IDs to their corresponding PathData
     * objects, allowing efficient lookup and update
     * operations.
     */
    QMap<int, PathData *> m_pathData;

    /**
     * @brief Button to compare selected paths
     *
     * When clicked, emits a signal to request comparison of
     * checked paths.
     */
    QPushButton *m_compareButton;

    /**
     * @brief Button to export selected path
     *
     * When clicked, emits a signal to request export of the
     * selected path.
     */
    QPushButton *m_exportButton;

    /**
     * @brief Flag to prevent recursive UI updates
     *
     * Used to avoid infinite loops when updating the UI in
     * response to events that could trigger further
     * updates.
     */
    bool m_updatingUI;

    QPushButton *m_selectAllButton;
    QPushButton *m_unselectAllButton;

    PathScrollEventFilter *m_scrollEventFilter;

    /// Plan 8.2: per-path predicted/actual metrics and row mapping.
    QHash<int, Backend::Scenario::PathMetrics> m_predicted;
    QHash<int, Backend::Scenario::PathMetrics> m_actual;
    QHash<int /*pathId*/, int /*row*/>         m_rowByPathId;

    /// Plan 8.2: rewrite columns 5..15 for a given pathId row.
    void refreshRow(int pathId);
};

} // namespace GUI
} // namespace CargoNetSim
