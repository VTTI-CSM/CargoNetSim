#include "DeleteConnectionCommand.h"

#include "../../../Backend/Commons/LogCategories.h"
#include "../../../Backend/Scenario/ScenarioDocument.h"
#include "../../Scenario/ScenarioMutator.h"

#include <QLoggingCategory>
#include <QObject>

namespace CargoNetSim::GUI::Input {

DeleteConnectionCommand::DeleteConnectionCommand(
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
    setText(QObject::tr("Delete connection %1 \u2192 %2").arg(m_fromId, m_toId));
}

void DeleteConnectionCommand::redo()
{
    if (!m_doc) { setObsolete(true); return; }
    if (!m_captured) {
        for (const auto &c : m_doc->connections) {
            if (c.fromTerminalId == m_fromId
             && c.toTerminalId   == m_toId
             && c.mode           == m_mode) {
                m_snapshot = c;
                m_captured = true;
                break;
            }
        }
        if (!m_captured) {
            qCWarning(lcGuiInputCmd)
                << "DeleteConnectionCommand::redo: connection not found"
                << m_fromId << "->" << m_toId;
            setObsolete(true);
            return;
        }
    }
    Scenario::ScenarioMutator::removeConnection(
        m_doc.data(), m_fromId, m_toId, m_mode);
    qCInfo(lcGuiInputCmd) << "DeleteConnectionCommand::redo"
                          << m_fromId << "->" << m_toId;
}

void DeleteConnectionCommand::undo()
{
    if (!m_doc || !m_captured) return;
    Scenario::ScenarioMutator::restoreConnection(m_doc.data(), m_snapshot);
    qCInfo(lcGuiInputCmd) << "DeleteConnectionCommand::undo"
                          << m_fromId << "->" << m_toId;
}

} // namespace CargoNetSim::GUI::Input
