#pragma once

#include "../../../Backend/Scenario/TerminalPlacement.h"
#include "CommandId.h"

#include <QPointF>
#include <QPointer>
#include <QString>
#include <QUndoCommand>

namespace CargoNetSim {
namespace Backend::Scenario { class ScenarioDocument; }
namespace GUI::Input {

/// Creates a new Transit terminal from a drag-and-drop gesture. Undo removes
/// the created terminal. Re-redo creates a fresh uuid id, mirroring
/// CreateTerminalAtMapPointCommand.
class CreateTerminalAtPointCommand final : public QUndoCommand {
public:
    using TerminalRole = Backend::Scenario::TerminalPlacement::TerminalRole;

    CreateTerminalAtPointCommand(Backend::Scenario::ScenarioDocument* doc,
                                 QString                              region,
                                 QString                              terminalType,
                                 QPointF                              localLatLon,
                                 TerminalRole                         role =
                                     TerminalRole::Transit,
                                 QUndoCommand*                        parent = nullptr);

    void redo() override;
    void undo() override;
    int  id() const override { return CommandId::CreateTerminalAtPoint; }

private:
    QPointer<Backend::Scenario::ScenarioDocument> m_doc;
    QString                                       m_region;
    QString                                       m_terminalType;
    QPointF                                       m_localLatLon;
    TerminalRole                                  m_role;
    QString                                       m_createdTerminalId;
    bool                                          m_wasCreated = false;
};

} // namespace GUI::Input
} // namespace CargoNetSim
