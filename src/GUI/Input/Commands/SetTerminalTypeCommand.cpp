#include "SetTerminalTypeCommand.h"

#include "../../../Backend/Application/ScenarioEditService.h"
#include "../../../Backend/Commons/LogCategories.h"
#include "../../../Backend/Scenario/ScenarioDocument.h"

#include <QLoggingCategory>
#include <QObject>

namespace CargoNetSim::GUI::Input {

SetTerminalTypeCommand::SetTerminalTypeCommand(
    Backend::Scenario::ScenarioDocument* doc,
    QString                              terminalId,
    QString                              newType,
    QUndoCommand*                        parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_terminalId(std::move(terminalId))
    , m_newType(std::move(newType))
{
    Q_ASSERT(doc);
    Q_ASSERT(!m_terminalId.isEmpty());
    Q_ASSERT(!m_newType.isEmpty());
    setText(QObject::tr("Set type on terminal %1").arg(m_terminalId));
}

void SetTerminalTypeCommand::redo()
{
    if (!m_doc) { setObsolete(true); return; }
    if (!m_captured) {
        auto it = m_doc->terminals.find(m_terminalId);
        if (it == m_doc->terminals.end()) {
            qCWarning(lcGuiInputCmd)
                << "SetTerminalTypeCommand::redo: terminal not found"
                << m_terminalId;
            setObsolete(true);
            return;
        }
        m_oldType  = it->type;
        m_captured = true;
    }
    Backend::Application::ScenarioEditService::setTerminalType(
        m_doc.data(), m_terminalId, m_newType);
    qCInfo(lcGuiInputCmd) << "SetTerminalTypeCommand::redo"
                          << m_terminalId << "type=" << m_newType;
}

void SetTerminalTypeCommand::undo()
{
    if (!m_doc || !m_captured) return;
    Backend::Application::ScenarioEditService::setTerminalType(
        m_doc.data(), m_terminalId, m_oldType);
    qCInfo(lcGuiInputCmd) << "SetTerminalTypeCommand::undo"
                          << m_terminalId << "type=" << m_oldType;
}

} // namespace CargoNetSim::GUI::Input
