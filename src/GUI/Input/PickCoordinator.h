#pragma once

#include "PickSession.h"

#include <QList>
#include <QObject>
#include <QString>
#include <optional>

namespace CargoNetSim {
namespace GUI {
namespace Input {

class InteractionController;

/**
 * Single app-scope owner of the active PickSession.
 *
 * The GUI has two InteractionControllers (one per scene: region and global).
 * A pick session initiated in one scene must remain actionable when the user
 * switches to the other scene. This coordinator owns the session by value and
 * mediates both controllers so that whichever scene is currently visible is
 * the one running PickDestinationMode.
 *
 * Pick results flow through exactly one signal: destinationResolved. Any view
 * that needs to react (PropertiesPanel) subscribes here — never to a mode or
 * a controller directly.
 *
 * Ownership: MainWindow owns one instance. Both InteractionControllers hold a
 * non-owning back-pointer via InteractionController::setCoordinator().
 */
class PickCoordinator : public QObject
{
    Q_OBJECT
public:
    explicit PickCoordinator(QObject* parent = nullptr);

    /// Register a controller; coordinator does NOT take ownership.
    void registerController(InteractionController* controller);

    /// Called by MainWindow whenever the visible scene changes. If a session
    /// is live, activates PickDestinationMode on @p controller and returns
    /// any previously active controller to NormalMode.
    void setActiveController(InteractionController* controller);
    InteractionController* activeController() const { return m_active; }

    /// Begin a new pick session. Cancels any existing session first.
    void begin(PickSession session);

    /// Resolve the active session with the picked terminal. No-op if no
    /// session. Emits destinationResolved with (terminalId, terminalName)
    /// and ends the session. If @p terminalId equals the session's
    /// originTerminalId, the pick is rejected (session stays live) so the
    /// user can click a different terminal.
    void resolve(const QString& terminalId, const QString& terminalName);

    /// Cancel the active session (user pressed Esc, requester deleted, etc.).
    void cancel();

    const std::optional<PickSession>& activeSession() const { return m_session; }
    bool                              isActive()      const { return m_session.has_value(); }

signals:
    /// Fired when begin/cancel/resolve changes the session state. Views
    /// subscribe once and re-render purely from activeSession() + model.
    void sessionChanged();

    /// Fired by resolve() with the picked terminal. The session has already
    /// been cleared by the time this fires, so subscribers can read the
    /// *previous* slot via the forwarded PickSlot. Subscribers route by slot:
    /// primary → scalar write; multi-row → dispatch to editor row handler.
    void destinationResolved(const CargoNetSim::GUI::Input::PickSession& session,
                             const QString& terminalId,
                             const QString& terminalName);

private:
    /// active controller -> PickDestinationMode (iff session live);
    /// every other registered controller -> NormalMode.
    void syncControllerModes();

    // Raw pointers: coordinator lifetime is a strict subset of MainWindow's
    // lifetime, which owns all controllers. MainWindow destroys controllers
    // before the coordinator (declaration order), so by the time ~PickCoordinator
    // runs these are potentially dangling -- but the destructor touches none of
    // them (QObject::~QObject handles child QObject teardown).
    QList<InteractionController*>  m_controllers;
    InteractionController*         m_active = nullptr;
    std::optional<PickSession>     m_session;
};

} // namespace Input
} // namespace GUI
} // namespace CargoNetSim
