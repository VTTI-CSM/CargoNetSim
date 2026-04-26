#include "DeleteGlobalLinkCommand.h"

#include "../../../Backend/Application/RouteAuthoringService.h"
#include "../../../Backend/Commons/LogCategories.h"
#include "../../../Backend/Controllers/CargoNetSimController.h"
#include "../../../Backend/Scenario/ScenarioDocument.h"

#include <QLoggingCategory>
#include <QObject>

namespace CargoNetSim::GUI::Input {

DeleteGlobalLinkCommand::DeleteGlobalLinkCommand(
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
    setText(QObject::tr("Delete global link %1 \u2192 %2").arg(m_fromId, m_toId));
}

void DeleteGlobalLinkCommand::redo()
{
    if (!m_doc) { setObsolete(true); return; }
    if (!m_captured) {
        if (const auto *g = m_doc->findGlobalLink(
                m_fromId, m_toId, m_mode)) {
            m_snapshot = *g;
            m_captured = true;
        }
        if (!m_captured) {
            qCWarning(lcGuiInputCmd)
                << "DeleteGlobalLinkCommand::redo: link not found"
                << m_fromId << "->" << m_toId;
            setObsolete(true);
            return;
        }
    }
    auto &controller =
        CargoNetSim::CargoNetSimController::getInstance();
    Backend::Application::RouteAuthoringService routeAuthoringService(
        &controller);
    routeAuthoringService.removeRoute(
        *m_doc, m_fromId, m_toId, m_mode);
    qCInfo(lcGuiInputCmd) << "DeleteGlobalLinkCommand::redo"
                          << m_fromId << "->" << m_toId;
}

void DeleteGlobalLinkCommand::undo()
{
    if (!m_doc || !m_captured) return;
    auto &controller =
        CargoNetSim::CargoNetSimController::getInstance();
    Backend::Application::RouteAuthoringService routeAuthoringService(
        &controller);
    routeAuthoringService.restoreGlobalLink(*m_doc, m_snapshot);
    qCInfo(lcGuiInputCmd) << "DeleteGlobalLinkCommand::undo"
                          << m_fromId << "->" << m_toId;
}

} // namespace CargoNetSim::GUI::Input
