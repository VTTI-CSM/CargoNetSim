#pragma once

#include "CommandId.h"

#include <QPointF>
#include <QPointer>
#include <QString>
#include <QUndoCommand>

namespace CargoNetSim {
namespace Backend::Scenario { class ScenarioDocument; }
namespace GUI::Input {

/// Updates a region's global position (Shared Latitude / Longitude) in the
/// ScenarioDocument. QPointF convention: x = longitude, y = latitude.
/// Consecutive commands on the same region merge so a single Ctrl+Z reverts
/// the full edit — mirrors UpdateRegionLocalOriginCommand.
class UpdateRegionGlobalPositionCommand final : public QUndoCommand {
public:
    UpdateRegionGlobalPositionCommand(
        Backend::Scenario::ScenarioDocument *doc,
        QString                              regionName,
        QPointF                              newLatLon,
        QUndoCommand                        *parent = nullptr);

    void redo() override;
    void undo() override;
    int  id() const override { return CommandId::UpdateRegionGlobalPosition; }
    bool mergeWith(const QUndoCommand *other) override;

private:
    QPointer<Backend::Scenario::ScenarioDocument> m_doc;
    QString                                       m_regionName;
    QPointF                                       m_newLatLon;
    QPointF                                       m_oldLatLon;
    bool                                          m_captured = false;
};

} // namespace GUI::Input
} // namespace CargoNetSim
