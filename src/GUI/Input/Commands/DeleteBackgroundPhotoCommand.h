#pragma once

#include "../../Scenario/BackgroundPhotoItemFactory.h"
#include "CommandId.h"

#include <QPointer>
#include <QString>
#include <QUndoCommand>

namespace CargoNetSim {
namespace GUI {

class BackgroundPhotoItem;
class GraphicsScene;
class MainWindow;

namespace Input {

/// Removes a BackgroundPhotoItem from its scene and unpublishes the matching
/// RegionDataController variable. Undo rebuilds the item via
/// BackgroundPhotoItemFactory so the reconstruction path — signal wiring,
/// controller publication, scene registration — is identical to fresh import.
class DeleteBackgroundPhotoCommand final : public QUndoCommand {
public:
    DeleteBackgroundPhotoCommand(BackgroundPhotoItem *item,
                                 QUndoCommand        *parent = nullptr);

    void redo() override;
    void undo() override;
    int  id() const override { return CommandId::DeleteBackgroundPhoto; }

private:
    QPointer<GraphicsScene>           m_scene;
    QPointer<MainWindow>              m_mainWindow;
    QPointer<BackgroundPhotoItem>     m_liveItem;
    Scenario::BackgroundPhotoSpec     m_spec;
    bool                              m_isGlobal = false;
};

} // namespace Input
} // namespace GUI
} // namespace CargoNetSim
