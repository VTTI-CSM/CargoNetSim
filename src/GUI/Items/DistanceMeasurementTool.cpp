#include "DistanceMeasurementTool.h"
#include "Backend/Commons/LogCategories.h"
#include "Backend/Commons/Units.h"
#include "../Widgets/GraphicsView.h"

#include <QFontMetrics>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QtMath>

namespace CargoNetSim
{
namespace GUI
{

DistanceMeasurementTool::DistanceMeasurementTool(
    GraphicsView *view, QGraphicsItem *parent)
    : GraphicsObjectBase(parent)
    , m_view(view)
    , m_startPoint()
    , m_endPoint()
    , m_cachedDistance(0.0)
    , m_distanceDirty(true)
{
    qCInfo(lcGuiScene)
        << "DistanceMeasurementTool::DistanceMeasurementTool:"
        << "created, view=" << (view ? "valid" : "null");

    // Ensure the tool is always drawn on top of other items
    setZValue(1000);

    // Make sure the tool doesn't interfere with selection
    setFlag(QGraphicsItem::ItemIsSelectable, false);
    setFlag(QGraphicsItem::ItemIsMovable, false);
}

DistanceMeasurementTool::~DistanceMeasurementTool()
{
    qCInfo(lcGuiScene)
        << "DistanceMeasurementTool::~DistanceMeasurementTool:"
        << "destroyed";
}

void DistanceMeasurementTool::setStartPoint(
    const QPointF &point)
{
    if (m_startPoint != point)
    {
        qCDebug(lcGuiScene)
            << "DistanceMeasurementTool::setStartPoint:"
            << "point=" << point;

        prepareGeometryChange();
        m_startPoint    = point;
        m_distanceDirty = true;
        update();

        emit startPointChanged(m_startPoint);

        // If both points are set, recalculate and emit
        // distance change
        if (!m_startPoint.isNull() && !m_endPoint.isNull())
        {
            double distance = getDistance();
            emit   distanceChanged(distance,
                                   getDistanceText());
        }
    }
}

void DistanceMeasurementTool::setEndPoint(
    const QPointF &point)
{
    if (m_endPoint != point)
    {
        qCDebug(lcGuiScene)
            << "DistanceMeasurementTool::setEndPoint:"
            << "point=" << point;

        prepareGeometryChange();
        m_endPoint      = point;
        m_distanceDirty = true;
        update();

        emit endPointChanged(m_endPoint);

        // If both points are set, recalculate and emit
        // distance change
        if (!m_startPoint.isNull() && !m_endPoint.isNull())
        {
            double distance = getDistance();
            emit   distanceChanged(distance,
                                   getDistanceText());

            // Emit measurement completed signal when both
            // points are set
            emit measurementCompleted(m_startPoint, m_endPoint,
                                      distance);
        }
    }
}

double DistanceMeasurementTool::getDistance() const
{
    if (m_distanceDirty)
    {
        m_cachedDistance = calculateDistance();
        m_distanceDirty  = false;
    }
    return m_cachedDistance;
}

QString DistanceMeasurementTool::getDistanceText() const
{
    const auto distanceMeters =
        CargoNetSim::Backend::Units::meters(getDistance());

    if (distanceMeters.value()
        >= CargoNetSim::Backend::Units::toMeters(
               CargoNetSim::Backend::Units::kilometers(1.0))
               .value())
    {
        return QString("%1 km").arg(
            CargoNetSim::Backend::Units::toKilometers(
                distanceMeters)
                .value(),
            0, 'f', 1);
    }
    else
    {
        return QString("%1 m").arg(qRound(distanceMeters.value()));
    }
}

void DistanceMeasurementTool::reset()
{
    qCDebug(lcGuiScene)
        << "DistanceMeasurementTool::reset:"
        << "clearing measurement";

    prepareGeometryChange();
    m_startPoint    = QPointF();
    m_endPoint      = QPointF();
    m_distanceDirty = true;
    update();

    emit startPointChanged(m_startPoint);
    emit endPointChanged(m_endPoint);
    emit distanceChanged(0.0, "0 m");
}

bool DistanceMeasurementTool::hasStartPoint() const
{
    return !std::isnan(m_startPoint.x())
           && !std::isnan(m_startPoint.y());
}

bool DistanceMeasurementTool::hasEndPoint() const
{
    return !std::isnan(m_endPoint.x())
           && !std::isnan(m_endPoint.y());
}

QRectF DistanceMeasurementTool::boundingRect() const
{
    if (m_startPoint.isNull() || m_endPoint.isNull())
    {
        return QRectF();
    }

    // Get the current view scale
    qreal scale = 1.0;
    if (m_view)
    {
        scale = m_view->transform().m11();
    }

    // Calculate base rectangle
    QRectF rect = QRectF(m_startPoint, m_endPoint).normalized();

    // Scale padding inversely with zoom level
    const qreal basePadding = 1000.0;
    qreal       padding     = (scale != 0.0)
                                  ? basePadding / qAbs(scale)
                                  : basePadding;

    // Ensure minimum padding
    padding = qMax(padding, 100.0);

    return rect.adjusted(-padding, -padding, padding,
                         padding);
}

void DistanceMeasurementTool::paint(
    QPainter                       *painter,
    const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    if (m_startPoint.isNull() || m_endPoint.isNull())
    {
        return;
    }

    qCDebug(lcGuiScene)
        << "DistanceMeasurementTool::paint:"
        << "start=" << m_startPoint
        << "end=" << m_endPoint;

    // --- Draw the measurement line ---
    QPen pen(QColor("red"), 1);
    pen.setCosmetic(
        true); // Line width is constant regardless of zoom
    painter->setPen(pen);
    painter->drawLine(m_startPoint, m_endPoint);

    // --- Calculate distance ---
    QString distanceText = getDistanceText();

    // Set constant font size
    QFont font = painter->font();
    font.setPointSize(12);
    painter->setFont(font);

    // Get text dimensions
    QFontMetrics fm = painter->fontMetrics();
    int textWidth   = fm.horizontalAdvance(distanceText);
    int textHeight  = fm.height();

    // Calculate the midpoint in scene coordinates
    QPointF midPoint((m_startPoint.x() + m_endPoint.x()) / 2.0,
                     (m_startPoint.y() + m_endPoint.y()) / 2.0);

    // Convert scene coordinates to view coordinates for
    // text placement
    QGraphicsView *graphicsView =
        scene() ? scene()->views().isEmpty()
                      ? nullptr
                      : scene()->views().first()
                : nullptr;
    if (!graphicsView)
    {
        return;
    }

    QPointF viewPos = graphicsView->mapFromScene(midPoint);
    QPointF screenPos(viewPos.x() - textWidth / 2.0,
                      viewPos.y() - textHeight / 2.0);

    // Save painter state
    painter->save();

    // Reset the transformation to paint in view coordinates
    painter->resetTransform();

    // Calculate padding
    const int padding = 4;

    // Create text rectangle in view coordinates
    QRectF textRect(screenPos.x(), screenPos.y(), textWidth,
                    textHeight);

    // Draw background
    QRectF backgroundRect = textRect.adjusted(
        -padding, -padding, padding, padding);
    painter->setBrush(QBrush(QColor(255, 255, 255, 230)));
    painter->setPen(Qt::NoPen);
    painter->drawRect(backgroundRect);

    // Draw text
    painter->setPen(QColor("red"));
    painter->drawText(textRect, Qt::AlignCenter,
                      distanceText);

    // Restore painter state
    painter->restore();
}

double DistanceMeasurementTool::calculateDistance() const
{
    if (m_startPoint.isNull() || m_endPoint.isNull())
    {
        return 0.0;
    }

    qCDebug(lcGuiScene)
        << "DistanceMeasurementTool::calculateDistance:"
        << "start=" << m_startPoint
        << "end=" << m_endPoint;

    // Get start/end coordinates in WGS84
    double startLat = 0.0, startLon = 0.0, endLat = 0.0,
           endLon = 0.0;

    if (m_view)
    {
        auto startLonLat =
            m_view->sceneToWGS84(m_startPoint);
        auto endLonLat = m_view->sceneToWGS84(m_endPoint);

        startLat =
            startLonLat.y(); // Latitude is y in WGS84
        startLon =
            startLonLat.x(); // Longitude is x in WGS84
        endLat = endLonLat.y();
        endLon = endLonLat.x();
    }

    // Calculate distance based on coordinate system
    if (m_view && m_view->isUsingProjectedCoords())
    {
        return calculateProjectedDistance(
            startLat, startLon, endLat, endLon);
    }
    else
    {
        return calculateGeodesicDistance(startLat, startLon,
                                         endLat, endLon);
    }
}

double DistanceMeasurementTool::calculateProjectedDistance(
    double lat1, double lon1, double lat2,
    double lon2) const
{
    // In projected mode, convert both points using WGS84
    // World Mercator projection
    QPointF point1, point2;

    if (m_view)
    {
        point1 = m_view->convertCoordinates(
            QPointF(lon1, lat1), "to_projected");
        point2 = m_view->convertCoordinates(
            QPointF(lon2, lat2), "to_projected");

        // Calculate Euclidean distance in the projected
        // plane (meters)
        double dx = point2.x() - point1.x();
        double dy = point2.y() - point1.y();
        return std::sqrt(dx * dx + dy * dy);
    }

    return 0.0;
}

double DistanceMeasurementTool::calculateGeodesicDistance(
    double lat1, double lon1, double lat2,
    double lon2) const
{
    // Handle edge cases
    if (qAbs(lat1 - lat2) < 1e-8
        && qAbs(lon1 - lon2) < 1e-8)
    {
        return 0.0;
    }

    // Vincenty's formulae for WGS84 ellipsoid
    const double a = 6378137.0; // WGS84 semi-major axis
    const double b =
        6356752.314245; // WGS84 semi-minor axis
    const double f = (a - b) / a;

    // Convert to radians
    double phi1    = qDegreesToRadians(lat1);
    double phi2    = qDegreesToRadians(lat2);
    double lambda1 = qDegreesToRadians(lon1);
    double lambda2 = qDegreesToRadians(lon2);
    double L       = lambda2 - lambda1;

    // Calculate reduced latitudes
    double U1 = qAtan((1.0 - f) * qTan(phi1));
    double U2 = qAtan((1.0 - f) * qTan(phi2));

    double sinU1 = qSin(U1);
    double cosU1 = qCos(U1);
    double sinU2 = qSin(U2);
    double cosU2 = qCos(U2);

    // Initialize lambda with L
    double lambda = L;

    // Iterative part of the algorithm
    const int maxIterations = 100;
    double    sigma         = 0.0;
    double    sinSigma      = 0.0;
    double    cosSigma      = 0.0;
    double    cos2Alpha     = 0.0;
    double    cos2SigmaM    = 0.0;
    double    sinLambda     = 0.0;
    double    cosLambda     = 0.0;

    for (int i = 0; i < maxIterations; ++i)
    {
        sinLambda = qSin(lambda);
        cosLambda = qCos(lambda);

        // Calculate sin(sigma)
        sinSigma =
            qSqrt(qPow(cosU2 * sinLambda, 2)
                  + qPow(cosU1 * sinU2
                             - sinU1 * cosU2 * cosLambda,
                         2));

        // Handle points close to antipodal
        if (sinSigma == 0.0)
        {
            return 0.0;
        }

        cosSigma =
            sinU1 * sinU2 + cosU1 * cosU2 * cosLambda;
        sigma = qAtan2(sinSigma, cosSigma);

        double sinAlpha =
            cosU1 * cosU2 * sinLambda / sinSigma;
        cos2Alpha = 1.0 - sinAlpha * sinAlpha;

        // Handle points on equator
        cos2SigmaM =
            cos2Alpha != 0.0
                ? cosSigma - 2.0 * sinU1 * sinU2 / cos2Alpha
                : 0.0;

        double C = f / 16.0 * cos2Alpha
                   * (4.0 + f * (4.0 - 3.0 * cos2Alpha));
        double lambdaPrev = lambda;
        lambda =
            L
            + (1.0 - C) * f * sinAlpha
                  * (sigma
                     + C * sinSigma
                           * (cos2SigmaM
                              + C * cosSigma
                                    * (-1.0
                                       + 2.0
                                             * qPow(
                                                 cos2SigmaM,
                                                 2))));

        // Check for convergence
        if (qAbs(lambda - lambdaPrev) < 1e-12)
        {
            break;
        }
    }

    // Calculate final distance
    double u2 = cos2Alpha * (a * a - b * b) / (b * b);
    double A =
        1.0
        + u2 / 16384.0
              * (4096.0
                 + u2
                       * (-768.0
                          + u2 * (320.0 - 175.0 * u2)));
    double B =
        u2 / 1024.0
        * (256.0 + u2 * (-128.0 + u2 * (74.0 - 47.0 * u2)));

    double deltaSigma =
        B * sinSigma
        * (cos2SigmaM
           + B / 4.0
                 * (cosSigma
                        * (-1.0 + 2.0 * qPow(cos2SigmaM, 2))
                    - B / 6.0 * cos2SigmaM
                          * (-3.0 + 4.0 * qPow(sinSigma, 2))
                          * (-3.0
                             + 4.0 * qPow(cos2SigmaM, 2))));

    return b * A * (sigma - deltaSigma);
}

} // namespace GUI
} // namespace CargoNetSim
