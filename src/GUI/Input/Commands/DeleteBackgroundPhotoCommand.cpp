#include "DeleteBackgroundPhotoCommand.h"

#include "../../../Backend/Commons/LogCategories.h"
#include "../../../Backend/Controllers/CargoNetSimController.h"
#include "../../../Backend/Controllers/RegionDataController.h"
#include "../../Items/BackgroundPhotoItem.h"
#include "../../Widgets/GraphicsScene.h"

#include <QLoggingCategory>
#include <QObject>

namespace CargoNetSim::GUI::Input {

namespace {

constexpr const char* kGlobalRegion      = "global";
constexpr const char* kRegionVariableKey = "backgroundPhotoItem";
constexpr const char* kGlobalVariableKey = "globalBackgroundPhotoItem";

void publishToController(BackgroundPhotoItem* item,
                         const QString&       region,
                         bool                 isGlobal)
{
    auto* rdc = CargoNetSim::CargoNetSimController::getInstance()
                    .getRegionDataController();
    if (!rdc) return;
    if (isGlobal) {
        rdc->setGlobalVariable(
            QString::fromLatin1(kGlobalVariableKey),
            QVariant::fromValue(item));
    } else {
        rdc->setRegionVariable(
            region,
            QString::fromLatin1(kRegionVariableKey),
            QVariant::fromValue(item));
    }
}

void unpublishFromController(const QString& region, bool isGlobal)
{
    auto* rdc = CargoNetSim::CargoNetSimController::getInstance()
                    .getRegionDataController();
    if (!rdc) return;
    if (isGlobal) {
        rdc->removeGlobalVariable(QString::fromLatin1(kGlobalVariableKey));
    } else {
        if (auto* data = rdc->getRegionData(region))
            data->removeVariable(QString::fromLatin1(kRegionVariableKey));
    }
}

} // namespace

DeleteBackgroundPhotoCommand::DeleteBackgroundPhotoCommand(
    BackgroundPhotoItem* item, QUndoCommand* parent)
    : QUndoCommand(parent)
    , m_liveItem(item)
{
    if (!item) {
        setObsolete(true);
        return;
    }

    // Capture scene and state up front — item->scene() goes null after redo()
    // deletes the item, and the snapshot must survive to drive undo().
    m_scene      = qobject_cast<GraphicsScene*>(item->scene());
    m_region     = item->getRegion();
    m_isGlobal   = (m_region == QString::fromLatin1(kGlobalRegion));
    m_pixmap     = item->pixmap();
    m_pos        = item->pos();
    m_scale      = static_cast<qreal>(item->getScale());
    m_opacity    = item->opacity();
    m_zValue     = item->zValue();
    m_properties = item->getProperties();

    setText(QObject::tr("Delete Background Image"));
}

void DeleteBackgroundPhotoCommand::redo()
{
    qCInfo(lcGuiInputCmd)
        << "DeleteBackgroundPhotoCommand::redo region=" << m_region
        << "isGlobal=" << m_isGlobal;

    if (!m_scene || !m_liveItem) {
        qCWarning(lcGuiInputCmd)
            << "DeleteBackgroundPhotoCommand::redo: missing scene or item;"
            << "marking obsolete";
        setObsolete(true);
        return;
    }

    const QString key = m_liveItem->sceneRegistryKey();
    unpublishFromController(m_region, m_isGlobal);
    m_scene->removeItemWithId<BackgroundPhotoItem>(key);
    m_liveItem.clear(); // removeItemWithId deletes the QGraphicsItem.
}

void DeleteBackgroundPhotoCommand::undo()
{
    qCInfo(lcGuiInputCmd)
        << "DeleteBackgroundPhotoCommand::undo region=" << m_region;

    if (!m_scene) {
        qCWarning(lcGuiInputCmd)
            << "DeleteBackgroundPhotoCommand::undo: scene gone;"
            << "nothing to restore";
        return;
    }

    auto* restored = new BackgroundPhotoItem(m_pixmap, m_region);
    restored->updateProperties(m_properties);
    restored->setPos(m_pos);
    restored->setScale(static_cast<float>(m_scale));
    restored->setOpacity(m_opacity);
    restored->setZValue(m_zValue);

    m_scene->addItemWithId(restored, restored->sceneRegistryKey());
    publishToController(restored, m_region, m_isGlobal);
    m_liveItem = restored;
}

} // namespace CargoNetSim::GUI::Input
