#include "ConnectMode.h"

#include "../../../Backend/Commons/LogCategories.h"
#include "../../Controllers/SceneVisibilityController.h"
#include "../../Items/GlobalTerminalItem.h"
#include "../../Items/TerminalItem.h"
#include "../../MainWindow.h"
#include "../ClickContext.h"
#include "../Commands/CommandBus.h"
#include "../Commands/CreateConnectionCommand.h"
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

    // Resolve source + target terminal ids; GlobalTerminalItem proxies to
    // its linked TerminalItem so cross-region global-link clicks work too.
    auto extractId = [](QGraphicsObject* obj) -> QString {
        if (auto* t = qobject_cast<TerminalItem*>(obj))       return t->getTerminalId();
        if (auto* g = qobject_cast<GlobalTerminalItem*>(obj)) {
            if (auto* t = g->getLinkedTerminalItem()) return t->getTerminalId();
        }
        return {};
    };
    const QString srcId = extractId(m_firstItem.data());
    const QString tgtId = extractId(candidate);
    if (srcId.isEmpty() || tgtId.isEmpty()) {
        qCWarning(lcGuiInputMode)
            << "ConnectMode: could not resolve terminal ids; aborting";
        ctx.controller->setMode<NormalMode>();
        return Handled::Yes;
    }

    ctx.commandBus->submit(std::make_unique<CreateConnectionCommand>(
        ctx.document.data(), srcId, tgtId, m_connectionType));
    qCInfo(lcGuiInputMode) << "ConnectMode: submitted CreateConnectionCommand"
                           << srcId << "->" << tgtId;

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
