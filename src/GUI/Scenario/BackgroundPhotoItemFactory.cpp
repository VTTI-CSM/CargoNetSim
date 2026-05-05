#include "BackgroundPhotoItemFactory.h"

#include "Backend/Application/NetworkViewService.h"
#include "Backend/Commons/LogCategories.h"
#include "GUI/Items/BackgroundPhotoItem.h"
#include "GUI/Scenario/ItemEventBinder.h"
#include "GUI/Widgets/GraphicsScene.h"

#include <QLoggingCategory>

namespace CargoNetSim {
namespace GUI {
namespace Scenario {

namespace {

constexpr const char *kGlobalRegion      = "global";
constexpr const char *kRegionVariableKey = "backgroundPhotoItem";
constexpr const char *kGlobalVariableKey = "globalBackgroundPhotoItem";

QMap<QString, QVariant>
defaultProperties(const QString &region)
{
    return {
        {QStringLiteral("Type"),
         QStringLiteral("Background - %1").arg(region)},
        {QStringLiteral("Region"),    region},
        {QStringLiteral("Scale"),     1.0},
        {QStringLiteral("Latitude"),  0.0},
        {QStringLiteral("Longitude"), 0.0},
        {QStringLiteral("Locked"),    false},
        {QStringLiteral("Opacity"),   1.0},
    };
}

} // namespace

bool BackgroundPhotoItemFactory::isGlobalRegion(const QString &region)
{
    return region == QString::fromLatin1(kGlobalRegion);
}

void BackgroundPhotoItemFactory::publishToController(
    BackgroundPhotoItem *item, const QString &region, bool isGlobal)
{
    Backend::Application::NetworkViewService networkView;
    if (isGlobal) {
        networkView.setGlobalVariable(
            QString::fromLatin1(kGlobalVariableKey),
            QVariant::fromValue(item));
    } else {
        networkView.setRegionVariable(
            region,
            QString::fromLatin1(kRegionVariableKey),
            QVariant::fromValue(item));
    }
}

void BackgroundPhotoItemFactory::unpublishFromController(
    const QString &region, bool isGlobal)
{
    Backend::Application::NetworkViewService networkView;
    if (isGlobal) {
        networkView.removeGlobalVariable(
            QString::fromLatin1(kGlobalVariableKey));
    } else {
        networkView.removeRegionVariable(
            region, QString::fromLatin1(kRegionVariableKey));
    }
}

BackgroundPhotoItem *BackgroundPhotoItemFactory::create(
    const BackgroundPhotoSpec &spec, GraphicsScene *scene, MainWindow *mw)
{
    qCDebug(lcGuiScene)
        << "BackgroundPhotoItemFactory::create region=" << spec.region
        << "pos=" << spec.scenePos
        << "scale=" << spec.scale
        << "opacity=" << spec.opacity;

    if (!scene) {
        qCWarning(lcGuiScene)
            << "BackgroundPhotoItemFactory::create: null scene";
        return nullptr;
    }

    auto *item = new BackgroundPhotoItem(spec.pixmap, spec.region);
    item->updateProperties(spec.properties.isEmpty()
                               ? defaultProperties(spec.region)
                               : spec.properties);
    item->setPos(spec.scenePos);
    item->setScale(static_cast<float>(spec.scale));
    item->setOpacity(spec.opacity);
    item->setZValue(spec.zValue);

    scene->addItemWithId(item, item->sceneRegistryKey());
    ItemEventBinder::bindBackgroundPhotoItem(item, mw);

    const bool isGlobal = isGlobalRegion(spec.region);
    publishToController(item, spec.region, isGlobal);

    return item;
}

} // namespace Scenario
} // namespace GUI
} // namespace CargoNetSim
