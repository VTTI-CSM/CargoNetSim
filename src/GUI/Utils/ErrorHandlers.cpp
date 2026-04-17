#include "ErrorHandlers.h"

#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QTextStream>
#include <QThread>
#include <QThreadPool>
#include <exception>
#include <iostream>
#include "Backend/Commons/LogCategories.h"

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <csignal>
#include <cstdlib>
#endif

namespace CargoNetSim
{
namespace GUI
{

ErrorHandlers::ErrorHandlers()
    : QObject(nullptr)
{
    // Ensure the logs directory exists
    QDir logsDir(QApplication::applicationDirPath()
                 + "/logs");
    if (!logsDir.exists())
    {
        logsDir.mkpath(".");
    }
}

ErrorHandlers &ErrorHandlers::getInstance()
{
    static ErrorHandlers instance;
    return instance;
}

void ErrorHandlers::installExceptionHandlers()
{
    qCInfo(lcGuiUtil) << "ErrorHandlers::installExceptionHandlers:"
                      << "installing handlers";
    // Set the Qt message handler
    qInstallMessageHandler(qtMessageHandler);

    // Set the C++ exception handler
    std::set_terminate([]() {
        try
        {
            std::exception_ptr eptr =
                std::current_exception();
            if (eptr)
            {
                try
                {
                    std::rethrow_exception(eptr);
                }
                catch (const std::exception &e)
                {
                    QString errorMsg =
                        QString(
                            "Unhandled C++ exception: %1")
                            .arg(e.what());
                    getInstance().errorOccurred(errorMsg,
                                                3); // Fatal
                    writeToErrorLog(errorMsg);

                    // Show error dialog if we have a GUI
                    if (QApplication::instance()
                        && QThread::currentThread()
                               == QApplication::instance()
                                      ->thread())
                    {
                        QMessageBox::critical(
                            nullptr, "Fatal Error",
                            QString("An unhandled "
                                    "exception occurred:"
                                    "\n%1\n\nThe "
                                    "application will now "
                                    "terminate.")
                                .arg(e.what()));
                    }

                    std::cerr << errorMsg.toStdString()
                              << std::endl;
                }
            }
        }
        catch (...)
        {
            // Last resort
            QString errorMsg =
                "Unhandled exception of unknown type";
            getInstance().errorOccurred(errorMsg,
                                        3); // Fatal
            writeToErrorLog(errorMsg);
            std::cerr << errorMsg.toStdString()
                      << std::endl;
        }
        std::abort();
    });

    // For Qt6, we don't have direct thread pool exception
    // handling Instead log a message about proper thread
    // handling
    qCInfo(lcGuiUtil) << "Exception handlers installed!";
}

void ErrorHandlers::handleException(
    int exceptionType, void *exceptionValue,
    void *exceptionTraceback)
{
    QString errorMessage =
        QString("Uncaught Python-like exception (type %1)")
            .arg(exceptionType);

    // Log the error
    writeToErrorLog(errorMessage);

    // Emit signal for UI components
    getInstance().errorOccurred(errorMessage,
                                3); // Fatal severity

    // Show error dialog if in main thread
    if (QApplication::instance()
        && QThread::currentThread()
               == QApplication::instance()->thread())
    {
        QMessageBox::critical(
            nullptr, "Fatal Error",
            QString("An unhandled exception occurred."
                    "\n\nThe application will now "
                    "terminate."));
    }

    // Print to stderr
    std::cerr << errorMessage.toStdString() << std::endl;

    // Terminate the application
    std::abort();
}

void ErrorHandlers::qtMessageHandler(
    QtMsgType type, const QMessageLogContext &context,
    const QString &message)
{
    QString formattedMessage;

    // Format based on message type
    switch (type)
    {
    case QtDebugMsg:
        formattedMessage =
            QString("[Debug] %1").arg(message);
        break;
    case QtInfoMsg:
        formattedMessage =
            QString("[Info] %1").arg(message);
        break;
    case QtWarningMsg:
        formattedMessage =
            QString("[Warning] %1 (%2:%3)")
                .arg(message, context.file,
                     QString::number(context.line));
        getInstance().errorOccurred(formattedMessage,
                                    1); // Warning severity
        break;
    case QtCriticalMsg:
        formattedMessage =
            QString("[Critical] %1 (%2:%3)")
                .arg(message, context.file,
                     QString::number(context.line));
        getInstance().errorOccurred(formattedMessage,
                                    2); // Error severity
        break;
    case QtFatalMsg:
        formattedMessage =
            QString("[Fatal] %1 (%2:%3)")
                .arg(message, context.file,
                     QString::number(context.line));
        getInstance().errorOccurred(formattedMessage,
                                    3); // Fatal severity
        writeToErrorLog(formattedMessage);

        // Show error dialog if in main thread
        if (QApplication::instance()
            && QThread::currentThread()
                   == QApplication::instance()->thread())
        {
            QMessageBox::critical(
                nullptr, "Fatal Error",
                QString("A fatal error occurred:"
                        "\n%1\n\nThe application will now "
                        "terminate.")
                    .arg(message));
        }

        std::abort();
    }

    // Always output to console
    std::cerr << formattedMessage.toStdString()
              << std::endl;

    // For warnings and above, also log to file
    if (type >= QtWarningMsg)
    {
        writeToErrorLog(formattedMessage);
    }
}

void ErrorHandlers::writeToErrorLog(
    const QString &errorText)
{
    QString logPath = QApplication::applicationDirPath()
                      + "/logs/error_log.txt";
    QFile logFile(logPath);

    if (logFile.open(QIODevice::WriteOnly
                     | QIODevice::Append | QIODevice::Text))
    {
        QTextStream stream(&logFile);

        stream << "\n==============================="
                  "===========================\n";
        stream << "Timestamp: "
               << QDateTime::currentDateTime().toString(
                      "yyyy-MM-dd hh:mm:ss")
               << "\n";
        stream << errorText << "\n";

        logFile.close();
        // NOTE: Cannot use qCDebug here -- this function is
        // called from qtMessageHandler and would recurse.
    }
    else
    {
        // NOTE: Cannot use qCWarning here -- this function is
        // called from qtMessageHandler and would recurse.
        std::cerr << "Failed to write to error log: "
                  << logPath.toStdString() << std::endl;
    }
}

// SafeRunnable implementation
void SafeRunnable::run()
{
    try
    {
        runSafe();
    }
    catch (const std::exception &e)
    {
        QString errorMsg =
            QString("Exception in thread: %1")
                .arg(e.what());
        ErrorHandlers::getInstance().errorOccurred(
            errorMsg, 2); // Error
        ErrorHandlers::writeToErrorLog(errorMsg);
    }
    catch (...)
    {
        QString errorMsg = "Unknown exception in thread";
        ErrorHandlers::getInstance().errorOccurred(
            errorMsg, 2); // Error
        ErrorHandlers::writeToErrorLog(errorMsg);
    }
}

// Global function implementation
void installExceptionHandlers()
{
    ErrorHandlers::installExceptionHandlers();
}

} // namespace GUI
} // namespace CargoNetSim
