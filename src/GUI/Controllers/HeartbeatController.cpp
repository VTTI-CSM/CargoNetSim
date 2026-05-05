#include "HeartbeatController.h"
#include "../MainWindow.h"
#include "Backend/Application/AvailabilityService.h"
#include "Backend/Controllers/CargoNetSimController.h"
#include <QDateTime>
#include <QDebug>
#include <QLabel>
#include <QMap>
#include <QThread>
#include <QTimer>
#include <QVariant>
#include <QtWidgets/qstatusbar.h>
#include <exception>
#include "Backend/Commons/LogCategories.h"

namespace CargoNetSim
{
namespace GUI
{

HeartbeatController::HeartbeatController(
    MainWindow *mainWindow)
    : QObject(mainWindow)
    , mainWindow(mainWindow)
    , consumerCheckTimer(nullptr)
    , monitorThread(nullptr)
    , isRunning(false)
    , lastConsumerCheck(0)
    , consumerCheckInterval(20) // Check every 20 seconds
{
    qCDebug(lcGuiHeartbeat) << "HeartbeatController::HeartbeatController: begin";
    // Initialize server indicators mapping from MainWindow
    // status bar
    QStatusBar *statusBar = mainWindow->statusBar();

    // Define servers with their IDs - these must match the
    // IDs in MainWindow.cpp
    QStringList serverIDs = {"TerminalSim", "NeTrainSim",
                             "ShipNetSim", "INTEGRATION"};

    // Find server indicators in the status bar
    for (const QString &serverId : serverIDs)
    {
        QMap<QString, QVariant> indicatorData;

        // Find the indicator label for this server
        QList<QLabel *> labels =
            statusBar->findChildren<QLabel *>();
        for (QLabel *label : labels)
        {
            if (label->toolTip().contains(serverId)
                || (label->size() == QSize(10, 10)
                    && label->parent()
                               ->findChild<QLabel *>(
                                   QString(),
                                   Qt::FindDirectChildrenOnly)
                               ->text()
                           == serverId))
            {
                // This is the indicator for this server
                indicatorData["indicator"] =
                    QVariant::fromValue<QLabel *>(label);
                indicatorData["description"] = serverId;
                serverIndicators[serverId] = indicatorData;
                break;
            }
        }
    }
}

HeartbeatController::~HeartbeatController()
{
    if (monitorThread)
    {
        isRunning = false;
        monitorThread->quit();
        monitorThread->wait();
        delete monitorThread;
    }

    if (consumerCheckTimer)
    {
        consumerCheckTimer->stop();
        delete consumerCheckTimer;
    }
}

void HeartbeatController::initialize()
{
    qCInfo(lcGuiHeartbeat) << "HeartbeatController::initialize: begin";
    // Make sure we have our server indicators set up
    if (serverIndicators.isEmpty())
    {
        QStringList serverIDs = {"TerminalSim",
                                 "NeTrainSim", "ShipNetSim",
                                 "INTEGRATION"};
        for (const QString &serverId : serverIDs)
        {
            QMap<QString, QVariant> indicatorData;
            indicatorData["description"] = serverId;
            serverIndicators[serverId]   = indicatorData;
        }
    }

    // Initial server status check
    checkQueueConsumers();

    // Create and start monitor thread for periodic checks
    qCDebug(lcGuiHeartbeat) << "HeartbeatController::initialize: creating monitor thread and timer";
    monitorThread = new QThread();

    // Create a timer for the monitoring thread
    consumerCheckTimer = new QTimer(nullptr);
    consumerCheckTimer->setInterval(consumerCheckInterval
                                    * 1000);
    consumerCheckTimer->moveToThread(monitorThread);

    connect(
        monitorThread, &QThread::started,
        consumerCheckTimer,
        static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(consumerCheckTimer, &QTimer::timeout, this,
            &HeartbeatController::checkQueueConsumers);
    connect(monitorThread, &QThread::finished,
            consumerCheckTimer, &QTimer::stop);
    connect(monitorThread, &QThread::finished,
            consumerCheckTimer, &QTimer::deleteLater);

    isRunning = true;
    monitorThread->start();
}

void HeartbeatController::checkQueueConsumers()
{
    qint64 currentTime =
        QDateTime::currentDateTime().toMSecsSinceEpoch();
    if (currentTime - lastConsumerCheck
        < (consumerCheckInterval * 1000))
    {
        return;
    }
    lastConsumerCheck = currentTime;

    const auto statuses =
        Backend::Application::AvailabilityService()
            .pollAll();
    QStringList changedServers;
    qCDebug(lcGuiHeartbeat) << "HeartbeatController::checkQueueConsumers:"
                            << "serversPolled=" << statuses.size();

    for (const auto &status : statuses)
    {
        const bool ok = status.commandAvailable;
        const bool hadPrevious =
            activeConsumers.contains(status.server);
        const bool previous =
            activeConsumers.value(status.server, false);
        qCDebug(lcGuiHeartbeat) << "HeartbeatController::checkQueueConsumers:"
                                << "server=" << status.server
                                << "connected=" << ok;
        updateServerStatus(status.server, ok);
        if (!hadPrevious || previous != ok)
            changedServers.append(status.server);
        activeConsumers[status.server] = ok;
    }

    if (!changedServers.isEmpty())
    {
        qCInfo(lcGuiHeartbeat)
            << "HeartbeatController::checkQueueConsumers:"
            << "availability changed for" << changedServers;
        emit backendAvailabilityChanged();
    }
}

void HeartbeatController::updateServerStatus(
    const QString &serverId, bool connected)
{

    if (!serverIndicators.contains(serverId))
    {
        qCWarning(lcGuiHeartbeat) << "Server indicator not found for"
                                  << serverId;
        return;
    }

    // Get the indicator label
    QLabel *indicator = nullptr;

    if (serverIndicators[serverId].contains("indicator"))
    {
        indicator = serverIndicators[serverId]["indicator"]
                        .value<QLabel *>();
    }

    if (!indicator)
    {
        // Try to find the indicator in the main window's
        // status bar
        QList<QLabel *> labels =
            mainWindow->statusBar()
                ->findChildren<QLabel *>();

        for (QLabel *label : labels)
        {

            if (label->size() == QSize(10, 10))
            {
                // Check if this small dot label is part of
                // a layout containing the server name
                QWidget *parent = qobject_cast<QWidget *>(
                    label->parent());
                if (parent)
                {
                    // Find all labels in this parent
                    // container
                    QList<QLabel *> siblingLabels =
                        parent->findChildren<QLabel *>(
                            QString(),
                            Qt::FindDirectChildrenOnly);

                    for (QLabel *siblingLabel :
                         siblingLabels)
                    {
                        if (siblingLabel != label
                            && siblingLabel->text()
                                   == serverId)
                        {
                            // Found the right indicator
                            indicator = label;
                            serverIndicators
                                [serverId]["indicator"] =
                                    QVariant::fromValue<
                                        QLabel *>(
                                        indicator);
                            break;
                        }
                    }
                }

                if (indicator)
                    break; // Exit outer loop if indicator
                           // found
            }
        }
    }

    if (!indicator)
    {
        qCWarning(lcGuiHeartbeat) << "Could not find indicator label for"
                                  << serverId;
        return;
    }

    qCDebug(lcGuiHeartbeat) << "HeartbeatController::updateServerStatus:"
                            << "server=" << serverId
                            << "connected=" << connected;
    // Update the indicator color based on connection status
    if (connected)
    {
        indicator->setStyleSheet(
            "background-color: #00ff00; border-radius: "
            "5px;");
        indicator->setToolTip(
            serverIndicators[serverId]["description"]
                .toString()
            + " - Connected");
    }
    else
    {
        indicator->setStyleSheet(
            "background-color: #ff0000; border-radius: "
            "5px;");
        indicator->setToolTip(
            serverIndicators[serverId]["description"]
                .toString()
            + " - Disconnected");
    }
}

} // namespace GUI
} // namespace CargoNetSim
