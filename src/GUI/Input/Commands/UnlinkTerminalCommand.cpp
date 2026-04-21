#include "UnlinkTerminalCommand.h"

#include "../../../Backend/Commons/LogCategories.h"
#include "../../../Backend/Scenario/ScenarioDocument.h"
#include "../../Scenario/ScenarioMutator.h"

#include <QLoggingCategory>
#include <QObject>

namespace CargoNetSim::GUI::Input {

UnlinkTerminalCommand::UnlinkTerminalCommand(
    Backend::Scenario::ScenarioDocument* doc,
    QString                              terminalId,
    QString                              networkName,
    int                                  nodeId,
    QUndoCommand*                        parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_terminalId(std::move(terminalId))
    , m_networkName(std::move(networkName))
    , m_nodeId(nodeId)
{
    Q_ASSERT(doc);
    Q_ASSERT(!m_terminalId.isEmpty());
    setText(QObject::tr("Unlink terminal %1 from node %2").arg(m_terminalId).arg(m_nodeId));
}

void UnlinkTerminalCommand::redo()
{
    if (!m_doc) { setObsolete(true); return; }
    if (!m_captured) {
        for (const auto &l : m_doc->linkages) {
            if (l.terminalId  == m_terminalId
             && l.networkName == m_networkName
             && l.nodeId      == m_nodeId) {
                m_snapshot = l;
                m_captured = true;
                break;
            }
        }
        if (!m_captured) {
            qCWarning(lcGuiInputCmd)
                << "UnlinkTerminalCommand::redo: linkage not found"
                << m_terminalId << m_networkName << m_nodeId;
            setObsolete(true);
            return;
        }
    }
    Scenario::ScenarioMutator::unlinkTerminalFromNode(
        m_doc.data(), m_terminalId, m_networkName, m_nodeId);
    qCInfo(lcGuiInputCmd) << "UnlinkTerminalCommand::redo"
                          << m_terminalId << m_networkName << "node=" << m_nodeId;
}

void UnlinkTerminalCommand::undo()
{
    if (!m_doc || !m_captured) return;
    Scenario::ScenarioMutator::restoreLinkage(m_doc.data(), m_snapshot);
    qCInfo(lcGuiInputCmd) << "UnlinkTerminalCommand::undo"
                          << m_terminalId << m_networkName << "node=" << m_nodeId;
}

} // namespace CargoNetSim::GUI::Input
