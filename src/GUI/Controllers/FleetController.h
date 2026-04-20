#pragma once

#include <QObject>
#include <QStringList>

namespace CargoNetSim
{
namespace GUI
{

class MainWindow;

class FleetController : public QObject
{
    Q_OBJECT
public:
    explicit FleetController(MainWindow *mainWindow,
                             QObject    *parent = nullptr);

    /// Append @p paths to doc.fleet.trainsFiles and write back through
    /// ScenarioMutator. No-op if paths is empty or runtime is null.
    void appendTrainFiles(const QStringList &paths);

    /// Append @p paths to doc.fleet.shipsFiles and write back through
    /// ScenarioMutator. No-op if paths is empty or runtime is null.
    void appendShipFiles(const QStringList &paths);

private:
    MainWindow *m_mainWindow;
};

} // namespace GUI
} // namespace CargoNetSim
