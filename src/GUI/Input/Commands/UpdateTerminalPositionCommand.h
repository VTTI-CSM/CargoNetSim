#pragma once

#include "CommandId.h"

#include <QPointF>
#include <QPointer>
#include <QString>
#include <QUndoCommand>

namespace CargoNetSim {
namespace Backend::Scenario { class ScenarioDocument; }
namespace GUI::Input {

/// Updates a terminal's local (region-space) lat/lon. @p newLocalLatLon is a
/// QPointF using Qt convention: x = longitude, y = latitude. Consecutive
/// commands targeting the same terminal merge into a single undo entry so one
/// Ctrl+Z reverts a full drag gesture.
class UpdateTerminalPositionCommand final : public QUndoCommand {
public:
    UpdateTerminalPositionCommand(Backend::Scenario::ScenarioDocument* doc,
                                  QString                              terminalId,
                                  QPointF                              newLocalLatLon,
                                  QUndoCommand*                        parent = nullptr);

    void redo() override;
    void undo() override;
    int  id() const override { return CommandId::UpdateTerminalPosition; }
    bool mergeWith(const QUndoCommand* other) override;

private:
    QPointer<Backend::Scenario::ScenarioDocument> m_doc;
    QString                                       m_terminalId;
    QPointF                                       m_newLatLon;
    QPointF                                       m_oldLatLon;
    bool                                          m_captured = false;
};

} // namespace GUI::Input
} // namespace CargoNetSim
