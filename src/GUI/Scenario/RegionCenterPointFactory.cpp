#include "RegionCenterPointFactory.h"
#include "Backend/Commons/LogCategories.h"

#include "Backend/Controllers/CargoNetSimController.h"
#include "Backend/Controllers/RegionDataController.h"
#include "Backend/Scenario/RegionSpec.h"
#include "GUI/Items/RegionCenterPoint.h"
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

/// Properties map mirroring the legacy keys read by
/// ViewController::updateTerminalGlobalPosition (ViewController.cpp:241-258).
/// Kept so existing property-bag readers keep resolving during the
/// Plan-4 transition.
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

/// Transitional publication: expose the new point under the GUI-owned
/// legacy key so `RegionData::getVariableAs<RegionCenterPoint*>(
/// "regionCenterPoint")` continues to work. Isolated here so the call
/// site in fromRegionSpec stays readable, and so the compat layer is
/// trivial to delete once Task 21 moves observers to ScenarioDocument
/// signals. Silently no-ops when the controller / region is absent
/// (headless tests, early bring-up).
void publishToControllerLegacyKey(RegionCenterPoint *cp,
                                  const QString      &regionName)
{
    auto *rdc = CargoNetSim::CargoNetSimController::getInstance()
                    .getRegionDataController();
    if (!rdc) return;
    rdc->setRegionVariable(regionName, "regionCenterPoint",
                           QVariant::fromValue(cp));
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
    // Bind the view to its spec — Task 12 added m_regionSpec so
    // center-point views carry a non-owning pointer back to the model.
    cp->setRegionSpecModel(region);
    // sceneRegistryKey() resolves to cp->getRegionName() (== region->name)
    // now that the spec is bound. This matches the `regionRemoved` observer
    // in MainWindow, which removes by region name — the registration key
    // symmetry that was silently broken before.
    scene->addItemWithId(cp, cp->sceneRegistryKey());
    ItemEventBinder::bindRegionCenterPoint(cp, mainWindow);
    publishToControllerLegacyKey(cp, region->name);
    qCDebug(lcGuiScene) << "RegionCenterPointFactory::fromRegionSpec: created center point for"
                        << region->name;
    return cp;
}

} // namespace Scenario
} // namespace GUI
} // namespace CargoNetSim
