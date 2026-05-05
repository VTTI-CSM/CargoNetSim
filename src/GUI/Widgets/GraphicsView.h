#pragma once

#include <QGraphicsView>
#include <QLabel>
#include <QPointF>
#include <QTimer>
#include <cmath>

namespace CargoNetSim
{
namespace GUI
{

namespace Input { class InteractionController; }

class GraphicsScene;
class DistanceMeasurementTool;

/**
 * @brief Custom graphics view with zoom, pan, and
 * coordinate transformation capabilities
 *
 * Extends QGraphicsView with enhanced navigation features
 * like zooming, panning, coordinate transformations between
 * different coordinate systems, and measurement tools.
 */
class GraphicsView : public QGraphicsView
{
    Q_OBJECT

public:
    /**
     * @brief Scale factor for coordinate transformations
     */
    static constexpr double SCALE_FACTOR = 100.0;

    /**
     * @brief Constructor
     * @param scene The graphics scene to display
     * @param parent Parent widget
     */
    explicit GraphicsView(QGraphicsScene *scene  = nullptr,
                          QWidget        *parent = nullptr);
    virtual ~GraphicsView();

    /**
     * @brief Convert between WGS84 geodetic coordinates and
     * Web Mercator projected coordinates
     *
     * @param point Point(lon, lat) for geodetic or Point(x,
     * y) for projected
     * @param direction Direction of conversion:
     * "to_projected" or "to_geodetic"
     * @return QPointF Point(x, y) for projected or
     * Point(lon, lat) for geodetic
     */
    QPointF convertCoordinates(
        const QPointF &point,
        const QString &direction = "to_projected") const;

    /**
     * @brief Convert scene coordinates to WGS84
     * latitude/longitude
     *
     * @param scenePos Position in scene coordinates
     * @return sQPointF of (longitude, latitude) in
     * degrees
     */
    QPointF sceneToWGS84(const QPointF &scenePos) const;

    /**
     * @brief Convert WGS84 coordinates to scene coordinates
     *
     * @param point QPointF of (Latitude in degrees,
     * Longitude in degrees)
     * @return QPointF Position in scene coordinates
     */
    QPointF wgs84ToScene(QPointF point) const;

    /**
     * @brief Convert latitude to Mercator Y coordinate
     * @param lat Latitude in degrees
     * @return double Mercator Y coordinate
     */
    double latToMercator(double lat) const;

    /**
     * @brief Convert Mercator Y coordinate to latitude
     * @param mercatorY Mercator Y coordinate
     * @return double Latitude in degrees
     */
    double mercatorToLat(double mercatorY) const;

    /**
     * @brief Update scroll bar ranges based on current zoom
     * level
     */
    void updateScrollBarRanges();

    /**
     * @brief Set the visibility of the background grid
     * @param visible True to show the grid, false to hide
     * it
     */
    void setGridVisibility(bool visible);

    /**
     * @brief Get the scene object
     * @return The scene object
     */
    GraphicsScene *getScene() const;

    bool isUsingProjectedCoords()
    {
        return useProjectedCoords;
    }

    void setUsingProjectedCoords(bool useProjectedCoords)
    {
        if (this->useProjectedCoords != useProjectedCoords) {
            this->useProjectedCoords = useProjectedCoords;
            emit coordinateSystemChanged(useProjectedCoords);
        }
    }

    QString getCurrentPanMode()
    {
        return _panMode;
    }

    void setCurrentPanMode(QString &newPanMode)
    {
        _panMode = newPanMode;
    }

    /**
     * @brief Fit content in view with zoom constraints
     * @param rect Rectangle to fit in view
     * @param aspectRatioMode Aspect ratio mode for fitting
     */
    void fitInView(const QRectF       &rect,
                   Qt::AspectRatioMode aspectRatioMode =
                       Qt::KeepAspectRatio);

    void setInputController(Input::InteractionController* ctrl)
    {
        m_inputController = ctrl;
    }

signals:
    void coordinateSystemChanged(bool isProjected);

protected:
    /**
     * @brief Draw background grid and axes
     * @param painter Painter to use for drawing
     * @param rect Area to draw
     */
    void drawBackground(QPainter     *painter,
                        const QRectF &rect) override;

    /**
     * @brief Handle mouse wheel events for zooming
     * @param event Mouse wheel event
     */
    void wheelEvent(QWheelEvent *event) override;

    /**
     * @brief Handle mouse press events for navigation and
     * tools
     * @param event Mouse press event
     */
    void mousePressEvent(QMouseEvent *event) override;

    /**
     * @brief Handle mouse move events for navigation and
     * coordinate display
     * @param event Mouse move event
     */
    void mouseMoveEvent(QMouseEvent *event) override;

    /**
     * @brief Handle mouse leave events
     * @param event Mouse leave event
     */
    void leaveEvent(QEvent *event) override;

    /**
     * @brief Handle resize events
     * @param event Resize event
     */
    void resizeEvent(QResizeEvent *event) override;

    /**
     * @brief Filter events
     * @param obj Object receiving the event
     * @param event Event being received
     * @return bool True if event was handled
     */
    bool eventFilter(QObject *obj, QEvent *event) override;

    /**
     * @brief Handle mouse release events for navigation
     * @param event Mouse release event
     */
    void mouseReleaseEvent(QMouseEvent *event) override;

    /**
     * @brief Handle mouse double-click events for
     * navigation
     * @param event Mouse double-click event
     */
    void mouseDoubleClickEvent(QMouseEvent *event) override;

    /**
     * @brief Handle drag enter events for terminal creation
     * @param event Drag enter event
     */
    void dragEnterEvent(QDragEnterEvent *event) override;

    /**
     * @brief Handle drag move events for terminal creation
     * @param event Drag move event
     */
    void dragMoveEvent(QDragMoveEvent *event) override;

    /**
     * @brief Handle drop events for terminal creation
     * @param event Drop event
     */
    void dropEvent(QDropEvent *event) override;

    /**
     * @brief Handle key press events for navigation
     * @param event Key press event
     */
    void keyPressEvent(QKeyEvent *event) override;

    /**
     * @brief Handle key release events for navigation
     * @param event Key release event
     */
    void keyReleaseEvent(QKeyEvent *event) override;

private:
    /**
     * @brief Current zoom level
     */
    int _zoom;

    /**
     * @brief Maximum zoom level allowed
     */
    static constexpr int MAX_ZOOM = 15;

    /**
     * @brief Minimum zoom level allowed
     */
    static constexpr int MIN_ZOOM = -35;

    /**
     * @brief Pan mode (ctrl_left or middle_mouse)
     */
    QString _panMode;

    /**
     * @brief Grid size for background grid
     */
    double _gridSize;

    /**
     * @brief Flag indicating whether grid is enabled
     */
    bool _gridEnabled;

    /**
     * @brief Accumulated scroll position in X direction
     */
    int _accumulatedScrollX;

    /**
     * @brief Accumulated scroll position in Y direction
     */
    int _accumulatedScrollY;

    /**
     * @brief Label for displaying coordinates
     */
    QLabel *_coordinateLabel;

    /**
     * @brief WGS84 bounds - minimum longitude
     */
    double _minLon;

    /**
     * @brief WGS84 bounds - maximum longitude
     */
    double _maxLon;

    /**
     * @brief WGS84 bounds - minimum latitude
     */
    double _minLat;

    /**
     * @brief WGS84 bounds - maximum latitude
     */
    double _maxLat;

    /**
     * @brief Flag indicating whether projected coordinates
     * should be used
     */
    bool useProjectedCoords;

    Input::InteractionController *m_inputController = nullptr;
};

} // namespace GUI
} // namespace CargoNetSim
