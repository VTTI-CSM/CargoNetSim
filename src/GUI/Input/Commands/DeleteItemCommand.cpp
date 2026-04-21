#include "DeleteItemCommand.h"

#include "DeleteBackgroundPhotoCommand.h"
#include "DeleteConnectionCommand.h"
#include "DeleteGlobalLinkCommand.h"
#include "DeleteTerminalCommand.h"

namespace CargoNetSim::GUI::Input {

std::unique_ptr<QUndoCommand> DeleteItemCommand::forTerminal(
    Backend::Scenario::ScenarioDocument* doc, QString terminalId)
{
    return std::make_unique<DeleteTerminalCommand>(doc, std::move(terminalId));
}

std::unique_ptr<QUndoCommand> DeleteItemCommand::forConnection(
    Backend::Scenario::ScenarioDocument*                doc,
    QString                                             fromId,
    QString                                             toId,
    Backend::TransportationTypes::TransportationMode    mode)
{
    return std::make_unique<DeleteConnectionCommand>(
        doc, std::move(fromId), std::move(toId), mode);
}

std::unique_ptr<QUndoCommand> DeleteItemCommand::forGlobalLink(
    Backend::Scenario::ScenarioDocument*                doc,
    QString                                             fromId,
    QString                                             toId,
    Backend::TransportationTypes::TransportationMode    mode)
{
    return std::make_unique<DeleteGlobalLinkCommand>(
        doc, std::move(fromId), std::move(toId), mode);
}

std::unique_ptr<QUndoCommand> DeleteItemCommand::forBackgroundPhoto(
    BackgroundPhotoItem* item)
{
    return std::make_unique<DeleteBackgroundPhotoCommand>(item);
}

} // namespace CargoNetSim::GUI::Input
