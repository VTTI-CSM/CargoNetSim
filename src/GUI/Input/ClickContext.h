#pragma once

#include "../../Backend/Scenario/ScenarioDocument.h"
#include "../MainWindow.h"
#include "../Widgets/GraphicsScene.h"
#include "../Widgets/GraphicsView.h"

#include <QGraphicsItem>
#include <QList>
#include <QPoint>
#include <QPointF>
#include <QPointer>
#include <Qt>

namespace CargoNetSim {

namespace GUI {

namespace Input {

class CommandBus;
class IInteractionMode;
class InteractionController;

/**
 * Per-event dependency-injection container. Built by InteractionController
 * before every handler call; passed `const &` to every mode / item interface.
 *
 * Invariants enforced by InteractionController::buildContext():
 *   - controller, commandBus, currentMode are NEVER null when a handler sees
 *     this context. Handlers may Q_ASSERT them.
 *   - scene, view, mainWindow, document are QPointer-guarded and MAY be null
 *     in corner cases (e.g., scenario not yet loaded). Handlers must null-
 *     check before dereference.
 *   - target is nullable by design (empty-area click).
 */
struct ClickContext {
    // --- Geometry ---
    QPointF               scenePos;
    QPoint                screenPos;
    Qt::KeyboardModifiers modifiers;
    Qt::MouseButton       button;   // NoButton for move / hover events

    // --- Target (nullable) ---
    QGraphicsItem*        target;              // topmost at scenePos, or nullptr
    QList<QGraphicsItem*> itemsUnderCursor;    // full Z-stack; target == first when non-empty

    // --- Environment (QPointer-guarded) ---
    QPointer<GraphicsScene>                       scene;
    QPointer<GraphicsView>                        view;
    QPointer<MainWindow>                          mainWindow;
    QPointer<Backend::Scenario::ScenarioDocument> document;

    // --- Dispatch services (never null when handler runs) ---
    InteractionController* controller;
    CommandBus*            commandBus;
    IInteractionMode*      currentMode;
};

} // namespace Input
} // namespace GUI
} // namespace CargoNetSim
