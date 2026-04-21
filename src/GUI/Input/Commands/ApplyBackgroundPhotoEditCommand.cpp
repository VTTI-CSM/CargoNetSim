#include "ApplyBackgroundPhotoEditCommand.h"

#include "../../../Backend/Commons/LogCategories.h"
#include "../../Items/BackgroundPhotoItem.h"

#include <QLoggingCategory>
#include <QObject>

namespace CargoNetSim::GUI::Input {

ApplyBackgroundPhotoEditCommand::ApplyBackgroundPhotoEditCommand(
    BackgroundPhotoItem *item, State before, State after, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_item(item)
    , m_before(std::move(before))
    , m_after(std::move(after))
{
    setText(QObject::tr("Edit Background Image"));
}

void ApplyBackgroundPhotoEditCommand::redo()
{
    qCInfo(lcGuiInputCmd) << "ApplyBackgroundPhotoEditCommand::redo";
    if (!m_item) { setObsolete(true); return; }
    applyState(m_after);
}

void ApplyBackgroundPhotoEditCommand::undo()
{
    qCInfo(lcGuiInputCmd) << "ApplyBackgroundPhotoEditCommand::undo";
    if (!m_item) return;
    applyState(m_before);
}

void ApplyBackgroundPhotoEditCommand::applyState(const State &state)
{
    // Order: properties first (they carry the authoritative Latitude / Longitude
    // / Scale strings). Then the derived visual fields — which are also
    // captured in the snapshot so the item stays in-sync with the property
    // bag even if callers mutated the map without calling the setters.
    m_item->updateProperties(state.properties);
    m_item->setScale(static_cast<float>(state.scale));
    m_item->setOpacity(state.opacity);
    m_item->setZValue(state.zValue);
    m_item->setPos(state.scenePos);
    emit m_item->positionChanged(state.scenePos);
}

} // namespace CargoNetSim::GUI::Input
