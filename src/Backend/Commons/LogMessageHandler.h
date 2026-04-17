#pragma once

#include <QtGlobal>

namespace CargoNetSim
{
namespace Backend
{

class LoggerInterface;

// Maps a cargonetsim.* category string to the ClientType
// index used by ApplicationLogger's GUI log tabs.
//   0 = ShipClient, 1 = TrainClient, 2 = TruckClient,
//   3 = TerminalClient, 4 = General/CargoNetSim
int categoryToClientType(const char *category);

// Installs a custom Qt message handler that:
//   1. Delegates to the previous handler for stderr output.
//   2. For info/warning/critical messages from cargonetsim.*
//      categories, forwards to the provided LoggerInterface
//      so they appear in the GUI "Servers Log" tabs.
//
// Pass nullptr for guiLogger in CLI mode (stderr-only).
void installCargoNetSimLogHandler(
    LoggerInterface *guiLogger = nullptr);

} // namespace Backend
} // namespace CargoNetSim
