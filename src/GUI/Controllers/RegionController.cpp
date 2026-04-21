#include "RegionController.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Scenario/RegionSpec.h"
#include "Backend/Scenario/ScenarioDocument.h"
#include "Backend/Scenario/ScenarioRuntime.h"
#include "GUI/MainWindow.h"
#include "GUI/Controllers/SceneVisibilityController.h"
#include "GUI/Items/BackgroundPhotoItem.h"
#include "GUI/Items/ConnectionLine.h"
#include "GUI/Items/MapLine.h"
#include "GUI/Items/MapPoint.h"
#include "GUI/Items/RegionCenterPoint.h"
#include "GUI/Items/TerminalItem.h"
#include "GUI/Scenario/ItemEventBinder.h"
#include "GUI/Scenario/ScenarioMutator.h"
#include "GUI/Widgets/GraphicsScene.h"

namespace CargoNetSim
{
namespace GUI
{

RegionController::RegionController(
    GraphicsScene             *regionScene,
    SceneVisibilityController *sceneVisibility,
    MainWindow                *mainWindow,
    StatusReporter            *status,
    QObject                   *parent)
    : QObject(parent)
    , m_regionScene(regionScene)
    , m_sceneVisibility(sceneVisibility)
    , m_mainWindow(mainWindow)
    , m_status(status)
{
    qCDebug(lcGuiView) << "RegionController: created";
}

RegionCenterPoint *
RegionController::createRegionCenter(
    const QString &regionName,
    const QColor  &color,
    const QPointF  pos,
    const bool     keepVisible)
{
    qCDebug(lcGuiView) << "RegionController::createRegionCenter:"
                       << "region=" << regionName;
    if (!m_regionScene)
    {
        qCDebug(lcGuiView) << "RegionController::createRegionCenter:"
                           << "null regionScene, returning nullptr";
        return nullptr;
    }

    RegionCenterPoint *centerPoint =
        new RegionCenterPoint(regionName, color);
    centerPoint->setPos(pos);

    m_regionScene->addItemWithId(
        centerPoint, centerPoint->sceneRegistryKey());

    Scenario::ItemEventBinder::bindRegionCenterPoint(centerPoint,
                                                     m_mainWindow);

    CargoNetSim::CargoNetSimController::getInstance()
        .getRegionDataController()
        ->setRegionVariable(
            regionName, "regionCenterPoint",
            QVariant::fromValue(centerPoint));

    centerPoint->setVisible(keepVisible);
    return centerPoint;
}

void RegionController::renameRegion(
    const QString &oldRegionName,
    const QString &newName)
{
    qCDebug(lcGuiView) << "RegionController::renameRegion:"
                       << oldRegionName << "->" << newName;
    if (!m_regionScene)
    {
        qCDebug(lcGuiView) << "RegionController::renameRegion:"
                           << "null regionScene, returning";
        return;
    }

    // Rename GUI items FIRST (before backend) to avoid
    // scene mutation during iteration. The backend
    // doc.renameRegion emits terminalChanged signals which
    // trigger observers that call setPlacement → update on
    // scene items. If we iterate the scene AFTER those
    // signals fire, the iterator is invalidated.
    qCDebug(lcGuiView) << "RegionController::renameRegion:"
                       << "step 1: renaming GUI items";
    int itemCount = 0;
    // Copy the items list — iterating a live scene while
    // signals modify it causes undefined behavior.
    const auto sceneItems = m_regionScene->items();
    for (QGraphicsItem *item : sceneItems)
    {
        if (MapPoint *mapPoint =
                dynamic_cast<MapPoint *>(item))
        {
            if (mapPoint->getRegion() == oldRegionName)
            {
                mapPoint->setRegion(newName);
                ++itemCount;
            }
        }
        else if (MapLine *mapLine =
                     dynamic_cast<MapLine *>(item))
        {
            if (mapLine->getRegion() == oldRegionName)
            {
                mapLine->setRegion(newName);
                ++itemCount;
            }
        }
        else if (RegionCenterPoint *regionCenter =
                     dynamic_cast<RegionCenterPoint *>(item))
        {
            if (regionCenter->getRegion() == oldRegionName)
            {
                regionCenter->setRegion(newName);
                ++itemCount;
            }
        }
        else if (TerminalItem *terminal =
                     dynamic_cast<TerminalItem *>(item))
        {
            if (terminal->getRegion() == oldRegionName)
            {
                terminal->setRegion(newName);
                ++itemCount;
            }
        }
        else if (ConnectionLine *connectionLine =
                     dynamic_cast<ConnectionLine *>(item))
        {
            if (connectionLine->getRegion() == oldRegionName)
            {
                connectionLine->setRegion(newName);
                ++itemCount;
            }
        }
        else if (BackgroundPhotoItem *backgroundPhoto =
                     dynamic_cast<BackgroundPhotoItem *>(item))
        {
            if (backgroundPhoto->getRegion() == oldRegionName)
            {
                backgroundPhoto->setRegion(newName);
                ++itemCount;
            }
        }
    }
    qCDebug(lcGuiView) << "RegionController::renameRegion:"
                       << "step 1 done, items processed:"
                       << itemCount;

    // Step 2: sync the backend ScenarioDocument AFTER GUI
    // items are renamed. doc.renameRegion emits
    // terminalChanged signals — but items already have the
    // new region name, so the observer's setPlacement refresh
    // is harmless (same data).
    qCDebug(lcGuiView) << "RegionController::renameRegion:"
                       << "step 2: syncing ScenarioDocument";
    if (m_mainWindow && m_mainWindow->runtime())
    {
        auto &doc = m_mainWindow->runtime()->document();
        if (!doc.renameRegion(oldRegionName, newName))
        {
            qCWarning(lcGuiView)
                << "RegionController::renameRegion:"
                << "ScenarioDocument::renameRegion failed"
                << "for" << oldRegionName;
        }
        else
        {
            qCDebug(lcGuiView)
                << "RegionController::renameRegion:"
                << "step 2 done, document synced";
        }
    }

    qCDebug(lcGuiView) << "RegionController::renameRegion:"
                       << "step 3: updating visibility";
    m_sceneVisibility->updateSceneVisibility();
    qCDebug(lcGuiView) << "RegionController::renameRegion:"
                       << "complete";
}

void RegionController::addRegion(const QString &name,
                                 const QColor  &color,
                                 const QPointF &pos)
{
    qCDebug(lcGuiView) << "RegionController::addRegion:" << name;

    // Step 1: RDC — createRegionCenter calls setRegionVariable on this region,
    // so the region must exist in RDC before createRegionCenter is called.
    auto *rdc = CargoNetSim::CargoNetSimController::getInstance()
                    .getRegionDataController();
    rdc->addRegion(name);
    qCInfo(lcGuiView)
        << "RegionController::addRegion: setRegionVariable color"
        << "region=" << name << "color=" << color.name()
        << "as QVariant(QColor)";
    rdc->setRegionVariable(name, "color", color);

    // Step 2: scene item
    createRegionCenter(name, color, pos, /*keepVisible=*/false);

    // Step 3: doc
    if (m_mainWindow && m_mainWindow->runtime())
    {
        CargoNetSim::Backend::Scenario::RegionSpec spec;
        spec.name  = name;
        spec.color = color.name();
        if (!GUI::Scenario::ScenarioMutator::addRegion(
                &m_mainWindow->runtime()->document(), spec))
            qCWarning(lcGuiView)
                << "RegionController::addRegion:"
                << "ScenarioMutator::addRegion failed for" << name;
    }
}

void RegionController::removeRegion(const QString &name)
{
    qCDebug(lcGuiView) << "RegionController::removeRegion:" << name;
    if (!m_regionScene) return;

    // Step 1: scene items — copy list first; remove before doc signals fire
    // to avoid iterator invalidation (same pattern as renameRegion).
    const auto sceneItems = m_regionScene->items();
    int removedCount = 0;
    for (QGraphicsItem *item : sceneItems)
    {
        bool owned = false;
        if (auto *p = dynamic_cast<MapPoint *>(item))
            owned = (p->getRegion() == name);
        else if (auto *l = dynamic_cast<MapLine *>(item))
            owned = (l->getRegion() == name);
        else if (auto *rc = dynamic_cast<RegionCenterPoint *>(item))
            owned = (rc->getRegion() == name);
        else if (auto *t = dynamic_cast<TerminalItem *>(item))
            owned = (t->getRegion() == name);
        else if (auto *cl = dynamic_cast<ConnectionLine *>(item))
            owned = (cl->getRegion() == name);
        else if (auto *bp = dynamic_cast<BackgroundPhotoItem *>(item))
            owned = (bp->getRegion() == name);

        if (owned)
        {
            m_regionScene->removeItem(item);
            delete item;
            ++removedCount;
        }
    }
    qCDebug(lcGuiView) << "RegionController::removeRegion:"
                       << "scene items removed:" << removedCount;

    // Step 2: RDC
    CargoNetSim::CargoNetSimController::getInstance()
        .getRegionDataController()->removeRegion(name);

    // Step 3: doc (cascades terminal/connection/global-link removal)
    if (m_mainWindow && m_mainWindow->runtime())
    {
        if (!GUI::Scenario::ScenarioMutator::removeRegion(
                &m_mainWindow->runtime()->document(), name))
            qCWarning(lcGuiView)
                << "RegionController::removeRegion:"
                << "ScenarioMutator::removeRegion failed for" << name;
    }

    if (m_sceneVisibility)
        m_sceneVisibility->updateSceneVisibility();
}

void RegionController::updateRegionColor(const QString &name,
                                         const QColor  &color)
{
    qCDebug(lcGuiView) << "RegionController::updateRegionColor:" << name;

    // Step 1: scene item (RegionCenterPoint)
    if (m_regionScene)
    {
        const auto sceneItems = m_regionScene->items();
        for (QGraphicsItem *item : sceneItems)
        {
            if (auto *rc = dynamic_cast<RegionCenterPoint *>(item))
            {
                if (rc->getRegion() == name)
                {
                    rc->setColor(color);
                    rc->update();
                    break;
                }
            }
        }
    }

    // Step 2: RDC
    auto *rdc = CargoNetSim::CargoNetSimController::getInstance()
                    .getRegionDataController();
    if (auto *data = rdc->getRegionData(name))
        data->setVariable("color", color);

    // Step 3: doc
    if (m_mainWindow && m_mainWindow->runtime())
    {
        if (!GUI::Scenario::ScenarioMutator::updateRegionColor(
                &m_mainWindow->runtime()->document(), name, color.name()))
            qCWarning(lcGuiView)
                << "RegionController::updateRegionColor:"
                << "ScenarioMutator::updateRegionColor failed for" << name;
    }
}

void RegionController::clearRegions()
{
    qCDebug(lcGuiView) << "RegionController::clearRegions";
    // Snapshot names before iterating — each removeRegion call mutates RDC.
    const QStringList all =
        CargoNetSim::CargoNetSimController::getInstance()
            .getRegionDataController()
            ->getAllRegionNames();
    for (const QString &name : all)
    {
        if (name != QStringLiteral("Default Region"))
            removeRegion(name);
    }
}

} // namespace GUI
} // namespace CargoNetSim
