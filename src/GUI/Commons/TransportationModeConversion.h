// src/GUI/Commons/TransportationModeConversion.h
#pragma once

#include "Backend/Commons/TransportationMode.h"
#include "GUI/Commons/NetworkType.h"

namespace CargoNetSim {
namespace GUI {

/// Single translation between the GUI's NetworkType and the Backend's
/// TransportationMode. Every GUI site that needs to cross this
/// boundary (calculator feeds, populator dispatches, allocator keys)
/// calls this — no open-coded switches.
Backend::TransportationTypes::TransportationMode
transportationModeFromNetworkType(NetworkType t);

} // namespace GUI
} // namespace CargoNetSim
