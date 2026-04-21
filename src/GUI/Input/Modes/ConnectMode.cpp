#include "ConnectMode.h"

#include "../../../Backend/Commons/LogCategories.h"
#include "../../Controllers/SceneVisibilityController.h"
#include "../../Items/GlobalTerminalItem.h"
#include "../../Items/GraphicsObjectBase.h"
#include "../../Items/TerminalItem.h"
#include "../../MainWindow.h"
#include "../ClickContext.h"
#include "../Commands/CommandBus.h"
#include "../InteractionController.h"
#include "../Modes/NormalMode.h"

#include <QApplication>
#include <QGraphicsItem>
#include <QLoggingCategory>
#include <QStatusBar>

namespace CargoNetSim::GUI::Input {

ConnectMode::ConnectMode(Mode type)
    : m_connectionType(type)
{
    qCDebug(lcGuiInputMode) << "ConnectMode ctor; type =" << static_cast<int>(type);
}

ConnectMode::~ConnectMode() = default;

void ConnectMode::onEnter(const ClickContext& ctx)
{
    qCInfo(lcGuiInputMode) << "ConnectMode::onEnter";
    if (ctx.mainWindow) {
        if (auto* sb = ctx.mainWindow->statusBar()) {
            sb->showMessage(QStringLiteral("Click two terminals to create a connection."));
        }
    }
}

void ConnectMode::onExit(const ClickContext& ctx)
{
    qCInfo(lcGuiInputMode) << "ConnectMode::onExit";
    m_firstItem = nullptr;
    if (ctx.mainWindow) {
        if (auto* sb = ctx.mainWindow->statusBar()) {
            sb->clearMessage();
        }
    }
}

Handled ConnectMode::onPress(const PressEvent& e, const ClickContext& ctx)
{
    if (e.button != Qt::LeftButton) return Handled::PassThrough;

    QGraphicsObject* candidate = nullptr;
    for (QGraphicsItem* item : ctx.itemsUnderCursor) {
        if (auto* t = dynamic_cast<TerminalItem*>(item))       { candidate = t; break; }
        if (auto* g = dynamic_cast<GlobalTerminalItem*>(item)) { candidate = g; break; }
    }
    if (!candidate) {
        qCDebug(lcGuiInputMode) << "ConnectMode: click missed terminal";
        return Handled::Reject;
    }

    if (!m_firstItem) {
        m_firstItem = candidate;
        qCInfo(lcGuiInputMode) << "ConnectMode: first item =" << candidate;
        if (ctx.mainWindow) {
            if (auto* sb = ctx.mainWindow->statusBar()) {
                sb->showMessage(QStringLiteral("Click the second terminal."));
            }
        }
        return Handled::Yes;
    }

    if (m_firstItem.data() == candidate) {
        qCDebug(lcGuiInputMode) << "ConnectMode: cannot connect item to itself";
        QApplication::beep();
        return Handled::Reject;
    }

    if (!ctx.document || !ctx.commandBus) {
        qCWarning(lcGuiInputMode) << "ConnectMode: null document or commandBus";
        ctx.controller->setMode<NormalMode>();
        return Handled::Yes;
    }

    // Polymorphic dispatch: each endpoint type knows what kind of "connect"
    // command it produces (region Connection for TerminalItem, GlobalLink
    // for GlobalTerminalItem). Mismatched pairs or unbound endpoints yield
    // nullptr and we reject. No per-type cast ladder in the mode.
    auto* first  = qobject_cast<GraphicsObjectBase*>(m_firstItem.data());
    auto* second = dynamic_cast<GraphicsObjectBase*>(candidate);
    if (!first || !second) {
        qCWarning(lcGuiInputMode)
            << "ConnectMode: endpoints are not GraphicsObjectBase; aborting";
        ctx.controller->setMode<NormalMode>();
        return Handled::Yes;
    }

    auto cmd = first->createConnectCommandTo(second, m_connectionType,
                                             ctx.document.data());
    if (!cmd) {
        qCWarning(lcGuiInputMode)
            << "ConnectMode: endpoint types do not support a connect command"
            << "between them; aborting";
        QApplication::beep();
        return Handled::Reject;
    }

    qCInfo(lcGuiInputMode) << "ConnectMode: submitting connect command"
                           << cmd->text();
    // Surface the mutator's verdict to the user. CommandBus::submit
    // returns false when the command self-obsoleted during redo — the
    // standard signal that the domain layer rejected the operation
    // (e.g., GlobalLink attempted on a same-region pair, Connection
    // attempted on a cross-region pair). We do NOT re-implement the
    // domain check in the view: one source of truth lives in the
    // ScenarioMutator.
    const bool submitted = ctx.commandBus->submit(std::move(cmd));
    if (!submitted) {
        qCWarning(lcGuiInputMode)
            << "ConnectMode: command rejected by the domain layer";
        QApplication::beep();
        if (ctx.mainWindow) {
            if (auto* sb = ctx.mainWindow->statusBar()) {
                sb->showMessage(
                    QStringLiteral("Cannot connect these terminals — "
                                   "the scenario rejected the operation."),
                    3000);
            }
        }
        // Reset chain — the first pick stands so the user can retry
        // with a different second endpoint without re-picking the first.
        return Handled::Reject;
    }

    if (ctx.mainWindow && ctx.mainWindow->sceneVisibility()) {
        ctx.mainWindow->sceneVisibility()->updateSceneVisibility();
    }

    // Chain: make second item the new first, so user can click a third to chain.
    m_firstItem = candidate;
    if (ctx.mainWindow) {
        if (auto* sb = ctx.mainWindow->statusBar()) {
            sb->showMessage(QStringLiteral("Connection created. Click next terminal to chain, or Esc."));
        }
    }
    return Handled::Yes;
}

Handled ConnectMode::onKeyPress(const KeyPressEvent& e, const ClickContext& ctx)
{
    if (e.key == Qt::Key_Escape) {
        qCDebug(lcGuiInputMode) << "ConnectMode: Escape — exiting";
        ctx.controller->setMode<NormalMode>();
        return Handled::Yes;
    }
    return Handled::PassThrough;
}

} // namespace CargoNetSim::GUI::Input
