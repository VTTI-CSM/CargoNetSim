#include "UpdateBackgroundPhotoPositionCommand.h"

#include "../../../Backend/Commons/LogCategories.h"
#include "../../Items/BackgroundPhotoItem.h"

#include <QLoggingCategory>
#include <QObject>

namespace CargoNetSim::GUI::Input {

UpdateBackgroundPhotoPositionCommand::UpdateBackgroundPhotoPositionCommand(
    BackgroundPhotoItem *item, QPointF oldPos, QPointF newPos,
    QPointF oldGeoLonLat, QPointF newGeoLonLat, QUndoCommand *parent)
    : QUndoCommand(parent)
    , m_item(item)
    , m_oldPos(oldPos)
    , m_newPos(newPos)
    , m_oldGeo(oldGeoLonLat)
    , m_newGeo(newGeoLonLat)
{
    setText(QObject::tr("Move Background Image"));
}

void UpdateBackgroundPhotoPositionCommand::redo()
{
    qCInfo(lcGuiInputCmd)
        << "UpdateBackgroundPhotoPositionCommand::redo"
        << "pos=" << m_newPos << "geo(lon,lat)=" << m_newGeo;
    if (!m_item) { setObsolete(true); return; }
    applyTo(m_newPos, m_newGeo);
}

void UpdateBackgroundPhotoPositionCommand::undo()
{
    qCInfo(lcGuiInputCmd)
        << "UpdateBackgroundPhotoPositionCommand::undo"
        << "pos=" << m_oldPos << "geo(lon,lat)=" << m_oldGeo;
    if (!m_item) return;
    applyTo(m_oldPos, m_oldGeo);
}

void UpdateBackgroundPhotoPositionCommand::applyTo(
    QPointF scenePos, QPointF geoLonLat)
{
    // Order: properties first so observers that re-read the item see lat/lon
    // consistent with the new scenePos when positionChanged fires.
    m_item->updateProperty(QStringLiteral("Latitude"),
                           QString::number(geoLonLat.y(), 'f', 6));
    m_item->updateProperty(QStringLiteral("Longitude"),
                           QString::number(geoLonLat.x(), 'f', 6));
    m_item->setPos(scenePos);
    emit m_item->positionChanged(scenePos);
}

bool UpdateBackgroundPhotoPositionCommand::mergeWith(const QUndoCommand *other)
{
    const auto *o = dynamic_cast<const UpdateBackgroundPhotoPositionCommand *>(other);
    if (!o) return false;
    if (m_item.data() != o->m_item.data()) return false;
    // Keep the original pre-drag anchor, adopt the latest destination.
    m_newPos = o->m_newPos;
    m_newGeo = o->m_newGeo;
    return true;
}

} // namespace CargoNetSim::GUI::Input
