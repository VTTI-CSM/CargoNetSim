#pragma once

#include "../../../Backend/Scenario/TerminalPlacement.h"
#include "CommandId.h"

#include <QPointer>
#include <QString>
#include <QUndoCommand>

namespace CargoNetSim {
namespace Backend::Scenario { class ScenarioDocument; }
namespace GUI::Input {

/// Sets a terminal's type (backend-defined string). Undo restores the complete
/// pre-edit placement because type transitions reset properties/interfaces.
class SetTerminalTypeCommand final : public QUndoCommand {
public:
    SetTerminalTypeCommand(Backend::Scenario::ScenarioDocument* doc,
                           QString                              terminalId,
                           QString                              newType,
                           QUndoCommand*                        parent = nullptr);

    void redo() override;
    void undo() override;
    int  id() const override { return CommandId::SetTerminalType; }

private:
    QPointer<Backend::Scenario::ScenarioDocument> m_doc;
    QString                                       m_terminalId;
    QString                                       m_newType;
    Backend::Scenario::TerminalPlacement          m_before;
    Backend::Scenario::TerminalPlacement          m_after;
    bool                                          m_captured = false;
};

} // namespace GUI::Input
} // namespace CargoNetSim
