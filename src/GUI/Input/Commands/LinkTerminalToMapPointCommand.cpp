#include "LinkTerminalToMapPointCommand.h"

#include "../../../Backend/Application/ScenarioEditService.h"
#include "../../../Backend/Commons/LogCategories.h"
#include "../../../Backend/Scenario/ScenarioDocument.h"

#include <QLoggingCategory>
#include <QObject>

namespace CargoNetSim::GUI::Input {

LinkTerminalToMapPointCommand::LinkTerminalToMapPointCommand(
    Backend::Scenario::ScenarioDocument* doc,
    QString                              terminalId,
    QString                              networkName,
    int                                  nodeId,
    Backend::Scenario::LinkageSource     source,
    QUndoCommand*                        parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_terminalId(std::move(terminalId))
    , m_networkName(std::move(networkName))
    , m_nodeId(nodeId)
    , m_source(source)
{
    Q_ASSERT(doc);
    Q_ASSERT(!m_terminalId.isEmpty());
    Q_ASSERT(!m_networkName.isEmpty());
    setText(QObject::tr("Link terminal %1 to node %2").arg(m_terminalId).arg(m_nodeId));
}

void LinkTerminalToMapPointCommand::redo()
{
    if (!m_doc) { setObsolete(true); return; }
    const bool ok = Backend::Application::ScenarioEditService::linkTerminalToNode(
        m_doc.data(), m_terminalId, m_networkName, m_nodeId, m_source);
    if (!ok) {
        qCWarning(lcGuiInputCmd)
            << "LinkTerminalToMapPointCommand::redo: linkTerminalToNode failed"
            << m_terminalId << m_networkName << m_nodeId;
        setObsolete(true);
        return;
    }
    m_wasCreated = true;
    qCInfo(lcGuiInputCmd) << "LinkTerminalToMapPointCommand::redo"
                          << m_terminalId << m_networkName << "node=" << m_nodeId;
}

void LinkTerminalToMapPointCommand::undo()
{
    if (!m_doc || !m_wasCreated) return;
    Backend::Application::ScenarioEditService::unlinkTerminalFromNode(
        m_doc.data(), m_terminalId, m_networkName, m_nodeId);
    qCInfo(lcGuiInputCmd) << "LinkTerminalToMapPointCommand::undo"
                          << m_terminalId << m_networkName << "node=" << m_nodeId;
}

} // namespace CargoNetSim::GUI::Input
