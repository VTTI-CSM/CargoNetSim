#pragma once

#include <QDialog>
#include <QHeaderView>
#include <QStringList>
#include <QTableWidget>
#include <QTextEdit>
#include <QToolBar>
#include <QToolButton>
#include <QVector>

#include "../../Backend/Models/TrainSystem.h"

namespace CargoNetSim
{
namespace GUI
{

class ToolbarController;
class BasicButtonController;

/**
 * @brief Dialog for managing train configurations
 *
 * The TrainManagerDialog allows users to load, view, and
 * manage train configurations used in simulations. It
 * handles importing train data from files and maintaining a
 * list of available trains.
 */
class TrainManagerDialog : public QDialog
{
    Q_OBJECT

    friend class ToolbarController;
    friend class BasicButtonController;

public:
    /**
     * @brief Constructor for TrainManagerDialog
     * @param parent The parent widget
     */
    explicit TrainManagerDialog(QWidget *parent = nullptr);

    /**
     * @brief Destructor for TrainManagerDialog
     */
    virtual ~TrainManagerDialog() = default;

    /**
     * @brief Get the current trains from the dialog
     * @return Vector of train objects
     */
    QVector<Backend::Train *> getTrains() const;

    /// @brief Returns the list of file paths loaded during this dialog session.
    QStringList newlyLoadedFiles() const { return m_newlyLoadedFiles; }

signals:
    /**
     * @brief Signal emitted when trains are loaded from a
     * file
     * @param count Number of trains loaded
     * @param success Whether the operation was successful
     */
    void trainsLoaded(int count, bool success);

    /**
     * @brief Signal emitted when a train is deleted
     * @param trainId The ID of the deleted train
     */
    void trainDeleted(const QString &trainId);

    /**
     * @brief Signal emitted when the train list changes
     */
    void trainListChanged();

public slots:
    /**
     * @brief Set the trains to display in the dialog
     * @param trains Vector of train objects
     */
    void setTrains(const QVector<Backend::Train *> trains);

private slots:
    /**
     * @brief Load trains from a selected file
     */
    void loadTrains();

    /**
     * @brief Delete the currently selected train
     */
    void deleteTrain();

    /**
     * @brief Update train details when selection changes
     */
    void updateTrainDetails();

protected:
    /**
     * @brief Update the trains table with current data
     */
    void updateTable();

private:
    /**
     * @brief Initialize the user interface
     */
    void initUI();

    /**
     * @brief Format train details for display
     * @param train The train to format details for
     * @return Formatted HTML string with train details
     */
    QString
    formatTrainDetails(const Backend::Train *train) const;

    // UI Components
    QTableWidget *m_table;
    QTextEdit    *m_detailsText;
    QToolButton  *m_loadButton;
    QToolButton  *m_deleteButton;

    // Data
    QVector<Backend::Train *> m_trains;
    QStringList               m_newlyLoadedFiles;
};

} // namespace GUI
} // namespace CargoNetSim
