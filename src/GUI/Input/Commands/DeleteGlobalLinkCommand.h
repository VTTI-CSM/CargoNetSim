#pragma once

#include "../../../Backend/Commons/TransportationMode.h"
#include "../../../Backend/Scenario/GlobalLink.h"
#include "CommandId.h"

#include <QPointer>
#include <QString>
#include <QUndoCommand>

namespace CargoNetSim {
namespace Backend::Scenario { class ScenarioDocument; }
namespace GUI::Input {

/// Removes a cross-region GlobalLink. Undo restores it from a captured snapshot.
/// GlobalLink counterpart of DeleteConnectionCommand.
class DeleteGlobalLinkCommand final : public QUndoCommand {
public:
    DeleteGlobalLinkCommand(Backend::Scenario::ScenarioDocument*                doc,
                            QString                                             fromId,
                            QString                                             toId,
                            Backend::TransportationTypes::TransportationMode    mode,
                            QUndoCommand*                                       parent = nullptr);

    void redo() override;
    void undo() override;
    int  id() const override { return CommandId::DeleteGlobalLink; }

private:
    QPointer<Backend::Scenario::ScenarioDocument>    m_doc;
    QString                                          m_fromId;
    QString                                          m_toId;
    Backend::TransportationTypes::TransportationMode m_mode;
    Backend::Scenario::GlobalLink                    m_snapshot;
    bool                                             m_captured = false;
};

} // namespace GUI::Input
} // namespace CargoNetSim
