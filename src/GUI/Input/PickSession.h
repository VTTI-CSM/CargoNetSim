#pragma once

#include <QString>
#include <cstddef>
#include <variant>

namespace CargoNetSim {
namespace GUI {
namespace Input {

/// Which field on a shipment/origin the picker targets.
struct PrimaryDestinationSlot {};
struct MultiDestinationRowSlot { std::size_t rowIndex; };

using PickSlot = std::variant<PrimaryDestinationSlot, MultiDestinationRowSlot>;

/// A single, active "pick a destination" request. Value semantics by design:
/// the coordinator stores it by value inside std::optional, so lifetime is
/// decoupled from any widget.
struct PickSession {
    QString  requesterId;       ///< stable ID of the shipment/origin initiating the pick
    QString  originTerminalId;  ///< terminal the shipment currently originates from (rejected as self-pick)
    PickSlot slot;              ///< which field on the requester receives the result

    bool targetsRow(std::size_t row) const {
        if (auto* s = std::get_if<MultiDestinationRowSlot>(&slot)) {
            return s->rowIndex == row;
        }
        return false;
    }

    bool targetsPrimary() const {
        return std::holds_alternative<PrimaryDestinationSlot>(slot);
    }
};

} // namespace Input
} // namespace GUI
} // namespace CargoNetSim
