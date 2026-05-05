#include "RegionCenterPointFactory.h"
#include "Backend/Application/NetworkViewService.h"
#include "Backend/Commons/LogCategories.h"

#include "Backend/GuiApi/ScenarioDocumentApi.h"
#include "Backend/Scenario/ScenarioRuntime.h"
#include "GUI/Items/RegionCenterPoint.h"
#include "GUI/MainWindow.h"
#include "GUI/Scenario/ItemEventBinder.h"
#include "GUI/Widgets/GraphicsScene.h"

#include <QColor>
#include <QMap>
#include <QString>
#include <QVariant>

namespace CargoNetSim {
namespace GUI {
namespace Scenario {

namespace {

/// Properties map for the visible RegionCenterPoint view. The backend
/// RegionSpec remains the source of truth; this map is a UI snapshot for
/// the properties panel and scene item.
QMap<QString, QVariant>
buildProperties(const Backend::Scenario::RegionSpec &region)
{
    QMap<QString, QVariant> props;
    props["Region"]           = region.name;
    props["Latitude"]         = region.localOrigin.latitude;
    props["Longitude"]        = region.localOrigin.longitude;
    props["Shared Latitude"]  = region.globalPosition.latitude;
    props["Shared Longitude"] = region.globalPosition.longitude;
    return props;
}

/// Publish the visible center item under the GUI-owned view key. This is
/// intentionally separate from serializable backend region state.
void publishToControllerLegacyKey(RegionCenterPoint *cp,
                                  const QString      &regionName)
{
    Backend::Application::NetworkViewService networkView;
    networkView.setRegionVariable(
        regionName, "regionCenterPoint", QVariant::fromValue(cp));
}

} // namespace

RegionCenterPoint *RegionCenterPointFactory::fromRegionSpec(
    Backend::Scenario::RegionSpec *region,
    GraphicsScene                 *scene,
    MainWindow                    *mainWindow)
{
    qCDebug(lcGuiScene) << "RegionCenterPointFactory::fromRegionSpec:"
                        << (region ? region->name : "null");
    if (!region || !scene)
    {
        qCWarning(lcGuiScene) << "RegionCenterPointFactory::fromRegionSpec:"
                              << "null region or scene";
        return nullptr;
    }

    auto *cp = new RegionCenterPoint(region->name,
                                     QColor(region->color),
                                     buildProperties(*region));
    // Bind the view to its document + region name. We deliberately do
    // NOT store a raw RegionSpec* on the view: ScenarioDocument::
    // renameRegion does regions.take(oldName) + insert(newName,...),
    // which destroys the QMap node and would strand any cached
    // spec address. Name-based lookup via `regionSpecModel()` stays
    // valid across renames. Doc pointer is tracked as QPointer inside
    // the view so runtime swaps / teardown are safe.
    Backend::Scenario::ScenarioDocument *doc =
        (mainWindow && mainWindow->runtime())
            ? &mainWindow->runtime()->document()
            : nullptr;
    cp->setRegionBinding(doc, region->name);
    // The registry key is the item's stable UUID (GraphicsObjectBase::
    // getID()) — see RegionCenterPoint's note explaining why region
    // name is NOT used as the key. Observers that have only a region
    // name in hand reach the item via `findByName()`.
    scene->addItemWithId(cp, cp->sceneRegistryKey());
    ItemEventBinder::bindRegionCenterPoint(cp, mainWindow);
    publishToControllerLegacyKey(cp, region->name);
    qCDebug(lcGuiScene) << "RegionCenterPointFactory::fromRegionSpec: created center point for"
                        << region->name;
    return cp;
}

RegionCenterPoint *RegionCenterPointFactory::findByName(
    GraphicsScene *scene, const QString &regionName)
{
    if (!scene || regionName.isEmpty()) return nullptr;
    for (RegionCenterPoint *cp :
         scene->getItemsByType<RegionCenterPoint>())
    {
        if (cp && cp->getRegionName() == regionName)
            return cp;
    }
    return nullptr;
}

} // namespace Scenario
} // namespace GUI
} // namespace CargoNetSim
