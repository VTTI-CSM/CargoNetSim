#pragma once

#include "../../../Backend/Commons/TransportationMode.h"
#include "../../../Backend/Scenario/Connection.h"
#include "CommandId.h"

#include <QPointer>
#include <QString>
#include <QUndoCommand>

namespace CargoNetSim {
namespace Backend::Scenario { class ScenarioDocument; }
namespace GUI::Input {

/// Removes a region-local Connection. Undo restores it from a captured snapshot.
class DeleteConnectionCommand final : public QUndoCommand {
public:
    DeleteConnectionCommand(Backend::Scenario::ScenarioDocument*                doc,
                            QString                                             fromId,
                            QString                                             toId,
                            Backend::TransportationTypes::TransportationMode    mode,
                            QUndoCommand*                                       parent = nullptr);

    void redo() override;
    void undo() override;
    int  id() const override { return CommandId::DeleteConnection; }

private:
    QPointer<Backend::Scenario::ScenarioDocument>    m_doc;
    QString                                          m_fromId;
    QString                                          m_toId;
    Backend::TransportationTypes::TransportationMode m_mode;
    Backend::Scenario::Connection                    m_snapshot;
    bool                                             m_captured = false;
};

} // namespace GUI::Input
} // namespace CargoNetSim
