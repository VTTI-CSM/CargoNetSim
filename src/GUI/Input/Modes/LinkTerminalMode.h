#pragma once

#include <QPointer>

#include "../Interfaces/IInteractionMode.h"

namespace CargoNetSim::GUI {
class TerminalItem;
namespace Input {

class LinkTerminalMode final : public IInteractionMode {
public:
    QString name() const override { return QStringLiteral("LinkTerminal"); }
    QCursor cursor() const override { return QCursor(Qt::CrossCursor); }

    Handled onPress   (const PressEvent&,    const ClickContext&) override;
    Handled onKeyPress(const KeyPressEvent&, const ClickContext&) override;

    void onEnter(const ClickContext&) override;
    void onExit (const ClickContext&) override;

private:
    QPointer<TerminalItem> m_firstTerminal;  // first click's TerminalItem
};

} // namespace Input
} // namespace CargoNetSim::GUI
