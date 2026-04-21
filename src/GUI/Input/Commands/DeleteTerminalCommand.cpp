#include "DeleteTerminalCommand.h"

#include "../../../Backend/Commons/LogCategories.h"
#include "../../../Backend/Scenario/ScenarioDocument.h"
#include "../../Scenario/ScenarioMutator.h"

#include <QLoggingCategory>
#include <QObject>

namespace CargoNetSim::GUI::Input {

DeleteTerminalCommand::DeleteTerminalCommand(
    Backend::Scenario::ScenarioDocument* doc,
    QString                              terminalId,
    QUndoCommand*                        parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_terminalId(std::move(terminalId))
{
    Q_ASSERT(doc);
    Q_ASSERT(!m_terminalId.isEmpty());
    setText(QObject::tr("Delete terminal %1").arg(m_terminalId));
}

void DeleteTerminalCommand::redo()
{
    if (!m_doc) { setObsolete(true); return; }
    if (!m_captured) {
        auto it = m_doc->terminals.find(m_terminalId);
        if (it == m_doc->terminals.end()) {
            qCWarning(lcGuiInputCmd)
                << "DeleteTerminalCommand::redo: terminal not found"
                << m_terminalId;
            setObsolete(true);
            return;
        }
        m_snapshot = *it;
        m_captured = true;
    }
    Scenario::ScenarioMutator::removeTerminal(m_doc.data(), m_terminalId);
    qCInfo(lcGuiInputCmd) << "DeleteTerminalCommand::redo" << m_terminalId;
}

void DeleteTerminalCommand::undo()
{
    if (!m_doc || !m_captured) return;
    Scenario::ScenarioMutator::restoreTerminal(m_doc.data(), m_snapshot);
    qCInfo(lcGuiInputCmd) << "DeleteTerminalCommand::undo" << m_terminalId;
}

} // namespace CargoNetSim::GUI::Input
