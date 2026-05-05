#include "CreateTerminalAtPointCommand.h"

#include "../../../Backend/Application/ScenarioEditService.h"
#include "../../../Backend/Commons/LogCategories.h"
#include "../../../Backend/Scenario/ScenarioDocument.h"

#include <QLoggingCategory>
#include <QObject>

namespace CargoNetSim::GUI::Input {

CreateTerminalAtPointCommand::CreateTerminalAtPointCommand(
    Backend::Scenario::ScenarioDocument* doc,
    QString                              region,
    QString                              terminalType,
    QPointF                              localLatLon,
    TerminalRole                         role,
    QUndoCommand*                        parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_region(std::move(region))
    , m_terminalType(std::move(terminalType))
    , m_localLatLon(localLatLon)
    , m_role(role)
{
    Q_ASSERT(doc);
    Q_ASSERT(!m_region.isEmpty());
    Q_ASSERT(!m_terminalType.isEmpty());
    setText(QObject::tr("Create %1 terminal").arg(m_terminalType));
}

void CreateTerminalAtPointCommand::redo()
{
    if (!m_doc) { setObsolete(true); return; }
    const QString newId = Backend::Application::ScenarioEditService::createTerminal(
        m_doc.data(), m_terminalType, m_region, m_localLatLon, m_role);
    if (newId.isEmpty()) {
        qCWarning(lcGuiInputCmd)
            << "CreateTerminalAtPointCommand::redo: createTerminal failed";
        setObsolete(true);
        return;
    }
    m_createdTerminalId = newId;
    m_wasCreated = true;
    qCInfo(lcGuiInputCmd) << "CreateTerminalAtPointCommand::redo"
                          << "id=" << newId << "type=" << m_terminalType;
}

void CreateTerminalAtPointCommand::undo()
{
    if (!m_doc || !m_wasCreated || m_createdTerminalId.isEmpty()) return;
    Backend::Application::ScenarioEditService::removeTerminal(
        m_doc.data(), m_createdTerminalId);
    qCInfo(lcGuiInputCmd) << "CreateTerminalAtPointCommand::undo"
                          << "id=" << m_createdTerminalId;
    m_wasCreated = false;
}

} // namespace CargoNetSim::GUI::Input
