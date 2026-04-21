#include "PanMode.h"

#include "../../../Backend/Commons/LogCategories.h"
#include "../../Widgets/GraphicsView.h"
#include "../ClickContext.h"
#include "../InteractionController.h"

#include <QLoggingCategory>
#include <QScrollBar>

namespace CargoNetSim::GUI::Input {

PanMode::PanMode(QPoint initialViewPos)
    : m_lastViewPos(initialViewPos)
{
    qCDebug(lcGuiInputMode) << "PanMode ctor; initial view pos =" << m_lastViewPos;
}

PanMode::~PanMode() = default;

Handled PanMode::onMove(const MoveEvent&, const ClickContext& ctx)
{
    if (!ctx.view) {
        qCInfo(lcGuiInputMode) << "PanMode::onMove: no view — PassThrough";
        return Handled::PassThrough;
    }
    QPoint currentViewPos = ctx.view->mapFromScene(ctx.scenePos);
    QPoint delta          = currentViewPos - m_lastViewPos;
    qCInfo(lcGuiInputMode) << "PanMode::onMove: viewPos=" << currentViewPos
                           << "delta=" << delta;
    m_lastViewPos         = currentViewPos;

    if (auto* hbar = ctx.view->horizontalScrollBar()) hbar->setValue(hbar->value() - delta.x());
    if (auto* vbar = ctx.view->verticalScrollBar())   vbar->setValue(vbar->value() - delta.y());
    return Handled::Yes;
}

Handled PanMode::onRelease(const ReleaseEvent& e, const ClickContext& ctx)
{
    Q_UNUSED(e);
    qCDebug(lcGuiInputMode) << "PanMode::onRelease — popping";
    ctx.controller->popMode();
    return Handled::Yes;
}

} // namespace CargoNetSim::GUI::Input
