#pragma once

#include "../../Backend/Models/ShipSystem.h"
#include <QAction>
#include <QDialog>
#include <QList>
#include <QSplitter>
#include <QStringList>
#include <QTableWidget>
#include <QTextEdit>
#include <QToolBar>
#include <memory>

namespace CargoNetSim
{
namespace GUI
{

class ToolbarController;
class BasicButtonController;

/**
 * @brief Dialog for managing ship entities in the
 * simulation
 *
 * ShipManagerDialog provides an interface for loading,
 * viewing, and managing ships that can be used in the
 * simulation. It displays ships in a table with detailed
 * properties in a separate view.
 */
class ShipManagerDialog : public QDialog
{
    Q_OBJECT

    friend class ToolbarController;
    friend class BasicButtonController;

public:
    /**
     * @brief Constructor
     * @param parent Parent widget
     */
    explicit ShipManagerDialog(QWidget *parent = nullptr);

    /**
     * @brief Destructor
     */
    ~ShipManagerDialog() = default;

    /**
     * @brief Get the managed ships
     * @return List of ship objects
     */
    QList<Backend::Ship *> getShips() const;

    /// @brief Returns the list of file paths loaded during this dialog session.
    QStringList newlyLoadedFiles() const { return m_newlyLoadedFiles; }

    /**
     * @brief Set the ships to be managed
     * @param ships List of ship objects
     */
    void setShips(const QList<Backend::Ship *> &ships);

signals:
    /**
     * @brief Signal emitted when ships are loaded
     * @param count Number of ships loaded
     */
    void shipsLoaded(int count);

    /**
     * @brief Signal emitted when a ship is selected
     * @param shipId ID of the selected ship
     */
    void shipSelected(const QString &shipId);

    /**
     * @brief Signal emitted when a ship is deleted
     * @param shipId ID of the deleted ship
     */
    void shipDeleted(const QString &shipId);

private slots:
    /**
     * @brief Load ships from a file
     */
    void loadShips();

    /**
     * @brief Delete the selected ship
     */
    void deleteShip();

    /**
     * @brief Update details view when selection changes
     */
    void updateDetails();

protected:
    /**
     * @brief Update the table with current ships
     */
    void updateTable();

private:
    /**
     * @brief Initialize the user interface
     */
    void initUI();

    /**
     * @brief Format ship details for display
     * @param ship Ship object
     * @return Formatted HTML string for display
     */
    QString
    formatShipDetails(const Backend::Ship &ship) const;

    // UI components
    QTableWidget *m_table;
    QTextEdit    *m_detailsText;
    QSplitter    *m_splitter;
    QToolBar     *m_toolbar;
    QAction      *m_loadAction;
    QAction      *m_deleteAction;

    // Data
    QList<Backend::Ship *> m_ships;
    QStringList            m_newlyLoadedFiles;
};

} // namespace GUI
} // namespace CargoNetSim
