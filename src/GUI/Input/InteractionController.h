#pragma once

#include <QGraphicsItem>
#include <QObject>
#include <QPointer>
#include <memory>
#include <utility>
#include <vector>

#include "Handled.h"
#include "InputEvent.h"

namespace CargoNetSim {

namespace Backend::Scenario { class ScenarioDocument; }

namespace GUI {
class GraphicsScene;
class GraphicsView;
class MainWindow;

namespace Input {

class CommandBus;
class IInteractionMode;
struct ClickContext;

/**
 * Mediator. Every input event (mouse, keyboard, wheel, context menu, hover,
 * drag/drop) flows through dispatch(). Routes first to the top-of-stack mode
 * (State pattern), then to the target item's interface implementations
 * (Strategy pattern), then returns a Handled verdict.
 *
 * Ownership: MainWindow owns one InteractionController. GraphicsScene and
 * GraphicsView hold non-owning raw pointers (lifetime-equal to MainWindow).
 */
class InteractionController : public QObject {
    Q_OBJECT

public:
    explicit InteractionController(MainWindow*                          mainWindow,
                                   GraphicsScene*                       scene,
                                   GraphicsView*                        view,
                                   Backend::Scenario::ScenarioDocument* document,
                                   CommandBus*                          commandBus,
                                   QObject*                             parent = nullptr);
    ~InteractionController() override;

    // --- Mode stack (State) ---

    void pushMode(std::unique_ptr<IInteractionMode> mode);
    void popMode();                       // refuses to pop NormalMode (stack bottom)
    void setMode(std::unique_ptr<IInteractionMode> mode);   // replaces top; NormalMode stays

    template <typename Mode, typename... Args>
    void setMode(Args&&... args) { setMode(std::make_unique<Mode>(std::forward<Args>(args)...)); }

    template <typename Mode, typename... Args>
    void pushMode(Args&&... args) { pushMode(std::make_unique<Mode>(std::forward<Args>(args)...)); }

    IInteractionMode* currentMode() const;                  // never null

    // --- Primary entry point ---

    Handled dispatch(const InputEvent& event);

    // --- Services ---

    CommandBus*    commandBus() const { return m_commandBus; }
    GraphicsScene* scene()      const;
    GraphicsView*  view()       const;
    MainWindow*    mainWindow() const;
    Backend::Scenario::ScenarioDocument* document() const;

    // --- Document swap (called by MainWindow::setRuntime on scenario reload) ---

    void setDocument(Backend::Scenario::ScenarioDocument* doc);

    // --- UI-state helpers (transient selection; NOT commands) ---

    void selectItem   (QGraphicsItem* item, bool exclusive = true);
    void clearSelection();

signals:
    void modeChanged       (IInteractionMode* newMode, IInteractionMode* oldMode);
    void destinationPicked (const QString& terminalId, const QString& terminalName);

private:
    Handled      dispatchToMode(const InputEvent&, const ClickContext&);
    Handled      dispatchToItem(const InputEvent&, const ClickContext&);
    ClickContext buildContext  (const InputEvent&);
    void         refreshCursor ();

    std::vector<std::unique_ptr<IInteractionMode>> m_modeStack;
    QPointer<MainWindow>                           m_mainWindow;
    QPointer<GraphicsScene>                        m_scene;
    QPointer<GraphicsView>                         m_view;
    QPointer<Backend::Scenario::ScenarioDocument>  m_document;
    CommandBus*                                    m_commandBus;   // non-owning, lifetime >= this
};

} // namespace Input
} // namespace GUI
} // namespace CargoNetSim
