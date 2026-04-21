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
    Backend::Scenario::ScenarioDocument *doc,
    Backend::Scenario::Connection       *connection,
    GraphicsScene                       *regionScene,
    MainWindow                          *mainWindow)
{
    if (!doc || !connection || !regionScene) return nullptr;

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
    line->bindToConnection(doc, connection->fromTerminalId,
                           connection->toTerminalId, connection->mode);
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
    // Match on the line's own stored binding triple, not on connectionModel()
    // — the latter returns nullptr once the connection has been removed from
    // the doc, but the scene still has the line (observers remove the line
    // from the scene by key AFTER mutation, so this lookup must succeed even
    // during that transient window).
    for (auto *line : all)
    {
        if (!line || !line->isConnectionBinding()) continue;
        if (line->boundFromTerminalId() == fromTerminalId
         && line->boundToTerminalId()   == toTerminalId
         && line->connectionType()      == mode)
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
        if (!line || !line->isGlobalLinkBinding()) continue;
        if (line->boundFromTerminalId() == fromTerminalId
         && line->boundToTerminalId()   == toTerminalId
         && line->connectionType()      == mode)
            return line;
    }
    return nullptr;
}

ConnectionLine *ConnectionLineFactory::fromGlobalLink(
    Backend::Scenario::ScenarioDocument *doc,
    Backend::Scenario::GlobalLink       *link,
    GraphicsScene                       *globalScene,
    MainWindow                          *mainWindow)
{
    if (!doc || !link || !globalScene) return nullptr;

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
    qCDebug(lcGuiScene)
        << "ConnectionLineFactory::fromGlobalLink:"
        << "startItem=" << startItem
        << "endItem=" << endItem;
    if (!startItem || !endItem)
    {
        qCWarning(lcGuiScene)
            << "ConnectionLineFactory::fromGlobalLink:"
            << "global terminal not found"
            << "startFound=" << (startItem != nullptr)
            << "endFound=" << (endItem != nullptr);
        return nullptr;
    }

    qCDebug(lcGuiScene)
        << "ConnectionLineFactory::fromGlobalLink:"
        << "constructing ConnectionLine";
    auto *line = new ConnectionLine(startItem, endItem, link->mode, {},
                                    QStringLiteral("Global"));
    qCDebug(lcGuiScene)
        << "ConnectionLineFactory::fromGlobalLink:"
        << "ConnectionLine constructed line=" << line;
    line->bindToGlobalLink(doc, link->fromTerminalId, link->toTerminalId,
                           link->mode);
    qCDebug(lcGuiScene)
        << "ConnectionLineFactory::fromGlobalLink:"
        << "calling addAndBindLine";
    addAndBindLine(line, globalScene, mainWindow);
    qCDebug(lcGuiScene)
        << "ConnectionLineFactory::fromGlobalLink:"
        << "done, returning line=" << line;
    return line;
}

} // namespace Scenario
} // namespace GUI
} // namespace CargoNetSim
