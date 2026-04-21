#include "CommandBus.h"

#include "../../../Backend/Commons/LogCategories.h"

#include <QLoggingCategory>
#include <QTimer>
#include <QUndoCommand>

namespace CargoNetSim::GUI::Input {

CommandBus::CommandBus(QObject* parent)
    : QObject(parent)
{
    m_stack.setUndoLimit(100);
    connect(&m_stack, &QUndoStack::canUndoChanged, this, [](bool canUndo) {
        qCDebug(lcGuiInputCmd) << "canUndo =" << canUndo;
    });
    connect(&m_stack, &QUndoStack::canRedoChanged, this, [](bool canRedo) {
        qCDebug(lcGuiInputCmd) << "canRedo =" << canRedo;
    });
}

CommandBus::~CommandBus() = default;

bool CommandBus::submit(std::unique_ptr<QUndoCommand> command)
{
    if (!command) {
        qCCritical(lcGuiInputCmd) << "submit(nullptr) — invariant violation";
        return false;
    }

    const QString desc = command->text();

    if (m_inSubmit) {
        qCWarning(lcGuiInputCmd)
            << "re-entrant submit detected; deferring via QTimer::singleShot 0. desc ="
            << desc;
        // Transfer ownership into a shared_ptr so we can capture in lambda.
        auto shared = std::shared_ptr<QUndoCommand>(command.release());
        QTimer::singleShot(0, this, [this, shared]() mutable {
            this->submit(std::unique_ptr<QUndoCommand>(shared.get()));
            // shared's deleter will no-op because unique_ptr now owns it.
            // But we need to release the shared without deleting:
            std::shared_ptr<QUndoCommand> *leak = new std::shared_ptr<QUndoCommand>(std::move(shared));
            (void)leak;
        });
        return true;
    }

    m_inSubmit = true;
    qCInfo(lcGuiInputCmd) << "submit:" << desc;

    // QUndoStack::push takes ownership of the raw pointer.
    QUndoCommand* raw = command.release();
    m_stack.push(raw);

    m_inSubmit = false;

    // After push, raw may be obsolete (QUndoStack removed it). Detect via undoStack state.
    // A simpler signal: if the stack's count did not increase, the command was obsoleted.
    const bool accepted = (m_stack.count() > 0 && m_stack.command(m_stack.count() - 1) == raw);
    if (!accepted) {
        qCWarning(lcGuiInputCmd) << "command obsoleted during redo:" << desc;
        emit commandFailed(nullptr, QStringLiteral("command marked obsolete"));
        return false;
    }

    qCDebug(lcGuiInputCmd) << "submit done, stack size =" << m_stack.count();
    emit commandSubmitted(raw);
    return true;
}

void CommandBus::beginMacro(const QString& text)
{
    qCInfo(lcGuiInputCmd) << "beginMacro:" << text;
    m_stack.beginMacro(text);
}

void CommandBus::endMacro()
{
    m_stack.endMacro();
    qCInfo(lcGuiInputCmd) << "endMacro (stack size now" << m_stack.count() << ")";
}

} // namespace CargoNetSim::GUI::Input
