#include "SetTerminalRoleCommand.h"

#include "../../../Backend/Application/ScenarioEditService.h"
#include "../../../Backend/Commons/LogCategories.h"
#include "../../../Backend/Scenario/ScenarioDocument.h"

#include <QLoggingCategory>
#include <QObject>

namespace CargoNetSim::GUI::Input {

SetTerminalRoleCommand::SetTerminalRoleCommand(
    Backend::Scenario::ScenarioDocument* doc,
    QString                              terminalId,
    TerminalRole                         newRole,
    QUndoCommand*                        parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_terminalId(std::move(terminalId))
    , m_newRole(newRole)
{
    Q_ASSERT(doc);
    Q_ASSERT(!m_terminalId.isEmpty());
    setText(QObject::tr("Set role on terminal %1").arg(m_terminalId));
}

void SetTerminalRoleCommand::redo()
{
    if (!m_doc) { setObsolete(true); return; }
    if (!m_captured) {
        auto it = m_doc->terminals.find(m_terminalId);
        if (it == m_doc->terminals.end()) {
            qCWarning(lcGuiInputCmd)
                << "SetTerminalRoleCommand::redo: terminal not found"
                << m_terminalId;
            setObsolete(true);
            return;
        }
        m_oldRole  = it->role;
        m_captured = true;
    }
    Backend::Application::ScenarioEditService::setTerminalRole(
        m_doc.data(), m_terminalId, m_newRole);
    qCInfo(lcGuiInputCmd) << "SetTerminalRoleCommand::redo"
                          << m_terminalId
                          << "role=" << static_cast<int>(m_newRole);
}

void SetTerminalRoleCommand::undo()
{
    if (!m_doc || !m_captured) return;
    Backend::Application::ScenarioEditService::setTerminalRole(
        m_doc.data(), m_terminalId, m_oldRole);
    qCInfo(lcGuiInputCmd) << "SetTerminalRoleCommand::undo"
                          << m_terminalId
                          << "role=" << static_cast<int>(m_oldRole);
}

} // namespace CargoNetSim::GUI::Input
