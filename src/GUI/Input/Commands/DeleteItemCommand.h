#pragma once

#include "../../../Backend/Commons/TransportationMode.h"

#include <QString>
#include <QUndoCommand>
#include <memory>

namespace CargoNetSim {
namespace Backend::Scenario { class ScenarioDocument; }
namespace GUI {
class BackgroundPhotoItem;
namespace Input {

/// Factory that maps a selected graphics item type to the right concrete
/// QUndoCommand for deletion. Keeps NormalMode::onKeyPress(Delete) and other
/// multi-select delete sites free from per-type switches.
class DeleteItemCommand {
public:
    /// Factory for terminal deletion.
    static std::unique_ptr<QUndoCommand> forTerminal(
        Backend::Scenario::ScenarioDocument* doc, QString terminalId);

    /// Factory for connection deletion.
    static std::unique_ptr<QUndoCommand> forConnection(
        Backend::Scenario::ScenarioDocument*                doc,
        QString                                             fromId,
        QString                                             toId,
        Backend::TransportationTypes::TransportationMode    mode);

    /// Factory for background-photo deletion (GUI-only, not in ScenarioDocument).
    static std::unique_ptr<QUndoCommand> forBackgroundPhoto(
        BackgroundPhotoItem* item);
};

} // namespace Input
} // namespace GUI
} // namespace CargoNetSim
