#include "UpdateRegionGlobalPositionCommand.h"

#include "../../../Backend/Application/ScenarioEditService.h"
#include "../../../Backend/Commons/LogCategories.h"
#include "../../../Backend/Scenario/ScenarioDocument.h"

#include <QLoggingCategory>
#include <QObject>

namespace CargoNetSim::GUI::Input {

UpdateRegionGlobalPositionCommand::UpdateRegionGlobalPositionCommand(
    Backend::Scenario::ScenarioDocument *doc,
    QString                              regionName,
    QPointF                              newLatLon,
    QUndoCommand                        *parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_regionName(std::move(regionName))
    , m_newLatLon(newLatLon)
{
    Q_ASSERT(doc);
    Q_ASSERT(!m_regionName.isEmpty());
    setText(QObject::tr("Move region %1 on global map").arg(m_regionName));
}

void UpdateRegionGlobalPositionCommand::redo()
{
    if (!m_doc) { setObsolete(true); return; }
    if (!m_captured) {
        auto it = m_doc->regions.find(m_regionName);
        if (it == m_doc->regions.end()) {
            qCWarning(lcGuiInputCmd)
                << "UpdateRegionGlobalPositionCommand::redo: region not found"
                << m_regionName;
            setObsolete(true);
            return;
        }
        m_oldLatLon = QPointF(it->globalPosition.longitude,
                              it->globalPosition.latitude);
        m_captured  = true;
    }
    const bool updated =
        Backend::Application::ScenarioEditService::updateRegionGlobalPosition(
            m_doc.data(), m_regionName, m_newLatLon);
    if (!updated) {
        qCWarning(lcGuiInputCmd)
            << "UpdateRegionGlobalPositionCommand::redo:"
            << "backend update rejected"
            << m_regionName;
        setObsolete(true);
        return;
    }
    qCInfo(lcGuiInputCmd) << "UpdateRegionGlobalPositionCommand::redo"
                          << m_regionName
                          << "lon=" << m_newLatLon.x()
                          << "lat=" << m_newLatLon.y();
}

void UpdateRegionGlobalPositionCommand::undo()
{
    if (!m_doc || !m_captured) return;
    const bool updated =
        Backend::Application::ScenarioEditService::updateRegionGlobalPosition(
            m_doc.data(), m_regionName, m_oldLatLon);
    if (!updated) {
        qCWarning(lcGuiInputCmd)
            << "UpdateRegionGlobalPositionCommand::undo:"
            << "backend update rejected"
            << m_regionName;
        return;
    }
    qCInfo(lcGuiInputCmd) << "UpdateRegionGlobalPositionCommand::undo"
                          << m_regionName;
}

bool UpdateRegionGlobalPositionCommand::mergeWith(const QUndoCommand *other)
{
    const auto *o =
        dynamic_cast<const UpdateRegionGlobalPositionCommand *>(other);
    if (!o || o->m_regionName != m_regionName) return false;
    m_newLatLon = o->m_newLatLon;
    return true;
}

} // namespace CargoNetSim::GUI::Input
