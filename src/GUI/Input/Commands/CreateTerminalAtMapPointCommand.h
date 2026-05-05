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

/// Composite: creates a new terminal at (region, localLatLon) then links it
/// to a specific (networkName, nodeId). Undo reverses in reverse order.
///
/// Note: re-redo after undo creates a FRESH terminal with a new uuid id.
/// m_createdTerminalId is overwritten on each redo(); undo() removes the most
/// recently created id, so repeated undo/redo remains coherent.
class CreateTerminalAtMapPointCommand final : public QUndoCommand {
public:
    using TerminalRole = Backend::Scenario::TerminalPlacement::TerminalRole;

    CreateTerminalAtMapPointCommand(Backend::Scenario::ScenarioDocument* doc,
                                    QString                              networkName,
                                    int                                  nodeId,
                                    QString                              terminalType,
                                    QString                              region,
                                    QPointF                              localLatLon,
                                    TerminalRole                         role =
                                        TerminalRole::Transit,
                                    QUndoCommand*                        parent = nullptr);

    void redo() override;
    void undo() override;
    int  id() const override { return CommandId::CreateTerminalAtMapPoint; }

private:
    QPointer<Backend::Scenario::ScenarioDocument> m_doc;
    QString                                       m_networkName;
    int                                           m_nodeId;
    QString                                       m_terminalType;
    QString                                       m_region;
    QPointF                                       m_localLatLon;
    TerminalRole                                  m_role;
    QString                                       m_createdTerminalId;
    bool                                          m_wasCreated = false;
};

} // namespace GUI::Input
} // namespace CargoNetSim
