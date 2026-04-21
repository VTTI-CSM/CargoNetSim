#include "UpdateRegionLocalOriginCommand.h"

#include "../../../Backend/Commons/LogCategories.h"
#include "../../../Backend/Scenario/ScenarioDocument.h"
#include "../../Scenario/ScenarioMutator.h"

#include <QLoggingCategory>
#include <QObject>

namespace CargoNetSim::GUI::Input {

UpdateRegionLocalOriginCommand::UpdateRegionLocalOriginCommand(
    Backend::Scenario::ScenarioDocument* doc,
    QString                              regionName,
    QPointF                              newLatLon,
    QUndoCommand*                        parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_regionName(std::move(regionName))
    , m_newLatLon(newLatLon)
{
    Q_ASSERT(doc);
    Q_ASSERT(!m_regionName.isEmpty());
    setText(QObject::tr("Move region %1").arg(m_regionName));
}

void UpdateRegionLocalOriginCommand::redo()
{
    if (!m_doc) { setObsolete(true); return; }
    if (!m_captured) {
        auto it = m_doc->regions.find(m_regionName);
        if (it == m_doc->regions.end()) {
            qCWarning(lcGuiInputCmd)
                << "UpdateRegionLocalOriginCommand::redo: region not found"
                << m_regionName;
            setObsolete(true);
            return;
        }
        m_oldLatLon = QPointF(it->localOrigin.longitude, it->localOrigin.latitude);
        m_captured  = true;
    }
    Scenario::ScenarioMutator::updateRegionLocalOrigin(
        m_doc.data(), m_regionName, m_newLatLon);
    qCInfo(lcGuiInputCmd) << "UpdateRegionLocalOriginCommand::redo"
                          << m_regionName
                          << "lon=" << m_newLatLon.x()
                          << "lat=" << m_newLatLon.y();
}

void UpdateRegionLocalOriginCommand::undo()
{
    if (!m_doc || !m_captured) return;
    Scenario::ScenarioMutator::updateRegionLocalOrigin(
        m_doc.data(), m_regionName, m_oldLatLon);
    qCInfo(lcGuiInputCmd) << "UpdateRegionLocalOriginCommand::undo"
                          << m_regionName;
}

bool UpdateRegionLocalOriginCommand::mergeWith(const QUndoCommand* other)
{
    const auto* o = dynamic_cast<const UpdateRegionLocalOriginCommand*>(other);
    if (!o || o->m_regionName != m_regionName) return false;
    m_newLatLon = o->m_newLatLon;
    return true;
}

} // namespace CargoNetSim::GUI::Input
