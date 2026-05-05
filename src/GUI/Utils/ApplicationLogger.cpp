#include "ApplicationLogger.h"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QThread>
#include <QTimer>
#include "Backend/Commons/LogCategories.h"

namespace CargoNetSim
{
namespace GUI
{

// Initialize static members
ApplicationLogger *ApplicationLogger::s_instance = nullptr;
QMutex             ApplicationLogger::s_logMutex;
QMutex             ApplicationLogger::s_progressMutex;
QWaitCondition     ApplicationLogger::s_initCondition;
bool             ApplicationLogger::s_isInitialized = false;
QQueue<LogEntry> ApplicationLogger::s_logQueue;
QQueue<QPair<float, int>>
    ApplicationLogger::s_progressQueue;

// LogEntry implementation
LogEntry::LogEntry(const QString &message, int clientIndex,
                   bool isError, qint64 timestamp)
    : message(message)
    , clientIndex(clientIndex)
    , isError(isError)
    , timestamp(timestamp)
{
}

// LogEvent implementation
LogEvent::LogEvent(const LogEntry &entry)
    : QEvent(LogEventType)
    , entry(entry)
{
}

// ProgressEvent implementation
ProgressEvent::ProgressEvent(float value, int clientIndex)
    : QEvent(ProgressEventType)
    , value(value)
    , clientIndex(clientIndex)
{
}

ApplicationLogger *ApplicationLogger::getInstance()
{
    if (!s_instance)
    {
        s_instance = new ApplicationLogger();
    }
    return s_instance;
}

ApplicationLogger::ApplicationLogger()
    : m_isRunning(false)
{
    // Create client logs maps with default entries
    for (int i = 0; i < 5; ++i)
    {
        m_clientLogs[i]     = QStringList();
        m_clientProgress[i] = 0;
    }

    // Move to the main thread if created in a different
    // thread
    if (QThread::currentThread()
        != QApplication::instance()->thread())
    {
        moveToThread(QApplication::instance()->thread());
    }

    // Start the processing timers
    QTimer::singleShot(0, this, &ApplicationLogger::start);
}

void ApplicationLogger::start()
{
    if (m_isRunning)
    {
        qCDebug(lcGuiUtil) << "ApplicationLogger::start:"
                           << "already running";
        return;
    }

    m_isRunning = true;

    // Create timer for processing log queue
    QTimer *logTimer = new QTimer(this);
    connect(logTimer, &QTimer::timeout, this,
            &ApplicationLogger::processLogQueue);
    logTimer->start(100); // Process every 100 ms

    // Create timer for processing progress queue
    QTimer *progressTimer = new QTimer(this);
    connect(progressTimer, &QTimer::timeout, this,
            &ApplicationLogger::processProgressQueue);
    progressTimer->start(100); // Process every 100 ms

    qCInfo(lcGuiUtil) << "ApplicationLogger started";
}

void ApplicationLogger::stop()
{
    qCInfo(lcGuiUtil) << "ApplicationLogger::stop";
    m_isRunning = false;

    // Delete timers
    for (QTimer *timer : findChildren<QTimer *>())
    {
        timer->stop();
        timer->deleteLater();
    }
}

void ApplicationLogger::log(const QString &message,
                            int            clientType)
{
    logMessageInternal(message, clientType, false);
}

void ApplicationLogger::logError(const QString &message,
                                 int            clientType)
{
    logMessageInternal(message, clientType, true);
}

void ApplicationLogger::updateProgress(float progressValue,
                                       int   clientType)
{
    // Clamp progress value to 0-100 range
    progressValue = qBound(0.0f, progressValue, 100.0f);
    int progress  = static_cast<int>(progressValue);

    // Ensure valid client index (default to "general" log
    // at index 4)
    clientType = (clientType >= 0 && clientType < 4)
                     ? clientType
                     : 4;

    // Add to queue using thread-safe approach
    QMutexLocker locker(&s_progressMutex);
    s_progressQueue.enqueue(
        qMakePair(progressValue, clientType));

    // Post event to main thread if not already there
    if (QThread::currentThread() != getInstance()->thread())
    {
        QApplication::postEvent(
            getInstance(),
            new ProgressEvent(progressValue, clientType));
    }
    else
    {
        // Process immediately if already in the main thread
        getInstance()->m_clientProgress[clientType] =
            progress;
        emit getInstance()
            -> progressUpdated(progress, clientType);
    }
}

void ApplicationLogger::signalInitComplete()
{
    QMutexLocker locker(&s_logMutex);
    s_isInitialized = true;
    s_initCondition.wakeAll();
    emit getInstance() -> initializationComplete();
}

bool ApplicationLogger::waitForInitComplete(int timeoutMs)
{
    QMutexLocker locker(&s_logMutex);
    if (s_isInitialized)
    {
        return true;
    }

    return s_initCondition.wait(
        &s_logMutex, timeoutMs < 0 ? ULONG_MAX : timeoutMs);
}

void ApplicationLogger::processLogQueue()
{
    // Get pending log entries
    QList<LogEntry> entries;

    // Use a mutex to safely access the queue
    {
        QMutexLocker locker(&s_logMutex);
        while (!s_logQueue.isEmpty())
        {
            entries.append(s_logQueue.dequeue());
        }
    }

    if (!entries.isEmpty())
    {
        qCDebug(lcGuiUtil) << "ApplicationLogger::processLogQueue:"
                           << "processing" << entries.size() << "entries";
    }

    // Process each log entry
    foreach (const LogEntry &entry, entries)
    {
        appendLogEntry(entry);
    }
}

void ApplicationLogger::processProgressQueue()
{
    // Get pending progress updates
    QList<QPair<float, int>> updates;

    // Use a mutex to safely access the queue
    {
        QMutexLocker locker(&s_progressMutex);
        while (!s_progressQueue.isEmpty())
        {
            updates.append(s_progressQueue.dequeue());
        }
    }

    if (!updates.isEmpty())
    {
        qCDebug(lcGuiUtil) << "ApplicationLogger::processProgressQueue:"
                           << "processing" << updates.size() << "updates";
    }

    // Process each progress update
    for (const auto &update : updates)
    {
        int progress    = static_cast<int>(update.first);
        int clientIndex = update.second;

        m_clientProgress[clientIndex] = progress;
        emit progressUpdated(progress, clientIndex);
    }
}

void ApplicationLogger::logMessageInternal(
    const QString &message, int clientType, bool isError)
{
    // Ensure valid client index (default to "general" log
    // at index 4)
    clientType = (clientType >= 0 && clientType < 4)
                     ? clientType
                     : 4;
    // Create log entry with current timestamp
    qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
    LogEntry entry(message, clientType, isError, timestamp);

    // Add to queue using thread-safe approach
    QMutexLocker locker(&s_logMutex);
    s_logQueue.enqueue(entry);

    // Post event to main thread if not already there
    if (QThread::currentThread() != getInstance()->thread())
    {
        QApplication::postEvent(getInstance(),
                                new LogEvent(entry));
    }
    else
    {
        // Process immediately if already in the main thread
        getInstance()->appendLogEntry(entry);
    }
}

void ApplicationLogger::customEvent(QEvent *event)
{
    // Handle log events
    if (event->type() == LogEvent::LogEventType)
    {
        LogEvent *logEvent = static_cast<LogEvent *>(event);
        appendLogEntry(logEvent->entry);
        return;
    }

    // Handle progress events
    if (event->type() == ProgressEvent::ProgressEventType)
    {
        ProgressEvent *progressEvent =
            static_cast<ProgressEvent *>(event);
        m_clientProgress[progressEvent->clientIndex] =
            static_cast<int>(progressEvent->value);
        emit progressUpdated(
            static_cast<int>(progressEvent->value),
            progressEvent->clientIndex);
        return;
    }

    // Pass unhandled events to parent
    QObject::customEvent(event);
}

void ApplicationLogger::appendLogEntry(
    const LogEntry &entry)
{
    // Format timestamp
    QDateTime dateTime =
        QDateTime::fromMSecsSinceEpoch(entry.timestamp);
    QString timeStr =
        dateTime.toString("yyyy-MM-dd hh:mm:ss.zzz");

    // Create formatted log message
    QString formattedMessage =
        QString("[%1] %2").arg(timeStr, entry.message);

    // Add to client logs
    m_clientLogs[entry.clientIndex].append(
        formattedMessage);

    // Emit signal with the new message
    emit newLogMessage(entry.message, entry.clientIndex,
                       entry.isError);
}

bool ApplicationLogger::saveLogsToFile(
    const QString &filePath)
{
    qCInfo(lcGuiUtil) << "ApplicationLogger::saveLogsToFile:"
                      << "path=" << filePath;
    QFile file(filePath);

    // Ensure parent directory exists
    QDir parentDir = QFileInfo(file).dir();
    if (!parentDir.exists())
    {
        if (!parentDir.mkpath("."))
        {
            qCWarning(lcGuiUtil) << "ApplicationLogger::saveLogsToFile:"
                                 << "failed to create directory"
                                 << parentDir.path();
            return false;
        }
    }

    // Open file for writing
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qCWarning(lcGuiUtil) << "ApplicationLogger::saveLogsToFile:"
                             << "failed to open file"
                             << filePath << file.errorString();
        return false;
    }

    QTextStream out(&file);

    // Write header
    out << "=== CargoNetSim Log File ===" << Qt::endl;
    out << "Generated: "
        << QDateTime::currentDateTime().toString()
        << Qt::endl;
    out << "==========================" << Qt::endl
        << Qt::endl;

    // Client type names for better readability
    QMap<int, QString> clientNames;
    clientNames[0] = "Network";
    clientNames[1] = "Simulation";
    clientNames[2] = "GUI";
    clientNames[3] = "Database";
    clientNames[4] = "General";

    // Write logs for each client type
    for (int clientIndex = 0; clientIndex < 5;
         ++clientIndex)
    {
        if (m_clientLogs[clientIndex].isEmpty())
        {
            continue;
        }

        out << "=== " << clientNames[clientIndex]
            << " Logs ===" << Qt::endl;

        foreach (const QString &logEntry,
                 m_clientLogs[clientIndex])
        {
            out << logEntry << Qt::endl;
        }

        out << Qt::endl;
    }

    file.close();
    qCDebug(lcGuiUtil) << "ApplicationLogger::saveLogsToFile:"
                       << "successfully saved to" << filePath;
    return true;
}

} // namespace GUI
} // namespace CargoNetSim
