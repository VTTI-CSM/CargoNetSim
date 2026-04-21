#pragma once

#include "CommandId.h"

#include <QPointF>
#include <QPointer>
#include <QUndoCommand>

namespace CargoNetSim {
namespace GUI {
class BackgroundPhotoItem;
namespace Input {

/// Atomically persists a BackgroundPhotoItem's scene-position change together
/// with its Latitude/Longitude property fields, so redo/undo keeps the
/// property bag and the visual position consistent. Drag-start state is
/// captured by the item (scene pos + geo) and passed in explicitly — the
/// command reads no live state.
///
/// Qt convention for the geo-point: x = longitude, y = latitude.
/// Consecutive commands on the same item merge into one undo entry so one
/// Ctrl+Z reverts a full drag gesture.
class UpdateBackgroundPhotoPositionCommand final : public QUndoCommand {
public:
    UpdateBackgroundPhotoPositionCommand(BackgroundPhotoItem *item,
                                         QPointF              oldPos,
                                         QPointF              newPos,
                                         QPointF              oldGeoLonLat,
                                         QPointF              newGeoLonLat,
                                         QUndoCommand        *parent = nullptr);

    void redo() override;
    void undo() override;
    int  id() const override { return CommandId::UpdateBackgroundPhotoPosition; }
    bool mergeWith(const QUndoCommand *other) override;

private:
    void applyTo(QPointF scenePos, QPointF geoLonLat);

    QPointer<BackgroundPhotoItem> m_item;
    QPointF                       m_oldPos;
    QPointF                       m_newPos;
    QPointF                       m_oldGeo; ///< x=lon, y=lat
    QPointF                       m_newGeo;
};

} // namespace Input
} // namespace GUI
} // namespace CargoNetSim
