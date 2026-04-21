#include "CreateConnectionCommand.h"

#include "../../../Backend/Commons/LogCategories.h"
#include "../../../Backend/Scenario/ScenarioDocument.h"
#include "../../Scenario/ScenarioMutator.h"

#include <QLoggingCategory>
#include <QObject>

namespace CargoNetSim::GUI::Input {

CreateConnectionCommand::CreateConnectionCommand(
    Backend::Scenario::ScenarioDocument*                doc,
    QString                                             fromId,
    QString                                             toId,
    Backend::TransportationTypes::TransportationMode    mode,
    QUndoCommand*                                       parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_fromId(std::move(fromId))
    , m_toId(std::move(toId))
    , m_mode(mode)
{
    Q_ASSERT(doc);
    Q_ASSERT(!m_fromId.isEmpty());
    Q_ASSERT(!m_toId.isEmpty());
    setText(QObject::tr("Create connection %1 \u2192 %2").arg(m_fromId, m_toId));
}

void CreateConnectionCommand::redo()
{
    if (!m_doc) { setObsolete(true); return; }
    const bool ok = Scenario::ScenarioMutator::createConnection(
        m_doc.data(), m_fromId, m_toId, m_mode);
    if (!ok) {
        qCWarning(lcGuiInputCmd)
            << "CreateConnectionCommand::redo: createConnection failed"
            << m_fromId << "->" << m_toId;
        setObsolete(true);
        return;
    }
    m_wasCreated = true;
    qCInfo(lcGuiInputCmd) << "CreateConnectionCommand::redo"
                          << m_fromId << "->" << m_toId
                          << "mode=" << static_cast<int>(m_mode);
}

void CreateConnectionCommand::undo()
{
    if (!m_doc || !m_wasCreated) return;
    Scenario::ScenarioMutator::removeConnection(
        m_doc.data(), m_fromId, m_toId, m_mode);
    qCInfo(lcGuiInputCmd) << "CreateConnectionCommand::undo"
                          << m_fromId << "->" << m_toId;
}

} // namespace CargoNetSim::GUI::Input
