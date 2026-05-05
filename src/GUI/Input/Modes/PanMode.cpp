#include "PanMode.h"

#include "../../../Backend/Commons/LogCategories.h"
#include "../../Widgets/GraphicsView.h"
#include "../ClickContext.h"
#include "../InteractionController.h"

#include <QLoggingCategory>
#include <QScrollBar>

namespace CargoNetSim::GUI::Input {

PanMode::PanMode(QPoint initialScreenPos)
    : m_lastScreenPos(initialScreenPos)
{
    qCDebug(lcGuiInputMode) << "PanMode ctor; initial screen pos =" << m_lastScreenPos;
}

PanMode::~PanMode() = default;

Handled PanMode::onMove(const MoveEvent& e, const ClickContext& ctx)
{
    if (!ctx.view) {
        qCInfo(lcGuiInputMode) << "PanMode::onMove: no view — PassThrough";
        return Handled::PassThrough;
    }
    // Delta in GLOBAL screen coords — unaffected by our own scrollbar edits.
    const QPoint delta = e.screenPos - m_lastScreenPos;
    m_lastScreenPos    = e.screenPos;
    qCInfo(lcGuiInputMode) << "PanMode::onMove: screenPos=" << e.screenPos
                           << "delta=" << delta;

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
