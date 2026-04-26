#pragma once

#include "Backend/Commons/Units.h"

#include <QMetaType>
#include <QObject>
#include <QThread>

namespace CargoNetSim
{
namespace Backend
{

class SimulationTime : public QObject
{
    Q_OBJECT
    Q_PROPERTY(double timeStep READ getTimeStep WRITE
                   setTimeStep NOTIFY timeStepChanged)
    Q_PROPERTY(double getCurrentTime READ getCurrentTime
                   NOTIFY currentTimeChanged)

public:
    explicit SimulationTime(double   timeStep = 60.0,
                            QObject *parent   = nullptr);
    ~SimulationTime() override = default;

    // Property getters
    double getTimeStep() const;
    double getCurrentTime() const;

    Units::TimeSeconds timeStepUnits() const
    {
        return Units::seconds(getTimeStep());
    }

    Units::TimeSeconds currentTimeUnits() const
    {
        return Units::seconds(getCurrentTime());
    }

public slots:
    // Property setters
    void setTimeStep(double timeStep);
    void advanceByTimeStep();

public:
    void setTimeStepUnits(Units::TimeSeconds timeStep)
    {
        setTimeStep(timeStep.value());
    }

signals:
    void timeStepChanged(double newTimeStep);
    void currentTimeChanged(double newTime);

private:
    double m_currentTime;
    double m_timeStep;
};

} // namespace Backend
} // namespace CargoNetSim

// Declare the type for QMetaType system
Q_DECLARE_METATYPE(CargoNetSim::Backend::SimulationTime)
Q_DECLARE_METATYPE(CargoNetSim::Backend::SimulationTime *)
