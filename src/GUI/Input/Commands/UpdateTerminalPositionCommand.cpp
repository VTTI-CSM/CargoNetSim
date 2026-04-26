#include "UpdateTerminalPositionCommand.h"

#include "../../../Backend/Application/ScenarioEditService.h"
#include "../../../Backend/Commons/LogCategories.h"
#include "../../../Backend/Scenario/ScenarioDocument.h"

#include <QLoggingCategory>
#include <QObject>

namespace CargoNetSim::GUI::Input {

UpdateTerminalPositionCommand::UpdateTerminalPositionCommand(
    Backend::Scenario::ScenarioDocument* doc,
    QString                              terminalId,
    QPointF                              newLocalLatLon,
    QUndoCommand*                        parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_terminalId(std::move(terminalId))
    , m_newLatLon(newLocalLatLon)
{
    Q_ASSERT(doc);
    Q_ASSERT(!m_terminalId.isEmpty());
    setText(QObject::tr("Move terminal %1").arg(m_terminalId));
}

void UpdateTerminalPositionCommand::redo()
{
    if (!m_doc) { setObsolete(true); return; }
    if (!m_captured) {
        auto it = m_doc->terminals.find(m_terminalId);
        if (it == m_doc->terminals.end()) {
            qCWarning(lcGuiInputCmd)
                << "UpdateTerminalPositionCommand::redo: terminal not found"
                << m_terminalId;
            setObsolete(true);
            return;
        }
        // Qt convention: x = longitude, y = latitude.
        m_oldLatLon = QPointF(it->latLon.longitude, it->latLon.latitude);
        m_captured  = true;
    }
    Backend::Application::ScenarioEditService::updateTerminalPosition(
        m_doc.data(), m_terminalId, m_newLatLon);
    qCInfo(lcGuiInputCmd) << "UpdateTerminalPositionCommand::redo"
                          << m_terminalId
                          << "lon=" << m_newLatLon.x()
                          << "lat=" << m_newLatLon.y();
}

void UpdateTerminalPositionCommand::undo()
{
    if (!m_doc || !m_captured) return;
    Backend::Application::ScenarioEditService::updateTerminalPosition(
        m_doc.data(), m_terminalId, m_oldLatLon);
    qCInfo(lcGuiInputCmd) << "UpdateTerminalPositionCommand::undo"
                          << m_terminalId
                          << "lon=" << m_oldLatLon.x()
                          << "lat=" << m_oldLatLon.y();
}

bool UpdateTerminalPositionCommand::mergeWith(const QUndoCommand* other)
{
    const auto* o = dynamic_cast<const UpdateTerminalPositionCommand*>(other);
    if (!o || o->m_terminalId != m_terminalId) return false;
    m_newLatLon = o->m_newLatLon;
    return true;
}

} // namespace CargoNetSim::GUI::Input
