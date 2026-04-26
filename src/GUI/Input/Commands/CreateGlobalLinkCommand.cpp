#include "CreateGlobalLinkCommand.h"

#include "../../../Backend/Application/RouteAuthoringService.h"
#include "../../../Backend/Commons/LogCategories.h"
#include "../../../Backend/Controllers/CargoNetSimController.h"
#include "../../../Backend/Scenario/RouteMetricUnits.h"
#include "../../../Backend/Scenario/ScenarioDocument.h"

#include <QLoggingCategory>
#include <QObject>

namespace CargoNetSim::GUI::Input {

CreateGlobalLinkCommand::CreateGlobalLinkCommand(
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
    setText(QObject::tr("Create global link %1 \u2192 %2").arg(m_fromId, m_toId));
}

void CreateGlobalLinkCommand::redo()
{
    if (!m_doc) { setObsolete(true); return; }
    auto &controller =
        CargoNetSim::CargoNetSimController::getInstance();
    Backend::Application::RouteAuthoringService routeAuthoringService(
        &controller);
    const auto result =
        routeAuthoringService.createGlobalLink(
            *m_doc, m_fromId, m_toId, m_mode,
            Backend::Scenario::RouteMetricUnits::
                defaultCanonicalProperties(),
            Backend::Scenario::LinkageSource::Manual);
    if (!result.succeeded()) {
        qCWarning(lcGuiInputCmd)
            << "CreateGlobalLinkCommand::redo: createGlobalLink failed"
            << m_fromId << "->" << m_toId;
        setObsolete(true);
        return;
    }
    m_wasCreated = true;
    qCInfo(lcGuiInputCmd) << "CreateGlobalLinkCommand::redo"
                          << m_fromId << "->" << m_toId
                          << "mode=" << static_cast<int>(m_mode);
}

void CreateGlobalLinkCommand::undo()
{
    if (!m_doc || !m_wasCreated) return;
    auto &controller =
        CargoNetSim::CargoNetSimController::getInstance();
    Backend::Application::RouteAuthoringService routeAuthoringService(
        &controller);
    routeAuthoringService.removeRoute(
        *m_doc, m_fromId, m_toId, m_mode);
    qCInfo(lcGuiInputCmd) << "CreateGlobalLinkCommand::undo"
                          << m_fromId << "->" << m_toId;
}

} // namespace CargoNetSim::GUI::Input
