#include "ConnectionLineFactory.h"

#include "Backend/Commons/LogCategories.h"
#include "Backend/Scenario/Connection.h"
#include "Backend/Scenario/GlobalLink.h"
#include "GUI/Items/ConnectionLine.h"
#include "GUI/Items/GlobalTerminalItem.h"
#include "GUI/Items/TerminalItem.h"
#include "GUI/Scenario/ItemEventBinder.h"
#include "GUI/Widgets/GraphicsScene.h"

namespace CargoNetSim {
namespace GUI {
namespace Scenario {

namespace {

/// Tail of both factory methods: register the line under its own id and
/// wire click signals. Kept file-local so both paths share it and there
/// is no duplication of the "add + bind" pattern.
void addAndBindLine(ConnectionLine *line, GraphicsScene *scene,
                    MainWindow *mainWindow)
{
    qCDebug(lcGuiScene)
        << "ConnectionLineFactory::addAndBindLine:"
        << "lineId=" << line->sceneRegistryKey();

    // ConnectionLine has no domainKey override → sceneRegistryKey() falls
    // back to getID() (UUID). Unchanged behavior; use the shared rule.
    scene->addItemWithId(line, line->sceneRegistryKey());
    ItemEventBinder::bindConnectionLine(line, mainWindow);
}

} // namespace

ConnectionLine *ConnectionLineFactory::fromConnection(
    Backend::Scenario::Connection *connection,
    GraphicsScene                 *regionScene,
    MainWindow                    *mainWindow)
{
    if (!connection || !regionScene) return nullptr;

    qCInfo(lcGuiScene)
        << "ConnectionLineFactory::fromConnection:"
        << "from=" << connection->fromTerminalId
        << "to=" << connection->toTerminalId;

    auto *startItem =
        regionScene->getItemById<TerminalItem>(connection->fromTerminalId);
    auto *endItem =
        regionScene->getItemById<TerminalItem>(connection->toTerminalId);
    if (!startItem || !endItem)
    {
        qCWarning(lcGuiScene)
            << "ConnectionLineFactory::fromConnection:"
            << "terminal not found"
            << "startFound=" << (startItem != nullptr)
            << "endFound=" << (endItem != nullptr);
        return nullptr;
    }

    auto *line = new ConnectionLine(startItem, endItem, connection->mode,
                                    {}, startItem->getRegion());
    line->setConnectionModel(connection);
    addAndBindLine(line, regionScene, mainWindow);
    return line;
}

ConnectionLine *ConnectionLineFactory::findRegionConnection(
    GraphicsScene *scene, const QString &fromTerminalId,
    const QString &toTerminalId,
    Backend::TransportationTypes::TransportationMode mode)
{
    qCDebug(lcGuiScene)
        << "ConnectionLineFactory::findRegionConnection:"
        << "from=" << fromTerminalId
        << "to=" << toTerminalId;

    if (!scene) return nullptr;
    const auto all = scene->getItemsByType<ConnectionLine>();
    for (auto *line : all)
    {
        if (!line) continue;
        auto *cm = line->connectionModel();
        if (!cm) continue;
        if (cm->fromTerminalId == fromTerminalId
         && cm->toTerminalId   == toTerminalId
         && cm->mode           == mode)
            return line;
    }
    return nullptr;
}

ConnectionLine *ConnectionLineFactory::findGlobalLink(
    GraphicsScene *scene, const QString &fromTerminalId,
    const QString &toTerminalId,
    Backend::TransportationTypes::TransportationMode mode)
{
    qCDebug(lcGuiScene)
        << "ConnectionLineFactory::findGlobalLink:"
        << "from=" << fromTerminalId
        << "to=" << toTerminalId;

    if (!scene) return nullptr;
    const auto all = scene->getItemsByType<ConnectionLine>();
    for (auto *line : all)
    {
        if (!line) continue;
        auto *gm = line->globalLinkModel();
        if (!gm) continue;
        if (gm->fromTerminalId == fromTerminalId
         && gm->toTerminalId   == toTerminalId
         && gm->mode           == mode)
            return line;
    }
    return nullptr;
}

ConnectionLine *ConnectionLineFactory::fromGlobalLink(
    Backend::Scenario::GlobalLink *link,
    GraphicsScene                 *globalScene,
    MainWindow                    *mainWindow)
{
    if (!link || !globalScene) return nullptr;

    qCInfo(lcGuiScene)
        << "ConnectionLineFactory::fromGlobalLink:"
        << "from=" << link->fromTerminalId
        << "to=" << link->toTerminalId;

    // GlobalTerminalItem::getID() (Task 4 follow-up) returns the linked
    // terminal's id, so the scene's registry is keyed by terminal id.
    // O(1) lookup via the type-indexed registry; no scan needed.
    auto *startItem = globalScene->getItemById<GlobalTerminalItem>(
        link->fromTerminalId);
    auto *endItem = globalScene->getItemById<GlobalTerminalItem>(
        link->toTerminalId);
    if (!startItem || !endItem)
    {
        qCWarning(lcGuiScene)
            << "ConnectionLineFactory::fromGlobalLink:"
            << "global terminal not found"
            << "startFound=" << (startItem != nullptr)
            << "endFound=" << (endItem != nullptr);
        return nullptr;
    }

    auto *line = new ConnectionLine(startItem, endItem, link->mode, {},
                                    QStringLiteral("Global"));
    line->setGlobalLinkModel(link);
    addAndBindLine(line, globalScene, mainWindow);
    return line;
}

} // namespace Scenario
} // namespace GUI
} // namespace CargoNetSim
