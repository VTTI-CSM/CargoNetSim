#pragma once

#include "GraphicsObjectBase.h"

#include <QGraphicsObject>
#include <QPointF>
#include <QRectF>
#include <memory>

namespace CargoNetSim
{
namespace GUI
{

class GraphicsView;

/**
 * @brief A graphics item that displays distance
 * measurements on the scene
 *
 * The DistanceMeasurementTool allows users to measure
 * distances between points on the map. It shows a line
 * between two points with the calculated distance displayed
 * in the appropriate units (meters or kilometers).
 */
class DistanceMeasurementTool : public GraphicsObjectBase
{
    Q_OBJECT

public:
    /// Unique graphics-item type id. See TerminalItem::Type for rationale.
    enum { Type = UserType + 9 };
    int type() const override { return Type; }

    /**
     * @brief Constructor
     * @param view The graphics view this tool is associated
     * with
     * @param parent The parent graphics item
     */
    explicit DistanceMeasurementTool(
        GraphicsView  *view,
        QGraphicsItem *parent = nullptr);

    /**
     * @brief Destructor
     */
    virtual ~DistanceMeasurementTool();

    /**
     * @brief Set the start point of the measurement
     * @param point The start point in scene coordinates
     */
    void setStartPoint(const QPointF &point);

    /**
     * @brief Set the end point of the measurement
     * @param point The end point in scene coordinates
     */
    void setEndPoint(const QPointF &point);

    /**
     * @brief Get the start point of the measurement
     * @return The start point in scene coordinates
     */
    QPointF getStartPoint() const
    {
        return m_startPoint;
    }

    /**
     * @brief Get the end point of the measurement
     * @return The end point in scene coordinates
     */
    QPointF getEndPoint() const
    {
        return m_endPoint;
    }

    /**
     * @brief Get the calculated distance
     * @return The distance in meters
     */
    double getDistance() const;

    /**
     * @brief Get the formatted distance text
     * @return Formatted distance with appropriate units (m
     * or km)
     */
    QString getDistanceText() const;

    /**
     * @brief Reset the measurement tool
     * Clears both start and end points
     */
    void reset();

    /**
     * @brief Check if the measurement tool has a valid
     * start point
     * @return true if the start point is set, false
     * otherwise
     */
    bool hasStartPoint() const;

    /**
     * @brief Check if the measurement tool has a valid end
     * point
     * @return true if the end point is set, false otherwise
     */
    bool hasEndPoint() const;

signals:
    /**
     * @brief Signal emitted when the start point changes
     * @param point The new start point
     */
    void startPointChanged(const QPointF &point);

    /**
     * @brief Signal emitted when the end point changes
     * @param point The new end point
     */
    void endPointChanged(const QPointF &point);

    /**
     * @brief Signal emitted when the distance changes
     * @param distance The new distance in meters
     * @param text The formatted distance text
     */
    void distanceChanged(double         distance,
                         const QString &text);

    /**
     * @brief Signal emitted when the measurement is
     * completed
     * @param startPoint The start point of the measurement
     * @param endPoint The end point of the measurement
     * @param distance The calculated distance in meters
     */
    void measurementCompleted(const QPointF &startPoint,
                              const QPointF &endPoint,
                              double         distance);

protected:
    /**
     * @brief Returns the bounding rectangle of the item
     * @return The bounding rectangle in scene coordinates
     */
    QRectF boundingRect() const override;

    /**
     * @brief Paint the measurement tool
     * @param painter The painter to use
     * @param option Style options
     * @param widget The widget being painted on
     */
    void paint(QPainter                       *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = nullptr) override;

private:
    /**
     * @brief Calculate the distance between the start and
     * end points
     * @return Distance in meters
     */
    double calculateDistance() const;

    /**
     * @brief Calculate Euclidean distance in projected
     * coordinate system
     * @param lat1 Latitude of first point
     * @param lon1 Longitude of first point
     * @param lat2 Latitude of second point
     * @param lon2 Longitude of second point
     * @return Distance in meters
     */
    double calculateProjectedDistance(double lat1,
                                      double lon1,
                                      double lat2,
                                      double lon2) const;

    /**
     * @brief Calculate geodetic distance on the WGS84
     * ellipsoid
     * @param lat1 Latitude of first point in degrees
     * @param lon1 Longitude of first point in degrees
     * @param lat2 Latitude of second point in degrees
     * @param lon2 Longitude of second point in degrees
     * @return Distance in meters
     */
    double calculateGeodesicDistance(double lat1,
                                     double lon1,
                                     double lat2,
                                     double lon2) const;

    GraphicsView *m_view; ///< The graphics view this tool
                          ///< is associated with
    QPointF m_startPoint = QPointF(
        std::nan(""),
        std::nan(
            "")); ///< The start point in scene coordinates
    QPointF m_endPoint = QPointF(
        std::nan(""),
        std::nan(
            "")); ///< The end point in scene coordinates
    mutable double
        m_cachedDistance;         ///< Cached distance value
    mutable bool m_distanceDirty; ///< Flag indicating if
                                  ///< the distance needs to
                                  ///< be recalculated
};

} // namespace GUI
} // namespace CargoNetSim

// Register the type for QVariant
Q_DECLARE_METATYPE(
    CargoNetSim::GUI::DistanceMeasurementTool)
Q_DECLARE_METATYPE(
    CargoNetSim::GUI::DistanceMeasurementTool *)
