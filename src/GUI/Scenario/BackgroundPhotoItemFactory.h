#pragma once

#include <QMap>
#include <QPixmap>
#include <QPointF>
#include <QString>
#include <QVariant>

namespace CargoNetSim {
namespace GUI {

class BackgroundPhotoItem;
class GraphicsScene;
class MainWindow;

namespace Scenario {

/**
 * Snapshot that fully describes a BackgroundPhotoItem's visual state plus its
 * owning region. Used as the single input to
 * BackgroundPhotoItemFactory::create — the importer fills it with viewport-
 * centered defaults, undo/load paths fill it with captured state.
 */
struct BackgroundPhotoSpec {
    QPixmap                 pixmap;
    QString                 region;   ///< "global" routes to the global scene / global variable.
    QPointF                 scenePos;
    qreal                   scale    = 1.0;
    qreal                   opacity  = 1.0;
    qreal                   zValue   = -1.0;
    QMap<QString, QVariant> properties; ///< Filled with sensible defaults when empty.
};

/**
 * Single construction path for BackgroundPhotoItem. Centralizes:
 *   1. Constructor + visual state application.
 *   2. Scene registration via addItemWithId.
 *   3. Backend-owned region/global view publication
 *      ("backgroundPhotoItem" for a region scene,
 *      "globalBackgroundPhotoItem" for the global-map scene).
 *   4. Signal wiring via ItemEventBinder.
 *
 * publish/unpublish helpers are exposed so DeleteBackgroundPhotoCommand can
 * reuse the exact same backend-variable semantics — one authoritative place
 * for "where does a BackgroundPhotoItem live in the region/global view store".
 */
class BackgroundPhotoItemFactory {
public:
    static BackgroundPhotoItem* create(const BackgroundPhotoSpec &spec,
                                       GraphicsScene             *scene,
                                       MainWindow                *mw);

    /// Store the item pointer under the appropriate backend-owned key.
    static void publishToController(BackgroundPhotoItem *item,
                                    const QString       &region,
                                    bool                 isGlobal);

    /// Remove the backend-owned key, leaving no dangling QVariant pointer after deletion.
    static void unpublishFromController(const QString &region,
                                        bool           isGlobal);

    /// "global" sentinel region check, shared with callers so the magic string
    /// has exactly one definition.
    static bool isGlobalRegion(const QString &region);
};

} // namespace Scenario
} // namespace GUI
} // namespace CargoNetSim
