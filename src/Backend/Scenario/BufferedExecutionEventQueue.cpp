#include "BufferedExecutionEventQueue.h"

#include <QMutexLocker>

namespace CargoNetSim
{
namespace Backend
{
namespace Scenario
{

void BufferedExecutionEventQueue::pushEvent(
    const ExecutionEvent &event)
{
    QMutexLocker locker(&m_mutex);
    m_events.append(event);
    m_hasWork.wakeAll();
}

void BufferedExecutionEventQueue::pushError(
    const QString &message)
{
    QMutexLocker locker(&m_mutex);
    m_errors.append(message);
    m_hasWork.wakeAll();
}

QVector<ExecutionEvent>
BufferedExecutionEventQueue::drainEvents()
{
    QMutexLocker locker(&m_mutex);
    QVector<ExecutionEvent> drained;
    drained.swap(m_events);
    return drained;
}

QStringList BufferedExecutionEventQueue::drainErrors()
{
    QMutexLocker locker(&m_mutex);
    QStringList drained;
    drained.swap(m_errors);
    return drained;
}

bool BufferedExecutionEventQueue::isEmpty() const
{
    QMutexLocker locker(&m_mutex);
    return m_events.isEmpty() && m_errors.isEmpty();
}

bool BufferedExecutionEventQueue::waitForWork(int timeoutMs)
{
    QMutexLocker locker(&m_mutex);
    if (!m_events.isEmpty() || !m_errors.isEmpty())
        return true;

    if (timeoutMs <= 0)
    {
        m_hasWork.wait(&m_mutex);
    }
    else
    {
        m_hasWork.wait(&m_mutex, timeoutMs);
    }

    return !m_events.isEmpty() || !m_errors.isEmpty();
}

} // namespace Scenario
} // namespace Backend
} // namespace CargoNetSim
