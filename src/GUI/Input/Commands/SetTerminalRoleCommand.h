#pragma once

#include "../../../Backend/Scenario/TerminalPlacement.h"
#include "CommandId.h"

#include <QPointer>
#include <QString>
#include <QUndoCommand>

namespace CargoNetSim {
namespace Backend::Scenario { class ScenarioDocument; }
namespace GUI::Input {

/// Sets a terminal's role (Origin / Destination / Transit). Undo restores the
/// complete pre-edit placement because role transitions may intentionally
/// mutate origin properties and destination splits.
class SetTerminalRoleCommand final : public QUndoCommand {
public:
    using TerminalRole = Backend::Scenario::TerminalPlacement::TerminalRole;

    SetTerminalRoleCommand(Backend::Scenario::ScenarioDocument* doc,
                           QString                              terminalId,
                           TerminalRole                         newRole,
                           QUndoCommand*                        parent = nullptr);

    void redo() override;
    void undo() override;
    int  id() const override { return CommandId::SetTerminalRole; }

private:
    QPointer<Backend::Scenario::ScenarioDocument> m_doc;
    QString                                       m_terminalId;
    TerminalRole                                  m_newRole;
    Backend::Scenario::TerminalPlacement          m_before;
    Backend::Scenario::TerminalPlacement          m_after;
    bool                                          m_captured = false;
};

} // namespace GUI::Input
} // namespace CargoNetSim
