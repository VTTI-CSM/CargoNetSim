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
#include "Backend/Application/PathPresentationService.h"
#include "Backend/Scenario/ExecutionPlanTypes.h"
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
#include <memory>
#include <optional>
#include <utility>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{
class ScenarioDocument;
class PreparedPathSet;
class ScenarioExecutionResultSet;
}
}

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
    using PathIdentity = QString;
    using PathData = Backend::Application::PathPresentationRecord;

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
    /// When non-empty, predicted/actual metric columns populate per
    /// canonical path identity.
    void addPaths(
        const QList<Backend::Path *> &paths,
        const QHash<PathIdentity, Backend::Scenario::PathMetrics> &predicted = {},
        const QHash<PathIdentity, Backend::Scenario::PathMetrics> &actual    = {});

    /// A2: consume backend-owned prepared paths directly without
    /// transferring raw-path ownership into the GUI layer.
    void setPreparedPaths(
        const Backend::Scenario::PreparedPathSet                  &prepared,
        const QHash<PathIdentity, Backend::Scenario::PathMetrics> &actual = {},
        const QHash<PathIdentity, Backend::Scenario::PreparedPathEligibility>
            &eligibility = {});

    /// Update backend-produced per-path eligibility for existing rows.
    void setPathEligibility(
        const QHash<PathIdentity, Backend::Scenario::PreparedPathEligibility>
            &eligibility);

    /// Post-run update. Calls refreshRow for each path identity present.
    void setActualMetrics(
        const QHash<PathIdentity, Backend::Scenario::PathMetrics> &actual);

    /// Store the backend-owned typed execution results for comparison/export.
    void setExecutionResults(
        const Backend::Scenario::ScenarioExecutionResultSet &results);

    /// Update per-path execution progress from the backend runtime snapshot.
    void setExecutionProgress(
        const Backend::Scenario::ExecutionProgressSnapshot &snapshot);

    /// Clear stale per-path execution progress before a new run starts.
    void clearExecutionProgress();

    /**
     * @brief Gets the size of the paths in the table
     * @return The number of paths in the table.
     */
    int pathsSize() const;

    /**
     * @brief Updates the prediction costs for an existing
     * path
     * @param pathKey Stable backend-derived path identity
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
    void updatePredictionCosts(const PathIdentity &pathKey,
                               double totalCost    = -1.0,
                               double edgeCost     = -1.0,
                               double terminalCost = -1.0);

    /**
     * @brief Updates the simulation costs for an existing
     * path
     * @param pathKey Stable backend-derived path identity
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
        const PathIdentity &pathKey,
        double simulationTotalCost = -1.0,
        double simulationEdgeCost     = -1.0,
        double simulationTerminalCost = -1.0);

    /**
     * @brief Retrieves path data for a specific path identity
     * @param pathKey Stable backend-derived path identity
     * @return Pointer to the path data or nullptr if not
     * found
     *
     * Provides access to the PathData object for a specific
     * path identity, allowing inspection of both prediction and
     * simulation costs.
     */
    const PathData *getDataByPathKey(
        const PathIdentity &pathKey) const;

    /**
     * @brief Gets all checked path entries
     * currently checked
     * @return List of checked path data pointers
     */
    const QList<const ShortestPathsTable::PathData *>
    getCheckedPathData() const;

    /**
     * @brief Gets the currently selected path identity
     * @return The selected path identity or an empty string if none selected
     *
     * Returns the stable identity of the path that is
     * currently selected in the table, or an empty string
     * if no path is selected.
     */
    PathIdentity getSelectedPathKey() const;

    /**
     * @brief Gets all path identities that are currently checked
     * @return Vector of checked path identities
     *
     * Returns a vector containing the identities of all paths that
     * have their checkbox checked in the table.
     */
    QVector<PathIdentity> getCheckedPathKeys() const;

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
     * @param pathKey Stable backend-derived path identity
     *
     * Notifies listeners that the user has selected a
     * different path in the table.
     */
    void pathSelected(const PathIdentity &pathKey);

    /**
     * @brief Signal emitted when the show path button is
     * clicked
     * @param pathKey Stable backend-derived path identity
     *
     * Notifies listeners that the user has requested to
     * visualize a specific path on the map or in another
     * view.
     */
    void showPathSignal(const PathIdentity &pathKey);

    /**
     * @brief Signal emitted when a path checkbox state
     * changes
     * @param pathKey Stable backend-derived path identity
     * @param checked New checkbox state (true = checked,
     * false = unchecked)
     *
     * Notifies listeners that the user has changed the
     * checked state of a path in the table.
     */
    void checkboxChanged(const PathIdentity &pathKey,
                         bool                checked);

    /**
     * @brief Signal emitted when path comparison is
     * requested
     * @param pathKeys Vector of path identities to compare
     *
     * Notifies listeners that the user has requested a
     * comparison of the specified paths.
     */
    void pathComparisonRequested(
        const QVector<PathIdentity> &pathKeys);

    /**
     * @brief Signal emitted when path export is requested
     * @param pathKey Stable backend-derived path identity
     *
     * Notifies listeners that the user has requested to
     * export data for a specific path.
     */
    void pathExportRequested(const PathIdentity &pathKey);

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
     * @param pathKeys Identities of the paths to export
     */
    void exportPathsToPdf(
        const QVector<PathIdentity> &pathKeys);

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
    bool isSelectable(const PathData *pathData) const;
    int  selectablePathCount() const;
    QString eligibilityTooltip(const PathData *pathData) const;
    QString availabilityBannerText() const;
    void updateAvailabilityBanner();

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
     * @param pathKey Stable backend-derived path identity
     * @param pathData The PathData object containing path
     * details
     * @return A widget representing the path with terminals
     * and transportation modes
     *
     * Generates a custom widget for visualizing a path,
     * including terminals, transportation modes, and a
     * button to show the path on a map.
     */
    QWidget *createPathRow(const PathIdentity &pathKey,
                           const PathData     *pathData);

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
    void flashPathMapLines(const PathIdentity &pathKey);

    void createSelectionPanel();

    /**
     * @brief Table widget for displaying path data
     *
     * The main UI component that shows paths, their
     * terminals, transportation modes, and associated
     * costs.
     */
    QTableWidget *m_table;
    QLabel       *m_availabilityBanner = nullptr;

    /**
     * @brief Storage for path data indexed by stable path identity
     *
     * Maps path identities to their corresponding PathData
     * objects, allowing efficient lookup and update
     * operations.
     */
    QMap<PathIdentity, PathData *> m_pathData;
    QVector<PathIdentity>          m_displayOrder;

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

    /// Plan 8.2/A2: per-path predicted/actual metrics and row mapping
    /// keyed by canonical backend path identity rather than local path_id.
    QHash<PathIdentity, Backend::Scenario::PathMetrics> m_predicted;
    QHash<PathIdentity, Backend::Scenario::PathMetrics> m_actual;
    QHash<PathIdentity, Backend::Scenario::PathProgressSnapshot>
        m_progressByPathKey;
    QHash<PathIdentity /*pathKey*/, int /*row*/>        m_rowByPathKey;

    PathIdentity appendPathRecord(PathData record);

    /// Plan 8.2/A2: rewrite metric columns for a given canonical path.
    void refreshRow(const PathIdentity &pathKey);
    void refreshProgressCell(const PathIdentity &pathKey);
};

} // namespace GUI
} // namespace CargoNetSim
