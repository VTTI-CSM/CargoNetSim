#include "GraphicsView.h"

#include <QApplication>
#include <QDrag>
#include <QListWidget>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QStatusBar>
#include <QWheelEvent>
#include <QWindow>
#include <cmath>

#include "../MainWindow.h"
#include "Backend/Commons/LogCategories.h"
#include "GUI/Widgets/GraphicsScene.h"

#include "../Input/Handled.h"
#include "../Input/InputEvent.h"
#include "../Input/InteractionController.h"
#include "../Input/Modes/PanMode.h"

namespace CargoNetSim
{
namespace GUI
{

GraphicsView::GraphicsView(QGraphicsScene *scene,
                           QWidget        *parent)
    : QGraphicsView(scene, parent)
    , useProjectedCoords(false)
    , _zoom(0)
    , _panMode("ctrl_left")
    , _gridSize(50)
    , _gridEnabled(true)
    , _accumulatedScrollX(0)
    , _accumulatedScrollY(0)
    , _minLon(-180.0)
    , _maxLon(180.0)
    , _minLat(-90.0)
    , _maxLat(90.0)
{
    // Set up drag mode for left mouse
    setDragMode(QGraphicsView::RubberBandDrag);

    // Configure view settings
    setTransformationAnchor(
        QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setRenderHint(QPainter::Antialiasing);
    setViewportUpdateMode(
        QGraphicsView::FullViewportUpdate);

    // Enable mouse tracking for coordinate display
    setMouseTracking(true);

    // Allow view to move beyond scene boundaries
    constexpr double SAFE_COORD_LIMIT = 1e9; // 1 billion
    setSceneRect(-SAFE_COORD_LIMIT, -SAFE_COORD_LIMIT,
                 2 * SAFE_COORD_LIMIT,
                 2 * SAFE_COORD_LIMIT);

    // Create coordinate label
    _coordinateLabel = new QLabel(this);
    _coordinateLabel->setStyleSheet(
        "QLabel {"
        "   background-color: rgba(255, 255, 255, 200);"
        "   color: black;"
        "   padding: 5px;"
        "   border: 1px solid gray;"
        "   border-radius: 3px;"
        "}");
    _coordinateLabel->hide();

    // Enable drop support
    setAcceptDrops(true);

    // Set very large initial range for scrollbars
    horizontalScrollBar()->setRange(-100000000, 100000000);
    verticalScrollBar()->setRange(-100000000, 100000000);

    // Update scrollbar ranges based on initial zoom
    updateScrollBarRanges();
}

GraphicsView::~GraphicsView()
{
    delete _coordinateLabel;
}

double GraphicsView::latToMercator(double lat) const
{
    // Standard Web Mercator formula
    // Convert latitude to Mercator Y coordinate, avoiding
    // ±90° errors Clamp latitude to valid range for Web
    // Mercator
    lat = std::max(std::min(lat, 85.051129), -85.051129);

    double latRad = lat * M_PI / 180.0;
    return std::log(std::tan(M_PI / 4 + latRad / 2));
}

double GraphicsView::mercatorToLat(double mercatorY) const
{
    // Standard inverse Web Mercator formula
    // Clamp mercator_y to avoid math domain errors
    double maxY = latToMercator(85.051129);
    double minY = latToMercator(-85.051129);
    mercatorY   = std::max(minY, std::min(maxY, mercatorY));

    return (2 * std::atan(std::exp(mercatorY)) - M_PI / 2)
           * 180.0 / M_PI;
}

QPointF
GraphicsView::sceneToWGS84(const QPointF &scenePos) const
{
    try
    {
        // Fixed scaling factor - not dependent on zoom
        constexpr double VIEW_SCALE_FACTOR = 1000.0;

        // Apply fixed scale to coordinates
        QPointF scaledPos(scenePos.x() / SCALE_FACTOR,
                          scenePos.y() / SCALE_FACTOR);

        // Convert to normalized coordinates (-1 to 1)
        double xNorm = scaledPos.x() / VIEW_SCALE_FACTOR;
        double yNorm = scaledPos.y() / VIEW_SCALE_FACTOR;

        // Safety checks for numerical stability
        if (!std::isfinite(xNorm) || !std::isfinite(yNorm))
        {
            return QPointF(0.0, 0.0);
        }

        // Map normalized coordinates to lon/lat
        double lon = xNorm * 180.0; // Scale to longitude
                                    // range (-180 to 180)

        // Convert y to latitude using inverse Mercator
        // function
        double yMercator = -yNorm; // Flip y axis (negative
                                   // y is north in scene)
        double lat;

        // Use the standard inverse Mercator calculation
        if (std::abs(yMercator) > 0.99)
        {
            lat = std::copysign(
                85.051129,
                yMercator); // Limit to valid Mercator range
        }
        else
        {
            lat = mercatorToLat(
                yMercator
                * M_PI); // Scale and convert using proper
                         // inverse Mercator
        }

        // Ensure results are valid
        if (!std::isfinite(lat) || !std::isfinite(lon))
        {
            return QPointF(0.0, 0.0);
        }

        // Constrain to valid ranges
        lat = std::max(-90.0, std::min(90.0, lat));
        lon = std::max(-180.0, std::min(180.0, lon));

        // Return in longitude (x), latitude (y) order
        return QPointF(lon, lat);
    }
    catch (const std::exception &e)
    {
        qCWarning(lcGuiScene) << "Exception in sceneToWGS84:"
                   << e.what();
        return QPointF(0.0, 0.0);
    }
    catch (...)
    {
        qCWarning(lcGuiScene) << "Unknown exception in sceneToWGS84";
        return QPointF(0.0, 0.0);
    }
}

QPointF GraphicsView::wgs84ToScene(QPointF point) const
{
    try
    {
        // Extract longitude and latitude
        double lon = point.x();
        double lat = point.y();

        // Safety check for input values
        if (!std::isfinite(lat) || !std::isfinite(lon))
        {
            return QPointF(0, 0);
        }

        // Constrain to valid ranges
        lat = std::max(-90.0, std::min(90.0, lat));
        lon = std::max(-180.0, std::min(180.0, lon));

        // Fixed scaling factor - not dependent on zoom
        constexpr double VIEW_SCALE_FACTOR = 1000.0;

        // Convert longitude to X coordinate (normalized)
        double xNorm = lon / 180.0; // Normalize to -1 to 1

        // Convert latitude to Y using Mercator
        double mercatorY =
            latToMercator(lat) / M_PI; // Normalize by PI
        double yNorm =
            -mercatorY; // Flip to scene coordinates where Y
                        // increases downward

        // Safety checks for numerical stability
        if (!std::isfinite(xNorm) || !std::isfinite(yNorm))
        {
            return QPointF(0, 0);
        }

        // Scale to scene coordinates
        double x = xNorm * VIEW_SCALE_FACTOR * SCALE_FACTOR;
        double y = yNorm * VIEW_SCALE_FACTOR * SCALE_FACTOR;

        // Final safety check for output values
        if (!std::isfinite(x) || !std::isfinite(y))
        {
            return QPointF(0, 0);
        }

        return QPointF(x, y);
    }
    catch (const std::exception &e)
    {
        qCWarning(lcGuiScene) << "Exception in wgs84ToScene:"
                   << e.what();
        return QPointF(0, 0);
    }
    catch (...)
    {
        qCWarning(lcGuiScene) << "Unknown exception in wgs84ToScene";
        return QPointF(0, 0);
    }
}

QPointF GraphicsView::convertCoordinates(
    const QPointF &point, const QString &direction) const
{
    qCDebug(lcGuiScene) << "GraphicsView::convertCoordinates: direction=" << direction;
    try
    {
        // Safety check for input values
        if (!std::isfinite(point.x())
            || !std::isfinite(point.y()))
        {
            return QPointF(0, 0);
        }

        // WGS 84 Web Mercator constants
        constexpr double EARTH_RADIUS =
            6378137.0; // Earth radius in meters
        constexpr double HALF_CIRCUMFERENCE =
            M_PI * EARTH_RADIUS; // Half the Earth's
                                 // circumference
        constexpr double ORIGIN_SHIFT =
            HALF_CIRCUMFERENCE; // Used for coordinate
                                // origin shift

        if (direction == "to_projected")
        {
            // Extract longitude and latitude
            double lon = point.x();
            double lat = point.y();

            // Clamp latitude to the valid range for Web
            // Mercator
            lat = std::max(std::min(lat, 85.051129),
                           -85.051129);

            // Convert to radians
            double lonRad = lon * M_PI / 180.0;
            double latRad = lat * M_PI / 180.0;

            // Calculate Web Mercator coordinates
            double x = EARTH_RADIUS * lonRad;
            double y =
                EARTH_RADIUS
                * std::log(std::tan(M_PI / 4 + latRad / 2));

            // Safety check for output values
            if (!std::isfinite(x) || !std::isfinite(y))
            {
                return QPointF(0, 0);
            }

            return QPointF(x, y);
        }
        else
        { // to_geodetic
            // Extract projected coordinates
            double x = point.x();
            double y = point.y();

            // Safety check for extreme values
            if (std::abs(x) > 1e15 || std::abs(y) > 1e15)
            {
                return QPointF(0, 0);
            }

            // Convert from Web Mercator to geodetic
            // coordinates
            double lonRad = x / EARTH_RADIUS;
            double latRad =
                2 * std::atan(std::exp(y / EARTH_RADIUS))
                - M_PI / 2;

            // Convert to degrees
            double lonDeg = lonRad * 180.0 / M_PI;
            double latDeg = latRad * 180.0 / M_PI;

            // Ensure longitude is in the range [-180, 180]
            lonDeg =
                std::fmod(lonDeg + 180.0, 360.0) - 180.0;

            // Safety check for output values
            if (!std::isfinite(latDeg)
                || !std::isfinite(lonDeg))
            {
                return QPointF(0, 0);
            }

            // Return geodetic coordinates (not scene
            // coordinates!)
            return QPointF(lonDeg, latDeg);
        }
    }
    catch (const std::exception &e)
    {
        qCWarning(lcGuiScene) << "Exception in convertCoordinates:"
                   << e.what();
        return QPointF(0, 0);
    }
    catch (...)
    {
        qCWarning(lcGuiScene)
            << "Unknown exception in convertCoordinates";
        return QPointF(0, 0);
    }
}

void GraphicsView::drawBackground(QPainter     *painter,
                                  const QRectF &rect)
{
    // Call base class implementation first
    QGraphicsView::drawBackground(painter, rect);

    // Early exit if grid is disabled
    if (!_gridEnabled)
    {
        qCWarning(lcGuiScene) << "drawBackground: grid disabled, skipping draw";
        return;
    }

    // Save painter state ONCE at the beginning
    painter->save();

    try
    {
        // Get the visible area in scene coordinates
        QRectF visibleRect =
            mapToScene(viewport()->rect()).boundingRect();

        // Get the viewport width in scene coordinates
        double viewportWidth = visibleRect.width();

        // Early exit check for extreme zoom levels or
        // invalid dimensions
        if (viewportWidth <= 0 || viewportWidth > 1e15
            || visibleRect.height() <= 0
            || visibleRect.height() > 1e15)
        {
            qCWarning(lcGuiScene) << "drawBackground: invalid viewport dimensions, skipping draw";
            painter->restore();
            return;
        }

        // Adjust target grid lines based on zoom level
        int    targetGridLines;
        double zoomFactor = std::abs(_zoom);
        if (zoomFactor > 50)
        {
            targetGridLines =
                40; // More grid lines at higher zoom
        }
        else
        {
            targetGridLines =
                20; // Default number of grid lines
        }

        // Calculate base grid size that would give us the
        // target number of lines
        double baseGridSize =
            viewportWidth / targetGridLines;
        if (baseGridSize <= 0
            || !std::isfinite(baseGridSize))
        {
            painter->restore();
            return;
        }

        // Round to the nearest power of 10 to get clean
        // numbers
        double magnitude;
        try
        {
            magnitude = std::pow(
                10.0, std::round(std::log10(baseGridSize)));
            if (!std::isfinite(magnitude))
            {
                painter->restore();
                return;
            }
        }
        catch (...)
        {
            painter->restore();
            return;
        }

        // Adjust grid size based on magnitude and zoom
        double gridSize;
        if (baseGridSize < magnitude)
        {
            gridSize = magnitude / 2;
        }
        else if (baseGridSize < magnitude * 2)
        {
            gridSize = magnitude;
        }
        else
        {
            gridSize = magnitude * 2;
        }

        // Adjust minimum and maximum constraints based on
        // zoom
        double minGrid =
            50.0
            / (1.0
               + zoomFactor
                     / 10.0); // Decrease minimum grid size
                              // as zoom increases
        double maxGrid =
            1000000.0
            * (1.0
               + zoomFactor
                     / 10.0); // Increase maximum grid size
                              // as zoom increases

        // Apply constraints
        gridSize =
            std::max(minGrid, std::min(gridSize, maxGrid));

        // Safety check for grid size
        if (gridSize <= 0 || !std::isfinite(gridSize))
        {
            painter->restore();
            return;
        }

        // Calculate number of grid cells needed in each
        // direction from origin Use safe conversion to int
        // to prevent overflow
        int cellsLeft, cellsRight, cellsTop, cellsBottom;
        try
        {
            double tempLeft =
                std::abs(visibleRect.left()) / gridSize;
            double tempRight =
                std::abs(visibleRect.right()) / gridSize;
            double tempTop =
                std::abs(visibleRect.top()) / gridSize;
            double tempBottom =
                std::abs(visibleRect.bottom()) / gridSize;

            // Check if values are within safe integer range
            if (tempLeft > INT_MAX / 2
                || tempRight > INT_MAX / 2
                || tempTop > INT_MAX / 2
                || tempBottom > INT_MAX / 2)
            {
                painter->restore();
                return;
            }

            cellsLeft =
                static_cast<int>(std::ceil(tempLeft));
            cellsRight =
                static_cast<int>(std::ceil(tempRight));
            cellsTop = static_cast<int>(std::ceil(tempTop));
            cellsBottom =
                static_cast<int>(std::ceil(tempBottom));

            // Safety check: limit maximum number of cells
            // to prevent freezing
            const int MAX_CELLS = 1000;
            if (cellsLeft + cellsRight > MAX_CELLS
                || cellsTop + cellsBottom > MAX_CELLS)
            {
                painter->restore();
                return;
            }
        }
        catch (...)
        {
            painter->restore();
            return;
        }

        // Calculate grid bounds ensuring they extend from
        // origin
        double left   = -cellsLeft * gridSize;
        double right  = cellsRight * gridSize;
        double top    = -cellsTop * gridSize;
        double bottom = cellsBottom * gridSize;

        // Define quadrant colors
        QColor q1Color(150, 150, 150); // Top Right
        QColor q2Color(170, 170, 170); // Top Left
        QColor q3Color(190, 190, 190); // Bottom Left
        QColor q4Color(130, 130, 130); // Bottom Right

        // Draw grid only if lines would be sufficiently far
        // apart
        constexpr int MIN_GRID_SPACING =
            5; // Reduced from 10 to allow denser grids

        // Calculate grid spacing in pixels - with safety
        // checks
        QPointF p1 = mapFromScene(QPointF(gridSize, 0));
        QPointF p2 = mapFromScene(QPointF(0, 0));
        double  gridSpacingPixels = p1.x() - p2.x();

        if (gridSpacingPixels < MIN_GRID_SPACING
            || !std::isfinite(gridSpacingPixels))
        {
            painter->restore();
            return;
        }

        // Reset transformation to draw in view coordinates
        painter->resetTransform();

        // Draw vertical lines starting from origin and
        // going both directions
        for (double x = 0; x <= right; x += gridSize)
        {
            if (x == 0)
            { // Skip origin line here, we'll
              // draw it later
                continue;
            }

            // Convert scene coordinates to view coordinates
            int viewX = mapFromScene(QPointF(x, 0)).x();

            // Draw top half
            QPen pen(q1Color, 1);
            pen.setCosmetic(
                true); // Make line width constant
                       // regardless of zoom
            painter->setPen(pen);
            painter->drawLine(
                viewX, mapFromScene(QPointF(0, top)).y(),
                viewX, mapFromScene(QPointF(0, 0)).y());

            // Draw bottom half
            pen = QPen(q4Color, 1);
            pen.setCosmetic(true);
            painter->setPen(pen);
            painter->drawLine(
                viewX, mapFromScene(QPointF(0, 0)).y(),
                viewX,
                mapFromScene(QPointF(0, bottom)).y());
        }

        for (double x = -gridSize; x >= left; x -= gridSize)
        {
            int viewX = mapFromScene(QPointF(x, 0)).x();

            // Draw top half
            QPen pen(q2Color, 1);
            pen.setCosmetic(true);
            painter->setPen(pen);
            painter->drawLine(
                viewX, mapFromScene(QPointF(0, top)).y(),
                viewX, mapFromScene(QPointF(0, 0)).y());

            // Draw bottom half
            pen = QPen(q3Color, 1);
            pen.setCosmetic(true);
            painter->setPen(pen);
            painter->drawLine(
                viewX, mapFromScene(QPointF(0, 0)).y(),
                viewX,
                mapFromScene(QPointF(0, bottom)).y());
        }

        // Draw horizontal lines starting from origin and
        // going both directions
        for (double y = 0; y <= bottom; y += gridSize)
        {
            if (y == 0)
            { // Skip origin line here, we'll
              // draw it later
                continue;
            }

            int viewY = mapFromScene(QPointF(0, y)).y();

            // Draw left half
            QPen pen(q3Color, 1);
            pen.setCosmetic(true);
            painter->setPen(pen);
            painter->drawLine(
                mapFromScene(QPointF(left, 0)).x(), viewY,
                mapFromScene(QPointF(0, 0)).x(), viewY);

            // Draw right half
            pen = QPen(q4Color, 1);
            pen.setCosmetic(true);
            painter->setPen(pen);
            painter->drawLine(
                mapFromScene(QPointF(0, 0)).x(), viewY,
                mapFromScene(QPointF(right, 0)).x(), viewY);
        }

        for (double y = -gridSize; y >= top; y -= gridSize)
        {
            int viewY = mapFromScene(QPointF(0, y)).y();

            // Draw left half
            QPen pen(q2Color, 1);
            pen.setCosmetic(true);
            painter->setPen(pen);
            painter->drawLine(
                mapFromScene(QPointF(left, 0)).x(), viewY,
                mapFromScene(QPointF(0, 0)).x(), viewY);

            // Draw right half
            pen = QPen(q1Color, 1);
            pen.setCosmetic(true);
            painter->setPen(pen);
            painter->drawLine(
                mapFromScene(QPointF(0, 0)).x(), viewY,
                mapFromScene(QPointF(right, 0)).x(), viewY);
        }

        // Draw origin axes with darker color
        QPen originPen(QColor(100, 100, 100), 4);
        originPen.setCosmetic(true);
        painter->setPen(originPen);

        // Draw axes in view coordinates
        QPoint originPos = mapFromScene(QPointF(0, 0));
        painter->drawLine( // Y axis
            originPos.x(),
            mapFromScene(QPointF(0, top)).y(),
            originPos.x(),
            mapFromScene(QPointF(0, bottom)).y());
        painter->drawLine( // X axis
            mapFromScene(QPointF(left, 0)).x(),
            originPos.y(),
            mapFromScene(QPointF(right, 0)).x(),
            originPos.y());

        // Draw origin crosshair
        QPen crosshairPen(Qt::red, 4);
        crosshairPen.setCosmetic(true);
        painter->setPen(crosshairPen);

        // Draw crosshair lines extending to first grid in
        // each direction
        painter->drawLine( // Vertical line
            originPos.x(),
            mapFromScene(QPointF(0, -gridSize)).y(), // Top
            originPos.x(),
            mapFromScene(QPointF(0, gridSize)).y() // Bottom
        );
        painter->drawLine( // Horizontal line
            mapFromScene(QPointF(-gridSize, 0)).x(), // Left
            originPos.y(),
            mapFromScene(QPointF(gridSize, 0)).x(), // Right
            originPos.y());
    }
    catch (const std::exception &e)
    {
        // Log the exception but don't crash
        qCWarning(lcGuiScene) << "Exception in drawBackground:"
                   << e.what();
    }
    catch (...)
    {
        // Catch any other unexpected exceptions
        qCWarning(lcGuiScene) << "Unknown exception in drawBackground";
    }

    // Always restore the painter state - must be executed
    painter->restore();
}

void GraphicsView::wheelEvent(QWheelEvent *event)
{
    if (m_inputController) {
        QPoint viewPos = event->position().toPoint();
        Input::WheelEvent ie{
            event->angleDelta().y(), event->modifiers(),
            mapToScene(viewPos), event->globalPosition().toPoint()
        };
        if (m_inputController->dispatch(ie) == Input::Handled::Yes) {
            event->accept();
            return;
        }
    }
    try
    {
        constexpr double zoomInFactor  = 1.25;
        constexpr double zoomOutFactor = 1 / zoomInFactor;

        // Get the scene position before scaling
        QPointF oldPos =
            mapToScene(event->position().toPoint());

        // Determine zoom direction
        double zoomFactor;
        int    newZoom;

        if (event->angleDelta().y() > 0)
        {
            zoomFactor = zoomInFactor;
            newZoom    = _zoom + 1;
        }
        else
        {
            zoomFactor = zoomOutFactor;
            newZoom    = _zoom - 1;
        }

        // Check zoom limits
        if (newZoom > MAX_ZOOM || newZoom < MIN_ZOOM)
        {
            event->accept();
            return;
        }

        // Apply zoom
        scale(zoomFactor, zoomFactor);
        _zoom = newZoom;

        // Get the new position under mouse after scaling
        QPointF newPos =
            mapToScene(event->position().toPoint());

        // Move scene to delta to keep mouse position fixed
        QPointF delta = newPos - oldPos;
        translate(delta.x(), delta.y());

        // Adjust scrollbar ranges based on zoom level
        updateScrollBarRanges();

        event->accept();
    }
    catch (const std::exception &e)
    {
        qCWarning(lcGuiScene) << "Exception in wheelEvent:"
                   << e.what();
        event->accept();
    }
    catch (...)
    {
        qCWarning(lcGuiScene) << "Unknown exception in wheelEvent";
        event->accept();
    }
}

void GraphicsView::mousePressEvent(QMouseEvent *event)
{
    // Transient pan mode: push it before dispatching the event so the just-
    // pushed PanMode's onPress is NOT called but the following Move/Release
    // events reach it (PanMode only implements onMove/onRelease).
    const bool panTrigger =
        (_panMode == QStringLiteral("ctrl_left")
             && event->button() == Qt::LeftButton
             && (event->modifiers() & Qt::ControlModifier))
        || (_panMode == QStringLiteral("middle_mouse")
             && event->button() == Qt::MiddleButton);

    qCInfo(lcGuiScene)
        << "GraphicsView::mousePressEvent: btn=" << event->button()
        << "mods=" << event->modifiers()
        << "panMode=" << _panMode
        << "panTrigger=" << panTrigger
        << "hasController=" << (m_inputController != nullptr);

    if (panTrigger && m_inputController) {
        qCInfo(lcGuiScene) << "  -> pushing PanMode with initial pos" << event->pos();
        m_inputController->pushMode<Input::PanMode>(event->pos());
        event->accept();
        return;
    }

    if (m_inputController) {
        Input::PressEvent ie{
            event->button(), event->modifiers(),
            mapToScene(event->pos()), event->globalPosition().toPoint()
        };
        if (m_inputController->dispatch(ie) == Input::Handled::Yes) {
            event->accept();
            return;
        }
    }
    QGraphicsView::mousePressEvent(event);
}

void GraphicsView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_inputController) {
        Input::MoveEvent ie{
            event->buttons(), event->modifiers(),
            mapToScene(event->pos()), event->globalPosition().toPoint()
        };
        if (m_inputController->dispatch(ie) == Input::Handled::Yes) {
            event->accept();
            return;
        }
    }
    try
    {
        // Update coordinate label
        QPointF scenePos = mapToScene(event->pos());

        // Display coordinates
        QString coordText;
        try
        {
            if (useProjectedCoords)
            {
                // Get lon/lat coordinates first
                QPointF geoCoord = sceneToWGS84(scenePos);
                // Then convert to projected Web Mercator
                QPointF projected = convertCoordinates(
                    geoCoord, "to_projected");
                coordText =
                    QString("X: %1m, Y: %2m")
                        .arg(projected.x(), 0, 'f', 2)
                        .arg(projected.y(), 0, 'f', 2);
            }
            else
            {
                QPointF geoCoord = sceneToWGS84(scenePos);
                double  lon      = geoCoord.x();
                double  lat      = geoCoord.y();

                // Constrain display values to valid ranges
                lat = std::max(-90.0, std::min(90.0, lat));
                lon =
                    std::max(-180.0, std::min(180.0, lon));

                coordText = QString("Lon: %1°, Lat: %2°")
                                .arg(lon, 0, 'f', 6)
                                .arg(lat, 0, 'f', 6);
            }
        }
        catch (...)
        {
            coordText = "Coordinates unavailable";
        }

        _coordinateLabel->setText(coordText);

        // Position the label
        QPoint labelPos  = event->pos() + QPoint(15, 15);
        QSize  labelSize = _coordinateLabel->sizeHint();

        if (labelPos.x() + labelSize.width() > width())
        {
            labelPos.setX(event->pos().x()
                          - labelSize.width() - 5);
        }
        if (labelPos.y() + labelSize.height() > height())
        {
            labelPos.setY(event->pos().y()
                          - labelSize.height() - 5);
        }

        _coordinateLabel->move(labelPos);
        _coordinateLabel->show();

        QGraphicsView::mouseMoveEvent(event);
    }
    catch (const std::exception &e)
    {
        qCWarning(lcGuiScene) << "Exception in mouseMoveEvent:"
                   << e.what();
        QGraphicsView::mouseMoveEvent(event);
    }
    catch (...)
    {
        qCWarning(lcGuiScene) << "Unknown exception in mouseMoveEvent";
        QGraphicsView::mouseMoveEvent(event);
    }
}

void GraphicsView::leaveEvent(QEvent *event)
{
    QGraphicsView::leaveEvent(event);
    _coordinateLabel->hide();
}

void GraphicsView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
}

bool GraphicsView::eventFilter(QObject *obj, QEvent *event)
{
    // Handle any resize-related events
    if (event->type() == QEvent::Resize
        || event->type() == QEvent::LayoutRequest
        || event->type() == QEvent::LayoutDirectionChange)
    {
    }

    return QGraphicsView::eventFilter(obj, event);
}

void GraphicsView::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_inputController) {
        Input::ReleaseEvent ie{
            event->button(), event->modifiers(),
            mapToScene(event->pos()), event->globalPosition().toPoint()
        };
        if (m_inputController->dispatch(ie) == Input::Handled::Yes) {
            event->accept();
            return;
        }
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void GraphicsView::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (m_inputController) {
        Input::DoubleClickEvent ie{
            event->button(), event->modifiers(),
            mapToScene(event->pos()), event->globalPosition().toPoint()
        };
        if (m_inputController->dispatch(ie) == Input::Handled::Yes) {
            event->accept();
            return;
        }
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}

void GraphicsView::dragEnterEvent(QDragEnterEvent *event)
{
    if (m_inputController) {
        Input::DragEnterEvent ie{
            event->mimeData(),
            mapToScene(event->position().toPoint())
        };
        if (m_inputController->dispatch(ie) == Input::Handled::Yes) {
            event->accept();
            return;
        }
    }
    if (event->mimeData()->hasFormat(
            "application/x-qabstractitemmodeldatalist"))
    {
        event->accept();
    }
    else
    {
        event->ignore();
    }
}

void GraphicsView::dragMoveEvent(QDragMoveEvent *event)
{
    if (m_inputController) {
        Input::DragMoveEvent ie{
            event->mimeData(),
            mapToScene(event->position().toPoint())
        };
        if (m_inputController->dispatch(ie) == Input::Handled::Yes) {
            event->accept();
            return;
        }
    }
    if (event->mimeData()->hasFormat(
            "application/x-qabstractitemmodeldatalist"))
    {
        event->accept();
    }
    else
    {
        event->ignore();
    }
}

void GraphicsView::dropEvent(QDropEvent *event)
{
    if (m_inputController) {
        Input::DropEvent ie{
            event->mimeData(),
            mapToScene(event->position().toPoint())
        };
        if (m_inputController->dispatch(ie) == Input::Handled::Yes) {
            event->accept();
            return;
        }
    }
    event->ignore();
}

void GraphicsView::updateScrollBarRanges()
{
    try
    {
        // Get current transform scale
        double scale = transform().m11();

        // Add safety check for extremely small scales
        if (scale <= 0.0000001)
        {
            scale = 0.0000001;
        }

        // Base range that stays within 32-bit integer
        // limits
        const int MAX_RANGE =
            1000000000; // Use a more conservative value
                        // than INT_MAX

        // Calculate new range based on scale
        int adjustedRange;

        // Extra safety check for potential overflow
        if (scale < 0.00000001)
        {
            adjustedRange = MAX_RANGE;
        }
        else
        {
            double rawRange = MAX_RANGE * (1 / scale);
            if (rawRange > MAX_RANGE
                || !std::isfinite(rawRange))
            {
                adjustedRange = MAX_RANGE;
            }
            else
            {
                adjustedRange = static_cast<int>(std::min(
                    rawRange,
                    static_cast<double>(MAX_RANGE)));
            }
        }

        // Set new ranges for both scrollbars
        horizontalScrollBar()->setRange(-adjustedRange,
                                        adjustedRange);
        verticalScrollBar()->setRange(-adjustedRange,
                                      adjustedRange);
    }
    catch (const std::exception &e)
    {
        qCWarning(lcGuiScene) << "Exception in updateScrollBarRanges:"
                   << e.what();

        // In case of exception, set a safe default range
        horizontalScrollBar()->setRange(-1000000, 1000000);
        verticalScrollBar()->setRange(-1000000, 1000000);
    }
    catch (...)
    {
        qCWarning(lcGuiScene)
            << "Unknown exception in updateScrollBarRanges";

        // In case of exception, set a safe default range
        horizontalScrollBar()->setRange(-1000000, 1000000);
        verticalScrollBar()->setRange(-1000000, 1000000);
    }
}

void GraphicsView::setGridVisibility(bool visible)
{
    _gridEnabled = visible;
}

GraphicsScene *GraphicsView::getScene() const
{
    QGraphicsScene *theScene = scene();
    return dynamic_cast<GraphicsScene *>(theScene);
}

void GraphicsView::fitInView(
    const QRectF &rect, Qt::AspectRatioMode aspectRatioMode)
{
    try
    {
        // Safety check for valid rect
        if (!rect.isValid() || rect.width() <= 0
            || rect.height() <= 0)
        {
            qCWarning(lcGuiScene) << "Invalid rect in fitInView, "
                          "using default view";
            return;
        }

        // Call the parent implementation to get the basic
        // fit
        QGraphicsView::fitInView(rect, aspectRatioMode);

        // Calculate the actual zoom level based on the
        // transform scale
        double currentScale = transform().m11();

        // Safety check for numerical stability
        if (currentScale <= 0
            || !std::isfinite(currentScale))
        {
            qCWarning(lcGuiScene) << "Invalid scale in fitInView";
            _zoom = 0;
            return;
        }

        _zoom =
            qRound(std::log(currentScale) / std::log(1.25));

        // Check if the calculated zoom is within bounds
        if (_zoom > MAX_ZOOM)
        {
            // Scale back to maximum allowed zoom
            double scaleFactor =
                std::pow(1.25, MAX_ZOOM - _zoom);
            scale(scaleFactor, scaleFactor);
            _zoom = MAX_ZOOM;
        }
        else if (_zoom < MIN_ZOOM)
        {
            // Scale up to minimum allowed zoom
            double scaleFactor =
                std::pow(1.25, MIN_ZOOM - _zoom);
            scale(scaleFactor, scaleFactor);
            _zoom = MIN_ZOOM;
        }

        // Update scrollbar ranges for the new zoom level
        updateScrollBarRanges();
    }
    catch (const std::exception &e)
    {
        qCWarning(lcGuiScene) << "Exception in fitInView:" << e.what();
    }
    catch (...)
    {
        qCWarning(lcGuiScene) << "Unknown exception in fitInView";
    }
}

void GraphicsView::keyPressEvent(QKeyEvent *event)
{
    if (m_inputController) {
        Input::KeyPressEvent ie{
            event->key(), event->modifiers(),
            event->text(), event->isAutoRepeat()
        };
        if (m_inputController->dispatch(ie) == Input::Handled::Yes) {
            event->accept();
            return;
        }
    }
    QGraphicsView::keyPressEvent(event);
}

void GraphicsView::keyReleaseEvent(QKeyEvent *event)
{
    if (m_inputController) {
        Input::KeyReleaseEvent ie{ event->key(), event->modifiers() };
        if (m_inputController->dispatch(ie) == Input::Handled::Yes) {
            event->accept();
            return;
        }
    }
    QGraphicsView::keyReleaseEvent(event);
}

} // namespace GUI
} // namespace CargoNetSim
