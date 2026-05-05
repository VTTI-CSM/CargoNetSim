#ifndef ICONFACTORY_H
#define ICONFACTORY_H

#include <QMap>
#include <QPixmap>
#include <QString>

/*!
 * \brief A namespace providing a set of functions to create
 * custom QPixmaps for various icons.
 *
 * Each function returns a QPixmap that has been drawn using
 * QPainter and related Qt classes.
 */
namespace CargoNetSim
{
namespace GUI
{
namespace IconFactory
{
// Returns a map of terminal icon names to their
// corresponding QPixmaps.
QMap<QString, QPixmap> createTerminalIcons();

QPixmap createConnectTerminalsPixmap(int size = 32);
QPixmap
createAssignSelectedToCurrentRegionPixmap(int size = 32);
QPixmap createSetBackgroundColorPixmap(int size = 32);
QPixmap createMeasureDistancePixmap(int size = 32);
QPixmap createClearMeasurementsPixmap(int size = 32);
QPixmap createPropertiesIcon(int size = 128);
QPixmap createFreightTerminalLibraryIcon(int size = 128);
QPixmap createRegionManagerIcon(int size = 128);
QPixmap createSimulationSettingsIcon(int size = 128);
QPixmap createShowHideGridIcon(int size = 128);
QPixmap createFreightTrainIcon(int size = 128);
QPixmap createFreightTruckIcon(int size = 128);
QPixmap createNetworkManagerIcon(int size = 128);
QPixmap createLinkTerminalIcon();
QPixmap createUnlinkTerminalIcon();
QPixmap createLinkTerminalsToNetworkIcon(int size = 32);
QPixmap createUnlinkTerminalsToNetworkIcon(int size = 32);
QPixmap createAutoConnectTerminalsIcon(int size = 32);
QPixmap createConnectByInterfaceIcon(int size = 32);
QPixmap createCheckNetworkIcon(int size = 128);
QPixmap createMoveNetworkIcon(int size = 128);
QPixmap createUnconnectTerminalsIcon(int size = 32);
QPixmap createSettingsIcon(int size = 128);
QPixmap createNewProjectIcon(int size = 128);
QPixmap createOpenProjectIcon(int size = 128);
QPixmap createSaveProjectIcon(int size = 128);
QPixmap createShortestPathsIcon(int size = 128);
QPixmap createVerifySimulationIcon(int size = 128);
QPixmap createPanModeIcon(int size = 128);
QPixmap createShowHideTerminalsIcon(int size = 128);
QPixmap createShowHideConnectionsIcon(int size = 128);
QPixmap createShowEyeIcon(int size = 128);
QPixmap createShowHidePathsTableIcon(int size = 128);
QPixmap createThickWhiteArrowPixmap(int size  = 32,
                                    int width = 200);
QPixmap createThickWhiteLinePixmap(int size  = 32,
                                   int width = 200);
QPixmap createImportTrainsIcon(int size = 128);
QPixmap createDeleteTrainIcon(int size = 128);
QPixmap createImportShipsIcon(int size = 128);
QPixmap createDeleteShipIcon(int size = 128);
QPixmap createTrainManagerIcon(int size = 128);
QPixmap createShipManagerIcon(int size = 128);
QPixmap createSetGlobalPositionIcon();
QPixmap createTransportationModePixmap(const QString &mode,
                                       int size  = 32,
                                       int width = 64);

QPixmap createCalculatorIcon(int size = 128);
QPixmap createFilterConnectionsIcon(int size = 128);
QPixmap createStatusReadyIcon(int size = 16);
QPixmap createStatusWarningIcon(int size = 16);
QPixmap createStatusUnavailableIcon(int size = 16);

} // namespace IconFactory
} // namespace GUI
} // namespace CargoNetSim

#endif // ICONFACTORY_H
