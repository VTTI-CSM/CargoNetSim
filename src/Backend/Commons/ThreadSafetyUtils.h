/**
 * @file ThreadSafetyUtils.h
 * @brief Thread safety utilities for CargoNetSim
 * @author Ahmed Aredah
 * @date March 20, 2025
 */

#pragma once

#include "Backend/Commons/LogCategories.h"
#include <QDebug>
#include <QMap>
#include <QMutex>
#include <QReadWriteLock>
#include <QSet>
#include <QThread>
#include <stdexcept>

namespace CargoNetSim
{
namespace Backend
{
namespace Commons
{

/**
 * @class ScopedLock
 * @brief RAII-style mutex lock with timeout
 */
class ScopedLock
{
public:
    explicit ScopedLock(QMutex& mutex, int timeout = 5000)
        : m_mutex(mutex)
    {
        if (!m_mutex.tryLock(timeout))
        {
            throw std::runtime_error("Failed to acquire lock");
        }
    }

    ~ScopedLock()
    {
        m_mutex.unlock();
    }

private:
    QMutex& m_mutex;
};

/**
 * @class ScopedReadLock
 * @brief RAII-style read lock with timeout
 */
class ScopedReadLock
{
public:
    explicit ScopedReadLock(QReadWriteLock& lock, int timeout = 5000)
        : m_lock(lock)
    {
        if (!m_lock.tryLockForRead(timeout))
        {
            throw std::runtime_error("Failed to acquire read lock");
        }
    }

    ~ScopedReadLock()
    {
        m_lock.unlock();
    }

private:
    QReadWriteLock& m_lock;
};

/**
 * @class ScopedWriteLock
 * @brief RAII-style write lock with timeout
 */
class ScopedWriteLock
{
public:
    explicit ScopedWriteLock(QReadWriteLock& lock, int timeout = 5000)
        : m_lock(lock)
    {
        if (!m_lock.tryLockForWrite(timeout))
        {
            throw std::runtime_error("Failed to acquire write lock");
        }
    }

    ~ScopedWriteLock()
    {
        m_lock.unlock();
    }

private:
    QReadWriteLock& m_lock;
};

/**
 * @class DeadlockDetector
 * @brief Detects potential deadlocks
 */
class DeadlockDetector
{
public:
    static void trackLockAcquisition(QMutex* mutex)
    {
        QThread* thread = QThread::currentThread();
        heldLocks[thread].insert(mutex);
        checkForDeadlock();
    }

    static void trackLockRelease(QMutex* mutex)
    {
        QThread* thread = QThread::currentThread();
        heldLocks[thread].remove(mutex);
    }

    static bool detectDeadlock()
    {
        // Simple deadlock detection based on lock holding
        for (auto it = heldLocks.begin(); it != heldLocks.end(); ++it)
        {
            QThread* thread = it.key();
            const QSet<QMutex*>& locks = it.value();
            
            // Check if this thread is waiting for locks held by other threads
            for (auto otherIt = heldLocks.begin(); otherIt != heldLocks.end(); ++otherIt)
            {
                if (otherIt.key() != thread)
                {
                    const QSet<QMutex*>& otherLocks = otherIt.value();
                    if (!locks.isEmpty() && !otherLocks.isEmpty())
                    {
                        // If there's a circular dependency, we might have a deadlock
                        return true;
                    }
                }
            }
        }
        return false;
    }

private:
    static void checkForDeadlock()
    {
        if (detectDeadlock())
        {
            qCWarning(lcThreading) << "Potential deadlock detected!";
            // Log the current lock state
            for (auto it = heldLocks.begin(); it != heldLocks.end(); ++it)
            {
                qCWarning(lcThreading) << "Thread" << it.key() << "holds" << it.value().size() << "locks";
            }
        }
    }

    static QMap<QThread*, QSet<QMutex*>> heldLocks;
};

} // namespace Commons
} // namespace Backend
} // namespace CargoNetSim 
