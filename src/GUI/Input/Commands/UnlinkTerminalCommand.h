#pragma once

#include "../../../Backend/Scenario/NodeLinkage.h"
#include "CommandId.h"

#include <QPointer>
#include <QString>
#include <QUndoCommand>

namespace CargoNetSim {
namespace Backend::Scenario { class ScenarioDocument; }
namespace GUI::Input {

/// Removes a NodeLinkage identified by (terminalId, networkName, nodeId).
/// Undo restores the captured snapshot (preserving source + excluded flags).
class UnlinkTerminalCommand final : public QUndoCommand {
public:
    UnlinkTerminalCommand(Backend::Scenario::ScenarioDocument* doc,
                          QString                              terminalId,
                          QString                              networkName,
                          int                                  nodeId,
                          QUndoCommand*                        parent = nullptr);

    void redo() override;
    void undo() override;
    int  id() const override { return CommandId::UnlinkTerminal; }

private:
    QPointer<Backend::Scenario::ScenarioDocument> m_doc;
    QString                                       m_terminalId;
    QString                                       m_networkName;
    int                                           m_nodeId;
    Backend::Scenario::NodeLinkage                m_snapshot;
    bool                                          m_captured = false;
};

} // namespace GUI::Input
} // namespace CargoNetSim
