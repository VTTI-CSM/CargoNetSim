#pragma once

#include <QMutex>
#include <QStringList>
#include <QVector>
#include <QWaitCondition>

#include "ExecutionPlanTypes.h"

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

class BufferedExecutionEventQueue
{
public:
    void pushEvent(const ExecutionEvent &event);
    void pushError(const QString &message);

    QVector<ExecutionEvent> drainEvents();
    QStringList drainErrors();

    bool isEmpty() const;
    bool waitForWork(int timeoutMs);

private:
    mutable QMutex          m_mutex;
    QWaitCondition          m_hasWork;
    QVector<ExecutionEvent> m_events;
    QStringList             m_errors;
};

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
