#pragma once

#include "CommandId.h"

#include <QPointF>
#include <QPointer>
#include <QString>
#include <QUndoCommand>

namespace CargoNetSim {
namespace Backend::Scenario { class ScenarioDocument; }
namespace GUI::Input {

/// Updates a region's local-origin lat/lon. QPointF convention: x=lon, y=lat.
/// Consecutive commands on the same region merge so one Ctrl+Z reverts a full
/// region-drag gesture.
class UpdateRegionLocalOriginCommand final : public QUndoCommand {
public:
    UpdateRegionLocalOriginCommand(Backend::Scenario::ScenarioDocument* doc,
                                   QString                              regionName,
                                   QPointF                              newLatLon,
                                   QUndoCommand*                        parent = nullptr);

    void redo() override;
    void undo() override;
    int  id() const override { return CommandId::UpdateRegionLocalOrigin; }
    bool mergeWith(const QUndoCommand* other) override;

private:
    QPointer<Backend::Scenario::ScenarioDocument> m_doc;
    QString                                       m_regionName;
    QPointF                                       m_newLatLon;
    QPointF                                       m_oldLatLon;
    bool                                          m_captured = false;
};

} // namespace GUI::Input
} // namespace CargoNetSim
