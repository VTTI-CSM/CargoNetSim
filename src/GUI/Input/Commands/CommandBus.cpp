#include "CommandBus.h"

#include "../../../Backend/Commons/LogCategories.h"

#include <QLoggingCategory>
#include <QTimer>
#include <QUndoCommand>

#include <memory>
#include <utility>

namespace CargoNetSim::GUI::Input {

namespace {

/// Side-channel state written by OutcomeTracker::redo() and read by
/// CommandBus::submit() after QUndoStack::push() returns. Lives in a
/// shared_ptr so the reader can access it even after Qt deletes the
/// tracker (MERGE and OBSOLETE paths both destroy the pushed command).
struct PushOutcome {
    enum State { NotReached, Completed, Obsoleted };
    State state = NotReached;
};

/// Private wrapper that makes QUndoStack::push's outcome observable
/// without inferring from stack arithmetic. Qt's push() can end in one
/// of three states — append (ACCEPT), delete-via-mergeWith (MERGE), or
/// delete-via-isObsolete (OBSOLETE) — and does not expose which
/// happened. The stack-count heuristic the previous implementation
/// used mis-classifies every successful MERGE as OBSOLETE, because
/// both leave stack size unchanged.
///
/// Instead, the tracker writes its outcome inside redo() to a shared
/// flag that survives the tracker's destruction. redo() runs exactly
/// once per push (Qt guarantee), before the ACCEPT/MERGE/OBSOLETE
/// branching — so the flag is reliably set regardless of the branch.
///
/// Tracker preserves every Qt-visible behavior of the wrapped command:
///   - id() delegates to inner → Qt's merge check compares the right
///     identifiers.
///   - mergeWith() delegates to inner's mergeWith() → merge semantics
///     are unchanged. Qt only ever calls this with trackers on both
///     sides (nothing in the codebase bypasses CommandBus::submit).
///   - undo() delegates to inner → undo behavior unchanged.
///   - text() is copied from inner at construction → undo-history
///     labels unchanged.
class OutcomeTracker final : public QUndoCommand
{
public:
    OutcomeTracker(std::unique_ptr<QUndoCommand> inner,
                   std::shared_ptr<PushOutcome>  outcome)
        : m_inner(std::move(inner))
        , m_outcome(std::move(outcome))
    {
        setText(m_inner->text());
    }

    int id() const override { return m_inner->id(); }

    void redo() override
    {
        m_inner->redo();
        if (m_inner->isObsolete()) {
            // Propagate to our own flag so Qt follows its OBSOLETE
            // branch and deletes us without appending/merging.
            setObsolete(true);
            m_outcome->state = PushOutcome::Obsoleted;
        } else {
            m_outcome->state = PushOutcome::Completed;
        }
    }

    void undo() override { m_inner->undo(); }

    bool mergeWith(const QUndoCommand *other) override
    {
        const auto *o = dynamic_cast<const OutcomeTracker *>(other);
        if (!o) return false;
        return m_inner->mergeWith(o->m_inner.get());
    }

private:
    std::unique_ptr<QUndoCommand> m_inner;
    std::shared_ptr<PushOutcome>  m_outcome;
};

} // namespace


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

    // Wrap the caller's command in an OutcomeTracker so push()'s
    // verdict is observable regardless of whether Qt takes the
    // ACCEPT, MERGE, or OBSOLETE branch. The outcome lives in a
    // shared_ptr so the reader can inspect it after Qt deletes the
    // tracker (MERGE and OBSOLETE both destroy the pushed command).
    auto outcome = std::make_shared<PushOutcome>();
    auto *tracker =
        new OutcomeTracker(std::move(command), outcome);
    m_stack.push(tracker);

    m_inSubmit = false;

    switch (outcome->state) {
    case PushOutcome::Completed:
        // Covers ACCEPT (tracker appended) and MERGE (tracker
        // deleted after mergeWith absorbed it). Either way, the
        // inner command's redo() ran and its effect is in the
        // model. Inside a macro this also fires per inner submit;
        // that's the correct answer — each inner command either
        // completed or self-obsoleted, independently.
        qCDebug(lcGuiInputCmd)
            << "submit done, stack size =" << m_stack.count();
        return true;

    case PushOutcome::Obsoleted:
        // The inner command's redo() called setObsolete(true)
        // (usually because of a null QPointer or a document-level
        // lookup failure). Commands typically qCWarning before
        // obsoleting themselves, but this log ensures the event is
        // observable even for the few that don't.
        qCWarning(lcGuiInputCmd)
            << "command obsoleted during redo:" << desc;
        return false;

    case PushOutcome::NotReached:
        // Defensive: Qt's push() contract guarantees it invokes
        // redo() before deciding ACCEPT/MERGE/OBSOLETE. If we land
        // here, either Qt changed its contract or the inner
        // command's redo() threw (Qt swallowed the exception and
        // unwound). Either way, treat as failure so callers that
        // key UX off the bool (ConnectMode) don't report success.
        qCWarning(lcGuiInputCmd)
            << "submit: push() returned without invoking redo():"
            << desc;
        return false;
    }
    Q_UNREACHABLE();
}

void CommandBus::beginMacro(const QString& text)
{
    qCInfo(lcGuiInputCmd) << "beginMacro:" << text;
    ++m_macroDepth;
    m_stack.beginMacro(text);
}

void CommandBus::endMacro()
{
    m_stack.endMacro();
    if (m_macroDepth > 0) --m_macroDepth;
    qCInfo(lcGuiInputCmd) << "endMacro (stack size now" << m_stack.count() << ")";
}

} // namespace CargoNetSim::GUI::Input
