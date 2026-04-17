#pragma once

#include <QObject>
#include <QPointF>
#include <QColor>
#include <QString>

namespace CargoNetSim
{
namespace GUI
{

class GraphicsScene;
class SceneVisibilityController;
class MainWindow;
class StatusReporter;
class RegionCenterPoint;

class RegionController : public QObject
{
    Q_OBJECT

public:
    explicit RegionController(
        GraphicsScene               *regionScene,
        SceneVisibilityController   *sceneVisibility,
        MainWindow                  *mainWindow,
        StatusReporter              *status,
        QObject                     *parent = nullptr);

    /**
     * @brief Creates a new region center point in the scene
     * @param regionName The name of the region
     * @param color The color of the region
     * @param pos The position of the region center (default origin)
     * @param keepVisible Whether to keep the center visible
     * @return Pointer to the created RegionCenterPoint
     */
    RegionCenterPoint *
    createRegionCenter(const QString &regionName,
                       const QColor  &color,
                       const QPointF  pos = QPointF(0, 0),
                       const bool     keepVisible = false);

    /**
     * @brief Renames a region across all scene items
     * @param oldRegionName The current region name
     * @param newName The new region name
     */
    void renameRegion(const QString &oldRegionName,
                      const QString &newName);

protected:
    GraphicsScene             *m_regionScene;
    SceneVisibilityController *m_sceneVisibility;
    MainWindow                *m_mainWindow;
    StatusReporter            *m_status;
};

} // namespace GUI
} // namespace CargoNetSim
