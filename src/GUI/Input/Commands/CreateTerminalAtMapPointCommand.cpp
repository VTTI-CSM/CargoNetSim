#include "CreateTerminalAtMapPointCommand.h"

#include "../../../Backend/Commons/LogCategories.h"
#include "../../../Backend/Scenario/LinkageSource.h"
#include "../../../Backend/Scenario/ScenarioDocument.h"
#include "../../Scenario/ScenarioMutator.h"

#include <QLoggingCategory>
#include <QObject>

namespace CargoNetSim::GUI::Input {

CreateTerminalAtMapPointCommand::CreateTerminalAtMapPointCommand(
    Backend::Scenario::ScenarioDocument* doc,
    QString                              networkName,
    int                                  nodeId,
    QString                              terminalType,
    QString                              region,
    QPointF                              localLatLon,
    TerminalRole                         role,
    QUndoCommand*                        parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_networkName(std::move(networkName))
    , m_nodeId(nodeId)
    , m_terminalType(std::move(terminalType))
    , m_region(std::move(region))
    , m_localLatLon(localLatLon)
    , m_role(role)
{
    Q_ASSERT(doc);
    Q_ASSERT(!m_terminalType.isEmpty());
    Q_ASSERT(!m_region.isEmpty());
    setText(QObject::tr("Create %1 terminal at node %2").arg(m_terminalType).arg(m_nodeId));
}

void CreateTerminalAtMapPointCommand::redo()
{
    if (!m_doc) { setObsolete(true); return; }

    const QString newId = Scenario::ScenarioMutator::createTerminal(
        m_doc.data(), m_terminalType, m_region, m_localLatLon, m_role);
    if (newId.isEmpty()) {
        qCWarning(lcGuiInputCmd)
            << "CreateTerminalAtMapPointCommand::redo: createTerminal failed";
        setObsolete(true);
        return;
    }
    m_createdTerminalId = newId;

    const bool linked = Scenario::ScenarioMutator::linkTerminalToNode(
        m_doc.data(), newId, m_networkName, m_nodeId,
        Backend::Scenario::LinkageSource::Manual);
    if (!linked) {
        qCWarning(lcGuiInputCmd)
            << "CreateTerminalAtMapPointCommand::redo: linkTerminalToNode failed; rolling back";
        Scenario::ScenarioMutator::removeTerminal(m_doc.data(), newId);
        m_createdTerminalId.clear();
        setObsolete(true);
        return;
    }
    m_wasCreated = true;
    qCInfo(lcGuiInputCmd) << "CreateTerminalAtMapPointCommand::redo"
                          << "id=" << newId
                          << "network=" << m_networkName
                          << "node=" << m_nodeId;
}

void CreateTerminalAtMapPointCommand::undo()
{
    if (!m_doc || !m_wasCreated || m_createdTerminalId.isEmpty()) return;
    Scenario::ScenarioMutator::unlinkTerminalFromNode(
        m_doc.data(), m_createdTerminalId, m_networkName, m_nodeId);
    Scenario::ScenarioMutator::removeTerminal(
        m_doc.data(), m_createdTerminalId);
    qCInfo(lcGuiInputCmd) << "CreateTerminalAtMapPointCommand::undo"
                          << "id=" << m_createdTerminalId;
    m_wasCreated = false;
}

} // namespace CargoNetSim::GUI::Input
