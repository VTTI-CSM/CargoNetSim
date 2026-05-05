#pragma once

class QMenu;

namespace CargoNetSim::GUI::Input {

struct ClickContext;

/**
 * Items that want to show a context menu on right-click implement this.
 * The controller owns the QMenu and calls exec(); action lambdas MUST
 * QPointer-capture the target item (the menu can outlive the item on scene
 * clear or scenario reload).
 */
class IContextMenuProvider {
public:
    virtual ~IContextMenuProvider() = default;
    virtual void buildContextMenu(QMenu* menu, const ClickContext&) = 0;
};

} // namespace CargoNetSim::GUI::Input
