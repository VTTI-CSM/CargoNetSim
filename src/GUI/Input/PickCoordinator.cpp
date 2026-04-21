#include "PickCoordinator.h"

#include "InteractionController.h"
#include "Modes/NormalMode.h"
#include "Modes/PickDestinationMode.h"

#include "../../Backend/Commons/LogCategories.h"

#include <QLoggingCategory>

namespace CargoNetSim {
namespace GUI {
namespace Input {

PickCoordinator::PickCoordinator(QObject* parent)
    : QObject(parent)
{
}

void PickCoordinator::registerController(InteractionController* controller)
{
    if (!controller) return;
    m_controllers.append(controller);
    qCInfo(lcGuiInput) << "PickCoordinator: registered controller" << controller;
}

void PickCoordinator::setActiveController(InteractionController* controller)
{
    if (m_active == controller) return;
    m_active = controller;
    qCInfo(lcGuiInput) << "PickCoordinator: active controller ->" << controller
                       << "session?" << m_session.has_value();
    syncControllerModes();
}

void PickCoordinator::begin(PickSession session)
{
    if (m_session) {
        qCInfo(lcGuiInput)
            << "PickCoordinator: begin() overrides existing session for"
            << m_session->requesterId;
    }
    m_session = std::move(session);
    qCInfo(lcGuiInput)
        << "PickCoordinator: session begin requester=" << m_session->requesterId
        << "primary=" << m_session->targetsPrimary();
    syncControllerModes();
    emit sessionChanged();
}

void PickCoordinator::resolve(const QString& terminalId, const QString& terminalName)
{
    if (!m_session) {
        qCWarning(lcGuiInput)
            << "PickCoordinator: resolve() called with no active session — ignored";
        return;
    }
    if (terminalId == m_session->originTerminalId) {
        qCInfo(lcGuiInput)
            << "PickCoordinator: ignoring self-pick attempt" << terminalId;
        return;  // session stays live; user can click a different terminal
    }

    // Move session out BEFORE emitting so observers see it cleared.
    const PickSession session = *std::move(m_session);
    m_session.reset();

    qCInfo(lcGuiInput)
        << "PickCoordinator: resolved requester=" << session.requesterId
        << "terminal=" << terminalId;

    syncControllerModes();
    emit destinationResolved(session, terminalId, terminalName);
    emit sessionChanged();
}

void PickCoordinator::cancel()
{
    if (!m_session) return;
    qCInfo(lcGuiInput) << "PickCoordinator: cancel requester=" << m_session->requesterId;
    m_session.reset();
    syncControllerModes();
    emit sessionChanged();
}

void PickCoordinator::syncControllerModes()
{
    for (auto* c : m_controllers) {
        if (!c) continue;
        const bool shouldPick = m_session.has_value() && (c == m_active);
        const QString current = c->currentMode() ? c->currentMode()->name() : QString();
        if (shouldPick && current != QStringLiteral("PickDestination")) {
            c->setMode<PickDestinationMode>();
        } else if (!shouldPick && current == QStringLiteral("PickDestination")) {
            c->setMode<NormalMode>();
        }
    }
}

} // namespace Input
} // namespace GUI
} // namespace CargoNetSim
