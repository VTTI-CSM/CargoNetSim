#include "DeleteBackgroundPhotoCommand.h"

#include "../../../Backend/Commons/LogCategories.h"
#include "../../Items/BackgroundPhotoItem.h"
#include "../../MainWindow.h"
#include "../../Scenario/BackgroundPhotoItemFactory.h"
#include "../../Widgets/GraphicsScene.h"
#include "../InteractionController.h"

#include <QLoggingCategory>
#include <QObject>

namespace CargoNetSim::GUI::Input {

DeleteBackgroundPhotoCommand::DeleteBackgroundPhotoCommand(
    BackgroundPhotoItem *item, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_liveItem(item)
{
    if (!item) {
        setObsolete(true);
        return;
    }

    m_scene = qobject_cast<GraphicsScene *>(item->scene());
    // MainWindow is resolved via the scene's installed InteractionController —
    // the binder is a no-op when mw is null (same contract as other factories),
    // so we fall back cleanly if the controller is absent (headless tests).
    if (auto *ctrl = item->interactionController())
        m_mainWindow = ctrl->mainWindow();

    m_isGlobal = Scenario::BackgroundPhotoItemFactory::isGlobalRegion(
        item->getRegion());

    m_spec.pixmap     = item->pixmap();
    m_spec.region     = item->getRegion();
    m_spec.scenePos   = item->pos();
    m_spec.scale      = static_cast<qreal>(item->getScale());
    m_spec.opacity    = item->opacity();
    m_spec.zValue     = item->zValue();
    m_spec.properties = item->getProperties();

    setText(QObject::tr("Delete Background Image"));
}

void DeleteBackgroundPhotoCommand::redo()
{
    qCInfo(lcGuiInputCmd)
        << "DeleteBackgroundPhotoCommand::redo region=" << m_spec.region
        << "isGlobal=" << m_isGlobal;

    if (!m_scene || !m_liveItem) {
        qCWarning(lcGuiInputCmd)
            << "DeleteBackgroundPhotoCommand::redo: missing scene or item;"
            << "marking obsolete";
        setObsolete(true);
        return;
    }

    const QString key = m_liveItem->sceneRegistryKey();
    Scenario::BackgroundPhotoItemFactory::unpublishFromController(
        m_spec.region, m_isGlobal);
    m_scene->removeItemWithId<BackgroundPhotoItem>(key);
    m_liveItem.clear();
}

void DeleteBackgroundPhotoCommand::undo()
{
    qCInfo(lcGuiInputCmd)
        << "DeleteBackgroundPhotoCommand::undo region=" << m_spec.region;

    if (!m_scene) {
        qCWarning(lcGuiInputCmd)
            << "DeleteBackgroundPhotoCommand::undo: scene gone;"
            << "nothing to restore";
        return;
    }

    m_liveItem = Scenario::BackgroundPhotoItemFactory::create(
        m_spec, m_scene.data(), m_mainWindow.data());
}

} // namespace CargoNetSim::GUI::Input
