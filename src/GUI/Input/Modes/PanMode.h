#pragma once

#include <QPoint>

#include "../Interfaces/IInteractionMode.h"

namespace CargoNetSim::GUI::Input {

/**
 * Transient mode. Pushed by GraphicsView when Ctrl+LeftPress or MiddlePress is
 * detected; pops itself on mouse release. Lets the user pan without leaving
 * the active mode (e.g., Ctrl-drag-pan inside ConnectMode).
 */
class PanMode final : public IInteractionMode {
public:
    explicit PanMode(QPoint initialViewPos);
    ~PanMode() override;

    QString name() const override { return QStringLiteral("Pan"); }
    QCursor cursor() const override { return QCursor(Qt::ClosedHandCursor); }
    bool    isTransient() const override { return true; }

    Handled onMove   (const MoveEvent&,    const ClickContext&) override;
    Handled onRelease(const ReleaseEvent&, const ClickContext&) override;

private:
    QPoint m_lastViewPos;   // last mouse position in VIEW coords
};

} // namespace CargoNetSim::GUI::Input
