#pragma once

#include "../../../Backend/Scenario/LinkageSource.h"
#include "CommandId.h"

#include <QPointer>
#include <QString>
#include <QUndoCommand>

namespace CargoNetSim {
namespace Backend::Scenario { class ScenarioDocument; }
namespace GUI::Input {

/// Links a terminal to a specific (networkName, nodeId) via a NodeLinkage.
/// Undo removes the linkage. @p source is stored so undo/redo uses the same
/// provenance tag (User-initiated link defaults to LinkageSource::Manual).
class LinkTerminalToMapPointCommand final : public QUndoCommand {
public:
    LinkTerminalToMapPointCommand(Backend::Scenario::ScenarioDocument* doc,
                                  QString                              terminalId,
                                  QString                              networkName,
                                  int                                  nodeId,
                                  Backend::Scenario::LinkageSource     source,
                                  QUndoCommand*                        parent = nullptr);

    void redo() override;
    void undo() override;
    int  id() const override { return CommandId::LinkTerminalToMapPoint; }

private:
    QPointer<Backend::Scenario::ScenarioDocument> m_doc;
    QString                                       m_terminalId;
    QString                                       m_networkName;
    int                                           m_nodeId;
    Backend::Scenario::LinkageSource              m_source;
    bool                                          m_wasCreated = false;
};

} // namespace GUI::Input
} // namespace CargoNetSim
