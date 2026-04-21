#include "MeasureMode.h"

#include "../../../Backend/Commons/LogCategories.h"
#include "../../Items/DistanceMeasurementTool.h"
#include "../../MainWindow.h"
#include "../../Widgets/GraphicsScene.h"
#include "../../Widgets/GraphicsView.h"
#include "../ClickContext.h"
#include "../InteractionController.h"
#include "../Modes/NormalMode.h"

#include <QLineF>
#include <QLoggingCategory>
#include <QStatusBar>

namespace CargoNetSim::GUI::Input {

MeasureMode::MeasureMode()  = default;
MeasureMode::~MeasureMode() = default;

QCursor MeasureMode::cursor() const { return QCursor(Qt::CrossCursor); }

void MeasureMode::onEnter(const ClickContext& ctx)
{
    qCInfo(lcGuiInputMode) << "MeasureMode::onEnter";
    if (ctx.mainWindow) {
        if (auto* sb = ctx.mainWindow->statusBar()) {
            sb->showMessage(QStringLiteral("Click two points to measure distance."));
        }
    }
}

void MeasureMode::onExit(const ClickContext& ctx)
{
    qCInfo(lcGuiInputMode) << "MeasureMode::onExit; discarding tool =" << m_tool.data();
    if (m_tool && ctx.scene) {
        ctx.scene->removeItem(m_tool);
        delete m_tool.data();
    }
    m_tool     = nullptr;
    m_hasStart = false;
    if (ctx.mainWindow) {
        if (auto* sb = ctx.mainWindow->statusBar()) {
            sb->clearMessage();
        }
    }
}

Handled MeasureMode::onPress(const PressEvent& e, const ClickContext& ctx)
{
    if (e.button != Qt::LeftButton) return Handled::PassThrough;
    if (!ctx.scene) {
        qCWarning(lcGuiInputMode) << "MeasureMode::onPress — null scene";
        return Handled::PassThrough;
    }

    if (!m_tool) {
        m_tool = new DistanceMeasurementTool(ctx.view);
        ctx.scene->addItem(m_tool);
        m_tool->setStartPoint(e.scenePos);
        m_tool->setEndPoint  (e.scenePos);
        m_startPoint = e.scenePos;
        m_hasStart   = true;
        qCDebug(lcGuiInputMode) << "MeasureMode: start at" << e.scenePos;
        if (ctx.mainWindow) {
            if (auto* sb = ctx.mainWindow->statusBar()) {
                sb->showMessage(QStringLiteral("Click again to finish measurement."));
            }
        }
        return Handled::Yes;
    }

    m_tool->setEndPoint(e.scenePos);
    const qreal dist = QLineF(m_startPoint, e.scenePos).length();
    qCInfo(lcGuiInputMode) << "MeasureMode: complete; scene-units =" << dist;
    if (ctx.mainWindow) {
        if (auto* sb = ctx.mainWindow->statusBar()) {
            sb->showMessage(
                QStringLiteral("Measurement complete: %1 scene units.").arg(dist, 0, 'f', 2),
                5000);
        }
    }

    // Leave tool on the scene after completion; user clicks again to start a new one.
    m_hasStart = false;
    m_tool     = nullptr;  // next press creates a new tool
    return Handled::Yes;
}

Handled MeasureMode::onMove(const MoveEvent& e, const ClickContext&)
{
    if (m_tool && m_hasStart) {
        m_tool->setEndPoint(e.scenePos);
        return Handled::Yes;
    }
    return Handled::PassThrough;
}

Handled MeasureMode::onKeyPress(const KeyPressEvent& e, const ClickContext& ctx)
{
    if (e.key == Qt::Key_Escape) {
        qCDebug(lcGuiInputMode) << "MeasureMode: Escape — exiting";
        ctx.controller->setMode<NormalMode>();
        return Handled::Yes;
    }
    return Handled::PassThrough;
}

} // namespace CargoNetSim::GUI::Input
