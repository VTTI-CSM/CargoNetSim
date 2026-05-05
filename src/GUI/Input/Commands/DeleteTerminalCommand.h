#pragma once

#include "../../../Backend/Scenario/TerminalPlacement.h"
#include "CommandId.h"

#include <QPointer>
#include <QString>
#include <QUndoCommand>

namespace CargoNetSim {
namespace Backend::Scenario { class ScenarioDocument; }
namespace GUI::Input {

/// Removes a TerminalPlacement by id. Undo restores the captured snapshot.
class DeleteTerminalCommand final : public QUndoCommand {
public:
    DeleteTerminalCommand(Backend::Scenario::ScenarioDocument* doc,
                          QString                              terminalId,
                          QUndoCommand*                        parent = nullptr);

    void redo() override;
    void undo() override;
    int  id() const override { return CommandId::DeleteTerminal; }

private:
    QPointer<Backend::Scenario::ScenarioDocument> m_doc;
    QString                                       m_terminalId;
    Backend::Scenario::TerminalPlacement          m_snapshot;
    bool                                          m_captured = false;
};

} // namespace GUI::Input
} // namespace CargoNetSim
