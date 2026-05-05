#pragma once

#include <QDockWidget>
#include <QListWidget>
#include <QMap>
#include <QPushButton>
#include <QString>

class QGridLayout;
class QToolButton;

namespace CargoNetSim
{
namespace GUI
{

class MainWindow;

/**
 * @brief The NetworkManagerDialog class provides a dock
 * widget for managing train and truck networks in the
 * CargoNetSim application.
 *
 * This dialog allows users to add, rename, delete, and
 * change the color of networks for different transportation
 * modes.
 */
class NetworkManagerDialog : public QDockWidget
{
    Q_OBJECT

public:
    /**
     * @brief Constructs a NetworkManagerDialog.
     * @param parent The parent widget, typically the main
     * window.
     */
    explicit NetworkManagerDialog(QWidget *parent = nullptr);

    /**
     * @brief Destructor.
     */
    virtual ~NetworkManagerDialog() = default;

    /**
     * @brief Updates the network list for the specified
     * network type.
     * @param networkType The type of network ("Rail
     * Network" or "Truck Network").
     */
    void updateNetworkList(const QString &networkType);

    /**
     * @brief Updates network lists for all changed regions.
     * @param regionName The name of the region that
     * changed.
     */
    void updateNetworkListForChangedRegion(
        const QString &regionName);

    /**
     * @brief Clears all network lists and removes networks
     * from canvas.
     */
    void clear();

private slots:
    /**
     * @brief Handles selection changes in the network list.
     * @param networkType The type of network that had a
     * selection change.
     */
    void onSelectionChanged(const QString &networkType);

    /**
     * @brief Adds a new network of the specified type.
     * @param networkType The type of network to add.
     */
    void addNetwork(const QString &networkType);

    /**
     * @brief Renames the selected network of the specified
     * type.
     * @param networkType The type of network to rename.
     */
    void renameNetwork(const QString &networkType);

    /**
     * @brief Deletes the selected network of the specified
     * type.
     * @param networkType The type of network to delete.
     */
    void deleteNetwork(const QString &networkType);

    /**
     * @brief Changes the color of the selected network of
     * the specified type.
     * @param networkType The type of network to change the
     * color of.
     */
    void changeNetworkColor(const QString &networkType);

    /**
     * @brief Handles checkbox state changes in network
     * items.
     * @param item The list widget item that changed.
     * @param networkType The type of network that changed.
     */
    void onItemCheckedChanged(QListWidgetItem *item,
                             const QString &networkType);

private:
    /**
     * @brief Creates a network tab for the specified
     * network type.
     * @param networkType The type of network tab to create.
     * @return The created tab widget.
     */
    QWidget *createNetworkTab(const QString &networkType);

    /**
     * @brief Creates a color pixmap for use in network list
     * items.
     * @param color The color to use for the pixmap.
     * @param size The size of the pixmap.
     * @return The created pixmap.
     */
    QPixmap createColorPixmap(const QColor &color,
                             int size = 16);

    MainWindow *mainWindow;
    QMap<QString, QMap<QString, QPushButton *>> networkButtons;
};

} // namespace GUI
} // namespace CargoNetSim
