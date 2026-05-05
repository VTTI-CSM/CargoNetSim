#pragma once

#include "CommandId.h"

#include <QMap>
#include <QPointF>
#include <QPointer>
#include <QString>
#include <QUndoCommand>
#include <QVariant>

namespace CargoNetSim {
namespace GUI {
class BackgroundPhotoItem;
namespace Input {

/// Bundled "Apply" from the PropertiesPanel for a BackgroundPhotoItem.
/// Captures the full observable state both before and after — redo applies
/// the new state, undo restores the old — so one panel Apply collapses to a
/// single undo entry.
class ApplyBackgroundPhotoEditCommand final : public QUndoCommand {
public:
    struct State {
        QPointF                 scenePos;
        qreal                   scale    = 1.0;
        qreal                   opacity  = 1.0;
        qreal                   zValue   = -1.0;
        QMap<QString, QVariant> properties;
    };

    ApplyBackgroundPhotoEditCommand(BackgroundPhotoItem *item,
                                    State                before,
                                    State                after,
                                    QUndoCommand        *parent = nullptr);

    void redo() override;
    void undo() override;
    int  id() const override { return CommandId::ApplyBackgroundPhotoEdit; }

private:
    void applyState(const State &state);

    QPointer<BackgroundPhotoItem> m_item;
    State                         m_before;
    State                         m_after;
};

} // namespace Input
} // namespace GUI
} // namespace CargoNetSim
