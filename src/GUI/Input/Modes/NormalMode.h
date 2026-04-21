#pragma once

#include "../Interfaces/IInteractionMode.h"

namespace CargoNetSim::GUI::Input {

/**
 * Null-ish default mode. Handles the cross-cutting no-mode-specific
 * interactions: Delete/Backspace on selection, item-library drop, and
 * middle-button double-click fit-to-view. Everything else falls through to
 * the item-interface dispatcher.
 */
class NormalMode final : public IInteractionMode {
public:
    QString name() const override { return QStringLiteral("Normal"); }

    Handled onKeyPress    (const KeyPressEvent&,     const ClickContext&) override;
    Handled onDrop        (const DropEvent&,         const ClickContext&) override;
    Handled onDoubleClick (const DoubleClickEvent&,  const ClickContext&) override;
};

} // namespace CargoNetSim::GUI::Input
