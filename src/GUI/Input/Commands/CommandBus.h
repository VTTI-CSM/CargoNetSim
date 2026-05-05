#pragma once

#include <QObject>
#include <QString>
#include <QUndoStack>
#include <memory>

class QUndoCommand;

namespace CargoNetSim::GUI::Input {

/**
 * Thin wrapper around QUndoStack. Every scenario mutation that originates in
 * the GUI input pipeline flows through submit(). Provides:
 *   - re-entrancy guard (nested submits are queued via QTimer::singleShot 0)
 *   - logging hook (submitted / failed) via Qt signals
 *   - convenient macro begin/end for multi-select operations
 */
class CommandBus : public QObject {
    Q_OBJECT
public:
    explicit CommandBus(QObject* parent = nullptr);
    ~CommandBus() override;

    /**
     * Take ownership and push. If the command's redo() calls setObsolete(true)
     * on itself, QUndoStack removes it immediately and submit() returns false.
     * If called re-entrantly (an observer triggered by redo() tries to submit
     * another command), the inner submit is deferred via QTimer::singleShot
     * and returns true (will execute on the next event-loop iteration).
     */
    bool submit(std::unique_ptr<QUndoCommand> command);

    void beginMacro(const QString& text);
    void endMacro();

    QUndoStack* undoStack() { return &m_stack; }

private:
    QUndoStack m_stack;
    bool       m_inSubmit    = false; // re-entrancy guard
    int        m_macroDepth  = 0;     // >0 while inside beginMacro/endMacro;
                                      // outcome tracking still works per-
                                      // submit via the wrapper's side channel,
                                      // but the bool return is trivially true
                                      // inside a macro because absorbed-into-
                                      // macro pushes never obsolete the outer
                                      // macro command.
};

} // namespace CargoNetSim::GUI::Input
