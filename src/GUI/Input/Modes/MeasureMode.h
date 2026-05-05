#pragma once

#include <QPointF>
#include <QPointer>

#include "../Interfaces/IInteractionMode.h"

namespace CargoNetSim::GUI {
class DistanceMeasurementTool;
namespace Input {

class MeasureMode final : public IInteractionMode {
public:
    MeasureMode();
    ~MeasureMode() override;

    QString name() const override { return QStringLiteral("Measure"); }
    QCursor cursor() const override;

    Handled onPress   (const PressEvent&,   const ClickContext&) override;
    Handled onMove    (const MoveEvent&,    const ClickContext&) override;
    Handled onKeyPress(const KeyPressEvent&,const ClickContext&) override;

    void onEnter(const ClickContext&) override;
    void onExit (const ClickContext&) override;

private:
    QPointer<DistanceMeasurementTool> m_tool;
    QPointF                           m_startPoint;
    bool                              m_hasStart = false;
};

} // namespace Input
} // namespace CargoNetSim::GUI
