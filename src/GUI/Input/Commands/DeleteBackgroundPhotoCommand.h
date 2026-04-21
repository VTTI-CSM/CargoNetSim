#pragma once

#include "CommandId.h"

#include <QMap>
#include <QPixmap>
#include <QPointF>
#include <QPointer>
#include <QString>
#include <QUndoCommand>
#include <QVariant>

namespace CargoNetSim {
namespace GUI {

class BackgroundPhotoItem;
class GraphicsScene;

namespace Input {

/// Removes a BackgroundPhotoItem from the scene and from the RegionDataController
/// backing variable ("backgroundPhotoItem" for a region scene,
/// "globalBackgroundPhotoItem" for the global-map scene). Undo reconstructs the
/// item from a state snapshot and re-publishes the controller variable, so the
/// delete-key flow is undoable on parity with terminal/connection deletion.
///
/// This command does NOT restore ad-hoc signal/slot wiring the creator attached
/// to the original item (e.g., PropertiesPanel position mirroring). Such wiring
/// is a creator concern and will be restored once the BackgroundPhotoItem
/// creation path is centralized behind a factory.
class DeleteBackgroundPhotoCommand final : public QUndoCommand {
public:
    DeleteBackgroundPhotoCommand(BackgroundPhotoItem* item,
                                 QUndoCommand*        parent = nullptr);

    void redo() override;
    void undo() override;
    int  id() const override { return CommandId::DeleteBackgroundPhoto; }

private:
    QPointer<GraphicsScene>           m_scene;
    QPointer<BackgroundPhotoItem>     m_liveItem;       ///< Tracks the currently-in-scene item across redo/undo cycles.
    QString                           m_region;         ///< "global" indicates the global-map scene / global variable.
    QPixmap                           m_pixmap;
    QPointF                           m_pos;
    qreal                             m_scale    = 1.0;
    qreal                             m_opacity  = 1.0;
    qreal                             m_zValue   = -1.0;
    QMap<QString, QVariant>           m_properties;
    bool                              m_isGlobal = false;
};

} // namespace Input
} // namespace GUI
} // namespace CargoNetSim
