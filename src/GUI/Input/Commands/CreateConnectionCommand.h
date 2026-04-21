#pragma once

#include "../../../Backend/Commons/TransportationMode.h"
#include "CommandId.h"

#include <QPointer>
#include <QString>
#include <QUndoCommand>

namespace CargoNetSim {
namespace Backend::Scenario { class ScenarioDocument; }
namespace GUI::Input {

/// Creates a region-local Connection between two terminals. Undo removes it.
class CreateConnectionCommand final : public QUndoCommand {
public:
    CreateConnectionCommand(Backend::Scenario::ScenarioDocument*                doc,
                            QString                                             fromTerminalId,
                            QString                                             toTerminalId,
                            Backend::TransportationTypes::TransportationMode    mode,
                            QUndoCommand*                                       parent = nullptr);

    void redo() override;
    void undo() override;
    int  id() const override { return CommandId::CreateConnection; }

private:
    QPointer<Backend::Scenario::ScenarioDocument>    m_doc;
    QString                                          m_fromId;
    QString                                          m_toId;
    Backend::TransportationTypes::TransportationMode m_mode;
    bool                                             m_wasCreated = false;
};

} // namespace GUI::Input
} // namespace CargoNetSim
