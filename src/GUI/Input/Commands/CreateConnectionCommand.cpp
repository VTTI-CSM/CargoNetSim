#include "CreateConnectionCommand.h"

#include "../../../Backend/Application/RouteAuthoringService.h"
#include "../../../Backend/Commons/LogCategories.h"
#include "../../../Backend/Controllers/CargoNetSimController.h"
#include "../../../Backend/Scenario/ScenarioDocument.h"

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
    auto &controller =
        CargoNetSim::CargoNetSimController::getInstance();
    Backend::Application::RouteAuthoringService routeAuthoringService(
        &controller);
    const auto propertyResult =
        routeAuthoringService.computeEndpointCanonicalRouteProperties(
            *m_doc, m_fromId, m_toId, m_mode);
    if (!propertyResult.succeeded()) {
        qCWarning(lcGuiInputCmd)
            << "CreateConnectionCommand::redo: metric computation failed"
            << m_fromId << "->" << m_toId
            << propertyResult.message;
        setObsolete(true);
        return;
    }
    const auto result =
        routeAuthoringService.createConnection(
            *m_doc, m_fromId, m_toId, m_mode,
            propertyResult.canonicalProperties,
            Backend::Scenario::LinkageSource::Manual);
    if (!result.succeeded()) {
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
    auto &controller =
        CargoNetSim::CargoNetSimController::getInstance();
    Backend::Application::RouteAuthoringService routeAuthoringService(
        &controller);
    routeAuthoringService.removeRoute(
        *m_doc, m_fromId, m_toId, m_mode);
    qCInfo(lcGuiInputCmd) << "CreateConnectionCommand::undo"
                          << m_fromId << "->" << m_toId;
}

} // namespace CargoNetSim::GUI::Input
