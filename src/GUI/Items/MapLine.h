#pragma once

#include "GUI/Items/TerminalItem.h"
#include "GUI/Input/Interfaces/IClickable.h"
#include "GraphicsObjectBase.h"

#include <QGraphicsObject>
#include <QMap>
#include <QPainterPath>
#include <QPen>
#include <QPointF>
#include <QString>
#include <QVariant>

namespace CargoNetSim
{
namespace GUI
{

/**
 * @brief Represents a line connecting two points in a
 * network
 *
 * MapLine is a visual representation of a connection
 * between two points in a network. It belongs to a specific
 * network region and can have various properties.
 *
 * Delete-policy: same as MapPoint — network-scoped; removed
 * en-bloc by NetworkDrawingController::removeNetwork. We
 * deliberately do NOT override
 * GraphicsObjectBase::createDeleteCommand. See MapPoint.h for
 * the full rationale.
 */
class MapLine : public GraphicsObjectBase,
                public Input::IClickable
{
    Q_OBJECT

public:
    /// Unique graphics-item type id. See TerminalItem::Type for rationale.
    enum { Type = UserType + 6 };
    int type() const override { return Type; }

    /**
     * @brief Constructs a new MapLine object
     *
     * @param startPoint The starting point of the line
     * @param endPoint The ending point of the line
     * @param region The network region this line belongs to
     * @param properties Optional additional properties
     */
    MapLine(const QString &referenceNetworkID,
            const QPointF &startPoint,
            const QPointF &endPoint,
            const QString &region = "Default Region",
            const QMap<QString, QVariant> &properties =
                QMap<QString, QVariant>());

    virtual ~MapLine() = default;

    void
    createAnimationVisual(const QColor &color) override;

    void clearAnimationVisuals() override;

    /**
     * @brief Sets the reference network that this point is
     * created from
     * @param network The network pointer
     */
    void setReferenceNetwork(QObject *network)
    {
        m_referenceNetwork = network;
    }

    /**
     * @brief Get the reference network that this point is
     * created from
     * @return QObject* The network pointer
     */
    QObject *getReferenceNetwork()
    {
        return m_referenceNetwork;
    }

    /**
     * @brief Sets the color of the line
     *
     * @param color The new color
     */
    void setColor(const QColor &color);

    /**
     * @brief Sets the pen used for drawing the line
     *
     * @param pen The pen to use
     */
    void setPen(const QPen &pen);

    /**
     * @brief Sets the region of the line
     */
    void setRegion(const QString region)
    {
        m_properties.value("region") = region;
    }

    void setPoints(const QPointF &newStartPoint,
                   const QPointF &newEndPoint);

    /**
     * @brief Get the start point
     */
    const QPointF &getStartPoint() const
    {
        return startPoint;
    }

    /**
     * @brief Get the end point
     */
    const QPointF &getEndPoint() const
    {
        return endPoint;
    }

    /**
     * @brief Get the region
     */
    const QString getRegion() const
    {
        return m_properties.value("region").toString();
    }

    /**
     * @brief Get the properties
     */
    const QMap<QString, QVariant> &getProperties() const
    {
        return m_properties;
    }

    QString getReferencedNetworkLinkID() const
    {
        return m_properties.value("Network_ID", "-1")
            .toString();
    }

    /**
     * @brief Sets a specific property value
     * @param key The property key
     * @param value The property value to set
     */
    void setProperty(const QString  &key,
                     const QVariant &value)
    {
        if (!m_properties.contains(key)
            || m_properties[key] != value)
        {
            m_properties[key] = value;
            emit propertyChanged(key, value);
        }
    }

    /**
     * @brief Gets a specific property value
     * @param key The property key
     * @return The property value or invalid QVariant if not
     * found
     */
    QVariant getProperty(const QString &key) const
    {
        return m_properties.value(key, QVariant());
    }

    /**
     * @brief Serializes the line to a dictionary for
     * storage
     */
    QMap<QString, QVariant> toDict() const;

    /**
     * @brief Creates a MapLine from a serialized dictionary
     */
    static MapLine *
    fromDict(const QMap<QString, QVariant> &data);

    Input::Handled
    onLeftClick(const Input::ClickContext &ctx) override;

signals:
    /**
     * @brief Signal emitted when the line's color changes
     */
    void colorChanged(const QColor &newColor);

    /**
     * @brief Signal emitted when any property changes
     */
    void propertyChanged(const QString  &key,
                         const QVariant &value);

protected:
    QRectF       boundingRect() const override;
    QPainterPath shape() const override;
    void         paint(QPainter                       *painter,
                       const QStyleOptionGraphicsItem *option,
                       QWidget *widget = nullptr) override;

private:
    QPointF                 startPoint;
    QPointF                 endPoint;
    QMap<QString, QVariant> m_properties;
    int                     baseWidth;
    QPen                    pen;
    QObject                *m_referenceNetwork;
};

} // namespace GUI
} // namespace CargoNetSim

// Register the type for QVariant
Q_DECLARE_METATYPE(CargoNetSim::GUI::MapLine)
Q_DECLARE_METATYPE(CargoNetSim::GUI::MapLine *)
