#include "SimulationTime.h"
#include "Backend/Commons/LogCategories.h"

namespace CargoNetSim
{
namespace Backend
{

SimulationTime::SimulationTime(double   timeStep,
                               QObject *parent)
    : QObject(parent)
    , m_currentTime(0.0)
    , m_timeStep(timeStep)
{
    qCDebug(lcModel) << "SimulationTime: created with timeStep=" << timeStep;
}

double SimulationTime::getTimeStep() const
{
    return m_timeStep;
}

double SimulationTime::getCurrentTime() const
{
    return m_currentTime;
}

void SimulationTime::setTimeStep(double timeStep)
{
    if (qFuzzyCompare(m_timeStep, timeStep))
    {
        qCDebug(lcModel) << "SimulationTime::setTimeStep:"
                         << "skipping — value unchanged at" << timeStep;
        return;
    }

    m_timeStep = timeStep;
    emit timeStepChanged(m_timeStep);
}

void SimulationTime::advanceByTimeStep()
{
    m_currentTime += m_timeStep;
    qCDebug(lcModel) << "SimulationTime::advanceByTimeStep:"
                     << "time=" << m_currentTime
                     << "step=" << m_timeStep;
    emit currentTimeChanged(m_currentTime);
}

} // namespace Backend
} // namespace CargoNetSim
