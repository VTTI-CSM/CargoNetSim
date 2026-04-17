// src/GUI/Commons/TransportationModeConversion.cpp
#include "TransportationModeConversion.h"

namespace CargoNetSim {
namespace GUI {

Backend::TransportationTypes::TransportationMode
transportationModeFromNetworkType(NetworkType t)
{
    using Mode = Backend::TransportationTypes::TransportationMode;
    switch (t)
    {
    case NetworkType::Train: return Mode::Train;
    case NetworkType::Truck: return Mode::Truck;
    case NetworkType::Ship:  return Mode::Ship;
    }
    return Mode::Any;  // unreachable; quiets warning
}

} // namespace GUI
} // namespace CargoNetSim
