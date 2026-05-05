#pragma once

#include "../Interfaces/IInteractionMode.h"

namespace CargoNetSim::GUI::Input {

class GlobalPositionMode final : public IInteractionMode {
public:
    QString name() const override { return QStringLiteral("GlobalPosition"); }
    QCursor cursor() const override { return QCursor(Qt::CrossCursor); }

    Handled onPress   (const PressEvent&,    const ClickContext&) override;
    Handled onKeyPress(const KeyPressEvent&, const ClickContext&) override;

    void onEnter(const ClickContext&) override;
    void onExit (const ClickContext&) override;
};

} // namespace CargoNetSim::GUI::Input
